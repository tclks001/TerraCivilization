# Terra NPC Agent

无 LLM 的外部 Agent。它是一个 MCP client，会自动连接 UE Runtime MCP server，并执行当前阵营的第一个合法行动。

## 前提

先启动 UE，并带上 MCP 参数：

```text
-TerraNpcMcpStartServer -TerraNpcMcpPort=8765 -TerraNpcMcpPath=/terra-npc-mcp
```

确认 MCP Inspector 能连接：

```text
http://127.0.0.1:8765/terra-npc-mcp
```

## 运行

在项目根目录执行：

```powershell
cd C:\workspace\TerraCivilization\Tools\NpcAgent
npm run run-once
```

也可以指定 MCP URL：

```powershell
node .\src\run-once.js --url http://127.0.0.1:8765/terra-npc-mcp
```

## 行为

脚本会：

1. `initialize` MCP session。
2. 发送 `notifications/initialized`。
3. `tools/list` 确认工具可用。
4. 调 `terra.get_turn_context`。
5. 调 `terra.list_legal_actions`。
6. 取 `actions[0]` 的 `piece_id` 和 `to_cell_id`。
7. 调 `terra.submit_action_proposal`。
8. 如果 `accepted=true`，调 `terra.execute_validated_action`。
9. 打印执行结果。

当前没有策略，也不接 LLM。

## Step B：LLM 只做选择

`run-llm-choice.js` 会继续由脚本调用 MCP tools，但把候选行动交给 LLM 选择。LLM 只返回 `piece_id`、`to_cell_id` 和 `reason`，最终的 `submit_action_proposal` 和 `execute_validated_action` 仍由 Agent 脚本执行。

配置 API key：

```powershell
$env:TERRA_NPC_LLM_API_KEY="你的 API key"
```

可选配置：

```powershell
$env:TERRA_NPC_LLM_MODEL="gpt-4.1-mini"
$env:TERRA_NPC_LLM_BASE_URL="https://api.openai.com/v1"
$env:TERRA_NPC_CANDIDATE_LIMIT="5"
```

运行：

```powershell
cd C:\workspace\TerraCivilization\Tools\NpcAgent
npm run run-llm-choice
```

也可以直接指定参数：

```powershell
node .\src\run-llm-choice.js --url http://127.0.0.1:8765/terra-npc-mcp --llm-model gpt-4.1-mini --candidate-limit 5
```

只检查发给 LLM 的 payload、不真正调用模型：

```powershell
node .\src\run-llm-choice.js --dry-run-llm
```

如果 LLM 调用失败、返回非法 JSON，或者选择了候选列表外的行动，脚本会 fallback 到确定性策略。

## 基础工具 LLM Agent

`run-llm-basic-tools.js` 是一个真正的基础工具循环 Agent。

它和 `run-llm-choice.js` 的区别是：

- `run-llm-choice.js`：脚本先把候选行动整理好，再让 LLM 只做“选哪个行动”。
- `run-llm-basic-tools.js`：LLM 自己决定下一步调用哪个基础工具。

当前它只允许使用这些基础工具：

- `terra.get_turn_context`
- `terra.list_legal_actions`
- `terra.evaluate_action_risk`
- `terra.submit_action_proposal`
- `terra.execute_validated_action`

并且每一次 LLM 想调用工具时，都必须同时输出：

- `tool_name`
- `arguments`
- `purpose`

脚本会把这个 `purpose` 打印到控制台，然后再实际调用 MCP tool。

### 配置

```powershell
$env:TERRA_NPC_LLM_API_KEY="你的 API key"
$env:TERRA_NPC_LLM_MODEL="gpt-4.1-mini"
$env:TERRA_NPC_LLM_BASE_URL="https://api.openai.com/v1"
```

可选：

```powershell
$env:TERRA_NPC_LLM_MAX_STEPS="12"
```

### 运行

```powershell
cd C:\workspace\TerraCivilization\Tools\NpcAgent
npm run run-llm-basic-tools
```

或者：

```powershell
node .\src\run-llm-basic-tools.js --url http://127.0.0.1:8765/terra-npc-mcp --llm-model gpt-4.1-mini
```

只看第一次发给 LLM 的请求 payload，不真正调用模型：

```powershell
node .\src\run-llm-basic-tools.js --dry-run-llm
```

### 行为

脚本会：

1. `initialize` MCP session。
2. `tools/list` 验证基础工具存在。
3. 把当前可用基础工具 schema、游戏规则摘要、以及已发生的工具调用历史发给 LLM。
4. LLM 返回下一步：
   - 调用哪个 tool
   - 传什么参数
   - 调用这个 tool 的目的是什么
5. 脚本打印 `purpose`，再实际调用 MCP tool。
6. 把 tool 结果追加进 history，再继续问 LLM。
7. 当 LLM 调用 `terra.execute_validated_action` 后，本回合结束。

这个版本的目标不是强策略，而是先测试：

- 仅靠当前基础工具，LLM 能否完整走完一个回合。
- LLM 的工具调用顺序是否合理。
- `purpose` 日志是否足够便于人类观看和复盘。

## 交互工具 LLM Agent

`run-llm-interactive-tools.js` 用来测试另一种工具面：

- 不给 LLM `terra.get_turn_context`
- 不给 LLM `terra.list_legal_actions`
- 不给 LLM `terra.evaluate_action_risk`
- 不给 LLM `terra.submit_action_proposal`
- 不给 LLM `terra.execute_validated_action`

它只把当前 phase 实际暴露出来的 `terra.ui_*` 工具提供给 LLM：

- `terra.ui_begin_turn_review`
- `terra.ui_select_piece`
- `terra.ui_preview_move`
- `terra.ui_cancel_selection`
- `terra.ui_confirm_action`

并且是动态的：

- 每一步调用前都会重新 `tools/list`
- 只把当前可见的 `ui_` 工具发给 LLM
- LLM 不能调用当前 phase 不可用的 tool

这版的目的不是直接提高胜率，而是观察：

- LLM 是否会先“观察局势”再行动
- LLM 是否会通过 select / preview / cancel 形成更接近真人点击式的思考过程
- 交互式工具面是否比基础工具面更容易让 LLM 表现出“在玩游戏”

### 运行

```powershell
cd C:\workspace\TerraCivilization\Tools\NpcAgent
npm run run-llm-interactive-tools
```

或者：

```powershell
node .\src\run-llm-interactive-tools.js --url http://127.0.0.1:8765/terra-npc-mcp --llm-model gpt-4.1-mini
```

只看第一次发给 LLM 的请求 payload：

```powershell
node .\src\run-llm-interactive-tools.js --dry-run-llm
```

### 行为

脚本会：

1. `initialize` MCP session。
2. 每一步先 `tools/list`。
3. 过滤出当前 phase 实际可见的 `terra.ui_*` 工具。
4. 把这些工具、最近的 `interaction_state`、之前的工具调用历史发给 LLM。
5. LLM 返回：
   - 下一步调用哪个 `ui_` tool
   - 传什么参数
   - 调用这个 tool 的目的是什么
6. 脚本打印 `purpose`，再实际调用该 tool。
7. 用 tool 返回的 `interaction_state` 覆盖本地缓存。
8. 如果调用了 `terra.ui_confirm_action` 且执行成功，则本回合结束。

### 和基础工具版的区别

`run-llm-basic-tools.js` 更像“查询表格后一次性提交动作”。

`run-llm-interactive-tools.js` 更像“通过点击逐步观察局势并确认动作”。

你现在可以把两个脚本的日志并排看，比较：

- 是否出现 inspect -> compare -> confirm 的节奏
- 是否会多次 select / preview / cancel
- `purpose` 是否更像一个正在看盘面的玩家

## 交互工具 + 结构化思考 Agent

`run-llm-interactive-think.js` 仍然只使用当前 `terra.ui_*` 交互工具，但它会强制 LLM 在每一步 tool call 之前先输出更完整的结构化思考。

相比 `run-llm-interactive-tools.js`，它新增要求 LLM 输出：

- `turn_goal`
- `tactical_focus`
- `current_hypothesis`
- `evidence_summary`
- `why_not_previous_option`
- `purpose`

然后才允许调用：

- `terra.ui_begin_turn_review`
- `terra.ui_select_piece`
- `terra.ui_preview_move`
- `terra.ui_cancel_selection`
- `terra.ui_confirm_action`

这个版本的目标不是立刻提高胜率，而是让你看清楚：

- 它每回合到底想达成什么目标
- 它为什么先看某个 piece
- 它为什么放弃上一个 preview
- 它最后确认某步时，依据到底是什么

当前默认最大步骤数更高，目的是给 LLM 更多 inspect / compare 空间，避免因为探索轮数不够过早停下。

### 运行

```powershell
cd C:\workspace\TerraCivilization\Tools\NpcAgent
npm run run-llm-interactive-think
```

或者：

```powershell
node .\src\run-llm-interactive-think.js --url http://127.0.0.1:8765/terra-npc-mcp --llm-model gpt-4.1-mini
```

只看第一次发给 LLM 的请求 payload：

```powershell
node .\src\run-llm-interactive-think.js --dry-run-llm
```

### 你应该重点观察什么

建议重点看以下几列日志：

- `Turn Goal`
- `Tactical Focus`
- `Hypothesis`
- `Evidence`
- `Why Not Previous`
- `Purpose`

如果这些字段开始稳定出现某些模式，例如：

- 总是先抢树林
- 总是优先看弓兵
- 总是以“安全无威胁”为唯一依据
- 经常提到“为下回合远程吃子做准备”

那就说明下一步该沉淀的 deterministic tool 已经浮出来了。

### 它和上一版的关键区别

`run-llm-interactive-tools.js` 主要暴露“它在调用什么工具”。

`run-llm-interactive-think.js` 主要暴露“它自称为什么这样调用工具”。

并且这版要求：只有 `terra.ui_confirm_action` 成功后，才算本回合真正完成。

## L1：Interactive Review 验收脚本

`run-interactive-review-once.js` 用于验收 L1 Interactive Tactical Review Tools 最小闭环。

它会按当前 Gameplay phase 驱动 MCP：

1. `terra.ui_begin_turn_review`
2. `terra.ui_select_piece`
3. `terra.ui_preview_move`
4. `terra.ui_confirm_action`

并且在每一步前重新调用 `tools/list`，检查当前 phase 实际暴露的工具集合。

运行：

```powershell
cd C:\workspace\TerraCivilization\Tools\NpcAgent
npm run run-interactive-review-once
```

或：

```powershell
node .\src\run-interactive-review-once.js --url http://127.0.0.1:8765/terra-npc-mcp
```

如果某个工具当前 phase 不可用，脚本会直接报错并退出。

## 常见问题

如果连接失败，检查 UE 是否带了：

```text
-TerraNpcMcpStartServer
```

如果工具列表为空，检查 UE 日志中是否有：

```text
[NpcMcp] Register tool terra.execute_validated_action: ok
```

如果 `actions` 为空，确认当前 Gameplay 容器已经初始化，并且当前交互状态是 Idle。
