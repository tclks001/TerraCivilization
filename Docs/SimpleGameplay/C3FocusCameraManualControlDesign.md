# TerraCivilization SimpleGameplay C3 焦点式手动相机设计稿

> 本稿从 [CameraTrackingAndInteractionDesign.md](CameraTrackingAndInteractionDesign.md) 的 C3 阶段拆出。
>
> C3 替换 G8 的手动相机语义：从"相机绕球心轨道"改为"视野中心在球面上移动，相机姿态和距离保持稳定"。

---

## 1. 目标

C3 要达成的体验：

1. `WSAD` 不改变视野中球体的相对屏幕位置。
2. `WSAD` 在当前球面视野中心点建立局部切平面坐标系，并沿切线移动视野中心。
3. 手动平移时，视线倾角保持不变。
4. 手动平移时，摄像机到球面视野中心的距离保持不变。
5. 鼠标滚轮只改变摄像机到球面视野中心的距离。
6. 滚轮缩放不改变视野中心经纬度，不改变视线倾角。
7. `WSAD` 沿真正的球面测地线（大圆）移动焦点，不沿纬线。
8. 焦点可平滑穿越南北极点，不出现原地打转或视角跳变。

---

## 2. 相机状态

### 2.1 内部真值

C3 手动相机内部真值：

```text
FocusUnitDir       : FVector（球面视野中心方向的单位向量，本地坐标）
DistanceToFocusCM  : float
TiltDeg            : float（由 C3.7 每帧从 DistanceToFocusCM 线性插值自动派生，不再是独立持久状态）
YawAroundFocusDeg  : float
```

关键：**焦点位置的内部真值是三维单位向量 `FocusUnitDir`，不是经纬度对。** 经纬度对只作为对外展示 / 编辑器可视化 / 调试 UI 的派生值，会在需要时从 `FocusUnitDir` 反解得到。

原因：

- 用经纬度作为真值时，跨越极点必须在 `Latitude` 到达 `±90°` 时人为翻转经度和 Yaw，这一步在数值上是奇异的，容易造成"到极点原地转圈"和"越过极点相机跳变"。
- 用单位向量作为真值时，"越过极点"就是单位向量在 `Z` 分量过零，没有奇异，也无需 clamp。

### 2.2 派生对外字段

对外可读字段：

| 字段 | 含义 |
| --- | --- |
| `FocusLongitudeDeg` | 从 `FocusUnitDir` 反解，仅供显示与调试 |
| `FocusLatitudeDeg` | 从 `FocusUnitDir` 反解，仅供显示与调试 |
| `DistanceToFocusCM` | 摄像机到球面视野中心点的距离 |
| `TiltDeg` | 视线中心方向与视野中心地面切平面的夹角 |
| `YawAroundFocusDeg` | 绕视野中心外法线的观察方位（含义见 §5.1） |

---

## 3. 几何定义

从焦点状态应用相机（Apply）：

```text
FocusDir   = FocusUnitDir
FocusWorld = PlanetCenter + FocusDir * GlobeRadiusCM
Up         = FocusDir
East       = Normalize(Cross(WorldZ, Up))
North      = Normalize(Cross(Up, East))
ForwardHint= cos(Yaw) * North + sin(Yaw) * East

Camera = FocusWorld - ForwardHint * Distance * cos(Tilt) + Up * Distance * sin(Tilt)
CameraForward  = Normalize(FocusWorld - Camera)
CameraRotation = MakeFromXZ(CameraForward, Up)
```

关键约束：

- 摄像机正前方严格看向球面视野中心。
- 摄像机 up 方向用视野中心外法线约束。
- `TiltDeg` 表示视线中心方向与该点地面切平面的夹角。
- `DistanceToFocusCM` 是摄像机到球面视野中心点的直线距离，不是离球心距离。

极区退化处理：

- 当 `FocusDir` 与 `WorldZ` 几乎平行时（例如焦点非常接近南北极点），`East = Cross(WorldZ, Up)` 会退化成零向量。此时改用 `East = Cross(WorldY, Up)` 作为退化补救；`North` 随之重新计算。
- 这个退化只发生在 Apply 内部；`FocusUnitDir` 本身不会因为经过极点而奇异。

---

## 4. 从当前视角同步状态

### 4.1 首次进入手动模式

C2 自动切镜结束后，玩家第一次按下 `WSAD` 或滚轮时，C3 需要一次性从当前 ViewTarget 反推整套内部真值：

1. 取当前相机位置和旋转。
2. 用相机 forward 与球体求交。
3. 若能命中球面，命中点单位化后作为 `FocusUnitDir`。
4. 若不能命中，退回用相机到球心方向估算最近球面点。
5. `DistanceToFocusCM = Distance(Camera, FocusWorld)`。
6. `TiltDeg = asin(dot(-CameraForward, FocusUp))`。
7. `YawAroundFocusDeg` 由相机在焦点切平面的后退方向反算。

### 4.2 后续帧的同步策略

**这里是与旧设计的关键差异**。手动模式已经初始化后：

- 每帧 **不再** 从当前相机重解 `YawAroundFocusDeg`。
- 每帧 **不再** 从当前相机重解 `FocusUnitDir`。
- Yaw 与 FocusUnitDir 的更新只有两条来源：
  - `WSAD` 输入按 §5.1 的规则更新（推进焦点 + 平行运输 Yaw）；
  - 未来的 `Q/E`、鼠标拖拽（本稿暂不处理，见 §8）。

理由：**每帧从相机重解 Yaw + Apply 每帧强制相机 Up 对齐新焦点外法线**，会形成一个自反闭环——玩家每按一帧 A/D，Yaw 都被反解回接近 0，切向都被重新对齐到"新焦点当地的东切向"。这条闭环等价于"沿单位球面上东矢量场的积分曲线走"，其结果是 **纬线** 而不是大圆（东矢量场在球面上不是平行场）。此外，闭环还会让 W/S 到极点后因基向量退化而卡住原地打转。

只有在下面几种明确事件里，才允许再次调用 §4.1 的完整反解：

- 首次进入手动模式；
- C2 自动切镜刚完成，且玩家开始输入的第一帧；
- 未来加入的显式"重置视角到当前相机"命令。

平时的 `TiltDeg` / `DistanceToFocusCM` 也不再每帧从相机反解，而是完全由玩家滚轮 / 未来的拖拽输入驱动。这样即便相机因为其他系统被外部瞬移，C3 手动状态仍是自洽的；下一次进入手动模式再统一 Sync。

---

## 5. 输入语义

### 5.1 WSAD

C3 不直接加减绝对经纬度，而是在当前视野中心点的局部切平面上沿测地线（大圆）滑行焦点，同时用平行运输更新 Yaw。

#### 5.1.1 局部切线基

```text
FocusDir       = FocusUnitDir
Up             = FocusDir
East           = Normalize(Cross(WorldZ, Up))       ; 极区退化时改用 Cross(WorldY, Up)
North          = Normalize(Cross(Up, East))
ForwardTangent = cos(Yaw) * North + sin(Yaw) * East
RightTangent   = Normalize(Cross(Up, ForwardTangent))
```

`ForwardTangent` 是"从相机看向焦点的水平投影方向"在焦点切平面里的单位向量。`RightTangent` 是屏幕右方向对应的切向量。

#### 5.1.2 输入映射

| 输入 | 行为 |
| --- | --- |
| `W` | 焦点沿 `+ForwardTangent` 方向的大圆推进 |
| `S` | 焦点沿 `-ForwardTangent` 方向的大圆推进 |
| `A` | 焦点沿 `-RightTangent` 方向的大圆推进（画面向右滑，视角向左走） |
| `D` | 焦点沿 `+RightTangent` 方向的大圆推进（画面向左滑，视角向右走） |

> 语义澄清：`A/D` 是"视角往左右走"，即 `A` 让玩家看到的地形向屏幕右侧滑动，等价于焦点向屏幕左侧推进。`RightTangent` 指向屏幕右，所以 `A` 对应 `-RightTangent`，`D` 对应 `+RightTangent`。

#### 5.1.3 每帧推进公式

先根据本帧 WSAD 组合出瞬时切向增量（弧度）：

```text
Δθ_forward = (W_pressed - S_pressed) * OrbitDegPerSec * dt
Δθ_right   = (D_pressed - A_pressed) * OrbitDegPerSec * dt
V          = ForwardTangent * Δθ_forward + RightTangent * Δθ_right
θ          = |V|
if θ < ε: return                                           ; 本帧无输入
T          = V / θ                                         ; 本帧的瞬时移动切向
```

沿大圆推进焦点：

```text
FocusUnitDir_new = FocusUnitDir * cos(θ) + T * sin(θ)
```

**Yaw 平行运输**（关键新增步骤）：

沿测地线 `T` 前进弧长 `θ` 时，切平面里的任何切向量都会随之被平行运输到新焦点。C3 只需要维护 Yaw，而 Yaw = "ForwardTangent 相对 (North, East) 基的方位角"，因此只要把 `ForwardTangent` 沿 `T` 平行运输到新焦点，再在**新焦点**的 `East_new / North_new` 基下重新反解，就得到 `Yaw_new`。

在球面上，"沿测地线切向 `T` 前进 `θ`"等价于"绕轴 `A = Cross(FocusUnitDir, T)` 旋转 `θ` 弧度"。这条 Rodrigues 旋转会把整个焦点切标架 `(Up, ForwardTangent, RightTangent)` 平滑地"抬"到新焦点的切平面里，这就是标准的球面平行运输。

工程实现按下面这条通用步骤走：

```text
1. RotationAxis = Normalize(Cross(FocusUnitDir, T))
     该轴垂直于 T，且位于旧切平面里；|Cross(FocusUnitDir, T)| = 1（因 T 是切向单位向量）。
2. 用 Rodrigues 公式 R(RotationAxis, θ) 同时旋转:
     FocusUnitDir_new    = R * FocusUnitDir      ; 等价于 FocusUnitDir * cos(θ) + T * sin(θ)
     ForwardTangent_new  = R * ForwardTangent
     (RightTangent_new 只在需要显式使用时再算，Yaw 反解并不需要它)
3. 在新焦点基下反解 Yaw:
     East_new  = Normalize(Cross(WorldZ, FocusUnitDir_new))     ; 极区退化时改用 Cross(WorldY, ...)
     North_new = Normalize(Cross(FocusUnitDir_new, East_new))
     Yaw_new   = atan2(dot(ForwardTangent_new, East_new),
                       dot(ForwardTangent_new, North_new))
```

物理意义：

- 步骤 1 的旋转轴 `Cross(FocusUnitDir, T)` 就是这条大圆所在平面的法向。
- 步骤 2 沿这个法向做同一次 Rodrigues 旋转，相当于把整根标架沿着大圆"贴地滑行"到新焦点。
- 步骤 3 的 Yaw 反解只是把新的 `ForwardTangent_new` 换到新基下表达，没有额外几何。
- 这样连续按住 A/D 或 W/S 都会走一条闭合的大圆，不会退化成纬线。

#### 5.1.4 与旧实现的差别

旧实现（存在 bug）：

```text
每帧:
  从当前相机反解 Yaw (Yaw≈0)
  以 (North_focus, East_focus, Yaw≈0) 组一个 ForwardTangent (≈North_focus)
  沿 RightTangent (≈-East_focus) 走 δ
  Apply 相机到新焦点，Up 强制对齐新焦点外法线
```

问题：因为下一帧 Yaw 又被重解成 ≈ 0，`ForwardTangent` 又被重新组成"新焦点的 North"，切向永远是"当地北 / 当地东"，不是"上一帧切向的平行运输"。这条积分曲线是纬线，不是大圆。

新实现：

```text
首帧:
  Sync 一次拿到 (FocusUnitDir, Yaw, Tilt, Distance)
后续每帧:
  用玩家 WSAD 组一个瞬时切向 T
  沿大圆推进 FocusUnitDir 到新位置
  用 Rodrigues 把 (Up, ForwardTangent, RightTangent) 一起搬到新焦点
  在新焦点基下反解出新的 Yaw
Apply:
  用 (FocusUnitDir_new, Yaw_new, Tilt, Distance) 摆相机
```

在这条新流程里：

- 连续按住 `A` 就走一整条大圆，`Yaw` 会随经过的纬度自然增减。
- 到达极点时 `FocusUnitDir` 平滑越过 Z 分量的极值，无奇异；Yaw 会翻 180°，沿大圆继续。
- W/S 到达极点后不再"原地转圈"，而是继续沿同一条经线大圆走到另一半球。

#### 5.1.5 极点附近数值稳定性

- `East / North` 只在做当帧计算时构造，不作为持久状态；因此它们在极点的退化不会污染任何跨帧数据。
- 当 `FocusUnitDir` 距 `WorldZ` 的夹角小于阈值（例如 0.001 弧度）时，用 `Cross(WorldY, FocusUnitDir)` 作为 East 的退化补救。
- 因为焦点真值是 3D 单位向量，**不再对 `FocusLatitudeDeg` 做 `[-89, 89]` 的 clamp**；只有对外派生显示时才做 clamp。

### 5.2 滚轮

| 输入 | 行为 |
| --- | --- |
| 滚轮上 | `DistanceToFocusCM -= G8CameraZoomStepCM` |
| 滚轮下 | `DistanceToFocusCM += G8CameraZoomStepCM` |

距离限制沿用：

```text
G8CameraMinHeightOffsetCM <= DistanceToFocusCM <= G8CameraMaxHeightOffsetCM
```

字段名仍沿用 G8 的 min/max，语义在 C3 中解释为"到焦点距离"的限制。

---

## 6. 与 C2 的关系

C2 回合开始设置的是一次自动视角。

C3 在玩家第一次手动输入时，会按 §4.1 从当前相机反推：

- `FocusUnitDir`
- 摄像机到焦点距离
- 当前视线倾角
- 当前绕焦点方位（Yaw）

反推之后进入手动状态；后续帧不再重复反推，避免形成 §4.2 所述的自反闭环。

---

## 7. 验收

1. C2 自动切镜后，按 `W/S/A/D` 时球体在屏幕中的相对构图不应明显漂移。
2. `W/S/A/D` 沿当前视野中心的局部切平面移动焦点，而不是加减绝对经纬度。
3. 连续平移时视线倾角保持稳定。
4. 连续平移时摄像机到球面视野中心距离保持稳定。
5. 滚轮上拉近、下拉远，只改变到视野中心的距离。
6. 滚轮缩放后再平移，仍保持缩放后的距离和原倾角。
7. **在纬度 60° 以上按住 `A` 或 `D`**：焦点应沿一条明显向赤道弯曲的大圆滑动，不应表现为绕纬线打圈。
8. **持续按住 `W`（或 `S`）跨越南北极点**：焦点平滑穿过极点继续沿同一条经线大圆前进，不出现原地打转、视角闪跳或方向反转。
9. **按住 `A`（或 `D`）绕地球一周**：焦点沿闭合大圆回到起点，途中相机构图连续。
10. `A/D` 语义方向正确：`A` 让画面向屏幕右侧滑动（视角向左走），`D` 反之。

---

## 8. 暂不处理

C3 暂不实现：

- 屏幕方向平移（W/S/A/D 之外的键位）。
- 鼠标拖拽。
- 自动镜头 Blend。
- 玩家输入打断自动镜头。
- Tab 棋子导航。
- **无输入帧的相机稳态**：C3 手动模式下 Apply 只在 W/S/A/D/滚轮任一有输入时执行；无输入帧不主动摆相机。因此若 UE 默认 Pawn / PlayerController 在无输入帧仍在推 `ControlRotation` 或 Pawn 姿态，相机会缓慢漂移，直到玩家再次按下按键才被 Apply "拉回焦点"。**此问题独立留给 [C3_5IdleFrameCameraStabilityDesign.md](C3_5IdleFrameCameraStabilityDesign.md) 阶段解决**，不塞进 C3。

**已从"暂不处理"移除**：

- 极点穿越。C3 用 3D 单位向量作为焦点真值 + Yaw 平行运输后，极点穿越是自然行为，不再是需要单独处理的特殊情况。
- Q/E 绕焦点旋转。已由 [C3_6QERollAroundFocusDesign.md](C3_6QERollAroundFocusDesign.md) 阶段落地。
- 独立 Tilt 自由度。已由 [C3_7AutoTiltFromDistanceDesign.md](C3_7AutoTiltFromDistanceDesign.md) 阶段替换为每帧从距离自动插值派生。
> Implementation note (2026-07-10): C3 focus-camera sync/apply/offset math
> has moved from `APlanetTessellatedMesh` into `FPlanetCameraController`
> (`Source/TerraCivilization/Public/Render/PlanetCameraController.h`,
> `Source/TerraCivilization/Private/Render/PlanetCameraController.cpp`).
> The old `APlanetTessellatedMesh` methods remain as compatibility wrappers.
