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
