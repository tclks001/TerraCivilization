# TerraCivilization SimpleGameplay C3.6 Q/E 绕焦点法线旋转视角设计稿

> 本稿从 [CameraTrackingAndInteractionDesign.md](CameraTrackingAndInteractionDesign.md) 的 C3.6 阶段拆出。
>
> 前置阶段：[C3FocusCameraManualControlDesign.md](C3FocusCameraManualControlDesign.md) 与 [C3_5IdleFrameCameraStabilityDesign.md](C3_5IdleFrameCameraStabilityDesign.md)。
>
> C3.6 只做一件事：**新增 `Q/E` 让玩家绕当前视野中心法线旋转相机方位**，其它一切（WSAD、滚轮、Tilt、Distance、焦点位置）均不受影响。

---

## 1. 目标

1. 按下 `Q` 或 `E` 时，相机绕**当前视野中心的球面外法线**旋转，相机与视野中心的连线改变，玩家从另一个方位看向同一视野中心。
2. `TiltDeg` 保持不变——视线与视野中心切平面的夹角不变。
3. `DistanceToFocusCM` 保持不变——相机到视野中心的距离不变。
4. `FocusUnitDir` 保持不变——视野中心在球面上的位置不变，屏幕正中依然对准同一片地形。
5. **QE 之后，WSAD 仍以新视角为参考系**：`W` 是"新视角下的向前推进"，`A/D` 是"新视角下的向左/向右"。玩家不需要在心里做角度换算。
6. 与 C3、C3.5 的既有语义完全兼容，不重新引入自反闭环、不影响无输入帧稳态。

---

## 2. 状态与参考系

### 2.1 复用 C3 内部真值

C3.6 不新增任何持久状态，只修改 C3 已有的 `YawAroundFocusDeg`：

| 字段 | C3.6 是否修改 |
| --- | --- |
| `FocusUnitDir` | 否 |
| `DistanceToFocusCM` | 否 |
| `TiltDeg` | 否 |
| `YawAroundFocusDeg` | **是**——按 `Q/E` 每帧增减 |

### 2.2 为什么改 Yaw 就够了

回顾 [C3FocusCameraManualControlDesign.md](C3FocusCameraManualControlDesign.md) §3 与 §5.1.1：

```text
Up             = FocusUnitDir
East           = Normalize(Cross(WorldZ, Up))
North          = Normalize(Cross(Up, East))
ForwardTangent = cos(Yaw) * North + sin(Yaw) * East
Camera         = FocusWorld - ForwardTangent * Distance * cos(Tilt) + Up * Distance * sin(Tilt)
```

只调整 `Yaw` 时：

- `Up / East / North` 都由 `FocusUnitDir` 决定，与 `Yaw` 无关；不变。
- `ForwardTangent` 在焦点切平面里绕 `Up` 旋转 `ΔYaw`。
- `Camera` = 焦点 − `ForwardTangent · Distance · cos(Tilt)` + `Up · Distance · sin(Tilt)`：
  - 上抬分量 `Up · Distance · sin(Tilt)` 与 Yaw 无关，Tilt 不变则不变——**"离开焦点法线方向的高度"**保持不变。
  - 后退分量 `ForwardTangent · Distance · cos(Tilt)` 只是绕 `Up` 转了 `ΔYaw`。
- 因此相机确实是在**以 `FocusUnitDir` 为轴、以 `Distance · cos(Tilt)` 为半径的水平圆**上滑动，视野中心与到视野中心的距离都不变。

这就是标题所说的"以视野中心法线为旋转轴，旋转摄像机的位置"，且由已有几何自动满足条件 1~4。

### 2.3 WSAD 为何自动跟随新视角

C3 §5.1.1 里 `ForwardTangent = cos(Yaw)·North + sin(Yaw)·East` 已经把 `Yaw` 当作**屏幕参考系相对当地 (North, East) 基的方位角**。改 `Yaw` 相当于把整个屏幕参考系整体绕 `Up` 转了同一个角度：

- `W`：焦点沿 `+ForwardTangent` 方向的大圆推进 = 屏幕正上方向对应的方向。
- `S / A / D`：分别对应 `−ForwardTangent`、`−RightTangent`、`+RightTangent`。

`ForwardTangent / RightTangent` 都由 `Yaw` 派生，因此 `Yaw` 变了以后：

- 玩家看到的"屏幕上"就是新的 `+ForwardTangent`，`W` 依然把地形从屏幕上方拉过来。
- 玩家看到的"屏幕右"就是新的 `+RightTangent`，`D` 依然把地形从屏幕右方拉过来。

**结论：C3.6 不需要改 WSAD 一行代码。** WSAD 天然以视角为参考系，因为它从一开始就以 `Yaw` 为参考系，而 `Yaw` 就是视角方位。

---

## 3. 输入语义

| 输入 | 行为 |
| --- | --- |
| `E` | `YawAroundFocusDeg += G8CameraOrbitDegreesPerSecond * DeltaTime` |
| `Q` | `YawAroundFocusDeg -= G8CameraOrbitDegreesPerSecond * DeltaTime` |

方向约定：

- `E` 让相机在焦点周围**顺时针**（从 `Up` 向下俯视看）滑动，画面里的地形随之向左横移；玩家感觉"从更右边看向同一点"。
- `Q` 反之。

这个方向来自 §2.2 的定义：`Yaw` 增大 → `ForwardTangent` 从 `+North` 向 `+East` 转 → 相机围绕焦点朝 East 侧滑。若实机手感与直觉相反，只需在实现里翻一次符号，不影响本设计稿其它部分。

**旋转速度**沿用 C3 已有的 `G8CameraOrbitDegreesPerSecond`，与 WSAD 保持同一节奏，避免调参分裂。若未来玩家反馈太快/太慢，再单独抽出 `C3YawSpinDegreesPerSecond` 也可，但 C3.6 初版不新增字段。

**Yaw 环绕**：仍走 `FRotator::NormalizeAxis`，把 `Yaw` 归一化到 `[-180, 180]`，避免长时间按住 QE 后浮点累计精度衰减。

**QE 与 WSAD 同帧**的处理：允许玩家同帧同时按下 QE 与 WSAD。C3.6 的规则是——**先算 QE 更新 `Yaw`，再用更新后的 `Yaw` 让 WSAD 走当帧的切向推进**。这样"边转视角边前进"是连续的球面运动，不会出现"WSAD 用旧 Yaw、QE 结果下一帧才生效"的一帧滞后。

---

## 4. 与既有阶段的兼容性

### 4.1 与 C3 §4.2 的自反闭环无关

C3 §4.2 反对的是"每帧从相机反解 `Yaw / FocusUnitDir` 覆盖真值"。C3.6 只是让**玩家输入**修改真值，不涉及从相机反解，因此不会引入闭环。

### 4.2 与 C3.5 每帧 Apply 兼容

C3.5 已经把 Apply 改为每帧无条件执行。C3.6 只在原本更新 `Yaw` 的位置多加两个 `IsInputKeyDown(Q/E)` 分支，其它部分不动；C3.5 的稳态锁自然覆盖 QE 之后的姿态。

### 4.3 与 Q/E 在总设计稿中的历史声明

在 [C3FocusCameraManualControlDesign.md](C3FocusCameraManualControlDesign.md) §8 "暂不处理"和 [CameraTrackingAndInteractionDesign.md](CameraTrackingAndInteractionDesign.md) §7.3 里，`Q/E` 曾被列为"绕焦点旋转镜头"暂不实现项。C3.6 落地后需要把这两处的 `Q/E` 从"暂不处理"移除，改指向本稿。

---

## 5. 落地范围

### 5.1 修改文件

- `Source/TerraCivilization/Private/Interaction/PlanetInteractionController.cpp`
  - `UpdateC3FocusCameraControl_`：在 W/S/A/D 处理块之后、Offset 调用之前，新增：
    ```cpp
    if (IsInputKeyDown(EKeys::E)) { C3YawAroundFocusDeg += OrbitDeltaDeg; }
    if (IsInputKeyDown(EKeys::Q)) { C3YawAroundFocusDeg -= OrbitDeltaDeg; }
    ```
  - 其余部分不动。

### 5.2 不修改

- `PlanetTessellatedMesh.h / .cpp`：三条焦点式相机函数签名与实现不动。
- `PlanetInteractionController.h`：字段不新增。
- 输入映射：不新增。
- WSAD 逻辑：不新增、不调整——它天然以 `Yaw` 为参考系，Yaw 变则参考系变。

### 5.3 兼容性

- 与 C3.5 每帧 Apply 兼容：Apply 已在 QE 更新 Yaw 之后统一发生。
- 与 C2 自动切镜兼容：QE 只作用于手动模式；未初始化状态下 Update 会先走 Init 再进入手动。
- 与未来 C4 智能选中聚焦兼容：C4 若需在 Blend 期间接管相机，可像 C3.5 §4.5 描述的那样通过所有权开关暂停 C3 手动 Update；QE 与 WSAD 一起被暂停。

---

## 6. 验收

1. 相机稳定看着一个中低纬度地形时按住 `E`：相机绕焦点法线在同一水平圆上滑动，地形在屏幕正中不动，视线倾角不变，画面从"从北看"平滑变成"从东看 / 从南看 / 从西看"最终回到起点。
2. 反向按住 `Q`：与 1 相反方向。
3. 转到任意方位后停下，按 `W`：焦点应向**当前屏幕上方**推进，而不是回到 Yaw=0 前的旧"上方"。
4. 转到任意方位后按 `A/D`：焦点应向**当前屏幕左/右方**推进，语义与 C3 §7.10 保持一致（`A` 让画面向屏幕右滑）。
5. 同时按住 `E + W`：相机绕焦点滑动的同时，焦点沿"当前屏幕上方对应的"大圆前进；轨迹应连续。
6. 靠近极点时按 `Q/E`：因为旋转轴 `FocusUnitDir` 在极点仍是良定义的（Z 分量趋近 ±1，但 `FocusUnitDir` 本身没有奇异），QE 应仍工作正常；只在 §7 讨论的极端情况中略有退化。
7. C3 §7、C3.5 §6 全部原有验收条款仍成立。

---

## 7. 极端与暂不处理

- **`FocusUnitDir` 恰在南北极点**：此时 `East = Cross(WorldZ, Up)` 会退化，Apply 内部会 fallback 到 `Cross(WorldY, Up)`，`North` 随之重算。QE 的语义仍是"绕 `Up` 旋转"，几何上正常；但 `Yaw = 0` 对应的"North"方向会因为退化基而与非极区不连续。这是 C3 §5.1.5 已经承认的极点参数化奇异，不由 C3.6 单独处理，也不阻挡验收 6。
- **`Tilt = 90°`**：此时相机正上方看焦点，`ForwardTangent · Distance · cos(Tilt) = 0`，QE 只是原地转屏幕方位（相机绕自身）。这在几何上合法，但玩家会觉得"没转"。C3 已经把 Tilt clamp 到 `[5°, 85°]`，不会真到 90°，因此不构成实际问题。
- **鼠标拖拽绕焦点旋转**：仍归后续阶段。C3.6 只做键盘 QE。
- **单独的 QE 速度参数**：暂不新增，共用 `G8CameraOrbitDegreesPerSecond`。
