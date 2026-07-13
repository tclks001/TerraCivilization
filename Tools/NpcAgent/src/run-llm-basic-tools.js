#!/usr/bin/env node

const DEFAULT_MCP_URL = "http://127.0.0.1:8765/terra-npc-mcp";
const DEFAULT_LLM_BASE_URL = "https://api.openai.com/v1";
const DEFAULT_LLM_MODEL = "gpt-4.1-mini";
const DEFAULT_MAX_STEPS = 12;
const PROTOCOL_VERSION = "2025-06-18";

function parseArgs(argv) {
  const args = {
    url: process.env.TERRA_NPC_MCP_URL || DEFAULT_MCP_URL,
    llmBaseUrl: process.env.TERRA_NPC_LLM_BASE_URL || process.env.OPENAI_BASE_URL || DEFAULT_LLM_BASE_URL,
    llmApiKey: process.env.TERRA_NPC_LLM_API_KEY || process.env.OPENAI_API_KEY || "",
    llmModel: process.env.TERRA_NPC_LLM_MODEL || process.env.OPENAI_MODEL || DEFAULT_LLM_MODEL,
    maxSteps: parsePositiveInt(process.env.TERRA_NPC_LLM_MAX_STEPS, DEFAULT_MAX_STEPS),
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
    } else if (arg === "--max-steps" && argv[i + 1]) {
      args.maxSteps = parsePositiveInt(argv[++i], DEFAULT_MAX_STEPS);
    } else if (arg.startsWith("--max-steps=")) {
      args.maxSteps = parsePositiveInt(arg.slice("--max-steps=".length), DEFAULT_MAX_STEPS);
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
  npm run run-llm-basic-tools
  node src/run-llm-basic-tools.js --url http://127.0.0.1:8765/terra-npc-mcp

Environment:
  TERRA_NPC_MCP_URL             Override MCP endpoint URL.
  TERRA_NPC_LLM_API_KEY         LLM API key. Falls back to OPENAI_API_KEY.
  TERRA_NPC_LLM_BASE_URL        OpenAI-compatible base URL. Default: ${DEFAULT_LLM_BASE_URL}
  TERRA_NPC_LLM_MODEL           Model name. Default: ${DEFAULT_LLM_MODEL}
  TERRA_NPC_LLM_MAX_STEPS       Max LLM planning steps. Default: ${DEFAULT_MAX_STEPS}

Options:
  --dry-run-llm                 Print the first LLM request payload and stop.
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
          name: "terra-npc-agent-llm-basic-tools",
          version: "0.1.0",
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

function filterBaseTools(toolsResult) {
  const allowed = new Set([
    "terra.get_turn_context",
    "terra.list_legal_actions",
    "terra.evaluate_action_risk",
    "terra.submit_action_proposal",
    "terra.execute_validated_action",
  ]);

  return (toolsResult?.tools || [])
    .filter((tool) => allowed.has(tool.name))
    .map((tool) => ({
      name: tool.name,
      description: tool.description || "",
      input_schema: tool.inputSchema || tool.input_schema || {},
    }));
}

function buildSystemPrompt() {
  return [
    "You are the NPC brain for TerraCivilization.",
    "You operate only through MCP tools. Never invent game state. Query tools when uncertain.",
    "Every time you want to use a tool, you must explain the purpose of that tool call.",
    "",
    "Game rules summary:",
    "1. Board topology:",
    "- The board is a sphere made of cells connected by adjacency.",
    "- Most cells are hex-like with 6 neighbors. There are exactly 12 pentagon cells with 5 neighbors.",
    "- All movement, jump, attack, and formation checks are defined on the cell adjacency graph, not on Euclidean geometry.",
    "- Straight lines on this spherical board are implemented as adjacency rays and may branch around pentagons. You must rely on tool results rather than trying to derive graph geometry exactly yourself.",
    "",
    "2. Factions and starting layout:",
    "- There are 12 factions total. Turn order follows faction_id cycling, defeated factions are skipped.",
    "- Each faction starts with 16 pieces in a radius-2 disk around its pentagon capital cell.",
    "- Radius 0: 1 commander on the capital cell.",
    "- Radius 1: 5 archers.",
    "- Radius 2: 5 cavalry and 5 infantry.",
    "- Commander piece order convention inside a faction is commander, archers, cavalry, infantry.",
    "",
    "3. Piece rules:",
    "- Commander: cannot move or jump, but counts as a friendly piece for capture formations. If the commander is captured, that faction is defeated.",
    "- Infantry: can move to an adjacent empty cell; can standard-jump; can chain-jump.",
    "- Cavalry: can move to an adjacent empty cell; can standard-jump; can chain-jump; can also make a special longer jump landing one extra cell farther than a standard jump.",
    "- Archer: can move to an adjacent empty cell; can standard-jump; can chain-jump; has ranged capture extension.",
    "",
    "4. Normal move and jump rules:",
    "- A normal move goes to one adjacent empty cell if terrain allows.",
    "- A standard jump crosses exactly one adjacent occupied cell and lands on the next empty cell beyond it.",
    "- The jumped-over piece may be friendly or enemy and is not captured by the jump itself.",
    "- After a jump, if that same piece has another legal jump, chain-jump may continue.",
    "- Captures are resolved after the whole action ends, not in the middle of a jump chain.",
    "",
    "5. Terrain rules:",
    "- Plains: no special effect.",
    "- Forest: a piece inside forest is immune to archer ranged capture, but forest does not block movement, jumping, or normal adjacent capture formations.",
    "- Mountain: cavalry cannot enter mountains and cannot jump across mountains. An archer standing on a mountain gets +1 range on ranged capture.",
    "",
    "6. Capture rules:",
    "- Core capture rule is two-on-one formation, written as A A B.",
    "- B is an enemy piece. The two A pieces are friendly to each other and form the capture line.",
    "- The action you choose should aim to create captures after movement resolves.",
    "- The moving piece can be part of the final capture formation.",
    "- Commander can be one of the supporting friendly pieces in a capture formation.",
    "",
    "7. Archer special ranged capture:",
    "- Normal archer ranged capture pattern is A' A - B, where A' is the acting archer.",
    "- If the acting archer stands on a mountain, the pattern extends to A' A - - B.",
    "- If target B is in forest, ranged archer capture does not work.",
    "",
    "8. Victory condition:",
    "- Last undefeated faction wins.",
    "",
    "Decision policy requirements:",
    "- Be conservative about unknown topology. Use tools to inspect legal actions and risk.",
    "- Prefer legal moves that create captures, preserve material, and avoid obviously threatened destinations unless the trade is worthwhile.",
    "- You are currently limited to the base deterministic tools only. You cannot click UI cells or inspect presentation-only state.",
    "",
    "Response contract:",
    "- Always return strict JSON.",
    "- If you want to call a tool, return:",
    '{"kind":"tool_call","tool_name":"<exact tool name>","arguments":{...},"purpose":"why this tool call helps your decision"}',
    "- If you have enough information and want to stop, return:",
    '{"kind":"final","summary":"short summary of the chosen plan and outcome"}',
    "- Do not wrap JSON in markdown fences.",
  ].join("\n");
}

function buildUserPrompt(toolSpecs, history) {
  return JSON.stringify({
    task: "Play exactly one NPC turn using the currently available base MCP tools.",
    available_tools: toolSpecs,
    interaction_rules: [
      "You may call one tool at a time.",
      "Use exact tool names from available_tools.",
      "When calling terra.execute_validated_action, provide expected_turn_index and expected_faction_id from the most recent successful terra.get_turn_context result.",
      "Do not skip validation. Usually submit_action_proposal should happen before execute_validated_action.",
      "If a tool result already proves an action is invalid, do not repeat the same invalid call.",
    ],
    history,
    expected_output_schema: {
      kind: "tool_call | final",
      tool_name: "string when kind=tool_call",
      arguments: "object when kind=tool_call",
      purpose: "string when kind=tool_call",
      summary: "string when kind=final",
    },
  }, null, 2);
}

async function askLlmForStep(args, systemPrompt, userPrompt) {
  if (!args.llmApiKey) {
    throw new Error("Missing LLM API key. Set TERRA_NPC_LLM_API_KEY or OPENAI_API_KEY.");
  }

  const requestBody = {
    model: args.llmModel,
    temperature: 0.2,
    response_format: { type: "json_object" },
    messages: [
      { role: "system", content: systemPrompt },
      { role: "user", content: userPrompt },
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

  return JSON.parse(extractJsonObjectText(content));
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

function validateLlmStep(step, allowedToolNames) {
  if (!step || typeof step !== "object") {
    throw new Error(`LLM step is not an object: ${JSON.stringify(step)}`);
  }

  if (step.kind === "final") {
    if (typeof step.summary !== "string" || !step.summary.trim()) {
      throw new Error(`LLM final step missing summary: ${JSON.stringify(step)}`);
    }
    return;
  }

  if (step.kind !== "tool_call") {
    throw new Error(`LLM step kind must be tool_call or final: ${JSON.stringify(step)}`);
  }

  if (typeof step.tool_name !== "string" || !allowedToolNames.has(step.tool_name)) {
    throw new Error(`LLM chose unavailable tool: ${JSON.stringify(step)}`);
  }

  if (typeof step.purpose !== "string" || !step.purpose.trim()) {
    throw new Error(`LLM tool call missing purpose: ${JSON.stringify(step)}`);
  }

  if (!step.arguments || typeof step.arguments !== "object" || Array.isArray(step.arguments)) {
    throw new Error(`LLM tool call arguments must be an object: ${JSON.stringify(step)}`);
  }
}

function sanitizeToolResult(result) {
  const text = JSON.stringify(result);
  if (text.length <= 12000) {
    return result;
  }
  return {
    note: "tool result truncated for prompt budget",
    json_preview: text.slice(0, 12000),
  };
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

  const toolsResult = await mcp.listTools();
  const requiredTools = [
    "terra.get_turn_context",
    "terra.list_legal_actions",
    "terra.evaluate_action_risk",
    "terra.submit_action_proposal",
    "terra.execute_validated_action",
  ];
  for (const toolName of requiredTools) {
    requireTool(toolsResult, toolName);
  }
  console.log(`[Agent] Tools OK: ${requiredTools.join(", ")}`);

  const toolSpecs = filterBaseTools(toolsResult);
  const allowedToolNames = new Set(toolSpecs.map((tool) => tool.name));
  const systemPrompt = buildSystemPrompt();
  const history = [];

  let executed = false;
  let finalResult = null;

  for (let stepIndex = 1; stepIndex <= args.maxSteps; ++stepIndex) {
    const userPrompt = buildUserPrompt(toolSpecs, history);
    const llmStep = await askLlmForStep(args, systemPrompt, userPrompt);
    validateLlmStep(llmStep, allowedToolNames);

    if (llmStep.kind === "final") {
      console.log(`[Agent] LLM final summary: ${llmStep.summary}`);
      finalResult = llmStep;
      break;
    }

    console.log(`[Agent] Step ${stepIndex}: ${llmStep.tool_name}`);
    console.log(`[Agent] Purpose: ${llmStep.purpose}`);
    console.log(`[Agent] Arguments: ${JSON.stringify(llmStep.arguments)}`);

    const toolResult = await mcp.callTool(llmStep.tool_name, llmStep.arguments);
    console.log(`[Agent] Tool result ${llmStep.tool_name}:`);
    console.log(JSON.stringify(toolResult, null, 2));

    history.push({
      step: stepIndex,
      tool_name: llmStep.tool_name,
      purpose: llmStep.purpose,
      arguments: llmStep.arguments,
      result: sanitizeToolResult(toolResult),
    });

    if (llmStep.tool_name === "terra.execute_validated_action") {
      executed = Boolean(toolResult?.ok && toolResult?.executed);
      finalResult = {
        kind: "final",
        summary: executed
          ? "Turn executed successfully."
          : "Execution tool was called but the action did not execute successfully.",
      };
      break;
    }
  }

  if (!finalResult) {
    throw new Error(`LLM agent reached max steps (${args.maxSteps}) without finishing the turn.`);
  }

  console.log("[Agent] Final result:");
  console.log(JSON.stringify({
    final: finalResult,
    executed,
    history,
  }, null, 2));

  if (!executed) {
    process.exitCode = 2;
  }
}

main().catch((error) => {
  console.error(`[Agent] Failed: ${error.stack || error.message || error}`);
  process.exitCode = 1;
});
