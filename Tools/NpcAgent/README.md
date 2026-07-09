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
