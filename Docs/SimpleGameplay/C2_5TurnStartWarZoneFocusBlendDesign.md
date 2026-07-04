# TerraCivilization SimpleGameplay C2.5 回合开始平滑切战区焦点设计稿

> 本稿从 [CameraTrackingAndInteractionDesign.md](CameraTrackingAndInteractionDesign.md) 的 C2.5 阶段拆出。
>
> C2.5 负责“回合开始”的相机行为：平滑移动 C3 视角中心到当前阵营战区中心，不硬设置相机位置，不改变当前相机到视角中心的距离。

---

## 1. 目标

C2.5 要达成：
1. 每次回合开始时，视角中心平滑切到当前阵营所有活棋子的战区中心。
2. 不改变 `C3DistanceToFocusCM`，即不改变摄像机到视角中心的距离。
3. 不改变 `C3YawAroundFocusDeg`，尽量保留玩家当前观察方位。
4. 倾角继续由 C3.7 根据当前距离自动派生。
5. 首局第一次镜头硬设置仍由 C2 负责；C2.5 只处理之后的回合开始。

---

## 2. 触发时机

仍复用既有的回合相机辅助触发：

```text
G2_5LastCameraFocusedTurnIndex != GameplayContainer->GetTurnIndex()
```

当检测到新回合：
1. 如果 `bC2GameStartCameraApplied == false`，执行 C2 游戏开始硬设置。
2. 否则执行 C2.5 回合开始焦点 Blend。

行动结束导致换阵营时，`HandleHISMClickHit(...)` 中仍会更新 `G2_5LastCameraFocusedTurnIndex` 并调用同一个统一入口，确保鼠标点击造成的回合推进也触发 C2.5。

---

## 3. 战区中心定义

C2.5 与 C2 复用同一个战区中心：

```text
WarZoneDir = Normalize(Sum(CurrentFactionAlivePiece.Cell.UnitCenter))
```

过滤条件：
- `Piece.bAlive == true`
- `Piece.OwnerFactionId == CurrentFactionId`
- `Piece.CellId` 有效

如果没有可用棋子，或方向和接近零，则本次 C2.5 失败并退回旧 G2.5 主将/大本营视角兜底。

---

## 4. 参数

| 参数 | 默认值 | 作用 |
| --- | --- | --- |
| `bEnableC2_5TurnStartWarZoneFocusBlend` | `true` | 是否启用回合开始战区焦点平滑切换 |
| `C2_5TurnStartFocusBlendSeconds` | `0.45` | 焦点 Blend 时长 |

C2.5 不读取：
- `C3InitialDistanceToFocusCM`
- `C2TurnStartCameraDistanceCM`
- `C2TurnStartCameraTiltDeg`

原因：C2.5 不负责设置距离或独立倾角，它只移动焦点。

---

## 5. 实现链路

C2.5 不直接设置相机 Actor。

链路：

```text
APlanetTessellatedMesh::BlendCameraFocusToCurrentFactionWarZone_()
    -> TryBuildCurrentFactionWarZoneDirection_(WarZoneDir)
    -> APlanetInteractionController::RequestC2_5FocusOnUnitDir(WarZoneDir, BlendSeconds)
    -> Controller 平滑修改 C3FocusUnitDir
    -> Controller 每帧 ApplyFocusCameraState
```

Controller 侧行为与 C4 的焦点 Blend 机制一致：

```text
StartFocusUnitDir = 当前 C3FocusUnitDir
TargetFocusUnitDir = WarZoneDir
FocusUnitDir = Slerp(Start, Target, Alpha)
```

保持：
- `C3DistanceToFocusCM`
- `C3YawAroundFocusDeg`
- C3.7 自动倾角派生规则

如果玩家在 Blend 过程中输入 `WSAD/Q/E/滚轮`，沿用现有自动焦点 Blend 打断规则，取消自动聚焦并交还手动控制。

---

## 6. 验收

1. PIE / 游戏开始时，第一次镜头仍由 C2 硬设置到当前阵营战区中心。
2. 第二个及后续回合开始时，镜头不硬跳，而是平滑把视角中心移动到当前阵营战区中心。
3. C2.5 Blend 过程中摄像机到视角中心的距离不发生跳变。
4. C2.5 Blend 过程中 `YawAroundFocusDeg` 不被重置。
5. 滚轮调过距离后，下一次回合开始仍保持玩家当前距离，只移动焦点。
6. Blend 过程中按 `WSAD/Q/E/滚轮` 会取消自动 Blend 并进入手动控制。

---

## 7. 暂不处理

C2.5 暂不实现：
- 根据敌我双方战区同时构图。
- 动态选择更优观察方位。
- 根据可行动棋子数量调整距离。
- 与行动演出镜头抢占优先级的完整状态机。
