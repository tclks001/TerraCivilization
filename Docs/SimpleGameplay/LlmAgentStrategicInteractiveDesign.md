# LLM Agent 战略思考与交互式战术工具设计稿

日期：2026-07-09

## 目标

本文是本项目战棋 NPC LLM Agent 的独立总体设计稿，不属于 `AI_Toolset_Runtime_MCP_Research_UE58.md` 的 A3 Step，也不作为 A4 阶段稿。

它的目标是定义一种更接近真人玩家体验的 NPC 决策方式：

```text
LLM Agent 不只在后台读取局势 JSON 并输出最终行动。
LLM Agent 应该拥有识别局势、战略思考、战术分析、可视化试探、回退确认等行为工具。
玩家应该能看到 NPC 像真人一样点选棋子、观察局部、预览落点、比较风险，最后确认行动。
```

本文讨论的是长期设计方向和工具分层，不直接替代已经完成的 A1/A2/A3 实现。

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
   - 已讨论 Step C：LLM 使用 MCP tool calling，但还未作为最终方向直接推进。

这些阶段证明了：

```text
UE Runtime MCP server 可用。
外部 Agent 可以连接 UE。
Gameplay 可以向外部 Agent 提供确定性信息。
Agent 可以提交并执行合法 NPC 行动。
LLM 可以参与行动选择，但执行权仍由 Agent 和 UE validator 控制。
```

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
terra.summarize_faction_state
terra.describe_strategic_options
terra.describe_frontline
terra.describe_ring_control
terra.find_terrain_control_points
terra.find_enemy_pressure
terra.summarize_recent_hostility
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

### 3. 战术分析工具

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

推荐的最小工具集：

```text
terra.ui_begin_turn_review
terra.ui_select_piece
terra.ui_preview_move
terra.ui_undo_preview
terra.ui_cancel_selection
terra.ui_confirm_action
```

#### `terra.ui_begin_turn_review`

作用：

```text
高亮当前阵营所有棋子。
返回每个棋子的概要。
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
      "nearby_terrain": ["forest", "hill"],
      "nearby_enemy_count": 2,
      "nearby_friendly_count": 1,
      "suggested_focus": ["cover", "ranged_threat"]
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
      "terrain": "forest",
      "is_jump": false,
      "capture_count": 0,
      "destination_threatened": false,
      "nearby_enemy_piece_ids": [34],
      "tactical_tags": ["cover", "future_archer_lane"]
    }
  ]
}
```

#### `terra.ui_preview_move(piece_id, to_cell_id)`

作用：

```text
不正式提交行动。
在 AI Preview State 中预览棋子到目标格。
画面显示预览路径、目标格、威胁线或后续跳点。
返回落点后的局部局势。
```

示例输出：

```json
{
  "ok": true,
  "preview": {
    "piece_id": 12,
    "from_cell_id": 101,
    "to_cell_id": 118,
    "terrain": "forest",
    "destination_threatened": false,
    "next_turn_attack_options": [
      {
        "enemy_piece_id": 34,
        "enemy_type": "cavalry",
        "requires_screen": true
      }
    ],
    "friendly_screen_candidates": [
      {
        "piece_id": 7,
        "screen_cell_id": 117,
        "reachable_next_turn": true
      }
    ]
  }
}
```

#### `terra.ui_undo_preview`

作用：

```text
回退上一个预览步骤。
画面也恢复到上一个 preview 或 selection 状态。
用于让 LLM 试探多个落点。
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
把当前 preview 中的最终行动提交给 Gameplay validator。
通过后调用真实执行路径。
失败则不修改正式 Gameplay。
```

它可以复用 A3 已验证的执行边界：

```text
submit_action_proposal
execute_validated_action
```

## Preview State 与正式 Gameplay State

必须区分两种状态：

```text
正式 Gameplay State
  当前权威棋盘。
  只有 confirm_action 通过 validator 后才能修改。

AI Preview State
  LLM 正在点选、预览、试探、回退的临时状态。
  可以驱动画面表现。
  可以撤销。
  不影响正式回合。
```

不建议让 `ui_preview_move` 直接修改正式 Gameplay。否则 LLM 的试错和回退会污染权威状态，也会让 replay、action log、胜负判断变复杂。

推荐原则：

```text
select / preview / undo / cancel:
  只修改 AI Preview State 和 UI 表现。

confirm:
  调 Gameplay validator。
  通过后修改正式 Gameplay State。
```

非法操作处理：

```text
非法 select_piece:
  不改变 UI，返回 error。

非法 preview_move:
  不改变 preview state，返回 error 和合法目标提示。

非法 confirm_action:
  不改变正式 Gameplay，返回 rejection reason。
```

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
10. LLM 可根据结果 undo、cancel 或选择另一棋子。
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
  ui_undo_preview。
  ui_cancel_selection。

Agent 代理调用或二次确认：
  submit_action_proposal。
  ui_confirm_action。

Agent-only：
  execute_validated_action。
```

原因：

```text
execute_validated_action 是正式写操作。
它会推进回合。
它应该保留在 Agent permission gateway 后面。
```

## 与 Step C 的关系

原 A3 手册中的 Step C 指的是：

```text
LLM 使用 MCP tool calling。
Agent 做工具白名单、执行确认、fallback 和日志。
```

本文建议：不要把 Step C 简单理解成“把现有 A2/A3 tools 全部交给 LLM 调用”。

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

1. Interactive Tactical Review 最小闭环
   - `terra.ui_begin_turn_review`
   - `terra.ui_select_piece`
   - `terra.ui_preview_move`
   - `terra.ui_cancel_selection`
   - `terra.ui_confirm_action`

2. AI Preview State
   - 与正式 Gameplay State 分离。
   - 支持 preview、undo、cancel。
   - 支持表现层高亮、路径、威胁线。

3. 战略语义工具
   - 阵营摘要。
   - 地形控制点。
   - 环形拓扑、前线、敌方压力。

4. 两回合战术工具
   - 弓兵进树林。
   - 骑兵屏障。
   - 跳跃突击。
   - 远程狙杀窗口。

5. 阵营记忆与 active plan
   - 每个 NPC 阵营独立上下文。
   - 记录计划、敌对关系、近期攻击和被攻击事件。

6. 真正的 Step C Tool Calling Agent
   - LLM 可调用白名单工具。
   - Agent 保留执行网关。
   - 所有工具调用写入 decision log。

## 验收标准

第一轮验收不要求 NPC 真的很聪明，而要求体验闭环成立：

```text
1. NPC 回合开始后，Agent 能高亮当前阵营棋子。
2. LLM 能选择一个棋子并触发可见选中。
3. LLM 能预览至少一个落点。
4. 画面能显示落点相关信息，例如可达格、威胁、地形或后续机会。
5. LLM 能取消或确认。
6. 确认后仍走 Gameplay validator。
7. 非法操作不会污染正式 Gameplay。
8. decision log 能复盘本次 NPC 为什么先看这个棋子、为什么预览这个落点、为什么最终确认。
```

长期验收目标：

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
