# TerraCivilization SimpleGameplay C3.5 无输入帧相机稳态设计稿

> 本稿从 [CameraTrackingAndInteractionDesign.md](CameraTrackingAndInteractionDesign.md) 的 C3.5 阶段拆出。
>
> 前置阶段：[C3FocusCameraManualControlDesign.md](C3FocusCameraManualControlDesign.md)。
>
> C3.5 只做一件事：**修复 C3 收尾时发现的"松开 WSAD 后相机会缓慢漂移"的副作用**。C3.5 不引入新的输入语义、不修改焦点推进公式。

---

## 1. 背景

C3 阶段落地了 "3D 单位向量 + 平行运输" 的焦点式手动相机。为消灭旧实现里的自反闭环，C3 明确了 §4.2：**手动模式初始化后，不再每帧从相机反解真值**。

配套的实现里，`APlanetInteractionController::UpdateC3FocusCameraControl_` 只在 W/S/A/D/滚轮任一有输入时才走 `ApplyFocusCameraState`；无输入帧直接 `return`。

C3 收尾自测时观察到：

- 松开 W/S/A/D 后，相机会缓慢偏开焦点。
- 再次按下 W/S/A/D 的瞬间，Apply 一执行就把相机拉回焦点、显得"正常"。
- 松开后又会重新开始漂。

这不是 C3 焦点数学的问题（`FocusUnitDir / Yaw` 保持不变），而是**无输入帧 C3 完全不摆相机**导致的另一个隐藏 bug 浮现。

---

## 2. 现象根因

### 2.1 漂移来源

漂移不是 C3 自己动的。C3 无输入帧根本不写 `ControlRotation` 或 ViewTarget 姿态。真正在动相机的可能来源：

1. **DefaultPawn / SpectatorPawn 的默认姿态回写**：UE 里 `ADefaultPawn` 及派生 Pawn 会在 Tick 中把 `PlayerController.ControlRotation` 应用回自身 Actor 旋转，或者反过来把 Pawn 姿态推回 ControlRotation。这层同步在无输入帧仍在跑。
2. **未屏蔽的 look input**：`APlanetInteractionController::BeginPlay` 里只调了 `SetIgnoreMoveInput(true)`，**没有** `SetIgnoreLookInput(true)`。任何未识别的 axis 输入（例如鼠标微小移动映射到默认 look 通道，或输入设备噪声）都会推动 `ControlRotation`。
3. **PlayerCameraManager 的默认平滑 / 插值**：某些相机管理器配置会对 `ControlRotation → CameraRotation` 做插值。C3 在有输入帧强行 `SetActorRotation` 会瞬间对齐，无输入帧插值继续跑就会偏。

三种来源不必逐一定位。C3.5 采取"堵源头"策略：**让 C3 手动模式下的相机姿态每帧都被显式锚定**，不给外部驱动源留窗口。

### 2.2 为什么旧实现看不到这个漂移

旧实现每帧都跑 `SyncFocusCameraStateFromView + ApplyFocusCameraState`。虽然 Sync 有自反闭环 bug（导致沿纬线走），但**每帧 Apply 也顺带把相机重新锚回焦点**，无形中掩盖了外部驱动源的存在。

C3 拿掉每帧 Sync 后，Apply 也随之退成"仅在有输入时执行"，这层隐式锚就消失了，漂移浮现。

---

## 3. 目标

1. C3 手动模式下，无论玩家是否输入，相机每帧都被严格锚定到当前 `(FocusUnitDir, Yaw, Tilt, Distance)`。
2. 松开 W/S/A/D 的瞬间与松开前最后一帧姿态完全一致，且此后保持不动。
3. **不重新引入 C3 已经消灭的"每帧从相机重解真值"闭环**——C3.5 只每帧 Apply（写入相机），不每帧 Sync（从相机读回真值）。
4. 与 C4 智能选中聚焦兼容：本稿的稳态锁仅对 C3 手动状态生效，未来自动镜头 Blend 可以在 Blend 期间临时接管，Blend 结束再交回 C3。

---

## 4. 方案

### 4.1 每帧 Apply

`APlanetInteractionController::UpdateC3FocusCameraControl_` 的更新流程改为：

```text
if (未初始化) 首帧 Sync 一次；否则跳过 Sync
读取 W/S/A/D/滚轮输入
if (有 W/S/A/D 增量) 沿测地线推进 FocusUnitDir、平行运输 Yaw
if (有滚轮) 更新 Distance
Clamp Distance / Tilt，Normalize Yaw / FocusUnitDir，派生调试经纬度
每帧无条件 Apply：Tess->ApplyFocusCameraState(FocusUnitDir, Distance, Tilt, Yaw)
```

关键差异：**取消"无输入直接 return"的短路**。有没有输入都走一次 Apply。

### 4.2 为什么这不会引起 C3 §4.2 讨厌的自反闭环

C3 §4.2 反对的是"每帧从相机反解 `Yaw / FocusUnitDir` 覆盖真值"。它反对的是 **写入 → 读回 → 覆盖真值** 这条闭环里的"读回覆盖"步骤。

C3.5 只是"每帧写入一次"（Apply），从不"读回覆盖"（Sync）。真值只由玩家输入修改。因此 C3.5 与 C3 §4.2 的策略完全兼容，不会重新引入沿纬线走 / 极点卡死的旧 bug。

### 4.3 为什么不改用 SetIgnoreLookInput / 自定义 Pawn

这些都是"堵局部源"的做法：

- `SetIgnoreLookInput(true)` 只屏蔽玩家 look input，屏蔽不了 DefaultPawn 姿态回写、也屏蔽不了 CameraManager 的插值。
- 换自定义 Pawn 侵入面大，且未来 C4 / C6 需要相机跟随时反而要自己实现回来。

"每帧 Apply"是**最小、最正交、最不侵入其他阶段**的做法：C3.5 只加一次函数调用，不引入新组件、不改 Pawn 类型。将来若真要引入自定义 Pawn 或屏蔽 look input，也不会跟本稿冲突——它们只是让"每帧 Apply"在无外部驱动的场景下无害。

### 4.4 性能

`ApplyFocusCameraState` 内部只做几次向量运算和一次 `SetActorLocation / SetActorRotation / SetControlRotation`。每帧 Apply 的成本是常数级，与 HISM 每帧的开销相比可以忽略。

### 4.5 与 C4 / 未来自动镜头 Blend 的接口

C4 会引入"点击棋子时智能聚焦"的自动镜头。届时的语义会是：

- Blend 期间，相机由自动镜头驱动，C3 手动状态**不写相机**（也就是 Update 里 skip 一次 Apply）。
- Blend 结束时，把最终的相机姿态 Sync 回 C3 手动状态（复用 §4.1 的首帧反解入口）。
- 之后 C3 手动状态重新每帧 Apply 直到玩家下一次触发自动镜头。

C3.5 的"每帧 Apply"只是把默认行为改成"锚定"，未来 C4 通过一个 `bC3ManualCameraOwnsView` 之类的显式开关来临时让权即可，不会破坏 C3.5 的验收。

---

## 5. 落地范围

### 5.1 修改文件

- `Source/TerraCivilization/Private/Interaction/PlanetInteractionController.cpp`
  - `UpdateC3FocusCameraControl_`：删除 `if (!bCameraChanged) return;` 的早退；把 Clamp / Apply 移出 `bCameraChanged` 分支，让每帧无条件 Apply。

### 5.2 不修改

- `PlanetTessellatedMesh.h / .cpp`：三个焦点式相机函数签名不变。
- `PlanetInteractionController.h`：字段布局不变。
- Pawn / PlayerController 类型：不切换。
- 输入映射：不新增任何按键。

### 5.3 兼容性

- 与 C2 自动切镜的一次性 `SetActorLocation/Rotation` 兼容：C2 切完后玩家下一次进入手动模式，C3 通过首帧 Sync 拿到当前姿态，随后 C3.5 的每帧 Apply 就把相机锁在这里。
- 与 G8 兼容旧路径无关：G8 手动路径已经被 C3 替换。

---

## 6. 验收

1. 进入 C3 手动模式（首次按下 W/S/A/D 或滚轮触发初始化）后，松开所有键盘鼠标输入，等待 5 秒；相机不应发生任何可见位移或旋转。
2. 反复"按 W 半秒 → 松开 3 秒 → 再按 W 半秒"：每次松开后的姿态与松开瞬间完全一致；不应看到"松开后偏一点，再按下 W 又跳回"的现象。
3. C3 §7 全部原有验收条款仍成立（沿测地线走、穿越极点、A/D 语义正确等）。
4. C3.5 落地前后对比：切到高纬度并按住 D，然后松开——落地前相机会明显飘走一段再回弹，落地后应完全静止。

---

## 7. 暂不处理

- 屏蔽默认 look input（`SetIgnoreLookInput(true)`）：本稿的"每帧 Apply"已足以掩盖它带来的漂移。是否额外屏蔽留给 C4 / 后续需要精细控制输入通道时再决定。
- 自定义 Pawn 类型：同上。
- 自动镜头 Blend 与手动模式的所有权切换：留给 C4。
- Q/E、鼠标拖拽、Tab 导航：仍归后续阶段。
