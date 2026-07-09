#!/usr/bin/env node

const DEFAULT_MCP_URL = "http://127.0.0.1:8765/terra-npc-mcp";
const DEFAULT_LLM_BASE_URL = "https://api.openai.com/v1";
const DEFAULT_LLM_MODEL = "gpt-4.1-mini";
const DEFAULT_CANDIDATE_LIMIT = 5;
const PROTOCOL_VERSION = "2025-06-18";

function parseArgs(argv) {
  const args = {
    url: process.env.TERRA_NPC_MCP_URL || DEFAULT_MCP_URL,
    llmBaseUrl: process.env.TERRA_NPC_LLM_BASE_URL || process.env.OPENAI_BASE_URL || DEFAULT_LLM_BASE_URL,
    llmApiKey: process.env.TERRA_NPC_LLM_API_KEY || process.env.OPENAI_API_KEY || "",
    llmModel: process.env.TERRA_NPC_LLM_MODEL || process.env.OPENAI_MODEL || DEFAULT_LLM_MODEL,
    candidateLimit: parsePositiveInt(process.env.TERRA_NPC_CANDIDATE_LIMIT, DEFAULT_CANDIDATE_LIMIT),
    dryRunLlm: false,
  };

  for (let i = 2; i < argv.length; ++i) {
    const arg = argv[i];
    if (arg === "--url" && argv[i + 1]) {
      args.url = argv[++i];
    } else if (arg.startsWith("--url=")) {
      args.url = arg.slice("--url=".length);
    } else if (arg === "--llm-base-url" && argv[i + 1]) {
      args.llmBaseUrl = argv[++i];
    } else if (arg.startsWith("--llm-base-url=")) {
      args.llmBaseUrl = arg.slice("--llm-base-url=".length);
    } else if (arg === "--llm-model" && argv[i + 1]) {
      args.llmModel = argv[++i];
    } else if (arg.startsWith("--llm-model=")) {
      args.llmModel = arg.slice("--llm-model=".length);
    } else if (arg === "--candidate-limit" && argv[i + 1]) {
      args.candidateLimit = parsePositiveInt(argv[++i], DEFAULT_CANDIDATE_LIMIT);
    } else if (arg.startsWith("--candidate-limit=")) {
      args.candidateLimit = parsePositiveInt(arg.slice("--candidate-limit=".length), DEFAULT_CANDIDATE_LIMIT);
    } else if (arg === "--dry-run-llm") {
      args.dryRunLlm = true;
    } else if (arg === "--help" || arg === "-h") {
      args.help = true;
    }
  }

  return args;
}

function parsePositiveInt(value, fallback) {
  const parsed = Number.parseInt(value, 10);
  return Number.isInteger(parsed) && parsed > 0 ? parsed : fallback;
}

function printHelp() {
  console.log(`Usage:
  npm run run-llm-choice
  node src/run-llm-choice.js --url http://127.0.0.1:8765/terra-npc-mcp

Environment:
  TERRA_NPC_MCP_URL             Override MCP endpoint URL.
  TERRA_NPC_LLM_API_KEY         LLM API key. Falls back to OPENAI_API_KEY.
  TERRA_NPC_LLM_BASE_URL        OpenAI-compatible base URL. Default: ${DEFAULT_LLM_BASE_URL}
  TERRA_NPC_LLM_MODEL           Model name. Default: ${DEFAULT_LLM_MODEL}
  TERRA_NPC_CANDIDATE_LIMIT     Candidate actions sent to LLM. Default: ${DEFAULT_CANDIDATE_LIMIT}

Options:
  --dry-run-llm                 Print the LLM request payload and stop before calling the model.
`);
}

class McpHttpClient {
  constructor(url) {
    this.url = url;
    this.nextId = 1;
    this.sessionId = "";
  }

  async initialize() {
    const response = await this.postJson({
      jsonrpc: "2.0",
      id: this.nextId++,
      method: "initialize",
      params: {
        protocolVersion: PROTOCOL_VERSION,
        capabilities: {},
        clientInfo: {
          name: "terra-npc-agent-llm-choice",
          version: "0.2.0",
        },
      },
    }, false);

    this.sessionId = response.headers.get("mcp-session-id") || "";
    if (!this.sessionId) {
      throw new Error("MCP initialize succeeded but no Mcp-Session-Id header was returned.");
    }

    const body = await parseJsonResponse(response);
    if (body.error) {
      throw new Error(`MCP initialize failed: ${JSON.stringify(body.error)}`);
    }

    await this.postJson({
      jsonrpc: "2.0",
      method: "notifications/initialized",
      params: {},
    }, true);

    return body.result;
  }

  async listTools() {
    return this.request("tools/list", {});
  }

  async callTool(name, args = {}) {
    const result = await this.request("tools/call", {
      name,
      arguments: args,
    });

    if (result?.structuredContent) {
      return result.structuredContent;
    }

    return result;
  }

  async request(method, params) {
    const response = await this.postJson({
      jsonrpc: "2.0",
      id: this.nextId++,
      method,
      params,
    }, true);

    const body = await parseJsonResponse(response);
    if (body.error) {
      throw new Error(`MCP request ${method} failed: ${JSON.stringify(body.error)}`);
    }
    return body.result;
  }

  async postJson(payload, includeSession) {
    const headers = {
      "content-type": "application/json",
      "accept": "application/json",
    };

    if (includeSession) {
      if (!this.sessionId) {
        throw new Error("MCP session has not been initialized.");
      }
      headers["mcp-session-id"] = this.sessionId;
    }

    const response = await fetch(this.url, {
      method: "POST",
      headers,
      body: JSON.stringify(payload),
    });

    if (!response.ok) {
      const text = await response.text().catch(() => "");
      throw new Error(`HTTP ${response.status} ${response.statusText}: ${text}`);
    }

    return response;
  }
}

async function parseJsonResponse(response) {
  const text = await response.text();
  if (!text.trim()) {
    return {};
  }
  if (text.trimStart().startsWith("event:") || text.includes("\ndata:")) {
    return parseSseJsonResponse(text);
  }
  return JSON.parse(text);
}

function parseSseJsonResponse(text) {
  const events = [];
  let eventName = "message";
  let dataLines = [];

  const flush = () => {
    if (dataLines.length > 0) {
      events.push({
        event: eventName,
        data: dataLines.join("\n"),
      });
    }
    eventName = "message";
    dataLines = [];
  };

  for (const rawLine of text.split(/\r?\n/)) {
    const line = rawLine.trimEnd();
    if (line === "") {
      flush();
      continue;
    }
    if (line.startsWith("event:")) {
      eventName = line.slice("event:".length).trim();
      continue;
    }
    if (line.startsWith("data:")) {
      dataLines.push(line.slice("data:".length).trimStart());
    }
  }
  flush();

  const messageEvent = events.find((event) => event.event === "message") || events[0];
  if (!messageEvent) {
    throw new Error(`SSE response did not contain a message event: ${text}`);
  }

  return JSON.parse(messageEvent.data);
}

function requireTool(toolsResult, toolName) {
  const tools = toolsResult?.tools || [];
  if (!tools.some((tool) => tool.name === toolName)) {
    throw new Error(`Required MCP tool is missing: ${toolName}`);
  }
}

function chooseFallbackAction(actions) {
  if (actions.length <= 0) {
    throw new Error("No legal actions are available for fallback.");
  }

  return [...actions].sort((left, right) => {
    const leftCaptures = Number(left.capture_count || 0);
    const rightCaptures = Number(right.capture_count || 0);
    if (leftCaptures !== rightCaptures) {
      return rightCaptures - leftCaptures;
    }

    const leftJump = left.is_jump ? 1 : 0;
    const rightJump = right.is_jump ? 1 : 0;
    if (leftJump !== rightJump) {
      return leftJump - rightJump;
    }

    if (left.piece_id !== right.piece_id) {
      return left.piece_id - right.piece_id;
    }

    return left.to_cell_id - right.to_cell_id;
  })[0];
}

function selectCandidateActions(actions, limit) {
  return [...actions]
    .sort((left, right) => {
      const leftCaptures = Number(left.capture_count || 0);
      const rightCaptures = Number(right.capture_count || 0);
      if (leftCaptures !== rightCaptures) {
        return rightCaptures - leftCaptures;
      }

      const leftJump = left.is_jump ? 1 : 0;
      const rightJump = right.is_jump ? 1 : 0;
      if (leftJump !== rightJump) {
        return leftJump - rightJump;
      }

      if (left.piece_id !== right.piece_id) {
        return left.piece_id - right.piece_id;
      }

      return left.to_cell_id - right.to_cell_id;
    })
    .slice(0, limit);
}

async function buildLlmCandidates(mcp, actions, limit) {
  const selected = selectCandidateActions(actions, limit);
  const candidates = [];

  for (const action of selected) {
    const risk = await mcp.callTool("terra.evaluate_action_risk", {
      piece_id: action.piece_id,
      to_cell_id: action.to_cell_id,
    });

    candidates.push({
      piece_id: action.piece_id,
      from_cell_id: action.from_cell_id,
      to_cell_id: action.to_cell_id,
      is_jump: Boolean(action.is_jump),
      capture_count: Number(action.capture_count || 0),
      destination_threatened: Boolean(risk.destination_threatened),
      threat_count: Number(risk.threat_count || 0),
      risk,
    });
  }

  return candidates;
}

function buildLlmPayload(context, candidates) {
  return {
    task: "choose_one_legal_action",
    turn: {
      turn_index: context.turn_index,
      current_faction_id: context.current_faction_id,
      interaction_phase: context.interaction_phase,
    },
    candidate_actions: candidates,
    output_schema: {
      piece_id: "integer",
      to_cell_id: "integer",
      reason: "string",
    },
  };
}

async function chooseActionWithLlm(args, payload) {
  if (!args.llmApiKey) {
    throw new Error("Missing LLM API key. Set TERRA_NPC_LLM_API_KEY or OPENAI_API_KEY.");
  }

  const systemPrompt = [
    "You are a tactical-game NPC decision policy.",
    "Choose exactly one action from candidate_actions.",
    "Do not invent piece_id or to_cell_id.",
    "Prefer higher capture_count.",
    "Avoid destination_threatened=true unless the capture value is worth the risk.",
    "Return only strict JSON with keys: piece_id, to_cell_id, reason.",
  ].join("\n");

  const requestBody = {
    model: args.llmModel,
    temperature: 0.2,
    response_format: { type: "json_object" },
    messages: [
      { role: "system", content: systemPrompt },
      { role: "user", content: JSON.stringify(payload) },
    ],
  };

  if (args.dryRunLlm) {
    console.log("[Agent] Dry-run LLM request:");
    console.log(JSON.stringify({
      endpoint: makeChatCompletionsUrl(args.llmBaseUrl),
      body: requestBody,
    }, null, 2));
    throw new Error("Dry-run complete. No action was executed.");
  }

  const response = await fetch(makeChatCompletionsUrl(args.llmBaseUrl), {
    method: "POST",
    headers: {
      "content-type": "application/json",
      "authorization": `Bearer ${args.llmApiKey}`,
    },
    body: JSON.stringify(requestBody),
  });

  if (!response.ok) {
    const text = await response.text().catch(() => "");
    throw new Error(`LLM HTTP ${response.status} ${response.statusText}: ${text}`);
  }

  const body = await response.json();
  const content = body?.choices?.[0]?.message?.content;
  if (!content) {
    throw new Error(`LLM response did not contain choices[0].message.content: ${JSON.stringify(body)}`);
  }

  const choice = JSON.parse(extractJsonObjectText(content));
  if (!Number.isInteger(choice.piece_id) || !Number.isInteger(choice.to_cell_id)) {
    throw new Error(`LLM choice must contain integer piece_id/to_cell_id: ${JSON.stringify(choice)}`);
  }

  return choice;
}

function makeChatCompletionsUrl(baseUrl) {
  return `${baseUrl.replace(/\/+$/, "")}/chat/completions`;
}

function extractJsonObjectText(text) {
  const trimmed = text.trim();
  if (trimmed.startsWith("{") && trimmed.endsWith("}")) {
    return trimmed;
  }

  const start = trimmed.indexOf("{");
  const end = trimmed.lastIndexOf("}");
  if (start >= 0 && end > start) {
    return trimmed.slice(start, end + 1);
  }

  throw new Error(`LLM did not return a JSON object: ${text}`);
}

function validateChoiceAgainstCandidates(choice, candidates) {
  return candidates.find((candidate) =>
    candidate.piece_id === choice.piece_id &&
    candidate.to_cell_id === choice.to_cell_id
  );
}

async function main() {
  const args = parseArgs(process.argv);
  if (args.help) {
    printHelp();
    return;
  }

  console.log(`[Agent] Connecting to MCP: ${args.url}`);
  const mcp = new McpHttpClient(args.url);
  const init = await mcp.initialize();
  console.log(`[Agent] MCP initialized. Protocol=${init?.protocolVersion || "unknown"} Session=${mcp.sessionId}`);

  const tools = await mcp.listTools();
  const requiredTools = [
    "terra.get_turn_context",
    "terra.list_legal_actions",
    "terra.evaluate_action_risk",
    "terra.submit_action_proposal",
    "terra.execute_validated_action",
  ];
  for (const toolName of requiredTools) {
    requireTool(tools, toolName);
  }
  console.log(`[Agent] Tools OK: ${requiredTools.join(", ")}`);

  const context = await mcp.callTool("terra.get_turn_context", {});
  if (!context.ok) {
    throw new Error(`terra.get_turn_context failed: ${JSON.stringify(context)}`);
  }
  console.log(`[Agent] Turn=${context.turn_index} Faction=${context.current_faction_id} Phase=${context.interaction_phase}`);

  const actionsResult = await mcp.callTool("terra.list_legal_actions", {});
  if (!actionsResult.ok) {
    throw new Error(`terra.list_legal_actions failed: ${JSON.stringify(actionsResult)}`);
  }

  const actions = actionsResult.actions || [];
  if (actions.length <= 0) {
    throw new Error("terra.list_legal_actions returned no legal actions.");
  }
  console.log(`[Agent] Legal actions=${actions.length}. Candidate limit=${args.candidateLimit}`);

  const candidates = await buildLlmCandidates(mcp, actions, args.candidateLimit);
  console.log("[Agent] Candidate actions:");
  console.log(JSON.stringify(candidates, null, 2));

  let choice = null;
  let fallback = false;
  let fallbackReason = "";
  const payload = buildLlmPayload(context, candidates);

  try {
    choice = await chooseActionWithLlm(args, payload);
    console.log("[Agent] LLM choice:");
    console.log(JSON.stringify(choice, null, 2));

    if (!validateChoiceAgainstCandidates(choice, candidates)) {
      throw new Error(`LLM chose an action outside candidate_actions: ${JSON.stringify(choice)}`);
    }
  } catch (error) {
    fallback = true;
    fallbackReason = error.message || String(error);
    const fallbackAction = chooseFallbackAction(actions);
    choice = {
      piece_id: fallbackAction.piece_id,
      to_cell_id: fallbackAction.to_cell_id,
      reason: `fallback: ${fallbackReason}`,
    };
    console.warn(`[Agent] LLM choice failed. Fallback to piece_id=${choice.piece_id} to_cell_id=${choice.to_cell_id}`);
    console.warn(`[Agent] Fallback reason: ${fallbackReason}`);
  }

  const proposal = await mcp.callTool("terra.submit_action_proposal", {
    piece_id: choice.piece_id,
    to_cell_id: choice.to_cell_id,
  });

  if (!proposal.ok || !proposal.accepted) {
    if (!fallback) {
      fallback = true;
      fallbackReason = `proposal rejected: ${JSON.stringify(proposal)}`;
      const fallbackAction = chooseFallbackAction(actions);
      choice = {
        piece_id: fallbackAction.piece_id,
        to_cell_id: fallbackAction.to_cell_id,
        reason: `fallback after proposal rejection: ${fallbackReason}`,
      };
      console.warn(`[Agent] Proposal rejected. Retry fallback piece_id=${choice.piece_id} to_cell_id=${choice.to_cell_id}`);

      const fallbackProposal = await mcp.callTool("terra.submit_action_proposal", {
        piece_id: choice.piece_id,
        to_cell_id: choice.to_cell_id,
      });
      if (!fallbackProposal.ok || !fallbackProposal.accepted) {
        throw new Error(`Fallback proposal rejected: ${JSON.stringify(fallbackProposal)}`);
      }
    } else {
      throw new Error(`Proposal rejected: ${JSON.stringify(proposal)}`);
    }
  }
  console.log(`[Agent] Proposal accepted. Fallback=${fallback}`);

  const execution = await mcp.callTool("terra.execute_validated_action", {
    expected_turn_index: context.turn_index,
    expected_faction_id: context.current_faction_id,
    piece_id: choice.piece_id,
    to_cell_id: choice.to_cell_id,
  });

  console.log("[Agent] Execution result:");
  console.log(JSON.stringify({
    choice,
    fallback,
    fallback_reason: fallbackReason,
    execution,
  }, null, 2));

  if (!execution.ok || !execution.executed) {
    process.exitCode = 2;
  }
}

main().catch((error) => {
  console.error(`[Agent] Failed: ${error.stack || error.message || error}`);
  process.exitCode = 1;
});
