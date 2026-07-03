# TerraCivilization SimpleGameplay G5 弓兵远程吃子设计稿

> 本稿记录 G5 已落地的规则与当前 C++ 实现语义。
>
> 范围：在 G4 基础二吃一之上，实现弓兵远程吃子、山脉增加射程、森林防远程，以及“最近可攻击目标 / 阻挡 / 行动棋子激活”相关边界。

---

## 1. 目标

G5 的目标是在已有的 `A A B` 基础二吃一上，为弓兵增加远程二吃一变体：

```text
普通弓兵远程：A' A - B
山脉弓兵远程：A' A - - B
```

其中：

| 符号 | 含义 |
| --- | --- |
| `A'` | 当前阵营弓兵，远程攻击者 |
| `A` | 当前阵营任意棋子，远程先锋 |
| `-` | 空 Cell，或森林中不可被远程攻击的敌方目标后继续扫描的通过点 |
| `B` | 敌方目标 |

G5 不改变 G3/G4 的交互主流程：

- 选择棋子后仍高亮普通移动 / 跳跃落点。
- 如果走到某个落点会形成吃子，则该落点对应的可吃目标仍使用深红色高亮。
- Hover 到落点时，对该落点可吃目标的高亮加深仍由现有 HISM 高亮桥接逻辑处理。
- 跳跃链中吃子只统计预览与待结算目标，真正移除仍发生在玩家点击自身结束行动之后。

---

## 2. 代码落点

当前 G5 逻辑集中在：

- `Source/Gameplay/Public/TerraGameplayContainer.h`
- `Source/Gameplay/Private/TerraGameplayContainer.cpp`

核心函数：

```cpp
void FTerraGameplayContainer::CollectCaptureCellsAfterHypotheticalMove_(
    const FTerraGameplayPieceState& Piece,
    int32 TargetCellId,
    TSet<int32>& OutCaptureCellIds) const;
```

该函数承担“假设当前行动棋子移动 / 跳跃到 `TargetCellId` 后，会吃掉哪些敌方棋子”的统一收集职责。

它现在包含两类规则：

1. **G4 基础二吃一**：扫描 `A A B`。
2. **G5 弓兵远程二吃一**：扫描 `A' A - B` 与山脉弓兵 `A' A - - B`。

---

## 3. 假想棋盘状态

吃子预览与结算都基于假想移动后的棋盘状态。

当前实现通过局部 lambda 模拟行动棋子的位置：

```text
GetHypotheticalPieceAtCell(CellId)
```

语义：

| 查询 Cell | 返回结果 |
| --- | --- |
| `TargetCellId` | 返回本次行动棋子 `Piece` |
| `Piece.CellId` | 视为空，因为行动棋子已经离开原位置 |
| 其他 Cell | 查询当前真实棋盘占用 |

因此，G5 远程判断与 G4 基础吃子一样，都反映“棋子走到这一步之后”的结果，而不是走之前的棋盘结果。

---

## 4. 基础二吃一保留逻辑

G5 没有删除或削弱 G4。

对 `TargetCellId` 周围每一个本方相邻棋子 `FirstCellId`，当前函数仍检查两种基础阵型：

### 4.1 行动棋子作为攻击者

```text
TargetCellId  FirstCellId  EnemyCellId
A             A            B
```

实现路径：

```text
StepForwardBranches_(TargetCellId, FirstCellId)
```

如果前方候选 Cell 内是敌方棋子，则加入 `OutCaptureCellIds`。

### 4.2 行动棋子作为先锋

```text
FirstCellId  TargetCellId  EnemyCellId
A            A             B
```

实现路径：

```text
StepForwardBranches_(FirstCellId, TargetCellId)
```

如果前方候选 Cell 内是敌方棋子，则加入 `OutCaptureCellIds`。

### 4.3 与森林的关系

基础二吃一不受森林影响。

如果 `EnemyCellId` 位于森林，只要满足 `A A B`，仍会被基础二吃一吃掉。

---

## 5. 弓兵远程扫描入口

当前实现的远程逻辑在 `CollectCaptureCellsAfterHypotheticalMove_` 内部通过局部 lambda 完成：

```text
ScanArcherRemoteCapture(ArcherCellId, FrontCellId)
```

它只会扫描与本次行动棋子相关的远程阵型，避免旧阵型每回合重复触发。

对行动棋子落点 `TargetCellId` 的每一个本方相邻棋子 `FirstCellId`，当前代码调用两次：

```text
ScanArcherRemoteCapture(TargetCellId, FirstCellId)
ScanArcherRemoteCapture(FirstCellId, TargetCellId)
```

含义如下。

### 5.1 行动棋子作为弓兵攻击者

```text
TargetCellId  FirstCellId  ...  Enemy
A'            A                 B
```

要求：

- 行动棋子走到 `TargetCellId` 后必须是本方弓兵。
- `FirstCellId` 必须是本方存活棋子。
- 从 `A' -> A` 方向继续扫描远程目标。

### 5.2 行动棋子作为远程先锋

```text
FirstCellId  TargetCellId  ...  Enemy
A'           A                  B
```

要求：

- `FirstCellId` 必须是本方存活弓兵。
- 行动棋子走到 `TargetCellId` 后作为本方先锋。
- 从 `A' -> A` 方向继续扫描远程目标。

### 5.3 不扫描的情况

当前实现不会扫描与本次行动棋子无关的远程阵型。

例如棋盘上原本已有一组静止的 `A' A - B`，但本次行动棋子不在这条远程阵型中，则不会因为本回合其他棋子行动而重复触发这组远程吃子。

---

## 6. 弓兵与先锋合法性

`ScanArcherRemoteCapture(ArcherCellId, FrontCellId)` 的入口校验如下：

| 条件 | 要求 |
| --- | --- |
| 弓兵 Cell | 必须有存活棋子 |
| 弓兵阵营 | 必须属于当前行动阵营 |
| 弓兵类型 | 必须是 `ETerraGameplayPieceType::Archer` |
| 先锋 Cell | 必须有存活棋子 |
| 先锋阵营 | 必须属于当前行动阵营 |

只要任一条件不满足，远程扫描直接返回。

注意：

- 先锋不要求是可移动棋子。
- 主将、步兵、骑兵、弓兵都可以作为先锋。
- 弓兵自己也必须属于当前行动阵营。

---

## 7. 射程规则

当前实现以“从弓兵 Cell 开始计距离”。

| 弓兵所在地形 | 最大距离 | 对应阵型 |
| --- | --- | --- |
| 非山脉 | `3` | `A' A - B` |
| 山脉 | `4` | `A' A - - B` |

实现语义：

```text
MaxDistanceFromArcher = 弓兵在山脉 ? 4 : 3
```

距离示意：

```text
Distance 0: A' 弓兵
Distance 1: A  先锋
Distance 2: -  中间格
Distance 3: B  普通弓兵最远目标
Distance 4: B  山脉弓兵最远目标
```

---

## 8. 分叉直线扫描

G5 复用现有直线推进接口：

```cpp
StepForwardBranches_(PrevCellId, CurCellId, OutNextCellIds)
```

因此远程扫描继承 G3/G4 的球面 Hex/Pent 直线语义：

- 六边形 Cell 上继续前进时通常只有一个后继。
- 五边形 Cell 上继续前进时可能产生两个后继，形成“人”字形分叉。
- 每个分叉会独立扫描。
- 多个分支命中同一敌方 Cell 时，由 `TSet<int32> OutCaptureCellIds` 自动去重。

当前远程扫描从 `A' -> A` 的下一步开始：

```text
InitialCandidates = StepForwardBranches_(ArcherCellId, FrontCellId)
```

初始候选 Cell 的距离记为 `2`。

---

## 9. 远程目标判定与阻挡规则

当前实现对每一条远程分支按距离从近到远扫描，直到命中目标、遇到阻挡、超过射程或射线终止。

### 9.1 空 Cell

如果当前扫描 Cell 为空：

```text
继续扫描下一格
```

这对应 `A' A - B` 或 `A' A - - B` 中的 `-`。

### 9.2 本方棋子阻挡

如果当前扫描 Cell 是本方棋子：

```text
停止当前分支
不攻击其后方目标
```

示例：

```text
A' A A B
```

弓兵 `A'` 不能越过本方棋子攻击 `B`。

### 9.3 近战距离内的敌方棋子不作为远程目标

如果当前扫描 Cell 是敌方棋子，且距离弓兵 `<= 2`：

```text
停止当前分支
不作为远程吃子目标
```

这用于保证“若最近目标通过基础二吃一即可攻击到，则不作为远程结算”。

示例：

```text
A' A B
```

这里 `B` 位于距离 `2`，属于基础 `A A B` 的范围。

结果：

- G4 基础二吃一会负责吃掉 `B`。
- G5 远程扫描不会重复把它作为远程目标。
- 该敌方棋子也会阻挡当前远程分支继续攻击后排。

### 9.4 非森林敌方棋子是最近可攻击目标

如果当前扫描 Cell 是敌方棋子，距离弓兵 `> 2`，且该 Cell 不是森林：

```text
加入 OutCaptureCellIds
停止当前分支
```

这实现“每个方向只能攻击最近的可攻击目标”。

示例：

```text
A' A - B B2
```

普通弓兵只攻击 `B`，不会继续攻击 `B2`。

山脉弓兵同理：

```text
A' A - B B2
```

即使射程够到 `B2`，也只攻击最近可攻击目标 `B`。

### 9.5 森林敌方目标不可被远程攻击，但允许继续扫描

如果当前扫描 Cell 是敌方棋子，距离弓兵 `> 2`，但该 Cell 地形是森林：

```text
不加入 OutCaptureCellIds
继续扫描下一格
```

这体现“森林防远程”，同时也匹配当前实现允许越过不可远程攻击的森林目标继续寻找后方可攻击目标。

示例：

```text
A' A - B' B
```

若：

- `A'` 是山脉弓兵，射程为 `4`。
- `B'` 位于森林。
- `B` 不在森林。

则结果是：

```text
B' 不被吃
B 被远程吃掉
```

如果 `B'` 不在森林，则结果是：

```text
B' 被吃
B 不会被扫描到
```

### 9.6 超出射程

如果当前扫描距离超过 `MaxDistanceFromArcher`：

```text
忽略该 Cell
停止继续展开
```

如果当前 Cell 是森林敌军，但已经达到最大射程，也不会继续越过它寻找更远目标。

---

## 10. 普通弓兵与山脉弓兵示例

### 10.1 普通弓兵成功远程吃子

```text
A' A - B
```

条件：

- `A'` 是本方弓兵。
- `A` 是本方先锋。
- `-` 为空。
- `B` 是敌方棋子且不在森林。
- `A'` 不在山脉也可以攻击到距离 `3` 的 `B`。

结果：

```text
B 加入可吃目标
```

### 10.2 普通弓兵不能攻击距离 4 目标

```text
A' A - - B
```

如果 `A'` 不在山脉，则最大距离为 `3`。

结果：

```text
B 不会被远程吃掉
```

### 10.3 山脉弓兵攻击距离 4 目标

```text
A' A - - B
```

如果 `A'` 位于山脉，则最大距离为 `4`。

结果：

```text
B 加入可吃目标
```

### 10.4 森林保护远程目标

```text
A' A - B
```

如果 `B` 位于森林：

```text
B 不会被远程吃掉
```

但如果同样的 `B` 可通过基础 `A A B` 吃掉，则森林不提供保护。

---

## 11. 与高亮和结算的关系

G5 不新增单独的高亮通道。

当前流程仍是：

```text
CollectCaptureCellsAfterHypotheticalMove_
  -> RebuildActionTargetCapturePreviews_
  -> ActionTargetCellIdToCaptureCellIds
  -> 深红色高亮可吃目标
  -> Hover 到落点时高亮加深
  -> 点击自身结束行动
  -> PendingCapturePieceIds / PendingCaptureCellIds
  -> ResolvePendingCaptures_
```

因此，远程吃子目标和基础吃子目标在 UI 上使用同一种“可吃目标”表现。

对于玩家来说：

- 点击某个普通移动 / 跳跃落点前，可以看到该落点会吃掉的所有目标。
- 如果这些目标来自弓兵远程，视觉上仍是深红色可吃目标。
- 真正移除仍发生在结束行动时。

---

## 12. 当前实现的重要边界

### 12.1 每个远程分支只吃最近可攻击目标

一条分支命中第一个非森林敌方远程目标后立即停止。

### 12.2 不能越过我方单位

本方单位在远程扫描范围内会阻挡后排。

### 12.3 不能越过普通可攻击敌人

非森林敌军一旦可被远程攻击，会作为最近目标被加入，并阻止继续扫描后排。

### 12.4 可以越过森林中的敌军

森林敌军不被远程吃掉，且当前实现允许继续扫描其后方。

### 12.5 近战范围敌军不走远程逻辑

距离弓兵 `2` 的敌军属于基础二吃一范围。

远程扫描不添加它，也不越过它。

### 12.6 同一目标去重

所有目标写入 `TSet<int32>`，所以同一敌方 Cell 被基础吃子和多个远程分支同时命中时，只记录一次。

---

## 13. 初版暂不处理的问题

G5 当前实现不处理以下内容：

- 不区分“远程吃子目标”和“基础吃子目标”的 UI 图标或颜色。
- 不为弓兵远程绘制攻击连线。
- 不展示每个目标来自哪一个阵型。
- 不实现复杂连锁吃子。
- 不因为吃掉某个目标立即触发新的远程扫描。
- 不处理 AI 决策中的远程收益评估。
- 不实现弓兵以外兵种的特殊攻击。

---

## 14. 验收清单

### 14.1 编译验收

- `FTerraGameplayContainer::CollectCaptureCellsAfterHypotheticalMove_` 能正常编译。
- `TerraGameplayContainer.h` 与 `TerraGameplayContainer.cpp` 中函数声明 / 定义名称一致。

### 14.2 基础二吃一回归

布置或走出：

```text
A A B
```

确认：

- `B` 仍显示为可吃目标。
- `B` 位于森林时仍可被基础二吃一吃掉。

### 14.3 普通弓兵远程

布置或走出：

```text
A' A - B
```

确认：

- `A'` 不在山脉时也可吃 `B`。
- `B` 不在森林时显示为可吃目标。

### 14.4 普通弓兵射程限制

布置或走出：

```text
A' A - - B
```

确认：

- `A'` 不在山脉时，`B` 不显示为可吃目标。

### 14.5 山脉弓兵加距

布置或走出：

```text
A' A - - B
```

并让 `A'` 位于山脉。

确认：

- `B` 显示为可吃目标。

### 14.6 森林防远程

布置或走出：

```text
A' A - B
```

并让 `B` 位于森林。

确认：

- `B` 不显示为远程可吃目标。

### 14.7 森林敌军可被越过

布置或走出：

```text
A' A - B' B
```

并让：

- `A'` 位于山脉。
- `B'` 位于森林。
- `B` 不在森林。

确认：

- `B'` 不显示为远程可吃目标。
- `B` 显示为远程可吃目标。

### 14.8 最近可攻击目标

布置或走出：

```text
A' A - B B2
```

并让 `B` 不在森林。

确认：

- `B` 显示为可吃目标。
- `B2` 不显示为可吃目标。

### 14.9 我方阻挡

布置或走出：

```text
A' A A B
```

确认：

- 弓兵 `A'` 不能越过第二个我方 `A` 攻击 `B`。

### 14.10 行动棋子激活限制

确认只有与本次行动棋子相关的阵型会触发：

- 行动棋子作为弓兵攻击者。
- 行动棋子作为远程先锋。
- 行动棋子作为基础二吃一攻击者。
- 行动棋子作为基础二吃一先锋。

棋盘上已有但与本次行动棋子无关的旧阵型不应重复结算。


