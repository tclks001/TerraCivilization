# TerraCivilization SimpleGameplay C6 行动镜头追踪设计稿

> 本稿从 [CameraTrackingAndInteractionDesign.md](CameraTrackingAndInteractionDesign.md) 的 C6 阶段拆出。
> C6 只处理“棋子行动表现期间镜头是否需要跟随”，不改变 Gameplay 规则和行动合法性。

---

## 1. 目标

C6 要达成：

1. 如果棋子即将以移动或跳跃进入 C4 舒适区外的 Cell，镜头同步平滑移动到棋子落点。
2. 如果棋子移动 / 跳跃的落点仍在 C4 舒适区内，镜头不动。
3. 如果玩家 Undo 一次已发生的移动 / 跳跃，且棋子回退后的原位置在 C4 舒适区外，镜头同步平滑移动回原位置。
4. 自动追踪过程中，玩家任意有效输入都会立刻停止镜头移动。

---

## 2. 舒适区标准

C6 直接复用 C4 的舒适区定义：

```text
CurrentFocusUnitDir = 当前视角中心所在球面方向
TargetUnitDir       = 棋子目标 Cell 的球面方向
AngleDeg            = acos(dot(CurrentFocusUnitDir, TargetUnitDir))

AngleDeg <= C4ComfortFocusAngleDeg  => 舒适区内，不移动镜头
AngleDeg >  C4ComfortFocusAngleDeg  => 舒适区外，请求 C6 行动追踪镜头
```

也就是说，C6 不引入新的“屏幕边缘”或“可见性”判定，避免 C4 与 C6 使用两套不一致的镜头阈值。

---

## 3. 触发时机

C6 绑定在表现层已经存在的 `FTerraPiecePresentationMoveEvent` 上。

当前鼠标点击或 Tab 选中后的 Gameplay 处理已经会在棋子实际移动 / 跳跃时生成：

```cpp
FTerraPiecePresentationMoveEvent
{
    PieceId
    FromCellId
    ToCellId
    MoveType
    FromWorldTransform
    ToWorldTransform
}
```

C6 在同一帧检查 `ToCellId`：

```text
若 ToCellId 在 C4 舒适区内：
    不请求镜头
若 ToCellId 在 C4 舒适区外：
    请求镜头焦点 Blend 到 ToCellId
```

Blend 时长与棋子表现移动时长一致：

| MoveType | 镜头 Blend 时长 |
| --- | --- |
| `Move` | `P2MoveDurationSeconds` |
| `Jump` | `P2JumpDurationSeconds` |

这样镜头和棋子的视觉移动同步开始、同步结束。

---

## 4. Undo 行为

G9 的 Undo 会把 Gameplay 状态回退到上一次点选前。C6 需要在 Tess 层比较 Undo 前后的棋子位置：

```text
Undo 前：PieceId -> CurrentCellId
Undo 后：PieceId -> RestoredCellId

若同一 PieceId 的 CellId 发生变化：
    生成反向 FTerraPiecePresentationMoveEvent
    FromCellId = Undo 前 Cell
    ToCellId   = Undo 后 Cell
```

反向事件会同时用于：

- 让模型用 P2 移动 / 跳跃表现回退，而不是瞬移。
- 让 C6 用 `ToCellId` 判断是否需要把镜头平滑移动回原位置。

MoveType 取 Undo 前阶段：

| Undo 前阶段 | 反向 MoveType |
| --- | --- |
| `PieceMovedCanEndTurn` | `Move` |
| `PieceJumpingCanContinue` | `Jump` |

如果只是从 `PieceSelected` Undo 回 `Idle`，棋子位置未变化，不生成反向移动事件，也不触发 C6。

---

## 5. 玩家输入打断

C6 与 C2.5 / C4 共用 Controller 侧的自动焦点 Blend 管线。该管线每帧在应用自动 Blend 前检查玩家输入。

当前阶段定义为“有效打断输入”的输入源：

```text
W / S / A / D
Q / E
MouseScrollUp / MouseScrollDown
LeftMouseButton
RightMouseButton
Tab
```

一旦检测到这些输入，Controller 立即取消当前自动 Blend，并把控制权交还给 C3 手动相机或输入对应的交互逻辑。

说明：

- 鼠标 hover 不算打断输入，否则镜头会被普通鼠标移动持续取消。
- 右键本身会先取消当前行动镜头，随后 G9 Undo 可能在同一帧发起新的 C6 回退镜头。

---

## 6. 实现方案

### 6.1 Tess 层

新增参数：

```cpp
bool bEnableC6ActionCameraTracking = true;
```

新增私有函数：

```cpp
float GetC6ActionCameraBlendSeconds_(ETerraPiecePresentationMoveType MoveType) const;
void RequestC6ActionCameraTrackingForMoveEvents_(const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents) const;
```

触发点：

1. `HandleGameplayCellClick_(...)` 生成 `P2MoveEvents` 后，调用 `RequestC6ActionCameraTrackingForMoveEvents_(P2MoveEvents)`。
2. `HandleHISMUndo()` 比较 Undo 前后棋子位置，生成反向 `UndoMoveEvents` 后，调用同一个函数。

### 6.2 Controller 层

新增语义入口：

```cpp
bool RequestC6FocusOnUnitDir(const FVector& TargetFocusUnitDir, float BlendSeconds);
```

该入口复用现有自动焦点 Blend 机制，不新增相机状态。

同时把原本只用于 C3 手动输入的打断判断扩展为自动镜头通用打断判断：

```cpp
HasAutoCameraInterruptInput_()
```

自动 Blend 活跃时，若 `HasAutoCameraInterruptInput_()` 为 true，则立即取消 Blend。

---

## 7. 验收

1. 选中棋子后移动到 C4 舒适区内的邻格：棋子移动，镜头不动。
2. 选中棋子后移动到 C4 舒适区外的邻格：棋子移动动画开始时，镜头同步平滑移动到落点。
3. 连续跳跃时，每次跳跃落点若超出舒适区，镜头同步跟到该跳落点。
4. 对已移动一步的棋子按右键 Undo：模型反向移动回原 Cell；如果原 Cell 在舒适区外，镜头同步平滑回原 Cell。
5. 行动镜头移动过程中按 `WSAD/QE/滚轮/左键/右键/Tab` 任一有效输入：当前自动镜头立刻停止。

---

## 8. 暂不处理

C6 暂不实现：

- 攻击者与被攻击者双目标框选。
- 远程攻击投射物镜头。
- 根据移动路径中间点连续跟随曲线。
- 根据棋子实际 Actor 插值位置每帧追踪。
- 对手回合行动镜头与玩家偏好视角恢复。
> Implementation note (2026-07-10): C6 action camera tracking is implemented in
> `UPlanetCameraComponent`. `APlanetTessellatedMesh::HandleGameplayCellClick_`
> and undo handling now delegate move-event tracking requests to that helper.
