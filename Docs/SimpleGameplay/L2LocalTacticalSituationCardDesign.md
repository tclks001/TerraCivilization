# L2 局部战术情报卡设计稿

日期：2026-07-13  
状态：C++ 已落地并完成 Runtime MCP 确定性验收；LLM 回归测试待配置 API key。

## 目标

L2 建立在已验收的 [L1 Interactive Tactical Review Tools 最小闭环设计稿](L1InteractiveTacticalReviewToolsDesign.md) 上。它不新增一套行动方式，也不把固定棋谱写进 LLM 提示词；它只为已有的 `terra.ui_*` 工具补充一张由 Gameplay 确定性计算的“局部战术情报卡”。

L2 要解决的具体问题是：当前 LLM 已经知道某个落点是否能吃子、是否危险、是否靠近敌人，但仍不知道该落点之后的局部拓扑是否通畅、骑兵是否被山阻断、友军能否借此形成跳跃链或屏障、敌方能否借该结构反制。因此它容易做出“安全但没有后续”的单步推进。

L2 的目标是让 LLM 能基于工具返回的事实，识别一类通用的兵种、地形和拓扑协同，而非只学习某一个开局套路：

```text
弓兵进入森林后的掩护与射线价值。
骑兵前方被山地封锁而失去后续机动。
友军棋子能否在后续作为合法跳跃锚点或屏障。
一步移动是否把己方结构推向敌人，或远离敌人。
一步移动是否加强、削弱或避开敌方的可验证攻击结构。
```

L2 不输出“最佳行动”或最终战略分数。LLM 仍负责在多个已观察的情报卡之间比较、形成意图、决定继续观察、取消或确认。

## 设计边界

### L2 做什么

```text
为一个具体合法 action 的 from_cell / to_cell 生成事实型局部态势卡。
使用同一套 Gameplay 移动、跳跃、地形与吃子规则进行假设棋盘查询。
把情报加入现有 ui_begin_turn_review、ui_select_piece、ui_preview_move 返回值。
以稳定字段、明确半径和明确模拟前提，让 Agent 与 LLM 可复盘。
```

### L2 不做什么

```text
不新增 MCP action tool。
不改变 ETerraGameplayInteractionPhase 或 tools/list 的 phase 动态暴露。
不直接移动棋子，不绕过 L1 的真实点击路径。
不把“弓兵进森林 + 骑兵屏障”等固定套路硬编码为唯一策略。
不替 LLM 选最佳 action，不在 L2 产出全局战略评分。
不假装知道敌人下一回合的真实行动；只报告假设棋盘上的规则可达性与敌方回应窗口。
```

## 与 L1、后续阶段的关系

```text
L1：真实点击状态机、动态 ui tools、基础局部信息、Inspector/Agent 验收。
L2：针对每个候选落点补齐局部拓扑、机动、友军协同与敌方互动事实。
L3+：在 L2 情报卡上构建战略语义、两回合候选计划、阵营记忆与 active plan。
```

L2 不维护平行 Agent phase。每次 `ui_` 调用仍以 Gameplay 返回的 `interaction_state` 为唯一权威，Agent 只缓存该快照。

## 本次实现范围（L2 v1）

本次已在 `FTerraGameplayContainer::BuildLocalTacticalSituationCard` 中实现并通过 MCP 返回：

```text
行动前后 terrain、相邻敌我、最近敌人的 BFS 图距离和 approach_to_enemy。
以 from -> to 最短图路径前驱格定义的 approach_cell_ids 与 forward neighbors。
落点 ring_1 / ring_2 地形和敌我数量摘要。
普通移动、首跳、多跳可达终点的 before/after 机动计数。
骑兵前方山地阻断、前方可进入空格数。
落点友军支援变化，以及 moved piece 是否实际新增友军跳跃目标。
已有 Gameplay 风险查询提供的敌方回应威胁和弓兵威胁者 ID。
aggressive_safe_probe、cavalry_forward_mountain_blocked、archer_forest_outpost、jump_chain_anchor 等可回溯标签。
```

本次暂未把设计中的 `friendly_followup_candidates`、`moved_piece_can_screen_for_*`、局部 group 敌方吃子总数和 `breaks_enemy_attack_path_for_piece_ids` 做成字段。这些需要进一步定义“下一回合友军可行动”和“攻击路径归因”的稳定口径，应作为 L2 v1.1 扩展，不能以邻接猜测替代。

## 核心模型：行动前后局部态势卡

一张卡只描述一个已验证 action：`piece_id: from_cell_id -> to_cell_id`。所有涉及“移动后”的字段都在以下假设棋盘上计算：

```text
当前权威棋盘
  + 当前 action 的移动结果
  + 当前 action 已确定的 capture entries
  - 被该 action 捕获的棋子
```

对于跳跃连续行动，卡描述的是“当前这一跳后的棋盘”，不是假设 LLM 一定会继续跳或一定会确认。`ui_preview_move` 返回的卡必须与当前真实中间 phase 对应。

建议统一对象名为 `local_tactical_card`，并显式带版本，后续扩字段不改变已有 UI tool 顶层契约：

```json
{
  "schema_version": 1,
  "piece_id": 12,
  "piece_type": "archer",
  "from_cell_id": 101,
  "to_cell_id": 118,
  "is_jump": true,
  "capture_count": 0,
  "from": {},
  "to": {},
  "topology": {},
  "mobility": {},
  "friendly_synergy": {},
  "enemy_interaction": {},
  "summary_tags": []
}
```

`summary_tags` 是对已返回事实的稳定短标签，不替代原始字段。LLM 必须能回看字段证据；Agent 不能只按 tag 自动下结论。

## 字段契约

### 1. `from` 与 `to`：落点前后基本事实

`from` 描述当前棋子尚未行动时的位置，`to` 描述假设棋盘上的落点。

```json
{
  "from": {
    "terrain_tags": ["plain"],
    "nearby_friendly_piece_ids": [7],
    "nearby_enemy_piece_ids": [],
    "nearest_enemy_distance": 5
  },
  "to": {
    "terrain_tags": ["forest"],
    "nearby_friendly_piece_ids": [7],
    "nearby_enemy_piece_ids": [34],
    "nearest_enemy_distance": 3,
    "destination_threatened": false,
    "threat_count": 0,
    "threatening_piece_ids": []
  },
  "approach_to_enemy": 2
}
```

字段含义：

| 字段 | 规则 |
| --- | --- |
| `nearest_enemy_distance` | 图上 BFS 圈层距离，不是世界空间距离；没有敌人时为 `null`。 |
| `approach_to_enemy` | `from.nearest_enemy_distance - to.nearest_enemy_distance`；正值为接近，负值为撤退，任一侧无敌人时为 `null`。 |
| `destination_threatened` / `threat_count` | 使用 Gameplay 的假设 action 风险规则，不能由邻接数量代替。 |
| `terrain_tags` | 当前只允许真实 terrain 名称，例如 `plain`、`forest`、`mountain`；不能在这里伪造战略解释。 |

L1 已存在的顶层 `terrain_tags`、`nearby_*`、`destination_threatened`、`nearest_enemy_distance_*`、`approach_to_enemy` 保持兼容。L2 在 `local_tactical_card` 中保留结构化版本，顶层字段继续作为易读快捷字段，不删除、不改义。

### 2. `topology`：以 action 方向定义的局部拓扑

球面图没有一个天然、稳定的“左/右/前/后”。L2 不向 LLM 输出人为的屏幕方向，而以当前 action 的有向路径 `from -> to` 定义“向前”：

```text
approach cell：to 的邻居中、位于从 from 到 to 的最短图路径上的前驱格；普通移动时通常就是 from，跳跃时通常是被跨越的中间格。
forward neighbor：to 的邻居，排除全部 approach cell。
local ring 1 / 2：以 to 为中心的 BFS 图距离 1 / 2。
```

```json
{
  "topology": {
    "to_cell_neighbor_count": 6,
    "approach_cell_ids": [117],
    "forward_neighbor_cell_ids": [119, 120, 121, 122, 123],
    "forward_empty_cell_count": 3,
    "forward_friendly_piece_ids": [7],
    "forward_enemy_piece_ids": [34],
    "ring_1": {
      "cell_count": 6,
      "plain_cell_count": 2,
      "forest_cell_count": 2,
      "mountain_cell_count": 1,
      "friendly_piece_count": 1,
      "enemy_piece_count": 1
    },
    "ring_2": {
      "cell_count": 12,
      "plain_cell_count": 7,
      "forest_cell_count": 3,
      "mountain_cell_count": 2,
      "friendly_piece_count": 2,
      "enemy_piece_count": 2
    }
  }
}
```

`ring_1`、`ring_2` 是数量摘要而非完整棋盘转储。完整 ID 列表只保留在方向明确、对该 action 有直接解释力的 `forward_*` 字段；这样既给模型足够结构信息，也避免把每个候选 action 扩成全图 JSON。

### 3. `mobility`：当前兵种在落点后的后续机动

这是 L2 判断“骑兵走到山前是否没有意义”的关键。它必须通过实际 Gameplay 的 movement rule 计算，不能用“前方山格数量”猜测。

```json
{
  "mobility": {
    "piece_rule_profile": "cavalry",
    "ordinary_target_count_after_move": 2,
    "jump_target_count_after_move": 1,
    "reachable_endpoint_count_after_move": 4,
    "forward_enterable_cell_count": 1,
    "forward_blocked_by_mountain_cell_ids": [121, 122],
    "mobility_delta_reachable_endpoints": -3,
    "can_continue_jump_now": false,
    "continue_jump_target_cell_ids": []
  }
}
```

规则：

| 字段 | 计算口径 |
| --- | --- |
| `ordinary_target_count_after_move` | 假设棋盘上，该兵种下一个可行动回合的合法普通移动数。 |
| `jump_target_count_after_move` | 假设棋盘上，合法首跳目标数。 |
| `reachable_endpoint_count_after_move` | 递归多跳去重后的可达终点数，加普通移动终点；只描述静态棋盘机动潜力。 |
| `forward_enterable_cell_count` | `forward_neighbor` 中该兵种依据真实地形规则可进入的空格数量。 |
| `forward_blocked_by_mountain_cell_ids` | 仅对不能进入或跨越山地的兵种列出阻断该方向机动的山格；其他兵种为空数组。 |
| `mobility_delta_reachable_endpoints` | `after_move - before_move`，两侧均在同一套递归口径下计算。 |
| `can_continue_jump_now` | 仅 `ui_preview_move` 的真实当前 phase 字段，不能被静态“下回合可跳”字段替代。 |

第一版不输出 `is_dead_end_for_cavalry` 这类单独布尔结论。模型可以从“骑兵 + forward enterable=0 + 跳跃端点下降”得出原因；若后续日志证明这一模式反复被误解，再把它添加为有严格定义的派生 tag。

### 4. `friendly_synergy`：友军支援、屏障与跳跃锚点

L2 只报告“在假设棋盘上可被规则验证”的协同机会，不承诺未来回合一定能发生，也不假定敌人会静止。

```json
{
  "friendly_synergy": {
    "adjacent_friendly_piece_ids_after_move": [7],
    "adjacent_friendly_piece_type_counts": {
      "archer": 0,
      "cavalry": 1,
      "infantry": 0,
      "commander": 0
    },
    "support_delta": 1,
    "friendly_followup_candidates": [
      {
        "piece_id": 7,
        "piece_type": "cavalry",
        "relation_tags": ["can_reach_adjacent_screen_cell", "can_use_moved_piece_as_jump_anchor"],
        "support_cell_ids": [117]
      }
    ],
    "moved_piece_can_be_jump_anchor_for_friendly_piece_ids": [7],
    "moved_piece_can_screen_for_friendly_piece_ids": [12]
  }
}
```

严格定义：

| 字段/标签 | 成立条件 |
| --- | --- |
| `support_delta` | `to` 的相邻友军数减去 `from` 的相邻友军数；它只是局部支援变化，不等于安全评分。 |
| `can_reach_adjacent_screen_cell` | 在假设棋盘、忽略敌方未来行动的条件下，该友军下一个自身回合存在至少一个合法终点，且该终点与 moved piece 相邻。 |
| `can_use_moved_piece_as_jump_anchor` | 用实际跳跃规则模拟该友军时，moved piece 的落点作为中继占位，使其存在至少一个合法跳跃目标；不能只按“两个棋子相邻”判定。 |
| `moved_piece_can_screen_for_*` | moved piece 落点位于该友军与至少一个已识别敌方威胁/攻击入口之间的、规则定义的邻接屏障候选位置。第一版应保守，只在拥有明确攻击路径规则时返回；否则为空数组。 |

`friendly_followup_candidates` 第一版设置固定预算，例如最多 6 个友军、每个最多 4 个 support cell，按“与落点图距离、可达性、兵种多样性”稳定排序。超过预算要返回 `truncated: true`，不能静默丢失。

### 5. `enemy_interaction`：敌方压力与结构影响

敌方信息同样必须来自假设棋盘上的合法规则查询，而非 LLM 由相邻敌人自行推断。

```json
{
  "enemy_interaction": {
    "enemy_piece_ids_in_ring_2": [34, 39],
    "enemy_response_threatens_moved_piece": false,
    "enemy_response_threatening_piece_ids": [],
    "enemy_response_capture_count_against_local_group": 0,
    "enemy_archer_line_threatening_piece_ids": [],
    "enemy_structure_delta": {
      "adjacent_enemy_count_delta": 1,
      "nearest_enemy_distance_delta": -2
    },
    "breaks_enemy_attack_path_for_piece_ids": []
  }
}
```

字段定义：

| 字段 | 计算口径 |
| --- | --- |
| `enemy_response_threatens_moved_piece` | 假设棋盘上，任一敌方阵营在自己的下一回合存在可合法形成对 moved piece capture 的 action；它是威胁事实，不代表敌方必然选择该 action。 |
| `enemy_response_capture_count_against_local_group` | 以 moved piece、其相邻友军为 local group，统计敌方可合法捕获 group 内棋子的不同 action 数；设固定上限并报告截断。 |
| `enemy_archer_line_threatening_piece_ids` | 上述威胁中，由弓兵远程吃子规则形成的攻击者 ID；必须复用森林、山地等实际规则。 |
| `breaks_enemy_attack_path_for_piece_ids` | 对比行动前后：某敌方棋子原本可合法攻击 local group 中某棋子，行动后该合法攻击消失，且能定位为结构/占位改变造成。第一版只在可稳定归因时返回。 |

L2 不以 `breaks_enemy_archer_line` 作为无证据 tag。先返回可复盘的 ID、规则结果和 before/after 计数；只有事实字段稳定后才增加简短标签。

### 6. `summary_tags`：可解释的组合标签

建议第一版只定义以下严格标签，并保持标签与字段一一可追溯：

| 标签 | 触发条件 |
| --- | --- |
| `aggressive_safe_probe` | `approach_to_enemy > 0`、`destination_threatened=false`，且 `mobility_delta_reachable_endpoints >= 0`。 |
| `cavalry_forward_mountain_blocked` | `piece_type=cavalry` 且 `forward_blocked_by_mountain_cell_ids` 非空。 |
| `cavalry_mobility_drop` | `piece_type=cavalry` 且 `mobility_delta_reachable_endpoints < 0`。 |
| `archer_forest_outpost` | `piece_type=archer`、`to.terrain_tags` 包含 `forest`、`destination_threatened=false`。 |
| `jump_chain_anchor` | `moved_piece_can_be_jump_anchor_for_friendly_piece_ids` 非空。 |
| `supported_advance` | `approach_to_enemy > 0` 且 `support_delta > 0`。 |
| `overextended_without_support` | `approach_to_enemy > 0`、`support_delta < 0`，且 `destination_threatened=true` 或 `enemy_response_threatens_moved_piece=true`。 |
| `breaks_enemy_attack_path` | `breaks_enemy_attack_path_for_piece_ids` 非空。 |

标签不包含“最好”“必胜”“应该走”等价值判断。标签的作用是压缩重复事实，便于 LLM 在日志中发现值得继续观察的候选，而不是跳过字段证据直接执行。

## ui 工具返回扩展

L2 不新增工具，仅扩展已有 UI tool 的结构化返回。

### `terra.ui_begin_turn_review`

回合开始一次性为所有棋子计算完整候选卡会造成大量重复模拟。第一版只补充每个棋子的轻量 `tactical_overview`，用于引导选中，不返回每个 action 的卡：

```json
{
  "piece_id": 12,
  "tactical_overview": {
    "reachable_action_count": 8,
    "can_capture_now": false,
    "threatened_if_hold": false,
    "nearest_enemy_distance": 5,
    "terrain_tags": ["plain"],
    "nearby_friendly_piece_ids": [7],
    "summary_tags": ["friendly_support"],
    "candidate_card_available": true
  }
}
```

现有 `pieces[]` 的 L1 字段保留。`tactical_overview` 可复用 `FPieceTurnSurvey`、现有地形/邻接/距离查询，不应触发全局敌方回应模拟。

### `terra.ui_select_piece`

在每个 `move_options[]` 项新增：

```json
{
  "to_cell_id": 118,
  "local_tactical_card": {
    "schema_version": 1
  }
}
```

这里是 L2 的主要信息入口。每张卡基于该 option 对应的合法 action 和假设棋盘生成。原有顶层 move option 字段仍保留，避免 Agent 脚本或外部 Inspector 兼容性回归。

### `terra.ui_preview_move`

在 `preview` 内新增同名 `local_tactical_card`：

```json
{
  "preview": {
    "piece_id": 12,
    "to_cell_id": 118,
    "local_tactical_card": {
      "schema_version": 1,
      "mobility": {
        "can_continue_jump_now": false
      }
    }
  }
}
```

实现必须沿用 L1 已验证的“点击前缓存 action/risk，再触发真实 click”的做法。点击后 phase 可能变化，不能再用“当前 faction 的初始合法 action”重新查询而得到错误或空卡。

对于 `PieceJumpingCanContinue`，卡必须区分：

```text
当前真实可继续跳：interaction_state.continue_jump_target_cell_ids。
假设棋盘上的下一回合静态机动：local_tactical_card.mobility.*。
```

二者不能混用。

## 推荐 C++ 责任边界

当前真实代码位置：

```text
确定性棋盘和规则：
  Source/Gameplay/Public/TerraGameplayContainer.h
  Source/Gameplay/Private/TerraGameplayContainer.cpp

ui tool JSON 组装：
  Source/NpcMcp/Private/TerraNpcMcpGameplayQueryTools.cpp

真实点击和表现同步：
  Source/TerraCivilization/Public/Render/PlanetGameplayComponent.h
  Source/TerraCivilization/Private/Render/PlanetGameplayComponent.cpp
```

建议新增 Gameplay 层结构，而不是把 BFS、假设棋盘、兵种规则和 JSON 拼装塞进 NpcMcp：

```cpp
struct FLocalTacticalCardQuery
{
    int32 PieceId = INDEX_NONE;
    int32 FromCellId = INDEX_NONE;
    int32 ToCellId = INDEX_NONE;
    bool bIsJump = false;
    TArray<FTerraGameplayCaptureEntry> CaptureEntries;
};

struct FLocalTacticalSituationCard
{
    // Typed terrain/topology/mobility/friendly/enemy fields.
    // No FJsonObject or MCP-specific type belongs here.
};

bool FTerraGameplayContainer::BuildLocalTacticalSituationCard(
    const FLocalTacticalCardQuery& Query,
    FLocalTacticalSituationCard& OutCard) const;
```

`TerraNpcMcpGameplayQueryTools.cpp` 只增加 `MakeLocalTacticalCardObject_`，将 typed result 转为 JSON。`UPlanetGameplayComponent` 不负责计算卡，只在 select/preview 时把对应合法 action 传给 Gameplay query，再按 L1 既有路径触发真实点击。

实现前必须重新读取真实字段名和现有假设棋盘 helper，尤其是：

```text
FTerraGameplayCellState::NeighborCellIds
FTerraGameplayContainer::CollectJumpTargetsForHypotheticalBoard_
FTerraGameplayContainer::CollectCaptureEntriesAfterHypotheticalMoveForFaction_
FTerraGameplayContainer::EvaluateCurrentFactionActionRisk
```

不要在 MCP 层复制 `CanEnterTerrain_`、跳跃或弓兵攻击规则。

## 计算、缓存与预算

L2 的主要成本来自多 action 的假设棋盘、递归多跳与敌方回应查询。棋盘规模当前可控，但 `ui_select_piece` 可能一次产生许多 move option，因此需要显式预算。

```text
缓存键：
  turn_index + current_faction_id + interaction_phase + piece_id + from_cell_id + to_cell_id
  + 当前 action 的 capture entry 集合。

失效：
  每次真实点击成功、cancel、confirm、undo 或 turn/faction 改变后失效。

预算：
  ring 半径固定为 2。
  friendly follow-up 固定最大 6 个候选友军。
  enemy response action 固定最大枚举数；超过时返回 truncated=true 与已计算数量。
  不允许 L2 因完整全盘深搜阻塞 UI 点击表现。
```

所有截断字段都必须包含：

```json
{
  "truncated": true,
  "enumerated_count": 24,
  "limit": 24
}
```

不得把“截断后没有发现威胁”写成 `false`。如果完整性无法保证，布尔值应为 `null` 并附带 `incomplete_reason`。

## Agent 使用方式

L2 不要求修改 Gameplay phase，也不要求 Agent 维护卡缓存的权威副本。当前交互式 Agent 的每步流程仍为：

```text
1. tools/list 获取本 phase 可用的 ui tools。
2. begin_turn_review 读取轻量 overview。
3. select_piece 获取该棋子的各个 move_options.local_tactical_card。
4. 比较少量候选卡；必要时 preview_move 获得真实中间状态下的同一张卡。
5. 根据证据 cancel、换棋子、继续跳，或 confirm_action。
```

系统提示词应强调：

```text
不要只按 capture_count 或 destination_threatened 选择。
先比较 approach_to_enemy、mobility、friendly_synergy、enemy_interaction 的证据。
卡中的未来协同是静态条件，不代表敌方不会回应。
当卡的 enemy response 结果 truncated 或 null 时，不要把它解释为安全。
```

L2 后的 `run-llm-interactive-think.js` 应重点记录：模型是否开始主动检查弓兵、是否会因骑兵前方山地阻断而放弃无后续推进、是否以协同与机动证据而不只是“有无吃子”作比较。脚本改动不属于本设计稿的本次实现范围。

## 验收标准

### 数据正确性

```text
1. 对每个 move option，local_tactical_card 的 piece/from/to 与 action 完全一致。
2. from/to terrain、邻近棋子、最短敌距与底层 Gameplay 查询一致。
3. 骑兵面对山地时，forward_blocked_by_mountain 只报告实际阻断其规则机动的格。
4. ordinary/jump/reachable endpoint 计数复用 Gameplay 规则；多跳端点去重。
5. jump anchor 和 enemy response 均由假设棋盘上的真实规则验证，不能由邻接关系猜测。
6. 所有 null、truncated、limit 语义可区分，不能把“未知”伪装为 false/0。
```

### L1 兼容性

```text
1. ui tools 名称、输入 schema、phase 动态暴露完全不变。
2. ui_select_piece 仍走真实 piece click，高亮与镜头保持工作。
3. ui_preview_move 仍走真实 target click，preview 卡与点击前缓存 action 对应。
4. ui_confirm_action 仍只确认当前 pending action。
5. L1 Inspector 测试和 run-interactive-review-once.js 无需修改即可通过。
```

### 行为观察

```text
1. MCP Inspector 能看到 begin overview、select option card、preview card 三个层级。
2. 选择弓兵进入森林时，卡同时显示 forest、approach、支援和敌方回应事实。
3. 选择骑兵走向山地封口时，卡能显示其前方可进入格下降和山地阻断。
4. 选择会成为友军跳跃锚点的行动时，卡能返回对应 friend piece id 与规则验证标签。
5. LLM 日志可从字段中解释“为什么看这个棋子、为什么放弃该落点、为什么确认”。
```

## 后续衔接

L2 完成后，优先根据 Agent 日志决定下一轮确定性抽象，而不是继续扩大原始 JSON 或硬编码套路。候选方向：

```text
局部战术模式查询：从 L2 的事实卡中枚举可验证的两回合计划。
战略语义工具：把多个 L2 卡压缩为阵营层的前线、地形控制与压力摘要。
多阵营记忆：把已确认 action 和被观察但放弃的结构写入各 faction 的 active plan。
```

L2 的成功标准不是让 LLM 第一次就下出最强棋，而是让它第一次拥有足以看见“为什么某个安全落点没有后续”“为什么某个森林落点与友军组合有意义”的可靠、可视、可复盘事实。
