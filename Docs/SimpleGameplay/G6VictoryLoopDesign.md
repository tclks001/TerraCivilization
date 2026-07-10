# TerraCivilization SimpleGameplay G6 胜负闭环设计稿

> 本稿承接 G5：当前已经支持普通移动、跳跃/连跳、基础二吃一、弓兵远程吃子、山脉增程、森林防远程，以及“点击自身结束回合后统一结算吃子”。
>
> G6 目标是把“主将被吃 -> 阵营失败 -> 跳过失败阵营 -> 只剩一方时宣布胜利”这条闭环补齐，并直接映射到当前 `FTerraGameplayContainer` 的 C++ 实现。

---

## 1. G6 目标

本阶段新增：

1. **主将被吃即阵营失败**
   - 若本次待结算被吃目标中包含某阵营主将，该阵营立即判定失败。
   - 失败阵营的 `FactionState.bAlive=false`。

2. **失败阵营剩余棋子全部移除**
   - 初版不保留“残兵继续阻挡”。
   - 阵营失败后，该阵营所有仍存活棋子立即从棋盘移除。
   - 这样后续行动、选中、高亮、回合推进都只面对存活阵营。

3. **回合推进时自动跳过失败阵营**
   - `AdvanceTurn_()` 只会停在 `FactionState.bAlive=true` 的阵营上。
   - 如果当前行动阵营在本回合结束后仍存活，也可能被正常轮转跳过到下一个存活阵营。

4. **只剩一个阵营时宣布胜利并封盘**
   - 若吃子结算完成后，棋盘上只剩 1 个存活阵营，则该阵营胜利。
   - 对局进入终局态，不再接受新的点击行动。
   - 初版不额外新增 UI 面板；先通过运行时状态和日志完成闭环。

---

## 2. 结算顺序

G6 沿用 G4/G5 的“点击自身结束回合后统一结算”时机，但把后处理顺序固定为：

```text
1. 结算 PendingCapturePieceIds
2. 找出其中被吃主将所属阵营
3. 将这些阵营标记为失败
4. 失败阵营剩余全部棋子移除
5. 统计当前存活阵营数
6. 若只剩 1 个存活阵营：
     - 记录 WinnerFactionId
     - 对局封盘
   否则：
     - 推进到下一个存活阵营
```

这样可以保证：

- 同一回合可以同时击败多个阵营。
- 某阵营主将被吃后，不会再保留其余棋子参与后续回合。
- 胜利判定基于“阵营失败全部完成后的最终局面”，不会受移除顺序干扰。

---

## 3. 当前容器上的最小状态扩展

`FTerraGameplayContainer` 新增：

```cpp
bool IsMatchEnded() const { return bMatchEnded; }
int32 GetWinningFactionId() const { return WinningFactionId; }
```

以及内部字段：

```cpp
bool bMatchEnded = false;
int32 WinningFactionId = INDEX_NONE;
```

含义：

| 字段 | 含义 |
| --- | --- |
| `bMatchEnded` | 对局是否已经结束；结束后不再响应棋盘点击 |
| `WinningFactionId` | 胜利阵营；若尚未决出则为 `INDEX_NONE` |

说明：

- 初版不额外增加公开“失败事件”或“胜利事件”委托。
- 当前阵营 `CurrentFactionId` 在胜利后保持为获胜阵营，便于现有渲染桥继续把胜者棋子当作当前阵营棋子渲染。

---

## 4. 新增内部 helper

为保持改动局部，G6 只在 `FTerraGameplayContainer` 内新增三个 helper：

| Helper | 职责 |
| --- | --- |
| `EliminateFaction_` | 将一个阵营置为失败，并移除其所有存活棋子 |
| `EvaluateWinStateAfterCaptures_` | 在吃子结算后统计存活阵营，决定是否进入终局 |
| `FinalizeTurnAfterResolution_` | 在结算完成后统一做“终局 or 推进回合”收尾 |

设计原则：

- 不拆出新 service，延续 G3/G4/G5 当前“容器内聚实现”风格。
- 先让闭环可玩，再视需要抽成 `TurnService` / `WinService`。

---

## 5. 主将失败判定细节

### 5.1 谁算主将

仍以：

```cpp
Piece.PieceType == ETerraGameplayPieceType::Commander
```

作为唯一标准。

### 5.2 同回合同时击败多个阵营

允许。

例如一次行动同时命中两个敌方主将，则两个阵营都立即失败并清盘。

### 5.3 失败阵营的主将自身

主将和本回合其他被吃棋子一样，先在 `ResolvePendingCaptures_()` 中被移除；
随后 `EliminateFaction_()` 会继续清理该阵营剩余存活棋子。

---

## 6. 回合推进语义

### 6.1 非终局

若存活阵营数大于 1：

- 继续使用现有 `AdvanceTurn_()`。
- 该函数只会在 `bAlive=true` 的阵营中循环。

### 6.2 终局

若存活阵营数为 1：

- `bMatchEnded=true`
- `WinningFactionId=唯一存活阵营`
- `CurrentFactionId=WinningFactionId`
- `InteractionPhase=Idle`
- `HandleCellClick()` 直接返回 `false`

初版不自动切镜头、不追加特殊高亮颜色；保持当前相机和高亮逻辑可继续工作即可。

---

## 7. 验收清单

### 7.1 主将失败

- 吃掉普通棋子时，不影响阵营存活状态。
- 吃掉主将时，该主将所属阵营 `bAlive` 变为 `false`。
- 该阵营其余棋子全部从棋盘移除。

### 7.2 跳过失败阵营

- 某阵营失败后，后续 `AdvanceTurn_()` 不会再轮到它。
- 当前阵营高亮只会出现在存活阵营上。

### 7.3 最终胜利

- 当只剩 1 个存活阵营时，记录 `WinningFactionId`。
- 对局进入封盘状态，后续点击棋盘不会再产生移动或回合变化。

---

## 8. 暂不处理

- 失败/胜利专用 UI。
- 胜利动画、音效、延迟清盘。
- 平局规则。
- 多主将、多队伍共享胜负。


