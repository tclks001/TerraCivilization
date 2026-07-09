# A3 LLM Agent 与 MCP 连接操作手册

日期：2026-07-09

## 目标

本文是 `A3ExternalLlmAgentNpcExecutionLoopDesign.md` 的配套操作手册，专门解释外部 LLM Agent 如何连接 UE Runtime MCP server，并一步步调用工具完成 NPC 行动。

本文不改 UE C++ 逻辑。当前前提是：

1. UE 已能启动 MCP server。
2. MCP Inspector 中手动调用 `terra.execute_validated_action` 已经跑通。
3. A1/A2/A3 的 UE 端工具已经可用。

## 先理解 Agent 在做什么

Inspector 是“人手动点工具”的 MCP client。

Agent 是“程序自动点工具”的 MCP client。

LLM Agent 则是：

```text
程序连接 MCP
  -> 程序拿到 UE 暴露的 tools
  -> 程序把局势和工具结果交给 LLM
  -> LLM 选择行动
  -> 程序把行动提交回 UE
  -> UE 校验并执行
```

关键点：

- LLM 不直接连接 UE 内存。
- LLM 不直接改 Gameplay。
- 所有规则判断仍由 UE tools 返回。
- 最终执行必须走 `terra.execute_validated_action`。

## 当前 UE MCP Tools

A3 需要用到这些工具：

```text
terra.get_turn_context
terra.list_legal_actions
terra.evaluate_action_risk
terra.submit_action_proposal
terra.execute_validated_action
```

推荐调用顺序：

```text
1. get_turn_context
2. list_legal_actions
3. evaluate_action_risk
4. submit_action_proposal
5. execute_validated_action
```

## 启动 UE MCP Server

启动 UE 时带参数：

```text
-TerraNpcMcpStartServer -TerraNpcMcpPort=8765 -TerraNpcMcpPath=/terra-npc-mcp
```

MCP endpoint：

```text
http://127.0.0.1:8765/terra-npc-mcp
```

如果 Inspector 可以连接并执行 `terra.execute_validated_action`，说明 UE 端 MCP 已经可用。

## 推荐分三步做 Agent

不要一开始就接 LLM。建议按三个小阶段推进：

```text
Step 0: Deterministic Agent，不用 LLM，只用脚本自动调用 MCP。
Step B: LLM 只做选择，脚本负责 MCP 调用和执行。
Step C: LLM 自己通过 MCP tool calling 调工具，脚本只做宿主和安全执行。
```

A3 当前最推荐先做 Step 0 和 Step B。

## Step 0：无 LLM 的 Deterministic Agent

### 目的

先证明“外部程序自动调用 MCP 并执行行动”可以跑通。

这一步不需要 API key，不需要模型。

### 逻辑

```text
connect MCP
call terra.get_turn_context
call terra.list_legal_actions
choose one action
call terra.submit_action_proposal
if accepted:
    call terra.execute_validated_action
```

### 选择行动的简单规则

第一版可以这样选：

```text
1. 优先 capture_count 最大的 action
2. 如果并列，优先 is_jump=false
3. 如果仍并列，按 piece_id、to_cell_id 排序取第一个
```

或者更简单：

```text
直接取 actions[0]
```

### 需要保存的状态

Agent 要记录最近处理过的回合，避免重复执行：

```json
{
  "last_turn_index": 10,
  "last_faction_id": 7
}
```

如果下一次 `get_turn_context` 返回同样的 `(turn_index, current_faction_id)`，不要重复执行。

### Step 0 验收

1. UE 启动 MCP server。
2. Agent 脚本启动。
3. Agent 自动调用 `list_legal_actions`。
4. Agent 自动执行一个合法行动。
5. UE 中棋子移动，回合推进。
6. Agent 打印 execution result。

## Step B：LLM 只做选择

这是 A3 推荐的第一种 LLM 接入方式。

### 思路

脚本仍然负责连接 MCP 和调用 tools。

LLM 不直接调用 MCP tools。LLM 只看脚本整理好的 JSON，然后输出一个选择：

```json
{
  "piece_id": 31,
  "to_cell_id": 428,
  "reason": "capture_count=1 and destination_threatened=false"
}
```

脚本拿到这个选择后：

```text
call submit_action_proposal
if accepted:
    call execute_validated_action
else:
    fallback
```

### 为什么先做方案 B

方案 B 更容易调试：

- MCP 连接问题在脚本层处理。
- LLM 输出格式简单。
- LLM 不能随意调用执行工具。
- 执行前脚本可以再校验一次。
- 出错后 fallback 简单。

### 方案 B 流程

```text
Agent:
  1. call terra.get_turn_context
  2. call terra.list_legal_actions
  3. pick top N candidate actions, for example N=5
  4. call terra.evaluate_action_risk for each candidate
  5. send compact JSON to LLM

LLM:
  6. choose one action
  7. return strict JSON

Agent:
  8. parse JSON
  9. call terra.submit_action_proposal
  10. if accepted, call terra.execute_validated_action
  11. if rejected or timeout, fallback
```

### 发给 LLM 的输入模板

可以把工具结果压缩成：

```json
{
  "task": "choose_one_legal_action",
  "turn": {
    "turn_index": 10,
    "current_faction_id": 7
  },
  "candidate_actions": [
    {
      "piece_id": 31,
      "from_cell_id": 421,
      "to_cell_id": 428,
      "is_jump": false,
      "capture_count": 1,
      "destination_threatened": false,
      "threat_count": 0
    },
    {
      "piece_id": 32,
      "from_cell_id": 430,
      "to_cell_id": 431,
      "is_jump": true,
      "capture_count": 0,
      "destination_threatened": true,
      "threat_count": 2
    }
  ],
  "output_schema": {
    "piece_id": "integer",
    "to_cell_id": "integer",
    "reason": "string"
  }
}
```

### System Prompt 示例

```text
你是一个战棋 NPC 决策器。

你只能从 candidate_actions 中选择一个行动。
不要发明 piece_id。
不要发明 to_cell_id。
优先选择 capture_count 更高的行动。
如果能吃子但 destination_threatened=true，需要谨慎。
如果没有吃子，选择 destination_threatened=false 的行动。

只输出 JSON：
{
  "piece_id": number,
  "to_cell_id": number,
  "reason": string
}
```

### 方案 B 的伪代码

```text
context = mcp.call("terra.get_turn_context", {})
actions = mcp.call("terra.list_legal_actions", {})

candidates = choose_top_actions(actions, 5)
for candidate in candidates:
    risk = mcp.call("terra.evaluate_action_risk", {
        "piece_id": candidate.piece_id,
        "to_cell_id": candidate.to_cell_id
    })
    candidate.risk = risk

llm_result = llm.generate_json(system_prompt, candidates)

proposal = mcp.call("terra.submit_action_proposal", {
    "piece_id": llm_result.piece_id,
    "to_cell_id": llm_result.to_cell_id
})

if proposal.accepted:
    execution = mcp.call("terra.execute_validated_action", {
        "expected_turn_index": context.turn_index,
        "expected_faction_id": context.current_faction_id,
        "piece_id": llm_result.piece_id,
        "to_cell_id": llm_result.to_cell_id
    })
else:
    execution = fallback_execute()
```

### 方案 B 验收

1. Agent 打印 `get_turn_context` 结果。
2. Agent 打印候选 actions。
3. Agent 打印 LLM 返回 JSON。
4. `submit_action_proposal.accepted=true`。
5. `execute_validated_action.executed=true`。
6. UE 中棋子移动，回合推进。
7. 如果 LLM 返回非法 action，fallback 仍能执行。

## Step C：LLM 使用 MCP Tool Calling

这是更接近最终设想的方式。

### 思路

LLM 不再只看脚本整理后的 JSON，而是由 Agent 把 MCP tools 暴露给 LLM。

LLM 可以自己决定调用：

```text
terra.get_turn_context
terra.list_legal_actions
terra.evaluate_action_risk
terra.submit_action_proposal
```

但建议仍然不要让 LLM 直接调用：

```text
terra.execute_validated_action
```

更安全的边界是：

```text
LLM submit proposal
Agent execute accepted proposal
```

### 方案 C 架构

```text
Agent connects to UE MCP server
Agent lists tools
Agent exposes selected tools to LLM
LLM calls read/query tools
LLM calls submit_action_proposal
Agent inspects accepted proposal
Agent calls execute_validated_action
```

### 方案 C 允许暴露给 LLM 的 tools

建议暴露：

```text
terra.get_turn_context
terra.list_legal_actions
terra.evaluate_action_risk
terra.submit_action_proposal
```

不建议直接暴露：

```text
terra.execute_validated_action
```

原因：

- execute 是写操作。
- execute 会推进回合。
- 最后执行应由 agent 做二次确认。

### 方案 C 的 LLM 指令模板

```text
你是一个战棋 NPC 决策器。

你必须按顺序使用工具：
1. 调用 terra.get_turn_context。
2. 调用 terra.list_legal_actions。
3. 从合法行动中选择最多 5 个候选。
4. 对候选调用 terra.evaluate_action_risk。
5. 选择一个行动。
6. 调用 terra.submit_action_proposal。

规则：
- 不要自己推导合法行动。
- 不要发明工具返回中不存在的 piece_id 或 to_cell_id。
- 如果 submit_action_proposal 返回 accepted=false，重新选择一次。
- 最多重试一次。
- 不要请求执行行动，只提交 proposal。
```

### 方案 C 的 Agent 职责

方案 C 里 Agent 仍然要负责：

1. MCP 连接。
2. LLM 会话。
3. tool call 转发。
4. 限制可用 tool 白名单。
5. 超时控制。
6. 解析最终 proposal。
7. 调用 `terra.execute_validated_action`。
8. fallback。
9. decision log。

### 方案 C 伪代码

```text
mcp_tools = mcp.list_tools()
llm_tools = allow_only([
    "terra.get_turn_context",
    "terra.list_legal_actions",
    "terra.evaluate_action_risk",
    "terra.submit_action_proposal"
])

conversation = start_llm_session(system_prompt, llm_tools)

while conversation.needs_tool_call:
    tool_call = conversation.next_tool_call
    tool_result = mcp.call(tool_call.name, tool_call.arguments)
    conversation.send_tool_result(tool_result)

proposal = conversation.final_proposal

if proposal.accepted:
    context = last_seen_context
    execution = mcp.call("terra.execute_validated_action", {
        "expected_turn_index": context.turn_index,
        "expected_faction_id": context.current_faction_id,
        "piece_id": proposal.piece_id,
        "to_cell_id": proposal.to_cell_id
    })
else:
    execution = fallback_execute()
```

## MCP 连接层需要做什么

无论方案 B 还是 C，Agent 都需要一个 MCP client 层。

这个 client 层负责：

```text
connect http://127.0.0.1:8765/terra-npc-mcp
initialize session
tools/list
tools/call
handle JSON result
handle timeout
handle error
```

建议优先使用现成 MCP SDK，而不是自己拼 HTTP JSON-RPC。

如果暂时没有 SDK，也可以先用 MCP Inspector 验证工具，再做脚本层。

## Agent 的目录建议

可以放在项目外部或项目内 `Tools` 目录，例如：

```text
Tools/
  NpcAgent/
    package.json
    src/
      index.ts
      mcpClient.ts
      deterministicPolicy.ts
      llmPolicy.ts
      decisionLog.ts
```

或者 Python：

```text
Tools/
  NpcAgent/
    pyproject.toml
    npc_agent/
      main.py
      mcp_client.py
      deterministic_policy.py
      llm_policy.py
      decision_log.py
```

## Decision Log

无论 B 还是 C，都建议 Agent 写 JSONL：

```json
{
  "timestamp": "2026-07-09T10:00:00Z",
  "mode": "llm_choice",
  "turn_index": 10,
  "faction_id": 7,
  "tool_calls": [
    "terra.get_turn_context",
    "terra.list_legal_actions",
    "terra.evaluate_action_risk",
    "terra.submit_action_proposal",
    "terra.execute_validated_action"
  ],
  "llm_choice": {
    "piece_id": 31,
    "to_cell_id": 428,
    "reason": "capture_count=1 and destination_threatened=false"
  },
  "accepted": true,
  "executed": true,
  "fallback": false,
  "reject_reason": ""
}
```

日志的作用：

- 复盘 NPC 为什么这样走。
- 判断是 LLM 选错、MCP tool 错、还是 Gameplay validator 拒绝。
- 后续调 prompt 和策略。

## Timeout 与 Fallback

建议第一版预算：

```text
MCP tool call timeout: 1s
LLM call timeout: 5s
single turn total budget: 8s
LLM retry: 1
```

fallback 策略：

```text
1. call terra.list_legal_actions
2. choose action with max capture_count
3. if tie, choose destination_threatened=false
4. if still tie, sort by piece_id then to_cell_id
5. call terra.execute_validated_action
```

fallback 必须不依赖 LLM。

## 最小可操作路线

推荐你按这个顺序做：

```text
1. Inspector 手动执行 execute_validated_action。已完成。
2. 写 deterministic Agent，不接 LLM。
3. deterministic Agent 自动执行一回合。
4. 加 decision log。
5. 接方案 B：LLM 只选择 action。
6. 确认非法 LLM 输出会 fallback。
7. 再考虑方案 C：LLM 使用 MCP tool calling。
```

不要跳过第 2 步。它能把 MCP 连接、工具调用、执行和日志先打通，后面接 LLM 时问题会少很多。
