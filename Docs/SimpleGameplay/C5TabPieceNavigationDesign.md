# TerraCivilization SimpleGameplay C5 Tab 棋子导航设计稿

> 本稿从 [CameraTrackingAndInteractionDesign.md](CameraTrackingAndInteractionDesign.md) 的 C5 阶段拆出。
> C5 只增加“键盘循环选中当前阵营可行动棋子”的输入方式，不改变 Gameplay 规则。

---

## 1. 目标

C5 要达成：

1. `Tab` 循环到当前阵营的下一个可行动棋子。
2. `Shift+Tab` 循环到当前阵营的上一个可行动棋子。
3. 无选中棋子时，`Tab` 等价于点击当前阵营可行动棋子中序号最前的棋子。
4. Tab 导航必须复用鼠标点击己方棋子的选择链路，包含 Gameplay 选中、高亮刷新、C4 智能选中聚焦、P1/P2 表现同步。
5. 右键 Undo 不做特殊分支：Tab 选中后仍通过 G9 操作栈回退到点选前状态。

---

## 2. 可导航棋子集合

C5 的导航集合定义为“当前阵营可被选择并可行动的活棋子”：

```text
Piece.bAlive == true
Piece.OwnerFactionId == CurrentFactionId
Piece.bCanMove == true
Piece.PieceType != Commander
```

这与 Gameplay 内部 `IsPieceSelectable_` 的语义一致。主将不参与 Tab 循环，避免把战略目标单位误当作可行动单位。

排序规则：

```text
按 PieceId 升序稳定排序
```

因此“序号排在最前面的棋子”就是当前可导航集合中 `PieceId` 最小的棋子。

---

## 3. 输入语义

| 输入 | 无选中棋子 | 已选中棋子 |
| --- | --- | --- |
| `Tab` | 选中 PieceId 最小的可行动棋子 | 选中当前棋子的下一个可行动棋子 |
| `Shift+Tab` | 选中 PieceId 最大的可行动棋子 | 选中当前棋子的上一个可行动棋子 |

循环规则：

```text
NextIndex = (CurrentIndex + 1) mod Count
PrevIndex = (CurrentIndex - 1 + Count) mod Count
```

如果当前选中棋子不在可导航集合中，则按“无选中棋子”处理。

---

## 4. 与鼠标点击的关系

C5 不直接写 `SelectedPieceId`，而是把目标棋子所在 `CellId` 交给与 HISM 鼠标点击相同的处理入口：

```text
Tab / Shift+Tab
    -> 计算目标 PieceId
    -> 取目标 CellId
    -> APlanetTessellatedMesh::HandleGameplayCellClick_(CellId)
    -> FTerraGameplayContainer::HandleCellClick(CellId)
    -> 高亮刷新 / C4 智能聚焦 / PiecePresentation 同步
```

这样可以保证：

- Tab 与鼠标点击得到相同的 Gameplay 状态。
- Tab 与鼠标点击得到相同的可移动 / 可跳跃高亮。
- Tab 与鼠标点击得到相同的 C4 智能选中聚焦。
- 后续若点击选中逻辑扩展，Tab 不需要另写一套分支。

---

## 5. 与 Undo 的关系

G9 已经规定：右键 Undo 回退到本次点选前的状态。

C5 复用 `HandleCellClick` 后，第一次从 Idle 选中棋子时会照常压入 Undo 快照：

```text
Idle
    -> Tab 选中棋子
    -> PushUndoSnapshot
    -> PieceSelected
    -> RightMouseButton Undo
    -> Idle
```

如果已经处于 `PieceSelected` 阶段，再按 Tab 切换到另一个可行动棋子，不额外压入新的 Undo 快照。此时右键 Undo 仍回到本次点选链开始前的 Idle 状态。

已经发生移动 / 跳跃后的阶段暂不允许 Tab 切换棋子：

```text
PieceMovedCanEndTurn
PieceJumpingCanContinue
```

这些阶段的输入重点是继续跳跃或确认行动。若此时允许 Tab 切棋子，会破坏“当前未提交行动”的操作栈语义。

---

## 6. 实现方案

### 6.1 Gameplay 层

新增只读查询接口：

```cpp
bool FTerraGameplayContainer::CollectCurrentFactionSelectablePieceIds(TArray<int32>& OutPieceIds) const;
```

职责：

- 复用 Gameplay 内部 `IsPieceSelectable_` 语义。
- 输出当前阵营可导航棋子 PieceId。
- 按 PieceId 升序排序。

### 6.2 Tess 层

新增公开输入入口：

```cpp
bool APlanetTessellatedMesh::HandleC5NavigateCurrentFactionPiece(bool bReverse);
```

职责：

1. 检查 Gameplay 是否初始化。
2. 只允许在 `Idle` 或 `PieceSelected` 阶段执行。
3. 查询当前阵营可导航棋子。
4. 根据当前选中棋子和方向计算目标 PieceId。
5. 取目标 CellId。
6. 调用统一的 `HandleGameplayCellClick_(...)`。

同时将原 `HandleHISMClickHit(...)` 中的点击后处理抽成私有共用函数：

```cpp
bool HandleGameplayCellClick_(int32 CellId, const TCHAR* SourceLabel, int32 InstanceIndex, const FString& ComponentName);
```

HISM 鼠标点击和 C5 Tab 导航都走它。

### 6.3 Controller 层

在 `APlanetInteractionController::PlayerTick(...)` 中监听：

```cpp
WasInputKeyJustPressed(EKeys::Tab)
```

并用：

```cpp
IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift)
```

判断是否反向循环。

---

## 7. 验收

1. 当前回合处于 Idle，按 `Tab`：选中当前阵营 PieceId 最小的可行动棋子，并显示移动 / 跳跃高亮。
2. 当前回合处于 Idle，按 `Shift+Tab`：选中当前阵营 PieceId 最大的可行动棋子，并显示移动 / 跳跃高亮。
3. 已选中一个棋子时连续按 `Tab`：按 PieceId 升序循环切换己方可行动棋子。
4. 已选中一个棋子时连续按 `Shift+Tab`：按 PieceId 降序循环切换己方可行动棋子。
5. 被切到的棋子若超过 C4 舒适角距离阈值，镜头按 C4 逻辑平滑聚焦；若在舒适区内，镜头不动。
6. Tab 选中后按右键 Undo：回到无选中棋子的 Idle 状态，高亮清理为当前阵营底色。
7. 棋子已经移动后，按 `Tab / Shift+Tab` 不切换棋子，不破坏当前待确认行动。

---

## 8. 暂不处理

C5 暂不实现：

- 屏幕边缘棋子方向提示。
- Tab 序号 UI。
- 按距离当前镜头排序。
- 多选或框选。
- 给不可行动棋子单独导航。
