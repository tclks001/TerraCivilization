# LLM Agent 战略思考与交互式战术工具设计稿

日期：2026-07-09
状态：总体设计持续维护中；A1/A2/A3、L1、L2、L3、L3.1 已实现并完成 Runtime 验证。

## 目标

本文是本项目战棋 NPC LLM Agent 的独立总体设计稿，不属于 `AI_Toolset_Runtime_MCP_Research_UE58.md` 的 A3 Step，也不作为 A4 阶段稿。

它的目标是定义一种更接近真人玩家体验的 NPC 决策方式：

```text
LLM Agent 不只在后台读取局势 JSON 并输出最终行动。
LLM Agent 应该拥有识别局势、战略思考、战术分析、可视化试探、回退确认等行为工具。
玩家应该能看到 NPC 像真人一样点选棋子、观察局部、预览落点、比较风险，最后确认行动。
```

本文讨论长期设计方向、已验证能力和后续分层，不替代 A1/A2/A3/L1 的具体实现设计稿。

## 已实现并验证的基础

当前已经完成并验证了 Runtime MCP 驱动 NPC 行动的基础闭环：

1. [A1 NPC MCP Runtime Server 与 Ping 设计稿](A1NpcMcpRuntimeServerAndPingDesign.md)
   - UE Runtime 能启动 MCP server。
   - 能注册并调用基础 ping tool。
   - 解决了 Runtime 加载 `ModelContextProtocol` 模块的问题。

2. [A2 NPC MCP 确定性查询工具设计稿](A2NpcMcpDeterministicQueryToolsDesign.md)
   - 已提供确定性 Gameplay 查询工具。
   - 包括当前回合上下文、合法行动、风险评估、proposal 校验。
   - 核心原则是：规则判断由工具返回，不让 LLM 自己从原始局势里幻觉推导。

3. [A3 外部 LLM Agent 驱动 NPC 执行闭环设计稿](A3ExternalLlmAgentNpcExecutionLoopDesign.md)
   - 已实现 `terra.execute_validated_action`。
   - MCP Inspector 手动调用执行工具已跑通。
   - 外部无 LLM Agent 已跑通。
   - Step B：LLM 只做行动选择，Agent 负责 MCP 调用和最终执行，已经验证完毕。

4. [A3 LLM Agent 与 MCP 连接操作手册](A3LlmAgentMcpConnectionGuide.md)
   - 已说明 Step 0 deterministic Agent。
   - 已实现并说明 Step B：LLM 只从候选行动里选择，Agent 执行校验和 fallback。
   - 已讨论 Step C：LLM 使用 MCP tool calling 的 Agent gateway、白名单与日志边界。

5. [L1 Interactive Tactical Review Tools 最小闭环设计稿](L1InteractiveTacticalReviewToolsDesign.md)
   - 已实现并验收 `terra.ui_begin_turn_review`、`terra.ui_select_piece`、`terra.ui_preview_move`、`terra.ui_cancel_selection`、`terra.ui_confirm_action`。
   - UI tools 复用真实 Gameplay 点击、高亮、镜头和确认路径，不维护独立 AI Preview 棋盘。
   - `tools/list` 按 Gameplay phase 动态只暴露当前可用的 UI tools；非法时机和非法参数会被拒绝且返回原因。
   - 已提供局部确定性情报：地形、邻近敌我、局部摘要、递归多跳可达数量、当前可吃与驻留威胁、敌人最近距离和接近/撤退幅度。
   - MCP Inspector、无 LLM review Agent 与交互式 LLM Agent 均已完成验证。

这些阶段证明了：

```text
UE Runtime MCP server 可用。
外部 Agent 可以连接 UE。
Gameplay 可以向外部 Agent 提供确定性信息。
Agent 可以提交并执行合法 NPC 行动。
LLM 可以参与行动选择，但执行权仍由 Agent 和 UE validator 控制。
```

当前 Agent 脚本已归档在 [L1 设计稿](L1InteractiveTacticalReviewToolsDesign.md#已实现-agent-归档)，运行说明在 [Tools/NpcAgent/README.md](C:/workspace/TerraCivilization/Tools/NpcAgent/README.md)。

本文在这些基础上继续讨论：如何让 LLM Agent 从“会选一个合法行动”升级为“像玩家一样观察、计划和执行”。

## 当前 Step B 的能力边界

Step B 已经能让 LLM 从候选行动中选择一个行动，例如：

```json
{
  "piece_id": 31,
  "to_cell_id": 428,
  "reason": "capture_count=1 and destination_threatened=false"
}
```

这个闭环适合验证 MCP、LLM 选择、proposal 校验、fallback 和执行路径，但它天然偏战术局部：

```text
能知道哪里能走。
能知道哪里能吃子。
能知道落点是否危险。
能在候选行动里做局部比较。
```

它还不擅长：

```text
识别外圈、内圈、侧翼、前线、屏障等拓扑概念。
理解森林、山地、高地等地形的战略价值。
为弓兵、骑兵、步兵等兵种建立两回合或三回合协同计划。
记住上一回合建立的 active plan。
让玩家看到 NPC 正在像真人一样观察和试探。
```

因此，后续不应只是把更多原始工具暴露给 LLM。更重要的是增加战略语义层和交互式战术观察层。

## 总体理念：操作即信息，信息即操作

真人玩家玩战棋时，不是先读一份完整 JSON 再输出最终行动。玩家通常会：

```text
看全局。
点自己的棋子。
看这个棋子的可达格。
看落点周围的敌人、地形、威胁线。
试着点一个落点。
发现不合适就返回。
换另一个棋子。
最后确认行动。
```

因此，LLM Agent 的工具也可以设计成同样的形态：

```text
每一次工具调用既是一次信息查询，也是一次可见操作。
合法操作会驱动画面反馈，并返回新的局势信息。
非法操作只返回错误，不污染 Gameplay。
```

这会带来两个收益：

1. 决策质量更自然。
   - LLM 不需要一次性理解完整棋盘。
   - LLM 可以像玩家一样逐步聚焦、观察和比较。

2. 玩家体验更强。
   - 玩家能看到 NPC 高亮棋子、检查骑兵、查看弓兵落点、预览树林、防线和威胁。
   - NPC 决策过程不再是几秒黑箱等待，而是一个可观察的对抗过程。

## 三层 Agent 架构

推荐把 LLM Agent 拆成三层，而不是让 LLM 直接面对所有底层工具。

```text
Strategic Layer
  识别本回合战略目标：抢树林、保弓兵、压内圈、防骑兵、集火邻居。

Tactical Analysis Layer
  生成候选战术：两回合组合、兵种协同、地形控制、威胁线、屏障位置。

Interactive Action Layer
  像玩家一样点选、观察、预览、回退、确认，并把过程展示给玩家。
```

三层的关系是：

```text
战略层决定为什么看。
战术层决定先看谁和看哪里。
交互层决定怎么点、怎么试、怎么确认。
```

示例：

```text
战略层：
  本回合目标是建立弓兵树林狙击阵位。

战术层：
  弓兵 12 可进入森林 118。
  骑兵 7 下回合可站到 117 形成屏障。
  敌方骑兵 34 下回合可能进入弓兵射程。

交互层：
  ui_begin_turn_review
  ui_select_piece(12)
  ui_preview_move(12, 118)
  ui_confirm_action()
```

## 工具分层

### 1. 底层确定性规则工具

这些是 A2/A3 已经建立的基础，负责权威规则判断：

```text
terra.get_turn_context
terra.list_legal_actions
terra.evaluate_action_risk
terra.submit_action_proposal
terra.execute_validated_action
```

这些工具不负责“有趣的战略”，但负责把所有合法性和执行性固定下来。

### 2. 战略语义工具

战略语义工具把原始棋盘压缩成 LLM 更容易理解的概念：

```text
terra.strategy.summarize_faction_state
terra.strategy.describe_strategic_options
terra.strategy.describe_frontline
terra.strategy.find_terrain_control_points
terra.strategy.find_enemy_pressure
```

返回的信息不应该是几百个格子的列表，而应该是战略摘要：

```json
{
  "faction_id": 5,
  "strategic_options": [
    {
      "id": "secure_north_forest_cluster",
      "type": "terrain_control",
      "priority": 72,
      "reason": "forest cluster protects archers and contests inner ring",
      "key_piece_ids": [12, 7],
      "key_cell_ids": [118, 117]
    },
    {
      "id": "avoid_cavalry_overextension",
      "type": "risk_control",
      "priority": 64,
      "reason": "enemy cavalry can punish isolated units near outer ring"
    }
  ]
}
```

### 3. L3.1 局部拓扑观察工具

[L3.1 局部拓扑观察工具设计稿](L31LocalTopologyObservationDesign.md) 补上“玩家看棋盘”与“LLM 读局部图”之间的观察层：

```text
terra.inspect_local_topology(cell_id)
```

它不是行动工具，也不替代 L2 的合法行动、风险和局部战术卡。它固定返回目标 Cell 半径 3 内的：

```text
cells：每个 Cell 的 terrain 与占据该格的 piece/faction/type。
edges：局部子图内的无向真实邻接边。
```

该工具始终可见、只读、幂等，不改变 UI 选择、高亮或镜头。LLM 应在战略工具指出 control point、前线或关键棋子后，主动调用它确认局部通路、山地阻断、森林掩护、友军锚点和敌我结构；不得仅从 CellId 数字或世界空间想象拓扑。

`cells` 不包含 offset、distance、neighbor list 等重复拓扑字段，`edges` 是唯一权威图关系。LLM 不负责以图论保证行动合法性，仍必须使用 `ui_select_piece` / `ui_preview_move` 和 Gameplay validator 验证具体走法。

### 4. 战术分析工具

战术分析工具负责把战略意图落到可验证的候选计划：

```text
terra.find_piece_synergy_candidates
terra.find_two_turn_tactics
terra.find_cover_positions
terra.find_threat_lanes
terra.find_screen_positions
terra.find_capture_setups
```

示例：

```json
{
  "tactics": [
    {
      "id": "archer_forest_screen_kill_001",
      "name": "archer_forest_screen_kill",
      "score": 72,
      "intent": "prepare_archer_fire_position",
      "turn_0_action": {
        "piece_id": 12,
        "to_cell_id": 118
      },
      "turn_1_followups": [
        {
          "piece_id": 7,
          "to_cell_id": 117,
          "purpose": "screen_archer"
        }
      ],
      "expected_gain": {
        "enemy_piece_threatened": 34,
        "enemy_piece_type": "cavalry"
      },
      "risks": [
        "enemy may move cavalry away",
        "forest cell has one enemy threat lane"
      ]
    }
  ]
}
```

这些工具应由 C++ 或 Python 脚本做确定性枚举和评分。LLM 负责比较和选择，不负责证明物理可行性。

### 4. Interactive Tactical Review Tools

这是本文设计的关键部分。

这些工具让 LLM 像真人玩家一样通过操作获得信息，同时把操作过程展示给玩家。

当前已实现的工具集：

```text
terra.ui_begin_turn_review
terra.ui_select_piece
terra.ui_preview_move
terra.ui_cancel_selection
terra.ui_confirm_action
```

#### `terra.ui_begin_turn_review`

作用：

```text
不额外触发表现层行为；回合开始已有的阵营高亮保持不变。
返回当前阵营每个棋子的概要。
帮助 LLM 决定先看哪个棋子。
```

示例输出：

```json
{
  "ok": true,
  "current_faction_id": 5,
  "pieces": [
    {
      "piece_id": 12,
      "type": "archer",
      "cell_id": 101,
      "terrain_tags": ["forest", "hill"],
      "nearby_enemy_piece_ids": [34],
      "nearby_friendly_piece_ids": [7],
      "legal_action_count": 4,
      "can_capture_now": false,
      "max_capture_count": 0,
      "threatened_if_hold": false,
      "summary_tags": ["forest_cover", "friendly_support"]
    }
  ]
}
```

#### `terra.ui_select_piece(piece_id)`

作用：

```text
画面选中棋子。
高亮该棋子的可行动格。
返回局部拓扑、地形、敌我距离、可吃目标、危险落点、可继续跳等信息。
```

示例输出：

```json
{
  "ok": true,
  "selected_piece_id": 12,
  "move_options": [
    {
      "to_cell_id": 118,
      "terrain_tags": ["forest"],
      "is_jump": false,
      "capture_count": 0,
      "destination_threatened": false,
      "nearby_enemy_piece_ids": [34],
      "nearby_friendly_piece_ids": [7],
      "nearest_enemy_distance_from": 5,
      "nearest_enemy_distance_to": 3,
      "approach_to_enemy": 2,
      "summary_tags": ["forest_cover", "friendly_support"]
    }
  ]
}
```

#### `terra.ui_preview_move(piece_id, to_cell_id)`

作用：

```text
通过真实 Gameplay 点击路径点击目标格，进入权威交互状态机中的中间状态。
它不是从零执行 action，也不是独立 AI Preview State；高亮、镜头、继续跳候选和棋子中间位置均复用现有 Gameplay 行为。
返回本次已点击行动和落点的局部局势。
```

示例输出：

```json
{
  "ok": true,
  "preview": {
    "piece_id": 12,
    "from_cell_id": 101,
    "to_cell_id": 118,
    "terrain_tags": ["forest"],
    "destination_threatened": false,
    "nearby_enemy_piece_ids": [34],
    "nearby_friendly_piece_ids": [7],
    "nearest_enemy_distance_from": 5,
    "nearest_enemy_distance_to": 3,
    "approach_to_enemy": 2,
    "summary_tags": ["forest_cover", "friendly_support"]
  }
}
```

#### `terra.ui_cancel_selection`

作用：

```text
取消当前棋子选择。
清空高亮或回到 begin_turn_review 状态。
用于让 LLM 换一个棋子重新观察。
```

#### `terra.ui_confirm_action`

作用：

```text
确认当前真实点击状态机里已经形成的 pending action，并按现有 Gameplay 路径结束行动/推进回合。
它不接受任意 `piece_id/to_cell_id`，不能绕过 `ui_select_piece` 和 `ui_preview_move`。
```

## 当前 Gameplay 状态与未来 Preview State

当前 L1 的唯一权威交互状态机是 Gameplay 的 `ETerraGameplayInteractionPhase`：`Idle`、`PieceSelected`、`PieceMovedCanEndTurn`、`PieceJumpingCanContinue`。Agent 只缓存每次工具返回的 `interaction_state`，下一步可调用的 tools 由 `tools/list` 按该 phase 决定。

因此当前语义为：`ui_select_piece` 和 `ui_preview_move` 会进入真实的 Gameplay 中间状态，`ui_cancel_selection` 复用既有取消路径，`ui_confirm_action` 结束当前 pending action。非法调用不应改变 Gameplay。

独立 AI Preview State 是未来的可选架构方向，不是当前实现前提。只有当回放、并发 NPC 试探或复杂多层撤销确实需要与真实交互状态分离时，才应单独设计其同步、表现和提交边界。

## Agent 决策循环

推荐的完整循环：

```text
1. Agent 调 get_turn_context。
2. Agent 调战略语义工具，获得本回合战略候选。
3. LLM 选择 active_intent。
4. Agent 调战术分析工具，获得候选 tactics。
5. LLM 选择要验证的 tactic 或 priority piece。
6. Agent 进入 Interactive Tactical Review。
7. LLM 调 ui_begin_turn_review。
8. LLM 调 ui_select_piece。
9. LLM 调 ui_preview_move。
10. LLM 可根据结果 cancel、重新选择或继续跳。
11. LLM 满意后调 ui_confirm_action。
12. Agent 记录 decision log、active plan 和工具调用轨迹。
```

简化为一句话：

```text
战略工具决定关注什么。
战术工具提出可验证计划。
交互工具让 LLM 像玩家一样观察和确认。
Gameplay validator 决定什么真正发生。
```

## 阵营记忆与 Active Plan

要让 NPC 产生连续战略，必须为每个阵营保存自己的 Agent 记忆。

建议记录：

```json
{
  "faction_id": 5,
  "current_strategy": "secure_inner_ring_and_preserve_archers",
  "active_plan": {
    "id": "archer_forest_screen_kill_001",
    "created_turn": 1,
    "expires_turn": 3,
    "steps": [
      {
        "turn_offset": 0,
        "piece_id": 12,
        "to_cell_id": 118,
        "status": "done"
      },
      {
        "turn_offset": 1,
        "piece_id": 7,
        "to_cell_id": 117,
        "status": "pending"
      }
    ]
  },
  "known_rivals": [
    {
      "faction_id": 3,
      "relationship": "hostile",
      "reason": "attacked scout on turn 2"
    }
  ]
}
```

每个 NPC 回合开始时，Agent 不应该直接重新选一个局部最优行动，而应该先问：

```text
上一回合的 active_plan 还有效吗？
如果有效，本回合是否应该继续执行下一步？
如果无效，是因为敌人移动、棋子死亡、地形被抢，还是风险变化？
```

## 玩家可见体验

这个设计的体验目标是让玩家看到 NPC 的思考过程：

```text
NPC 高亮自己的棋子。
NPC 点骑兵，查看外圈跳点。
NPC 取消骑兵。
NPC 点弓兵，查看树林落点。
NPC 预览弓兵进入树林。
NPC 显示敌方骑兵威胁线和己方骑兵屏障位置。
NPC 确认移动。
```

这比“NPC 思考数秒后棋子瞬移”更有对抗感。

但需要节奏控制：

```text
重要 NPC 或玩家附近 NPC：
  可以展示完整 review 过程。

远离玩家视野的 NPC：
  可以后台决策，只展示最终行动。

每个 NPC 回合：
  限制可视化工具调用次数，例如 8 到 15 次。
  限制 preview 次数，例如 3 到 5 次。
  每次可视化操作之间加入短延迟，例如 0.2 到 0.5 秒。
```

## 安全边界

即使后续让 LLM 使用 MCP tool calling，也不应把所有工具等价暴露。

推荐分级：

```text
LLM 可直接调用：
  战略语义工具。
  战术分析工具。
  ui_begin_turn_review。
  ui_select_piece。
  ui_preview_move。
  ui_cancel_selection。
  ui_confirm_action（仅在当前 Gameplay phase 可见，且只确认当前 pending action）。

Agent 可选的代理调用或二次确认：
  submit_action_proposal。

Agent-only：
  execute_validated_action。
```

原因：

```text
execute_validated_action 是正式写操作。
它会推进回合。
它应该保留在 Agent permission gateway 后面。
```

## 与 Step C 的关系和当前实验状态

原 A3 手册中的 Step C 指的是：

```text
LLM 使用 MCP tool calling。
Agent 做工具白名单、执行确认、fallback 和日志。
```

当前已经完成两类 Step C 风格实验 Agent：

```text
run-llm-basic-tools.js：
  LLM 自主调用基础工具；它容易退化为“找第一个安全合法行动”。

run-llm-interactive-tools.js / run-llm-interactive-think.js：
  LLM 只看到当前 phase 可用的 ui tools；会产生 select / preview / cancel / confirm 的可见观察过程。
  think 版本额外记录 turn goal、假设、证据和放弃理由，且只以 confirm 成功作为回合完成。
```

这些实验验证了交互工具面对玩家体验和可复盘性更好，但也暴露出：仅增加步骤和提示词不足以让 LLM 自动发现复杂兵种、地形、多跳协同。后续不能只继续堆 prompt 或行动步数。

更合理的 Step C 应该建立在本文工具体系上：

```text
LLM 调战略语义工具理解大局。
LLM 调战术分析工具生成计划。
LLM 调 Interactive Tactical Review Tools 做可视化观察和预览。
Agent 控制执行权限。
UE validator 保持权威。
```

也就是说，Step C 的核心不是“让 LLM 调更多工具”，而是“让 LLM 调对它有认知意义、对玩家有表现意义、对 Gameplay 有安全边界的工具”。

## 推荐后续落地顺序

本文是总体设计稿。后续可以拆成单独实现阶段：

1. L1 已完成：Interactive Tactical Review 与局部确定性情报
   - 以真实 Gameplay phase 和点击路径为执行面。
   - 继续保持 UI tools 与状态机一致，不新增平行 Agent phase。

2. [L2 局部战术情报卡](L2LocalTacticalSituationCardDesign.md)
   - 在现有 terrain/nearby/distance 字段上补充落点前方拓扑、通行阻断、友军支援增量、跳跃锚点和后续机动潜力。
   - 目标是表达通用事实，例如山前骑兵死角、可作为下一回合跳跃锚点、可形成友军屏障，而不是硬编码固定棋谱。

3. [L3 战略语义工具](L3StrategicSemanticToolsDesign.md)
   - 已完成：阵营摘要、地形控制点、前线和敌方压力。

4. [L3.2 战略候选重写](L32StrategicOptionsRewriteDesign.md)
   - 已完成：重写 `describe_strategic_options`，移除 priority，枚举前线、争议地形、地形走廊和暴露单位等调查候选。
   - 候选是可验证的观察线索，不是行动或分数排序命令。

5. [L3.1 局部拓扑观察工具](L31LocalTopologyObservationDesign.md)
   - 已完成：以指定 Cell 为中心返回固定半径 3 的真实局部子图，供 LLM 主动观察地形、棋子与连接关系。
   - 首轮 Agent 已主动查询战略 control point；验证了局部边关系读取，但也证明 raw topology 不能替代兵种受限可达性与目标对齐工具。

6. 多回合协同分析工具
   - 以确定性枚举输出可验证的两回合/多回合候选计划、屏障、远程火力窗口和多跳链机会。
   - LLM 比较计划与上下文，不自行证明规则可行性。

7. 阵营记忆与 active plan
   - 每个 NPC 阵营独立上下文。
   - 记录计划、敌对关系、近期攻击和被攻击事件。

8. 生产级多阵营 Agent 调度
   - 12 个阵营各自拥有独立记忆和 active plan，不共享隐式对话上下文。
   - Agent 按回合复用或重建会话均可，但每回合都以 Gameplay 返回的事实和该阵营持久记忆为准。
   - 依据玩家可见性、重要性和预算决定展示完整 review 还是后台快速执行。

独立 AI Preview State 保留为上述路线之外的可选重构项，不作为既定下一阶段。

## 验收标准

L1 对应的第一轮验收已经完成；以下条目同时作为该能力回归验收：

```text
1. NPC 回合开始后，Agent 能读取当前阵营摘要，并保留现有回合开始高亮。
2. LLM 能选择一个棋子并触发可见选中。
3. LLM 能预览至少一个落点。
4. 画面能显示落点相关信息，例如可达格、威胁、地形或后续机会。
5. LLM 能取消、重新选择、继续跳或确认。
6. 确认后仍走 Gameplay validator。
7. 非法操作不会污染正式 Gameplay。
8. decision log 能复盘本次 NPC 为什么先看这个棋子、为什么预览这个落点、为什么最终确认。
```

下一阶段验收目标：

```text
NPC 能基于地形、兵种协同和多回合 active plan 行动。
玩家能从画面上观察 NPC 的分析过程。
LLM 的高级策略来自战略语义工具和战术分析工具，而不是对原始棋盘的幻觉推理。
```

## 结论

本项目的 LLM NPC 不应只是一个后台行动选择器。它应该是一个能通过工具观察局势、形成战略意图、验证战术计划、并以玩家可见方式执行决策的 Agent。

核心路线是：

```text
确定性规则工具
  -> 战略语义工具
  -> 战术分析工具
  -> Interactive Tactical Review Tools
  -> Agent 执行网关
  -> UE Gameplay validator
```

这条路线可以同时满足三件事：

1. 降低 LLM 幻觉，让规则和拓扑判断由工具负责。
2. 提升 NPC 战略水平，让 LLM 在语义化战场信息上规划。
3. 提升玩家体验，让 NPC 的观察和试探过程在游戏中可见。
