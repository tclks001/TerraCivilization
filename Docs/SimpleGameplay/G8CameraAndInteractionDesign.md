# TerraCivilization SimpleGameplay G8 视角和交互逻辑设计稿

> 本稿承接 G2.5：当前已经支持回合开始切到当前阵营主将、点击己方棋子时旋转视角对准该棋子。
>
> G8 的目标是在不破坏 G2.5 原有自动视角切换逻辑的前提下，补充一套更用户友好的手动球面相机控制，便于观察棋局与调试后续棋子动画表现。

---

## 1. G8 目标

本阶段新增：

1. **手动球面相机控制**
   - `W / S` 控制摄像机纬度。
   - `A / D` 控制摄像机经度。
   - 鼠标滚轮控制摄像机高度。

2. **保留 G2.5 自动视角逻辑**
   - 回合开始仍自动切到当前阵营主将上方。
   - 点击己方棋子时仍自动旋转对准该棋子。

3. **让手动控制与自动聚焦共存**
   - 自动聚焦会更新手动相机的“球面锚点状态”。
   - 聚焦后玩家可立刻继续用 `WSAD + 滚轮` 微调视角。

---

## 2. 交互原则

G8 采用“双层控制”：

1. **G2.5 自动层**
   - 负责“什么时候切到哪”。
   - 仍由 `APlanetTessellatedMesh::FocusCameraOnCell_()` 与 `FocusCameraOnCurrentFactionBase_()` 驱动。

2. **G8 手动层**
   - 负责“玩家如何围绕当前观察目标继续调整视角”。
   - 由 `APlanetInteractionController` 每帧根据键盘与滚轮输入驱动。

关键约束：

- G8 不能替换 G2.5。
- G8 只是在 G2.5 自动切好的相机位置基础上继续允许手动调整。

---

## 3. 手动控制语义

### 3.1 纬度

- `W`：纬度增加，视角向“北”移动。
- `S`：纬度减少，视角向“南”移动。

纬度范围建议限制在：

```text
-85° ~ +85°
```

避免相机精确卡到极点导致控制方向退化。

### 3.2 经度

- `A`：经度减少。
- `D`：经度增加。

经度可在：

```text
-180° ~ +180°
```

之间循环。

### 3.3 高度

- 鼠标滚轮上滚：高度减小，摄像机拉近。
- 鼠标滚轮下滚：高度增大，摄像机拉远。

高度表示：

```text
CameraRadius = GlobeRadiusCM + CameraHeightOffsetCM
```

其中 `CameraHeightOffsetCM` 建议限制为：

```text
1000 cm ~ 30000 cm
```

---

## 4. 相机数学定义

G8 不再直接把玩家手动输入理解为世界坐标 XYZ 平移，而是理解为围绕球心的球面轨道参数：

```text
LongitudeDeg
LatitudeDeg
HeightOffsetCM
```

然后通过球坐标还原世界位置。

### 4.1 轨道位置

令：

```text
Lon = DegreesToRadians(LongitudeDeg)
Lat = DegreesToRadians(LatitudeDeg)
R   = GlobeRadiusCM + HeightOffsetCM
```

则单位方向：

```text
X = cos(Lat) * cos(Lon)
Y = cos(Lat) * sin(Lon)
Z = sin(Lat)
```

最终相机局部位置：

```text
LocalCamera = UnitDir * R
```

再经 `APlanetTessellatedMesh` 的 Actor Transform 变换到世界坐标。

### 4.2 朝向

手动轨道模式下，相机始终朝向球心：

```text
LookDirection = PlanetCenterWorld - CameraWorld
LookRotation = LookDirection.Rotation()
```

这样可保证：

- 不需要额外维护 yaw / pitch / roll。
- 玩家环绕球体移动时视角始终稳定指向球面中心区域。

---

## 5. 与 G2.5 自动聚焦的衔接

G2.5 原有逻辑保持不变：

- 回合开始：相机移动到当前阵营主将所在 Cell 正上方，并朝向该 Cell。
- 选中棋子：相机不平移，只旋转对准该棋子。

G8 新增一个约束：

> 每次 G2.5 自动相机切换完成后，都要把当前视角重新同步回 G8 的球面轨道参数。

也就是说：

- 自动切到主将后，更新 `LongitudeDeg / LatitudeDeg / HeightOffsetCM`。
- 之后玩家按 `WSAD` 或滚轮，就从新的位置继续微调。

这样可避免：

- 自动聚焦后，手动输入又突然把相机跳回旧位置。

---

## 6. 代码职责划分

### 6.1 `APlanetTessellatedMesh`

G8 在 `APlanetTessellatedMesh` 上新增：

- 一组可调手动相机参数：
  - 是否启用 G8 手动相机
  - 经度 / 纬度移动速度
  - 滚轮缩放速度
  - 最小 / 最大高度
- 一组相机辅助接口：
  - 获取球心世界坐标
  - 获取球半径
  - 把当前视角同步为轨道参数
  - 按轨道参数应用相机位置与朝向

### 6.2 `APlanetInteractionController`

G8 在 `APlanetInteractionController` 上新增：

- 当前是否启用手动轨道控制。
- 当前轨道参数缓存：
  - `CameraLongitudeDeg`
  - `CameraLatitudeDeg`
  - `CameraHeightOffsetCM`
- 当前帧滚轮输入累计值。
- 每帧读取：
  - `W`
  - `S`
  - `A`
  - `D`
  - `MouseScrollUp`
  - `MouseScrollDown`
- 若有输入，则调用 `APlanetTessellatedMesh` 应用新的轨道位置。

---

## 7. 输入方案

本项目当前输入层已经在 `APlanetInteractionController::PlayerTick()` 中使用：

- `WasInputKeyJustPressed`
- `GetHitResultUnderCursorByChannel`

因此 G8 延续这一风格，不强制切到 Enhanced Input。

### 7.1 键位

| 输入 | 作用 |
| --- | --- |
| `W` | 纬度增加 |
| `S` | 纬度减少 |
| `A` | 经度减少 |
| `D` | 经度增加 |
| `MouseScrollUp` | 拉近 |
| `MouseScrollDown` | 拉远 |

### 7.2 每帧输入读取

每帧：

```text
if IsInputKeyDown(W) -> Latitude += Speed * DeltaTime
if IsInputKeyDown(S) -> Latitude -= Speed * DeltaTime
if IsInputKeyDown(A) -> Longitude -= Speed * DeltaTime
if IsInputKeyDown(D) -> Longitude += Speed * DeltaTime
if WasInputKeyJustPressed(MouseScrollUp) -> Height -= ZoomStep
if WasInputKeyJustPressed(MouseScrollDown) -> Height += ZoomStep
```

---

## 8. 项目挂载步骤

这一节按“照做即可验收”的粒度写。

### 8.1 PlayerController

确认当前关卡 / GameMode 使用的是：

```text
BP_PlanetInteractionController
```

当前项目默认配置已经指向：

```text
Config/DefaultEngine.ini
GlobalDefaultGameMode=/Game/Blueprints/BP_HexTestGameMode.BP_HexTestGameMode_C
```

你需要在编辑器里确认：

1. 打开 `BP_HexTestGameMode`。
2. 在 Class Defaults 中确认 `PlayerControllerClass` 是 `BP_PlanetInteractionController`。

如果不是：

1. 把 `PlayerControllerClass` 改成 `BP_PlanetInteractionController`。
2. 保存蓝图。

### 8.2 地图

确认当前测试地图使用的是：

```text
/Game/Map/TessellatedMeshTestMap
```

并且场景中有：

- `BP_PlanetBinder`
- `BP_PlanetTessellatedMesh`

### 8.3 Binder 引用

在关卡中选中 `BP_PlanetBinder`，确认它的：

```text
TessellatedMeshRef
```

已经指向场景中的 `BP_PlanetTessellatedMesh` 实例。

否则鼠标 hover / click 与 G8 手动轨道控制都找不到球体宿主。

### 8.4 Tess Actor 参数

在关卡中选中 `BP_PlanetTessellatedMesh`，确认以下参数：

1. `bEnableG2_5CameraAssist = true`
2. `bEnableG8ManualCameraControl = true`
3. `G2_5TurnStartCameraHeightCM` 设为一个你觉得舒服的回合起始高度
4. `G8CameraMinHeightOffsetCM`
5. `G8CameraMaxHeightOffsetCM`
6. `G8CameraOrbitDegreesPerSecond`
7. `G8CameraZoomStepCM`

推荐初值：

```text
G2_5TurnStartCameraHeightCM = 8000
G8CameraMinHeightOffsetCM   = 2500
G8CameraMaxHeightOffsetCM   = 30000
G8CameraOrbitDegreesPerSecond = 45
G8CameraZoomStepCM          = 800
```

### 8.5 验收步骤

进入 PIE 后：

1. 开局应自动切到当前阵营主将上方。
2. 按 `W/S/A/D`，相机应围绕球体移动。
3. 滚轮上滚，相机应拉近。
4. 滚轮下滚，相机应拉远。
5. 点击当前阵营一个棋子，相机仍应保持 G2.5 逻辑自动对准该棋子。
6. 自动对准后继续按 `W/S/A/D`，应能从新位置继续微调，而不是跳回旧位置。

---

## 9. 验收清单

### 9.1 自动聚焦保留

- 回合开始仍自动切到当前阵营主将。
- 选中棋子仍自动对准该棋子。

### 9.2 手动平移

- `W/S` 能稳定改变纬度。
- `A/D` 能稳定改变经度。

### 9.3 手动缩放

- 滚轮上滚拉近。
- 滚轮下滚拉远。
- 不会缩进球体内部。
- 不会拉到过远失去可读性。

### 9.4 共存

- 自动聚焦后手动控制继续可用。
- 手动控制后再次回合切换，自动聚焦仍会生效。

---

## 10. 暂不处理

- 鼠标右键拖拽旋转。
- 中键平移。
- 视角平滑插值 / 缓动。
- 相机碰撞。
- 多相机模式切换。
