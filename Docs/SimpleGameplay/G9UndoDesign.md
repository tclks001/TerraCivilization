# TerraCivilization SimpleGameplay G9 撤销本次点选设计稿

> 本稿承接 G8：当前已经支持选中、普通移动、跳跃/连跳、吃子结算、行动日志，以及手动球面相机控制。
>
> G9 的目标是在不改变现有行动规则的前提下，为“尚未结束回合的当前操作”增加一次撤销能力，方便玩家纠正误点，也方便后续调试动画和交互表现。

---

## 1. G9 目标

本阶段新增：

1. **右键撤销上次点击**
   - 玩家已经进入本次点选流程后，按下右键即可撤销上一次成功点击。
   - 如果当前没有进入点选流程，右键无效果。

2. **回退到上次点击前完整状态**
   - 棋子位置恢复。
   - 选中状态恢复。
   - Gameplay 高亮恢复。
   - 可移动 / 可跳跃目标恢复。
   - 待吃子缓存恢复。
   - 行动路径缓存恢复。

3. **撤销采用操作栈**
   - 每次成功点击前先压入一个快照。
   - 右键时弹出栈顶并恢复。
   - 可以连续多次右键，逐步回退整条点击链。

4. **撤销只作用于未提交行动**
   - 一旦玩家已经点击自身结束回合并完成结算，G9 不再回滚那次行动。

---

## 2. “本次点选”定义

G9 中“一次点选”定义为：

```text
从玩家在 Idle 状态首次点中一个己方可行动棋子开始
到玩家点击自身结束回合、正式提交本次行动为止
```

因此：

- 仅选中但未移动：可撤销。
- 已普通移动但未结束回合：可撤销。
- 已跳跃 / 连跳但未结束回合：可撤销。
- 已结束回合：不可撤销。

点击链示例：

```text
点击棋子 -> 点击目的地移动 -> 点击自身确认结束回合
```

在第二步之后按右键，应回到：

```text
点击棋子之后、但尚未移动之前
```

而不是直接回到回合开始。

---

## 3. 撤销语义

右键撤销后，容器应恢复到“上次成功点击之前”的状态。

例如普通移动链：

```text
S0 = 回合开始
S1 = 点击棋子后（进入 PieceSelected）
S2 = 点击目的地移动后（进入 PieceMovedCanEndTurn）
S3 = 点击自身确认后（回合提交）
```

则：

- 在 `S2` 按右键，应回到 `S1`
- 在 `S1` 再按右键，应回到 `S0`
- 到了 `S3` 后，不再允许回滚

因此 G9 不再是“单份快照覆盖式回退”，而是：

```text
UndoStack.Push(点击前状态)
RightClick -> Restore(UndoStack.Top) -> Pop
```

---

## 4. 设计策略

G9 采用**完整快照 + 操作栈回滚**，而不是按步骤做逆操作。

### 4.1 为什么不用“逆操作”

如果只做“把棋子移回去”这种局部逆操作，会漏很多状态：

- `SelectedPieceId`
- `SelectedPieceStartCellId`
- `LastJumpStartCellId`
- `GameplayHighlights`
- `OrdinaryMoveTargetCellIds`
- `JumpTargetCellIds`
- `ActionTargetCellIdToCaptureCellIds`
- `PendingCapturePieceIds`
- `PendingCaptureCellIds`
- `CurrentActionPathCellIds`

这些状态一旦漏回滚，就容易出现：

- 高亮残留
- 连跳状态残留
- 路径日志污染
- 右键后还能错误结束回合

### 4.2 采用快照栈

因此 G9 在每次成功点击、且即将发生状态改变之前，都保存一份快照到栈中：

```text
InteractionUndoSnapshots.Push(ClickBeforeSnapshot)
```

右键撤销时：

```text
Restore(InteractionUndoSnapshots.Top)
InteractionUndoSnapshots.Pop()
```

这样可以自然支持：

- 选中后退回回合开始
- 移动后退回选中态
- 连跳多步后逐步一层层退回

---

## 5. 快照内容

快照至少保存以下字段：

| 字段 | 含义 |
| --- | --- |
| `Pieces` | 所有棋子运行时状态 |
| `CellToPieceId` | 棋盘占用表 |
| `GameplayHighlights` | 当前 Gameplay 高亮 |
| `OrdinaryMoveTargetCellIds` | 普通移动候选 |
| `JumpTargetCellIds` | 跳跃候选 |
| `ActionTargetCellIdToCaptureCellIds` | 目标 Cell 对应的可吃子预览 |
| `PendingCapturePieceIds` | 当前已锁定待吃子 |
| `PendingCaptureCellIds` | 当前待吃子所在 Cell |
| `SelectedPieceId` | 当前选中棋子 |
| `SelectedPieceStartCellId` | 本次行动起点 |
| `LastJumpStartCellId` | 连跳回退限制辅助状态 |
| `CurrentActionPathCellIds` | G7 行动路径缓存 |
| `InteractionPhase` | 当前交互阶段 |

说明：

- `CurrentFactionId`、`TurnIndex`、`WinningFactionId`、`Factions` 在未结束回合前不会被本次点选改变，因此初版不要求进快照。
- 只保存“当前行动可能改动到”的局部状态即可。

---

## 6. 容器接口设计

`FTerraGameplayContainer` 新增一个公开接口：

```cpp
bool UndoCurrentInteraction(TArray<int32>& OutDirtyCellIds);
```

返回语义：

- `true`：本次确实发生了撤销。
- `false`：当前没有可撤销的进行中点选。

### 6.1 快照保存时机

凡是一次**成功点击并将改变交互状态**时，都要先压栈保存快照。

包括：

- 成功选中棋子
- 成功普通移动
- 成功跳跃
- 成功从一个已选中棋子切换到另一个已选中棋子

不包括：

- 失败点击
- hover
- 已提交的结束回合点击

### 6.2 快照清空时机

以下时机清空快照：

1. 成功结束回合后
2. Gameplay 重新初始化时

说明：

- **右键撤销后不清空整栈**，只弹出栈顶。
- 这样才能继续向前回退更早的一步。

---

## 7. 右键输入链路

输入仍复用：

```text
APlanetInteractionController
    -> APlanetTessellatedMesh
        -> FTerraGameplayContainer
```

G9 新增：

1. `PlanetInteractionController::PlayerTick()` 检测右键。
2. 若当前命中的 `PlanetTessellatedMesh` 存在 Gameplay 容器，则调用新接口：

```cpp
Tess->HandleHISMUndo()
```

3. 再由 `APlanetTessellatedMesh` 内部调用：

```cpp
GameplayContainer->UndoCurrentInteraction(DirtyCellIds)
```

4. 最后刷新：
   - Gameplay 高亮
   - 当前阵营棋子底色
   - 调试棋子缓存

---

## 8. `APlanetTessellatedMesh` 职责

G9 不让输入层直接碰 Gameplay 容器细节。

因此 `APlanetTessellatedMesh` 新增一个桥接口：

```cpp
bool HandleHISMUndo();
```

职责：

1. 检查 Gameplay 容器是否 ready。
2. 调用 `UndoCurrentInteraction`。
3. 刷新脏 Cell 的高亮。
4. 刷新当前阵营底色。
5. 重建 G1 调试棋子缓存。
6. 输出一条 `UE_LOG`。

---

## 9. 相机语义

G9 初版不强制额外做复杂相机恢复。

理由：

- 用户要求必须恢复的是棋子位置、高亮状态等玩法状态。
- G2.5/G8 相机已有自己的自动聚焦与手动控制逻辑。
- 右键撤销后，相机保持当前观察位更方便玩家继续重新操作。

因此：

- 撤销只回滚 Gameplay 状态。
- 不回滚相机世界位置与朝向。

---

## 10. 项目挂载与验收步骤

G9 不需要新增蓝图资产，只沿用已有入口：

- `BP_HexTestGameMode`
- `BP_PlanetInteractionController`
- `BP_PlanetBinder`
- `BP_PlanetTessellatedMesh`

你只需要确认：

1. 当前关卡使用 `BP_HexTestGameMode`
2. 其 `PlayerControllerClass` 是 `BP_PlanetInteractionController`
3. 关卡中 `BP_PlanetBinder` 的 `TessellatedMeshRef` 指向 `BP_PlanetTessellatedMesh`

然后进入 PIE 做以下验收：

### 10.1 仅选中后撤销

1. 左键点击一个己方棋子。
2. 看到黄色选中和可行动高亮。
3. 按右键。
4. 应恢复到未选中状态。

### 10.2 普通移动后撤销

1. 左键选中棋子。
2. 左键移动到一个蓝色落点。
3. 但先不要点击自身结束回合。
4. 按右键。
5. 棋子应回到“选中但尚未移动”的状态，而不是直接回到回合开始。
6. 再按一次右键，才回到回合开始。

### 10.3 连跳中撤销

1. 左键选中可跳跃棋子。
2. 执行一次或多次跳跃。
3. 但先不要结束回合。
4. 按右键。
5. 棋子应只回退一跳。
6. 再按右键，继续回退上一跳。
7. 一直退到首次选中时，再按右键，才回到回合开始。

### 10.4 已结束回合后

1. 完整执行一次行动并结束回合。
2. 按右键。
3. 不应回滚已经提交的行动。

---

## 11. 验收清单

### 11.1 状态恢复

- 棋子位置恢复。
- 选中状态恢复。
- 高亮恢复。
- 连跳状态恢复。
- 每次只回滚一层点击。

### 11.2 行动路径恢复

- G7 的 `CurrentActionPathCellIds` 被清回点选前状态。
- 撤销后的下一次正式行动日志不包含已撤销路径残留。

### 11.3 不影响已提交回合

- 一旦结束回合，右键不再回滚该次行动。

---

## 12. 暂不处理

- 多层 undo / redo。
- 回滚相机位置。
- 回滚已经写入文件的历史日志。
- 跨回合撤销。
> Implementation Update
>
> - Undo 编排已从 `APlanetTessellatedMesh` 迁移到 `UPlanetGameplayComponent::HandleHISMUndo()`。
> - `APlanetTessellatedMesh::HandleHISMUndo()` 现仅为兼容桥接入口。
> - Undo 前后棋子快照比较、反向 `MoveEvent` 生成、HISM 高亮刷新、P1 表现同步，现都由 `UPlanetGameplayComponent` 负责。
