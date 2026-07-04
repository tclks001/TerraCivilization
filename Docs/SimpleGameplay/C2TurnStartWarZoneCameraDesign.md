# TerraCivilization SimpleGameplay C2 回合开始斜俯视战区中心设计稿

> 本稿从 [CameraTrackingAndInteractionDesign.md](CameraTrackingAndInteractionDesign.md) 的 C2 阶段拆出。
>
> C2 只改变“回合开始自动视角”的目标和构图，不改变 Gameplay 规则、不改变 HISM 点击/高亮、不改变 G8 的 `WSAD + 滚轮` 手动轨道控制。

---

## 1. 目标

C2 要解决 G2.5 回合开始“垂直俯视主将/大本营”过于僵硬的问题。

目标行为：

1. 回合开始时，镜头不再切到当前阵营主将/大本营正上方。
2. 镜头切到当前阵营所有活棋子的战区中心。
3. 镜头以斜俯视角观察战区中心，保留球面空间感。
4. 选中棋子时，仍保留 G2.5 的旧逻辑：只旋转当前相机对准被选棋子，不移动相机。

---

## 2. 当前基础

当前自动视角入口在：

```cpp
APlanetTessellatedMesh::FocusCameraOnCurrentFactionBase_()
```

触发时机：

1. Gameplay 初始化后的首个 Tick。
2. 回合切换后。

旧行为：

```text
当前阵营 BaseCellId
    -> Cell 球面外侧 G2_5TurnStartCameraHeightCM
    -> 相机看向该 Cell
```

问题：

- 视角接近垂直俯视，缺少前后空间感。
- 主将/大本营不一定代表玩家当前最需要看的区域。
- 棋子分散后，玩家需要额外手动寻找可行动棋子。

---

## 3. C2 战区中心定义

初版战区中心使用当前阵营所有活棋子的 Cell 球面方向平均值：

```text
WarZoneDir = Normalize(Sum(CurrentFactionAlivePiece.Cell.UnitCenter))
```

过滤条件：

- `Piece.bAlive == true`
- `Piece.OwnerFactionId == CurrentFactionId`
- `Piece.CellId` 有效

如果没有可用棋子，或方向和接近零，则退回旧逻辑：

```text
CurrentFactionBaseCellId
```

原因：

- 这个定义稳定、可解释、实现简单。
- 能自然覆盖“棋子离开主将后，镜头看向当前阵营整体分布”的情况。
- 后续 C6 行动跟随、C7 对手回合观看可以在这个基础上改为加权中心。

---

## 4. C2 斜俯视构图

新增参数：

| 参数 | 默认值 | 作用 |
| --- | --- | --- |
| `bEnableC2TurnStartWarZoneCamera` | `true` | 是否启用 C2 回合开始战区中心镜头 |
| `C2TurnStartCameraDistanceCM` | `18000` | 相机到战区中心的距离 |
| `C2TurnStartCameraTiltDeg` | `55` | 视线中心方向与目标点地面切平面的夹角 |

构图规则：

```text
Target = WarZoneDir * GlobeRadiusCM
Up = WarZoneDir
ForwardHint = Project(WarZoneDir - CommanderDir, Up)
Camera = Target - ForwardHint * Distance * cos(Tilt) + Up * Distance * sin(Tilt)
CameraForward = Normalize(Target - Camera)
CameraRotation = MakeFromXZ(CameraForward, Up)
```

其中：

- `ForwardHint` 表示从主将/大本营方向指向战区中心的切平面方向。
- 相机放在 `ForwardHint` 的反方向，并向球面外侧抬高。
- 这样画面语义是“从本阵营后方看向本阵营战区”。
- 相机正前方严格看向地表目标点 `Target`。
- `Tilt` 控制视线中心方向与目标点地面切平面的夹角。
- 相机 up 方向使用目标点外法线 `Up` 约束，避免不同经纬度下画面上下方向不稳定。

如果 `ForwardHint` 无法计算：

1. 优先从当前相机到战区中心的切平面方向推导。
2. 仍失败时，使用与 `Up` 正交的固定切线方向。

---

## 5. 与 G8 的关系

C2 仍直接设置当前 ViewTarget 的位置和旋转。

G8 每帧会从当前相机世界位置同步轨道参数：

```cpp
SyncOrbitCameraStateFromWorldPosition(...)
```

因此 C2 自动切镜后，玩家继续按 `WSAD` 或滚轮时，G8 会从 C2 的新位置继续控制，不会跳回旧轨道状态。

C2 不修改：

- `APlanetInteractionController`
- `G8CameraLongitudeDeg`
- `G8CameraLatitudeDeg`
- `G8CameraHeightOffsetCM`

---

## 6. 验收

1. PIE 启动后，首个回合镜头切到当前阵营整体棋子附近，而不是主将正上方。
2. 结束回合后，新阵营回合开始时同样切到该阵营整体棋子附近。
3. 镜头能看到主要可行动棋子。
4. 视角有明显斜俯视空间感，不是垂直俯视。
5. 点击己方棋子后，仍只旋转镜头对准该棋子，不移动镜头。
6. 自动切镜后，`WSAD + 滚轮` 仍可继续手动调整。

---

## 7. 暂不处理

C2 暂不实现：

- 平滑过渡。
- 玩家输入打断自动镜头。
- 屏幕内可见性判断。
- Tab/Shift+Tab 棋子导航。
- 行动过程相机跟随。
- 对手回合视角恢复。

这些内容分别留给 C3 之后的阶段。
