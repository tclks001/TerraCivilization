# L3 战略语义工具设计稿

日期：2026-07-13  
状态：已完成并独立 Runtime 验收。

## 目标

L3 建立在 [L1 Interactive Tactical Review Tools](L1InteractiveTacticalReviewToolsDesign.md) 与 [L2 局部战术情报卡](L2LocalTacticalSituationCardDesign.md) 之上，为 LLM 提供本回合的阵营级、前线级和地形控制级事实摘要。

它解决的不是“某个棋子此刻能走哪里”，而是：

```text
我的阵营目前强在哪里、弱在哪里。
最接近、最有压力的敌对阵营是谁。
我方与敌方实际接触的前线在哪些棋子和地块附近。
森林、山地等地形点对当前双方距离和兵种分布意味着什么。
本回合值得优先验证哪些战略意图。
```

L3 不代替 LLM 选最终行动，也不做固定棋谱。它把全图原始数据压缩为有明确计算口径、可追溯到 piece/cell id 的语义摘要，使 LLM 可以先形成战略意图，再通过 L1/L2 验证具体棋子和落点。

## 基本原则

```text
L3 tool 是只读、无副作用、空参数的查询。
同一 Gameplay snapshot 内重复调用必须返回相同语义结果和稳定排序。
L3 tool 始终注册，不受 Gameplay interaction phase 的动态 ui tools/list 影响。
L3 不修改 selected piece、highlights、camera、turn、action log 或 agent memory。
所有 distance 是球面 cell graph 的 BFS 距离，不是世界空间距离。
所有 priority 是透明、确定性的排序辅助，不是“胜率”或 LLM 不可解释的黑箱评分。
```

## 工具集

| 工具 | 关注问题 | 主要输出 |
| --- | --- | --- |
| `terra.strategy.summarize_faction_state` | 我方整体资源与态势如何？ | 兵种/存活数、可行动数、最近敌距、前线单位数、地形接触摘要。 |
| `terra.strategy.describe_frontline` | 与谁、在哪里接触？ | 最近敌对 faction、己方前线 piece、对应敌方 piece、接触 cell、局部地形与支援。 |
| `terra.strategy.find_terrain_control_points` | 哪些森林/山地值得关注？ | 候选 terrain cell、双方最近距离、控制状态、附近兵种、争夺标签。 |
| `terra.strategy.find_enemy_pressure` | 哪个阵营对我方压力最大？ | 按 enemy faction 聚合的接近距离、近前线单位数、边界压力与代表敌方 piece。 |
| `terra.strategy.describe_strategic_options` | 当前应优先验证什么？ | 透明的 intent 候选、priority、证据字段和 key piece/cell。 |

所有工具输入均为 `{}`，输出均包含同一 `snapshot`：

```json
{
  "snapshot": {
    "turn_index": 10,
    "current_faction_id": 5,
    "interaction_phase": "Idle"
  }
}
```

若 Gameplay 不可用或比赛结束，返回稳定错误，如 `gameplay_unavailable`、`match_ended`，不返回部分猜测数据。

## 数据口径

### 阵营兵力与可行动性

`summary_faction_state` 统计当前 faction 的存活棋子，按 `commander`、`archer`、`cavalry`、`infantry` 分组；`movable_piece_count` 排除 commander、死亡和不能移动的棋子。它还返回当前可行动棋子的最小敌距分布：

```json
{
  "material": {
    "total_alive_piece_count": 15,
    "commander_count": 1,
    "archer_count": 5,
    "cavalry_count": 5,
    "infantry_count": 4,
    "movable_piece_count": 14
  },
  "proximity": {
    "nearest_enemy_distance": 4,
    "frontline_piece_count": 5,
    "isolated_movable_piece_count": 1
  }
}
```

`frontline` 的 v1 定义为“当前 movable piece 中，敌方最近距离等于我方最小敌距或最小敌距 + 1 的棋子”。它是稳定的优先观察集合，不声称是唯一军事前线。

### 前线与敌方压力

`describe_frontline` 对每个己方前线 piece 找到最接近的敌方 piece，按 `distance`、己方 piece id、敌方 piece id 稳定排序。它返回单元格、双方兵种、落点 terrain、相邻友军和敌军数量。

`find_enemy_pressure` 以 enemy faction 聚合：

```text
nearest_contact_distance：我方任一 movable piece 与该敌方任一存活 piece 的最小 BFS 距离。
frontline_contact_count：该敌方在我方 frontline 距离阈值内可形成的 distinct closest-pair 数。
pressure_priority：100 - 15 * nearest_contact_distance + 4 * frontline_contact_count。
```

priority 仅用于排序；Agent 和 LLM 必须查看距离与 pair 数，不可将其当作胜率。

### 地形控制点

L3 v1 只考察 `forest` 与 `mountain`，因为二者已在现行 Gameplay 规则中分别影响弓兵远程攻击和骑兵通行/弓兵射程。

每个 control point 的双方距离使用：

```text
nearest_friendly_distance：当前 faction 任一 movable piece 到该 cell 的最小 BFS 距离。
nearest_enemy_distance：任一非当前 faction 存活 piece 到该 cell 的最小 BFS 距离。
control_status：friendly_closer / enemy_closer / contested / unclaimed。
```

候选按以下稳定规则截断至 12 个：先 `contested`，再较小双方最小距离，再地形优先级（forest、mountain），再 cell id。不会为了“战略感”返回远离战线的全图 terrain 列表。

### 战略选项（已由 L3.2 替换）

本节描述的是 L3 v1 的历史实现，已不应作为 Agent 行为依据。详细重写方案见 [L3.2 战略候选重写](L32StrategicOptionsRewriteDesign.md)。

`describe_strategic_options` 从上述事实生成有限的、可追溯的候选意图，而不是给具体 action：

| intent | 触发事实 | key data |
| --- | --- | --- |
| `advance_contact` | 存在最近接触 pair，且我方前线 unit 可行动 | key frontline piece / enemy faction / contact cell。 |
| `contest_terrain` | 存在 `contested` forest 或 mountain control point | key terrain cell / terrain type / 双方距离。 |
| `preserve_exposed_unit` | 存在邻近友军不足 1 且距离敌人不大于前线阈值的 movable piece | key piece / cell / enemy distance。 |
| `reinforce_frontline` | 最近前线 piece 友军支援不足而后方存在更近友军群 | key frontline piece / nearby support piece。 |

L3.2 移除 `priority`，并枚举全部候选。旧字段与旧命名仅用于理解此前验收日志，不应新增依赖。

## MCP 返回示例

### `terra.strategy.find_enemy_pressure`

```json
{
  "ok": true,
  "snapshot": {
    "turn_index": 1,
    "current_faction_id": 1,
    "interaction_phase": "Idle"
  },
  "enemy_pressures": [
    {
      "enemy_faction_id": 0,
      "nearest_contact_distance": 3,
      "frontline_contact_count": 2,
      "pressure_priority": 63,
      "representative_enemy_piece_ids": [11],
      "representative_friendly_piece_ids": [22]
    }
  ]
}
```

### `terra.strategy.describe_strategic_options`

```json
{
  "ok": true,
  "strategic_options": [
    {
      "intent": "advance_contact",
      "evidence_tags": ["nearest_enemy_distance_3", "movable_frontline_piece"],
      "key_piece_ids": [22],
      "key_cell_ids": [204],
      "target_enemy_faction_id": 0
    }
  ]
}
```

## 实现边界

```text
Gameplay：
  新增 typed strategic snapshot/query structs 与 BFS、聚合、稳定排序。
  不包含 JSON、MCP 类型和 LLM 提示词。

NpcMcp：
  新增 TerraNpcMcpGameplayStrategicTools.cpp。
  把 typed result 序列化为 JSON，统一附 snapshot。
  将五个工具加入 AlwaysOnToolNames；不得放进 RefreshInteractiveToolAvailability_。

Agent：
  tools/list 后识别 terra.strategy.*。
  对当前 snapshot 并行发起五个 tools/call。
  等待 Promise.all 结束，将结果作为本回合 LLM 的战略上下文。
  LLM 再顺序调用当前 phase 可见 ui tools；每次 UI 操作后都以 interaction_state 为准。
```

推荐 Gameplay API：

```cpp
struct FStrategicSnapshot;
struct FFactionStrategicSummary;
struct FFrontlineSummary;
struct FTerrainControlPoint;
struct FEnemyPressureSummary;
struct FStrategicOption;

bool BuildCurrentFactionStrategicSnapshot(FStrategicSnapshot& OutSnapshot) const;
```

单一 snapshot API 必须一次构建并复用中间距离表，防止五个并行 MCP 调用各自重复深搜且产生不一致口径。并行安全依赖于 Gameplay 在 game thread 上提供同一只读快照；若 MCP tool 线程模型不能保证同时读取 Container 安全，bridge 需要先在 game thread 捕获 immutable snapshot，再由各 tool 序列化自己的视图。

## 并行 Agent 工作流

L3 工具彼此没有参数依赖，也不写状态，因此每回合开始可由 Agent 并行预取：

```text
tools/list
  -> Promise.all([
       strategy.summarize_faction_state,
       strategy.describe_frontline,
       strategy.find_terrain_control_points,
       strategy.find_enemy_pressure,
       strategy.describe_strategic_options
     ])
  -> 收集同 snapshot 的五份结果
  -> 一次 LLM 请求，要求选择 turn_goal / tactical_focus
  -> 顺序调用当前 phase 的 ui tools
  -> preview / confirm
```

Agent 必须检查五个返回的 `turn_index/current_faction_id/interaction_phase` 相同。若任何 snapshot 不一致、工具失败或 phase 已不为 Idle，丢弃整组结果并重新 `tools/list` 后重试，而不是把不同回合的信息混给 LLM。

推荐实验脚本：`run-llm-strategy-interactive-think.js`。它复用现有 MCP HTTP client 和结构化 LLM 输出，并可把 strategy 工具与当前 phase 可见 UI 工具同时交给 LLM；后续 UI 调用不并行。

## 验收标准

```text
1. 五个 terra.strategy.* 工具在 Idle、PieceSelected、PieceMovedCanEndTurn、PieceJumpingCanContinue 均可见。
2. 连续同 snapshot 调用返回同一排序和相同 snapshot。
3. 调用任意战略工具不改变 Gameplay interaction_state、highlights、selected piece、turn 或 action log。
4. 每个返回 item 都能追溯到真实 faction/piece/cell id；无隐藏随机性。
5. Agent 使用 Promise.all 并检查 snapshot 一致性后才向 LLM 发第一轮请求。
6. LLM 日志能表述战略意图，并以 L3 evidence 解释为何优先观察某类棋子。
7. L1/L2 Inspector 与 deterministic review Agent 保持通过。
```

## 后续

## 实施与验收结项

L3 已在 Runtime MCP 中实现并完成独立 Game 验收：

```text
Gameplay：
  FCurrentFactionStrategicSnapshot、阵营/前线/地形控制/敌方压力/透明战略选项。

NpcMcp：
  TerraNpcMcpGameplayStrategicTools.cpp。
  五个 terra.strategy.* 工具均在 AlwaysOnToolNames 中注册。

Agent：
  run-llm-strategy-interactive-think.js 可同时暴露当前 ui_* 与五个 strategy 工具。
  LLM 已实际调用 describe_frontline / describe_strategic_options，并以返回的 intent 选择观察棋子。
```

独立 Staged Game 验收中，LLM 能完成 `begin_turn_review -> strategy query -> select -> preview -> confirm`，并由 Gameplay 正常推进回合。战略工具不改变 interaction phase，UI 工具仍只由 Gameplay 状态机控制。

本轮验收确认了 L3 v1 的边界：旧战略 `contest_terrain` 只说明值得关注的地形，不证明某一兵种可到达或可穿越它。L3.2 已以 `investigate_contested_terrain` 取代该命名，并要求 L2 局部卡和 L3.1 局部拓扑验证路径、阻断与协同。

## 后续

[L3.1 局部拓扑观察工具](L31LocalTopologyObservationDesign.md) 紧接 L3，提供以任意 Cell 为中心、半径固定为 3 的真实局部子图。它让 LLM 在决定查看某一战略 control point、棋子或落点时，获得与玩家观察棋盘相近的地形、兵种和图边信息。

L4 才应在 L2 卡、L3 intent 与 L3.1 局部子图的共同约束下，做可验证的兵种协同、屏障、远程射线和多跳链候选枚举。这样“战略为什么”与“战术如何做到”保持分层，而不会把大量棋谱塞进提示词。
