# TerraCivilization SimpleGameplay G7 行动日志设计稿

> 本稿承接 G6：当前已经支持回合推进、跳跃/连跳、吃子结算、主将失败与终局封盘。
>
> G7 的目标是在不改变现有玩法规则的前提下，把“一个玩家完整结束一次行动”输出为结构化日志，同时写入文件并在 `UE_LOG` 中输出一行，便于后续回放、排错与验收。

---

## 1. G7 目标

本阶段新增：

1. **每次完整行动输出一条结构化日志**
   - 只在玩家点击己方已行动棋子、正式结束本回合时输出。
   - 选中、半途中跳跃、预览高亮都不单独记日志。

2. **日志字段固定且最小化**
   - `turnIndex`
   - `playerId`
   - `pieceId`
   - `pathCellIds`
   - `capturedPieceIds`

3. **完整记录行动路径**
   - `pathCellIds[0]` 是行动起点。
   - `pathCellIds[last]` 是行动终点。
   - 若存在连跳，中间每一个落点都按顺序保留。

4. **每次启动独立日志文件**
   - 每次 PIE 启动或游戏启动都会生成一个带时间戳的新日志文件。
   - 不同对局的日志不混写到同一文件里。

5. **双通道输出**
   - 追加写入文件。
   - 同时在 `UE_LOG` 中输出同一条 JSON 字符串。

---

## 2. 行动定义

G7 中“一次行动”定义为：

```text
从玩家选中一个己方棋子开始
到该棋子点击自身结束回合、并完成吃子结算为止
```

因此：

- 普通移动后结束回合：记 1 条。
- 跳跃、连跳后结束回合：仍只记 1 条。
- 中途每一次跳点切换：不单独记日志，但会进入 `pathCellIds`。

---

## 3. 日志结构

每条日志输出一个 JSON object：

```json
{
  "turnIndex": 12,
  "playerId": 3,
  "pieceId": 41,
  "pathCellIds": [128, 137, 145],
  "capturedPieceIds": [77, 92]
}
```

字段语义：

| 字段 | 含义 |
| --- | --- |
| `turnIndex` | 本次行动开始并结束时所属的回合序号；取回合推进前的当前值 |
| `playerId` | 本次行动所属阵营 ID |
| `pieceId` | 执行动作的棋子 ID |
| `pathCellIds` | 本次行动完整路径；首项为起点，末项为终点，中间为所有经过落点 |
| `capturedPieceIds` | 本次行动最终结算吃掉的棋子 ID 列表；无吃子时为空数组 |

---

## 4. 输出时机

日志输出严格放在 `TryEndTurnOnSelectedCell_()` 内完成，顺序为：

```text
1. 读取当前行动上下文
   - TurnIndex
   - CurrentFactionId
   - SelectedPieceId
   - CurrentActionPathCellIds
   - PendingCapturePieceIds
2. 结算吃子
3. 输出一条 JSON 日志
4. 再执行回合推进 / 终局收尾
```

这样可保证：

- 记录到的是“真实结束路径”。
- `capturedPieceIds` 与实际结算使用的是同一份待吃集合。
- 日志所属 `turnIndex` 不会被 `AdvanceTurn_()` 提前加 1。

---

## 5. 文件落点

初版采用项目 Saved 目录下的单局独立文件追加写入：

```text
Saved/Logs/TerraGameplayActionLog_20260703_220501.jsonl
```

说明：

- 使用 `.jsonl`（JSON Lines）格式。
- 每一行都是一个独立 JSON object。
- 文件名中的时间戳在 `Initialize()` 时生成一次，本局内复用。
- 同一局内持续追加，不同局之间自然分文件。

---

## 6. 容器内最小改动

G7 继续保持 `FTerraGameplayContainer` 内聚实现，只新增最小状态和 helper：

| 名称 | 职责 |
| --- | --- |
| `CurrentActionPathCellIds` | 暂存当前行动的完整路径 |
| `ActionLogFilePath` | 暂存当前对局的日志文件绝对路径 |
| `InitializeActionLogFilePath_` | 在本局初始化时生成一次时间戳日志文件名 |
| `BuildActionLogJson_` | 把一次行动的最小字段序列化为 JSON 字符串 |
| `AppendActionLogToFile_` | 把 JSON 字符串按一行追加到当前对局日志文件 |
| `EmitActionLog_` | 同时做文件输出与 `UE_LOG` 输出 |

设计原则：

- 不新增独立日志 service。
- 不引入回放系统。
- 先保证格式稳定、字段固定、时机正确。

---

## 7. 路径记录规则

### 7.1 选中时初始化

当玩家首次选中一个可行动棋子时：

```text
CurrentActionPathCellIds = [当前棋子所在 Cell]
```

### 7.2 普通移动

当普通移动成功时：

```text
CurrentActionPathCellIds 追加 TargetCellId
```

通常普通移动路径长度为 2。

### 7.3 跳跃 / 连跳

每次跳跃成功时：

```text
CurrentActionPathCellIds 追加本次跳跃落点
```

例如：

```text
起点 120 -> 第一跳 133 -> 第二跳 149
```

输出为：

```json
"pathCellIds": [120, 133, 149]
```

---

## 8. 验收清单

### 8.1 普通移动

- 玩家完成一次普通移动并结束回合后，文件新增 1 行 JSON。
- `pathCellIds` 形如 `[起点, 终点]`。

### 8.2 连跳

- 玩家连续跳跃多次后结束回合，只输出 1 行 JSON。
- `pathCellIds` 中按顺序包含所有中途落点。

### 8.3 吃子

- 若本次行动吃掉多个棋子，`capturedPieceIds` 按数组输出全部 ID。
- 若没有吃子，输出空数组 `[]`。

### 8.4 分局日志文件

- 每次 PIE 启动或游戏启动都会生成新的时间戳文件。
- 两局日志不会混在同一个 `.jsonl` 文件里。

### 8.5 UE_LOG

- 每次完整行动结束时，`Output Log` 中额外出现一行 JSON 字符串。

---

## 9. 暂不处理

- 历史日志读回。
- 按玩家拆分文件。
- 日志轮转、大小上限。
- 更详细字段（兵种、地形、是否终局、胜者等）。
- 失败恢复与异步写盘。
