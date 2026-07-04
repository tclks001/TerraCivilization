# TerraCivilization SimpleGameplay C2 游戏开始斜俯视战区中心设计稿

> 本稿从 [CameraTrackingAndInteractionDesign.md](CameraTrackingAndInteractionDesign.md) 的 C2 阶段拆出。
>
> C2 现阶段只负责“游戏开始之初”的一次性硬设置镜头；回合开始时的平滑焦点切换由 [C2_5TurnStartWarZoneFocusBlendDesign.md](C2_5TurnStartWarZoneFocusBlendDesign.md) 负责。

---

## 1. 目标

C2 要解决游戏刚开始时“垂直俯视主将/大本营”过于僵硬的问题。

目标行为：
1. PIE / 游戏开始后的首次自动镜头，不再切到当前阵营主将/大本营正上方。
2. 镜头硬设置到当前阵营所有活棋子的战区中心。
3. 镜头使用 C3 焦点式相机状态，后续 `WSAD / Q/E / 滚轮` 可以从同一套状态继续控制。
4. 距离只由 `C3InitialDistanceToFocusCM` 控制。
5. 倾角只按 C3.7 自动倾角逻辑由距离派生。
6. 后续回合开始不再由 C2 硬设置镜头。

---

## 2. 触发时机

C2 只在本局开始后第一次需要自动镜头时触发。

运行期状态：

| 字段 | 作用 |
| --- | --- |
| `bC2GameStartCameraApplied` | 本局是否已经完成游戏开始硬设置镜头 |

触发入口仍复用旧的回合相机辅助入口：

```cpp
APlanetTessellatedMesh::FocusCameraOnCurrentFactionBase_()
```

但入口内部改为：

```text
if !bC2GameStartCameraApplied:
    bC2GameStartCameraApplied = true
    尝试 C2 游戏开始硬设置
else:
    交给 C2.5 回合开始平滑切焦点
```

重建 Gameplay 时必须把 `bC2GameStartCameraApplied` 重置为 `false`。

---

## 3. C2 战区中心定义

战区中心使用当前阵营所有活棋子的 Cell 球面方向平均值：

```text
WarZoneDir = Normalize(Sum(CurrentFactionAlivePiece.Cell.UnitCenter))
```

过滤条件：
- `Piece.bAlive == true`
- `Piece.OwnerFactionId == CurrentFactionId`
- `Piece.CellId` 有效

如果没有可用棋子，或方向和接近零，则退回当前阵营主将/大本营方向。

---

## 4. C2 构图职责

C2 只负责游戏开始硬设置：
1. 计算战区中心 `WarZoneDir`。
2. 计算观察方位 `ForwardHint`。
3. 使用 `C3InitialDistanceToFocusCM` 和 C3.7 自动倾角公式摆出初始镜头。
4. 将焦点、距离和 yaw 写入 C3 状态，避免下一帧被 C3.5 稳态锁拉回旧位置。

启用参数：

| 参数 | 默认值 | 作用 |
| --- | --- | --- |
| `bEnableC2GameStartWarZoneCamera` | `true` | 是否启用 C2 游戏开始战区中心硬设置 |
| `C3InitialDistanceToFocusCM` | `8000` | 游戏开始自动镜头到战区中心的初始距离 |
| `C3AutoTiltMinDistanceCM / C3AutoTiltMaxDistanceCM` | `2500 / 30000` | C3.7 自动倾角的距离区间 |
| `C3AutoTiltAtMinDistanceDeg / C3AutoTiltAtMaxDistanceDeg` | `10 / 85` | C3.7 自动倾角的角度区间 |

已禁用参数：

| 参数 | 状态 | 替代方案 |
| --- | --- | --- |
| `C2TurnStartCameraDistanceCM` | 禁用，仅保留兼容旧关卡序列化 | 使用 `C3InitialDistanceToFocusCM` |
| `C2TurnStartCameraTiltDeg` | 禁用，仅保留兼容旧关卡序列化 | 使用 C3.7 自动倾角 |

构图规则：

```text
Target = WarZoneDir * GlobeRadiusCM
Up = WarZoneDir
ForwardHint = Project(WarZoneDir - CommanderDir, Up)
Distance = Clamp(C3InitialDistanceToFocusCM, G8CameraMinHeightOffsetCM, G8CameraMaxHeightOffsetCM)
Tilt = C3.7_AutoTiltFromDistance(Distance)
Camera = Target - ForwardHint * Distance * cos(Tilt) + Up * Distance * sin(Tilt)
CameraForward = Normalize(Target - Camera)
CameraRotation = MakeFromXZ(CameraForward, Up)
```

---

## 5. 与 C3 的状态同步

C2 不能只直接设置 `ViewTarget` 的位置和旋转。C3.5 之后，焦点式手动相机会每帧按控制器内部状态执行 `ApplyFocusCameraState(...)`。

同步规则：
1. C2 计算出 `WarZoneDir / ForwardHint`。
2. C2 按 C3 的局部 `North/East` 基把 `ForwardHint` 反解为 `YawAroundFocusDeg`。
3. C2 调用 `APlanetInteractionController::SetC3FocusCameraState(...)` 写入：
   - `FocusUnitDir = WarZoneDir`
   - `DistanceToFocusCM = Clamp(C3InitialDistanceToFocusCM, G8CameraMinHeightOffsetCM, G8CameraMaxHeightOffsetCM)`
   - `YawAroundFocusDeg`
4. C2 立即调用 `ApplyFocusCameraState(...)`。

---

## 6. 验收

1. PIE / 游戏开始后，首次镜头硬设置到当前阵营整体棋子附近，而不是主将正上方。
2. C3.5 每帧 Apply 开启后，初始镜头不会在下一帧跳回旧焦点。
3. 调整 `C3InitialDistanceToFocusCM` 后，游戏开始镜头距离明显变化。
4. 调整 C3.7 自动倾角参数后，游戏开始镜头倾角按同一套规则变化。
5. 后续回合开始不再执行 C2 硬设置，而由 C2.5 平滑切焦点。

---

## 7. 暂不处理

C2 暂不实现：
- 游戏开始硬设置的平滑过渡。
- 玩家输入打断游戏开始硬设置。
- 回合开始平滑切焦点，该能力由 C2.5 实现。
