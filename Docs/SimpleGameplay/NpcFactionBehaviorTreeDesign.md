# 阵营级 NPC 行为树设计稿

日期：2026-07-15

## 目标

本稿为 TerraCivilization 增加 UE 行为树（Behavior Tree, BT）的设计边界与渐进验收路线。

本项目中，一个阵营就是一个 NPC 决策主体；**不是每个棋子各自拥有一个 AIController 或行为树**。NPC 在自己的回合中从当前阵营的合法行动中选择一次行动，交给 Gameplay 执行并推进回合。

第一阶段只验证无 LLM 的最小闭环：

```text
当前回合属于 NPC 阵营
  -> 阵营级行为树被唤醒
  -> 查询当前阵营合法行动
  -> 确定性选择一个行动
  -> Gameplay 再校验并执行
  -> 回合推进
```

第一阶段不接入 MCP、不请求外部 Agent、不调用 LLM。这样可以先证明 UE 内部的回合编排和权威执行稳定，再逐步引入更复杂的策略与外部提案。

## 阶段设计稿索引

| 阶段 | 设计稿 | 验收目标 |
| --- | --- | --- |
| BT1 | [BT1_NpcFactionBehaviorTreeMinimalLoopDesign.md](BT1_NpcFactionBehaviorTreeMinimalLoopDesign.md) | NPC 阵营在非玩家回合执行一次合法行动 |
| BT1.1 | [BT1_1_NpcTechnologyChoiceDesign.md](BT1_1_NpcTechnologyChoiceDesign.md) | NPC 阵营在待选科技时自主选择一个合法科技 |

## 1. 为什么需要行为树

行为树解决的是“NPC 当前该走哪一条流程”：等待玩家、识别自己的回合、收集上下文、选择方案、执行、失败回退、结束回合。

它不负责以下工作：

- 不重新推导走法、攻击、地形或奖励规则。
- 不直接修改棋盘状态。
- 不替代 `FTerraGameplayContainer` 的合法性校验。
- 不要求 LLM 常驻或可用。

对应职责应保持如下划分：

| 层级 | 负责内容 | 不负责内容 |
| --- | --- | --- |
| Gameplay / Validator | 地图、棋子、回合、合法行动、结算、最终执行 | 策略风格、等待外部模型 |
| 阵营级行为树 | NPC 回合编排、分支、超时、回退、调试可视化 | 绕过规则执行行动 |
| 确定性策略 | 对合法行动排序和选择 | 改写玩法规则 |
| MCP / 外部 LLM Agent | 提出候选行动与高层意图 | 最终状态写入和规则裁决 |

现有 Gameplay 公共接口已经适合作为权威边界：

- `CollectCurrentFactionLegalActions`：列出当前阵营合法行动。
- `EvaluateCurrentFactionActionRisk`：评估候选行动风险。
- `TryExecuteValidatedAction`：带回合快照校验的最终执行。

具体声明见 `Source/Gameplay/Public/TerraGameplayContainer.h`。行为树任务应调用这些 Gameplay API；不应在 UE 进程内绕一圈请求本机 MCP HTTP 服务。

## 2. 阵营级 NPC Brain

### 2.1 运行时实体

建议每个由 AI 控制的阵营拥有一个不可见的“阵营大脑”Pawn 与对应 AIController：

```text
ATerraNpcBrainPawn (一个 NPC 阵营一个实例)
  └-> ATerraNpcAIController
        └-> UBehaviorTree: BT_TerraFactionNpc
              └-> UBlackboardData: BB_TerraFactionNpc
```

它们不代表棋盘上的任何棋子，不参与寻路、碰撞、渲染或棋子动画。它们只保存阵营身份并运行行为树。

这样设计的原因：

- 一个回合的行动权属于阵营，而非单个棋子。
- 多棋子之间的取舍应在同一棵树中完成。
- 行为树调试器可以直接显示“Faction 7 正在等待 / 决策 / 执行 / 回退”。
- 后续中立阵营、敌对阵营、不同策略档案都能复用同一套 Brain 类型。

### 2.2 生命周期

建议由游戏侧的 NPC 管理入口在 Gameplay 初始化完成后创建/注册 Brain：

1. 为每个 AI 阵营创建一个 `ATerraNpcBrainPawn`。
2. 设定 `ControlledFactionId`。
3. 由 `ATerraNpcAIController::OnPossess` 初始化 Blackboard 并运行行为树。
4. 行为树常驻等待；它只在 `CurrentFactionId == ControlledFactionId` 时进入决策和执行分支。
5. 对局结束、地图重建或阵营被移除时停止对应 Brain，并清除其短期决策上下文。

第一版不需要让 Brain 持有 `FTerraGameplayContainer`。应由一个明确的 Gameplay Bridge / World Subsystem 提供受控访问，避免 AIController 直接依赖渲染 Actor 或 UI。

## 3. 行为树形状

第一版的行为树只做一个确定性行动：

```text
Root
└─ Selector
   ├─ Sequence: 对局结束
   │  ├─ Decorator: bMatchEnded == true
   │  └─ Task: StopBrain
   │
   ├─ Selector: 回合分发器 [Service: UpdateTurnContext]
   │  ├─ Sequence: 当前是本 NPC 阵营回合
   │  │  ├─ Decorator: bGameplayReady && bIsMyTurn
   │  │  ├─ Task: SelectDeterministicAction
   │  │  ├─ Task: ExecuteValidatedAction
   │  │  └─ Task: ClearTurnState
   │  └─ Task: WaitForTurn
```

UE 行为树是事件驱动的。条件应尽量放在 Decorator，周期性刷新轻量状态放在 Service，实际工作放在 Task。`WaitForTurn` 不应每帧轮询 Gameplay；应使用低频 Service、Gameplay 回合变化事件，或两者结合唤醒 Blackboard 条件。

## 4. Blackboard 与运行时上下文

Blackboard 只保存轻量、可观察、适合分支判断的数据。复杂的行动数组、风险详情和异步请求状态应放在独立的 `UTerraNpcDecisionContext`（或等价的 Subsystem 管理对象）中。

建议第一版 Blackboard 键：

| Key | 类型 | 含义 |
| --- | --- | --- |
| `ControlledFactionId` | Int | 此 Brain 控制的阵营 ID |
| `CurrentFactionId` | Int | Gameplay 当前回合阵营 |
| `TurnIndex` | Int | 当前回合序号 |
| `bGameplayReady` | Bool | Gameplay Bridge 可用 |
| `bMatchEnded` | Bool | 对局是否结束 |
| `bIsMyTurn` | Bool | 当前是否轮到本 Brain |
| `SelectedPieceId` | Int | 已选定的行动棋子 ID |
| `SelectedToCellId` | Int | 已选定的目标格 ID |
| `DecisionSource` | Enum / Int | Deterministic、LLM、Fallback |
| `LastError` | String | 最近一次失败原因，仅用于调试 |

后续 LLM 阶段额外增加：

| Key | 类型 | 含义 |
| --- | --- | --- |
| `bUseLlm` | Bool | 本局/本阵营是否启用 LLM 建议 |
| `LlmStatus` | Enum / Int | Idle、Pending、Accepted、TimedOut、Rejected |
| `DecisionRequestId` | String | 异步请求关联 ID |

`SelectedPieceId`、`SelectedToCellId` 与 `TurnIndex`、`CurrentFactionId` 一起构成执行快照。每次执行仍必须调用 `TryExecuteValidatedAction(ExpectedTurnIndex, ExpectedFactionId, ...)`，不能因为 Blackboard 已记录过而省略 Gameplay 的二次校验。

## 5. 渐进验收路线

### BT1：无 LLM 的行为树最小闭环

目标：非玩家阵营回合能由行为树执行一次合法行动并正常推进回合。

状态：已进入 C++ 与编辑器资产配置阶段。详细落地与验收见 [BT1_NpcFactionBehaviorTreeMinimalLoopDesign.md](BT1_NpcFactionBehaviorTreeMinimalLoopDesign.md)。

范围：

- 新增阵营级 Brain Pawn、AIController、Blackboard 与 Behavior Tree 资产。
- `UpdateTurnContext` Service 刷新当前阵营、回合、对局结束状态。
- `SelectDeterministicAction` Task 调用 `CollectCurrentFactionLegalActions`。
- 用稳定规则选择一个候选行动。
- `ExecuteValidatedAction` Task 调用 Gameplay Bridge，再由 `TryExecuteValidatedAction` 最终执行。
- 失败时记录 `LastError` 并回到等待状态；不得卡死玩家回合。

第一版确定性选择规则应刻意简单、稳定、可复现：

```text
合法行动按以下顺序排序：
1. capture_count 从大到小
2. bIsJump 为 true 优先
3. piece_id 从小到大
4. to_cell_id 从小到大
取第一项
```

此阶段不调用 `EvaluateCurrentFactionActionRisk`，以便验收只聚焦于“行为树能正确编排并执行合法行动”。

验收标准：

1. 在至少一个 AI 阵营回合，行为树调试器显示该阵营进入决策分支。
2. `SelectedPieceId` 与 `SelectedToCellId` 对应 `CollectCurrentFactionLegalActions` 返回的一项。
3. `TryExecuteValidatedAction` 返回 `bExecuted=true`，并且回合序号推进。
4. 人类玩家回合不执行 NPC 行动。
5. 对局结束后 Brain 不再发起执行。
6. 没有合法行动或执行被拒绝时，日志含原因，且游戏不会无限重试或阻塞。

建议日志前缀：

```text
LogTerraNpcBT: Faction=7 Turn=10 Decision=Deterministic Piece=31 To=428
LogTerraNpcBT: Faction=7 Turn=10 Execute=Succeeded TurnAfter=11
```

### BT2：行为树内的确定性策略分支

目标：在不引入 LLM 的情况下，让行为树根据玩法信息选择不同策略分支。

范围：

- 增加 `EvaluateCurrentFactionActionRisk` 的批量/有限候选调用。
- 增加如“安全吃子优先”“危险规避”“无吃子则推进”的选择策略。
- 以 Decorator / Selector 表达明确策略优先级。
- 为不同 NPC 阵营提供可配置策略档案，例如保守、均衡、进攻。

验收标准：固定相同棋盘状态下，策略档案得到可预期、可复盘的不同合法行动；任何分支仍必须经 `TryExecuteValidatedAction` 执行。

### BT3：行为树与 MCP 的观测耦合

目标：复用现有 `NpcMcp` 的日志和调试能力，但不让 MCP 成为 UE 内部执行依赖。

范围：

- MCP 的 `terra.get_turn_context`、`terra.list_legal_actions` 继续映射同一份 Gameplay 权威状态。
- MCP 输出可附带当前 Brain 的轻量诊断：阵营 ID、决策来源、状态、最后错误；只读。
- 行为树执行路径仍直接调用 Gameplay Bridge，不通过 `terra.execute_validated_action` HTTP 自调用。
- 统一 decision log 的字段，使外部工具、BT 调试和 Gameplay 执行结果可按 `turn_index + faction_id` 对齐。

验收标准：外部 MCP 查询结果与 BT Blackboard / UE 日志中的当前回合和最终执行结果一致。

### BT4：LLM 提案分支与确定性回退

目标：在 BT1-BT3 稳定后，让 LLM 只参与“从合法行动中提出建议”，不接管规则和最终执行。

建议行为树扩展为：

```text
本 NPC 回合
  -> CaptureTurnContext
  -> Selector
       ├─ Sequence: bUseLlm
       │  ├─ StartLlmDecision
       │  ├─ WaitLlmOrTimeout
       │  └─ ValidateLlmProposal
       └─ SelectDeterministicFallback
  -> ExecuteValidatedAction
  -> ClearTurnState
```

边界：

- 外部 Agent 通过现有 MCP tools 查询状态、提交 proposal。
- 行为树只等待已关联的请求结果，不把 API key 或模型 SDK 放入 UE。
- LLM proposal 必须含 `turn_index`、`faction_id`、`piece_id`、`to_cell_id`。
- proposal 过期、非法、超时或外部连接失败时，行为树立刻改走确定性 fallback。
- 行为树最终仍直接调用 Gameplay 的 `TryExecuteValidatedAction`；MCP 的 `terra.execute_validated_action` 仅保留外部联调/兼容路径，不能成为同进程 BT 的必经路径。

验收标准：禁用或断开外部 LLM Agent 时，所有 NPC 阵营仍能完成回合；开启 LLM 时，日志能明确标明提案被采用或为何回退。

## 6. 与现有 A1-A3 MCP 设计的关系

| 现有能力 | BT 中的定位 |
| --- | --- |
| Runtime MCP Server | 外部观察和后续 LLM Agent 的通信边界，不参与 BT1 执行 |
| `terra.get_turn_context` | 外部 Agent 读取回合；BT3 可加入 Brain 诊断字段 |
| `terra.list_legal_actions` | 与 BT Task 共用同一 Gameplay 合法行动来源 |
| `terra.evaluate_action_risk` | BT2 的策略输入，也可提供给外部 LLM 排序 |
| `terra.submit_action_proposal` | BT4 的 LLM 提案验证入口 |
| `terra.execute_validated_action` | 外部联调可用；BT 内部不经 HTTP 调它 |

A3 目前的外部 Agent 轮询模式可继续存在，但在 BT4 以后应避免它和行为树同时执行同一回合。建议明确唯一编排者：

- A3 联调模式：外部 Agent 可以执行，UE 不启用对应阵营的 BT 执行分支。
- BT 正式模式：UE 行为树负责决定何时执行；外部 Agent 只提交已关联请求的建议。

任何时刻，一个 `(TurnIndex, FactionId)` 只能有一个执行者。`TryExecuteValidatedAction` 的快照校验是最后防线，但不能把重复触发当作正常流程。

## 7. 后续玩法的接入方式

中立单位、可变地形、掉落物、拾取、兵种升变和积分奖励都应先在 Gameplay 层形成四类权威能力：

1. 状态：例如中立归属、地形效果、掉落物位置、经验和积分。
2. 合法行动：移动、攻击、拾取、使用、升变等候选动作。
3. 结果预览/评分：行动的即时收益、风险、奖励和可解释摘要。
4. 最终执行：带快照校验的规则执行 API。

完成后，行为树增加对应的分支和优先级；LLM 只消费这些结构化信息并在合法候选间给出偏好。例如：

```text
紧急防御
  -> 可立即获胜
  -> 安全拾取高价值掉落物
  -> 可用升变
  -> 争夺地形/积分目标
  -> 普通移动
```

不要为了让 LLM “理解玩法”先改 prompt。若玩法尚未具有合法行动与权威结算，LLM 的文字判断无法变成可靠的游戏行为。

## 8. 预计 C++ 与资产边界（后续落地时再实施）

建议将 BT 运行逻辑放在游戏 Runtime 模块或独立 Runtime 模块中，而不是塞进 `NpcMcp`：

```text
Source/TerraCivilization 或新 Runtime 模块
  Public/AI/TerraNpcBrainPawn.h
  Public/AI/TerraNpcAIController.h
  Public/AI/BTService_UpdateTerraTurnContext.h
  Public/AI/BTTask_SelectDeterministicAction.h
  Public/AI/BTTask_ExecuteTerraValidatedAction.h
  Private/AI/...

Content/AI/NPC
  BB_TerraFactionNpc.uasset
  BT_TerraFactionNpc.uasset
```

所选模块需加入：

```cpp
"AIModule",
"GameplayTasks"
```

只有未来出现真实 Pawn 移动、导航或环境寻路时，再评估是否需要 `NavigationSystem`。本项目当前是棋盘行动，不应因为使用行为树而引入 NavMesh 依赖。

`NpcMcp` 继续维持 Runtime MCP 工具白名单和外部通信职责；它不应拥有 Brain 的生命周期，也不应反向控制行为树。

## 9. 风险与排错

| 症状 | 可能原因 | 处理方式 |
| --- | --- | --- |
| NPC 在玩家回合行动 | `bIsMyTurn` 刷新滞后或没有检查当前阵营 | 每次 Task 前读取 Blackboard，并由 Gameplay 执行 API 再校验 `ExpectedFactionId` |
| 同一 NPC 回合执行两次 | BT 和外部 Agent 同时具备执行权 | 采用单一编排者；用 `(TurnIndex, FactionId)` 去重 |
| LLM 超时导致游戏卡住 | Task 长时间等待外部响应 | BT4 使用有限超时并无条件转 fallback |
| 行为树选择了非法行动 | 缓存候选跨回合失效 | 以完整快照调用 `TryExecuteValidatedAction`，拒绝后重新等待/回退 |
| 运行时依赖 Editor 工具集 | 误用 AI Toolset Registry | 只依赖 `AIModule` 的 Runtime 行为树能力；MCP 保持自定义 Runtime tools |
| Blackboard 塞入大量行动数组 | 状态难追踪、资产键类型受限 | Blackboard 只存 ID/状态；复杂数据放 Decision Context |

## 10. 本阶段不做什么

本设计稿不在总稿中展开实现细节：

- BT1 的 C++ 行为树节点、Pawn、AIController 或任何 `.uasset` 的编辑器配置；详见 BT1 独立设计稿。
- 每个棋子的独立 AI。
- NavMesh、路径寻找或移动到世界坐标。
- LLM 长期记忆、外交、自然语言角色扮演。
- 把 API key、模型 SDK 或外部网络请求嵌入 UE。
- 更改现有 MCP tool 的执行语义。

BT1 以“一个 NPC 阵营在非玩家回合做一次合法行动”为唯一验收目标；BT2-BT4 仍保持未开始状态。
