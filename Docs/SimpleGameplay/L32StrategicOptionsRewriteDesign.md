# L3.2 战略候选重写设计稿

日期：2026-07-14  
状态：已实现并完成独立 Runtime 首轮验收。

## 背景

L3 v1 的 `terra.strategy.describe_strategic_options` 存在结构性问题：

```text
1. 固定 priority 让 LLM 把启发式数值误读为战略命令。
2. 只保留第一个 frontline contact、第一处 contested terrain、第一名 exposed unit。
3. contested terrain 只使用普通 BFS 距离，不检查兵种能否进入/穿越地形。
4. 单个地形格无法表达 58 -> 222 -> 17 这类可利用的连续森林走廊。
5. key_cell_id 是证据位置，却容易被模型当成应立即占据的落点。
```

L3.2 将工具重新定义为“战略调查候选枚举器”。它不推荐行动、不评估胜率、不替 LLM 排序；它只列出当前回合值得调查的结构性事实，并要求 LLM 通过 L3.1、L2 和 UI 工具验证。

## 目标与非目标

```text
目标：
  枚举全部当前语义候选，而不是每类只留一个。
  表达前线接触、争议地形、连续地形走廊、暴露单位等结构。
  将 key piece/cell 与 route 明确区分。
  对每个候选标注证据范围和必须进一步验证的事项。

非目标：
  不输出 priority、score、rank、recommended_action 或“最佳”字样。
  不证明本回合行动合法，也不承诺 route 可由该棋子本回合走完。
  不替代 ui_select_piece / ui_preview_move 的 Gameplay validator。
```

## MCP 契约

工具名维持不变：

```text
terra.strategy.describe_strategic_options
```

输入仍为 `{}`，始终注册、只读、幂等。输出：

```json
{
  "ok": true,
  "snapshot": { "turn_index": 0, "current_faction_id": 0, "interaction_phase": "Idle" },
  "strategic_options": [
    {
      "intent": "investigate_terrain_corridor",
      "evidence_scope": "local_graph_and_piece_type",
      "evidence_tags": ["forest_corridor", "frontline_direction", "piece_type_cavalry"],
      "key_piece_ids": [9],
      "key_cell_ids": [17],
      "route_cell_ids": [58, 222, 17],
      "terrain_tags": ["forest"],
      "target_enemy_faction_id": 5,
      "validation_questions": [
        "Can piece 9 legally use the route this turn or over future turns?",
        "Does reaching the endpoint improve a concrete frontline or terrain-control plan?"
      ]
    }
  ]
}
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `intent` | 调查主题，不是行动命令。 |
| `evidence_scope` | 该候选使用的确定性证据边界。`graph_distance_only` 不可当作兵种可达性；`local_graph_and_piece_type` 仍须 UI 验证。 |
| `evidence_tags` | 可追溯的事实标签，不含分数。 |
| `key_piece_ids` | 需要优先观察的我方棋子；可为空。 |
| `key_cell_ids` | 关键目标/接触 Cell；不是推荐落点。 |
| `route_cell_ids` | 图上的结构性路径。起点可能是 key piece 当前 Cell；不是合法 action path。 |
| `terrain_tags` | 候选涉及的地形类型。 |
| `target_enemy_faction_id` | 对应敌对阵营；无明确敌方时为 `null`。 |
| `validation_questions` | LLM 必须用后续工具回答的问题，防止把候选误当结论。 |

`priority` 从 typed struct、JSON 和 Agent 提示词中完全移除。

## 候选枚举规则

L3.2 对同一 snapshot 枚举以下候选，不在候选间进行分数排序。

### 1. `investigate_frontline_contact`

对每个 distinct `FFrontlineContact` 生成一个候选：

```text
key_piece_ids = [friendly_piece_id]
key_cell_ids = [friendly_cell_id, enemy_cell_id]
target_enemy_faction_id = enemy_faction_id
evidence_tags = nearest_enemy_distance_N, friendly_support_N, friendly_terrain_<tag>
evidence_scope = graph_distance_only
```

它回答“这对敌我接触值得查看”，不等价于“该棋子应直线前进”。

### 2. `investigate_contested_terrain`

对**每一个** `control_status == contested` 的 forest/mountain control point 生成候选：

```text
key_cell_ids = [terrain_cell_id]
terrain_tags = [forest | mountain]
evidence_tags = contested_terrain, nearest_friendly_distance_N, nearest_enemy_distance_N
evidence_scope = graph_distance_only
```

该命名刻意使用 `investigate_`：双方普通 BFS 距离相等，只说明该地形值得研究，不能推出某个兵种应抢占。

候选从完整 terrain point 集合生成，而不是从 `find_terrain_control_points` 用于展示的截断列表生成。

### 3. `investigate_terrain_corridor`

对每个当前可动我方棋子，枚举长度为两条边的 simple graph route：

```text
[piece current cell, intermediate cell, endpoint cell]
```

仅当三格均为相同的 `forest` 或均为相同的 `mountain` 时生成候选。候选表达“连续同地形结构”，不表达已验证移动：

```text
key_piece_ids = [piece_id]
key_cell_ids = [endpoint_cell_id]
route_cell_ids = [start, intermediate, endpoint]
terrain_tags = [forest | mountain]
evidence_scope = local_graph_and_piece_type
```

额外事实：

```text
piece_type_<type>
cavalry_mountain_route_blocked（仅提示静态兵种限制）
nearest_enemy_distance_from_N / nearest_enemy_distance_to_N（仅当两个距离可计算）
```

如果 endpoint 最近敌方来自唯一 faction，则填 `target_enemy_faction_id`；否则保持 `null`。通过 stable route tuple 去重。

该规则直接覆盖 `58 -> 222 -> 17` 一类森林走廊。它不会声称骑兵一定能走完全程；LLM 必须选择棋子并预览。

### 4. `investigate_exposed_unit`

对每个当前 isolated movable unit 生成候选：

```text
key_piece_ids = [piece_id]
key_cell_ids = [piece_cell_id]
evidence_tags = isolated_movable_piece, enemy_distance_N
evidence_scope = graph_distance_only
```

### 5. `observe_and_develop`

只有在上述候选均为空时才返回，表示当前工具未发现已编码的结构；不是消极行动建议。

## 排序与容量

候选不含优先级，仍需稳定排序以支持测试和 Agent 回放：

```text
intent
-> target_enemy_faction_id (null last)
-> key_piece_ids lexicographic
-> key_cell_ids lexicographic
-> route_cell_ids lexicographic
```

不按分数截断战略候选。若日后实测单回合 token 过高，应增加显式分页/按 intent 查询，而不是暗中丢弃“编号靠后”的候选。

## Agent 使用规则

Agent 恢复暴露 `describe_strategic_options`，但提示词必须更新：

```text
不得按不存在的 priority 选择候选。
先比较 candidate 的 evidence scope、兵种类型、route、目标敌方和 validation questions。
candidate 是“下一步值得调查什么”，不是“确认哪个行动”。
对 graph_distance_only 候选，至少再查 L3.1 或 L2/UI 后才可确认。
对 terrain_corridor，优先使用 inspect_local_topology 检查局部边和 terrain，再用 ui_select_piece / ui_preview_move 验证真实合法移动。
```

每次 Agent 日志仍记录 tool purpose 与 LLM takeaway，用于判断模型选择/忽略候选的原因。

## 实现位置

```text
Source/Gameplay/Public/TerraGameplayContainer.h
Source/Gameplay/Private/TerraGameplayContainer.cpp
Source/NpcMcp/Private/TerraNpcMcpGameplayStrategicTools.cpp
Tools/NpcAgent/src/run-llm-interactive-think.js
Tools/NpcAgent/src/run-llm-strategy-topology-interactive-think.js
```

## 验收标准

```text
1. JSON 不再包含 priority。
2. 每个 FrontlineContact、每个 contested terrain、每个 isolated movable piece 都有相应候选。
3. 58 -> 222 -> 17 这类连续 forest route 在满足规则时返回 terrain_corridor 候选。
4. candidate 稳定排序、无重复、每项都可追溯至真实 piece/cell/graph 事实。
5. candidate 不改变 Gameplay state，且 route 不被误标记为合法 action。
6. 独立 Game 中 Agent 能看到重写后的工具，LLM 的日志不再引用 priority。
7. LLM 选择候选后必须进行拓扑或 UI 验证；验收报告明确说明它是否真正利用了候选而非仅复述字段。
```

## 实施与首轮验收

已实现：

```text
FStrategicOption：
  移除 Priority。
  新增 EvidenceScope、RouteCellIds、TerrainTags、ValidationQuestions。

BuildCurrentFactionStrategicSnapshot：
  对每个 frontline contact 生成 investigate_frontline_contact。
  使用完整 terrain point 集合，对每个 contested point 生成 investigate_contested_terrain。
  对所有当前可动棋子枚举同 forest / mountain 的两边 simple route，生成 investigate_terrain_corridor。
  对每个 isolated movable piece 生成 investigate_exposed_unit。
  仅无候选时生成 observe_and_develop。

MCP / Agent：
  JSON 不再序列化 priority。
  Agent 恢复暴露 describe_strategic_options，提示词要求把 candidate 视为无排序调查线索。
```

独立 Staged Game 首轮验收：

```text
describe_strategic_options 返回多个 frontline / contested-terrain 候选。
日志中 priority_fields=0。
LLM 未再追逐 cell 12；它选择 faction 1 的 frontline contact，
select piece 10 -> preview 196 -> confirm，落点为无威胁 forest，回合正常推进。
```

该结果说明 L3.2 消除了旧版“固定分值 + 第一项”的误导，但不应被解读为 LLM 已经能完整比较候选：它在本轮只调用了一次 options 后便调查并确认一个 frontline candidate，没有主动展开第二条候选、没有调用 topology，也没有显式比较多个 `validation_questions`。

首轮 faction 0 输出中没有出现 `58 -> 222 -> 17` 的 corridor；该链路需要在对应阵营/局势下单独断言 terrain 与 graph edge 后验证。L3.2 的 corridor 规则和 JSON 契约已经落地，但“该特定链路被枚举”的验收不能以不包含该链路的 faction 0 回合替代。

复验发现：完整 L3.2 options 原始 JSON 约为 25,498 字符，而 Agent 原有 12,000 字符 history 截断会丢失候选尾部。Agent 已对 `strategic_options` 专门提升到 32,000 字符，保证该局面完整候选集进入下一轮 LLM 上下文。

在完整上下文的干净 faction 0 回合中，LLM 的实际序列为：

```text
begin_turn_review
-> describe_strategic_options
-> inspect_local_topology(cell 12)
-> find_enemy_pressure
-> select cavalry 6
-> preview 6 -> 167
-> confirm
```

关键行为变化：LLM 先检查了 `investigate_contested_terrain` 的 cell 12，随后基于局部图明确写出“本回合没有友军可到达该格”，因此转去调查 enemy pressure，而非像 L3 v1 一样把 cell 12 当作必须抢占的目标。最后的 `6 -> 167` 仍是一次安全前探，不能说明 L3.2 已解决多回合协同；但它已验证“候选无优先级 + 验证问题 + 完整上下文”能够让 LLM 调查并否定不适合的 terrain candidate。
