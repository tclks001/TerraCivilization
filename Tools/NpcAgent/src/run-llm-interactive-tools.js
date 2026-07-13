#!/usr/bin/env node

const DEFAULT_MCP_URL = "http://127.0.0.1:8765/terra-npc-mcp";
const DEFAULT_LLM_BASE_URL = "https://api.openai.com/v1";
const DEFAULT_LLM_MODEL = "gpt-4.1-mini";
const DEFAULT_MAX_STEPS = 16;
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
  npm run run-llm-interactive-tools
  node src/run-llm-interactive-tools.js --url http://127.0.0.1:8765/terra-npc-mcp

Environment:
  TERRA_NPC_MCP_URL             Override MCP endpoint URL.
  TERRA_NPC_LLM_API_KEY         LLM API key. Falls back to OPENAI_API_KEY.
  TERRA_NPC_LLM_BASE_URL        OpenAI-compatible base URL. Default: https://api.openai.com/v1
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
          name: "terra-npc-agent-llm-interactive-tools",
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

function filterInteractiveTools(toolsResult) {
  return (toolsResult?.tools || [])
    .filter((tool) => typeof tool.name === "string" && tool.name.startsWith("terra.ui_"))
    .map((tool) => ({
      name: tool.name,
      description: tool.description || "",
      input_schema: tool.inputSchema || tool.input_schema || {},
    }));
}

function buildSystemPrompt() {
  return [
    "You are the NPC brain for TerraCivilization.",
    "You operate through interactive MCP UI tools only.",
    "Do not assume hidden state. Use the returned interaction_state and tool payloads.",
    "Every time you want to use a tool, you must explain the purpose of that tool call.",
    "",
    "Game rules summary:",
    "1. The board is a spherical adjacency graph of cells. Most have 6 neighbors, 12 pentagon cells have 5 neighbors.",
    "2. There are 12 factions. Each faction starts with 16 pieces: 1 commander, 5 archers, 5 cavalry, 5 infantry.",
    "3. Commanders do not move, but they do count in capture formations. Losing the commander defeats the faction.",
    "4. Infantry can move 1 cell, standard-jump, and chain-jump.",
    "5. Cavalry can move 1 cell, standard-jump, chain-jump, and make a longer jump; cavalry cannot enter or jump across mountains.",
    "6. Archers can move 1 cell, standard-jump, chain-jump, and use extended ranged capture patterns.",
    "7. Plains have no effect. Forest protects targets from archer ranged capture only. Mountains block cavalry and extend archer ranged capture if the archer stands on the mountain.",
    "8. Captures resolve after the action finishes, based on two-on-one formations and archer ranged variants.",
    "9. Last undefeated faction wins.",
    "",
    "Interactive tool semantics:",
    "- terra.ui_begin_turn_review: get the current faction summary for this turn.",
    "- terra.ui_select_piece: click a piece, reuse the real gameplay selection path, and get move_options.",
    "- terra.ui_preview_move: click a target cell for the selected piece, reuse the real gameplay preview path, and get preview data.",
    "- terra.ui_cancel_selection: cancel the current selection or preview and return to a stable gameplay phase.",
    "- terra.ui_confirm_action: confirm the current already-previewed action and end the turn.",
    "",
    "Decision policy requirements:",
    "- Do not blindly select the first option. Use the interaction flow to inspect pieces and candidate moves.",
    "- Prefer moves that improve position, create future pressure, preserve safety, and exploit terrain or ranged structure.",
    "- Because these are interactive tools, behave as if you are observing and testing board state through clicks.",
    "- Use the returned move_options and preview payloads to compare alternatives.",
    "- You may inspect one piece, cancel, inspect another, and then decide which preview to confirm.",
    "",
    "State discipline:",
    "- Only use tools that are currently available in available_tools.",
    "- The gameplay state machine is authoritative. Read interaction_state carefully.",
    "- If interaction_state says confirm is available and the preview looks good, you may confirm.",
    "",
    "Response contract:",
    "- Always return strict JSON.",
    "- If you want to call a tool, return:",
    '{"kind":"tool_call","tool_name":"<exact tool name>","arguments":{...},"purpose":"why this tool call helps your decision"}',
    "- If you have enough information and want to stop without acting further, return:",
    '{"kind":"final","summary":"short summary"}',
    "- Do not wrap JSON in markdown fences.",
  ].join("\n");
}

function buildUserPrompt(availableTools, history, currentState) {
  return JSON.stringify({
    task: "Play exactly one NPC turn using only the currently visible interactive UI tools.",
    available_tools: availableTools,
    current_interaction_state: currentState,
    interaction_rules: [
      "Call one tool at a time.",
      "Use exact tool names from available_tools only.",
      "Do not invent piece ids or destination cell ids.",
      "Read move_options and preview payloads before confirming.",
      "If you want to compare a different piece after selecting or previewing, you may use terra.ui_cancel_selection when available.",
      "Prefer a deliberate inspect-compare-confirm flow over immediately confirming the first visible move.",
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
    temperature: 0.3,
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

function extractInteractionState(result, fallbackState) {
  if (result && typeof result === "object" && result.interaction_state && typeof result.interaction_state === "object") {
    return result.interaction_state;
  }
  return fallbackState;
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

  const systemPrompt = buildSystemPrompt();
  const history = [];
  let currentState = null;
  let executed = false;
  let finalResult = null;

  for (let stepIndex = 1; stepIndex <= args.maxSteps; ++stepIndex) {
    const toolsResult = await mcp.listTools();
    const availableTools = filterInteractiveTools(toolsResult);
    const allowedToolNames = new Set(availableTools.map((tool) => tool.name));

    if (availableTools.length <= 0) {
      throw new Error("No interactive UI tools are currently exposed by MCP.");
    }

    console.log(`[Agent] Visible interactive tools: ${availableTools.map((tool) => tool.name).join(", ")}`);

    const userPrompt = buildUserPrompt(availableTools, history, currentState);
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

    currentState = extractInteractionState(toolResult, currentState);
    history.push({
      step: stepIndex,
      visible_tools: availableTools.map((tool) => tool.name),
      tool_name: llmStep.tool_name,
      purpose: llmStep.purpose,
      arguments: llmStep.arguments,
      result: sanitizeToolResult(toolResult),
      interaction_state_after: currentState,
    });

    if (llmStep.tool_name === "terra.ui_confirm_action") {
      executed = Boolean(toolResult?.ok && toolResult?.executed);
      finalResult = {
        kind: "final",
        summary: executed
          ? "Turn executed successfully through interactive tools."
          : "Confirm tool was called but the action did not execute successfully.",
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
    final_interaction_state: currentState,
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
