#!/usr/bin/env node

const DEFAULT_MCP_URL = "http://127.0.0.1:8765/terra-npc-mcp";
const DEFAULT_LLM_BASE_URL = "https://api.openai.com/v1";
const DEFAULT_LLM_MODEL = "gpt-4.1-mini";
const DEFAULT_MAX_STEPS = 32;
const PROTOCOL_VERSION = "2025-06-18";
const STRATEGIC_TOOL_NAMES = [
  "terra.strategy.summarize_faction_state",
  "terra.strategy.describe_frontline",
  "terra.strategy.find_terrain_control_points",
  "terra.strategy.find_enemy_pressure",
  "terra.strategy.describe_strategic_options",
];

function parseArgs(argv) {
  const args = {
    url: process.env.TERRA_NPC_MCP_URL || DEFAULT_MCP_URL,
    llmBaseUrl: process.env.TERRA_NPC_LLM_BASE_URL || process.env.OPENAI_BASE_URL || DEFAULT_LLM_BASE_URL,
    llmApiKey: process.env.TERRA_NPC_LLM_API_KEY || process.env.OPENAI_API_KEY || "",
    llmModel: process.env.TERRA_NPC_LLM_MODEL || process.env.OPENAI_MODEL || DEFAULT_LLM_MODEL,
    maxSteps: parsePositiveInt(process.env.TERRA_NPC_LLM_MAX_STEPS, DEFAULT_MAX_STEPS),
    dryRunLlm: false,
    exposeStrategicTools: false,
    prefetchStrategicContext: true,
    llmToolTakeaways: false,
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
    } else if (arg === "--allow-strategy-tools") {
      args.exposeStrategicTools = true;
    } else if (arg === "--no-strategy-prefetch") {
      args.prefetchStrategicContext = false;
    } else if (arg === "--llm-tool-takeaways") {
      args.llmToolTakeaways = true;
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
  npm run run-llm-interactive-think
  node src/run-llm-interactive-think.js --url http://127.0.0.1:8765/terra-npc-mcp

Environment:
  TERRA_NPC_MCP_URL             Override MCP endpoint URL.
  TERRA_NPC_LLM_API_KEY         LLM API key. Falls back to OPENAI_API_KEY.
  TERRA_NPC_LLM_BASE_URL        OpenAI-compatible base URL. Default: https://api.openai.com/v1
  TERRA_NPC_LLM_MODEL           Model name. Default: ${DEFAULT_LLM_MODEL}
  TERRA_NPC_LLM_MAX_STEPS       Max LLM planning steps. Default: ${DEFAULT_MAX_STEPS}

Options:
  --dry-run-llm                 Print the first LLM request payload and stop.
  --allow-strategy-tools        Let the LLM call terra.strategy.* tools alongside current UI tools.
  --no-strategy-prefetch        Do not have the supervisor prefetch strategic context at turn start.
  --llm-tool-takeaways          Ask the LLM for one factual takeaway after every tool result.
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
          name: "terra-npc-agent-llm-interactive-think",
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

function filterAvailableTools(toolsResult, exposeStrategicTools) {
  const interactiveTools = filterInteractiveTools(toolsResult);
  if (!exposeStrategicTools) {
    return interactiveTools;
  }

  const strategicTools = (toolsResult?.tools || [])
    .filter((tool) => STRATEGIC_TOOL_NAMES.includes(tool.name))
    .map((tool) => ({
      name: tool.name,
      description: tool.description || "",
      input_schema: tool.inputSchema || tool.input_schema || {},
    }));
  return [...interactiveTools, ...strategicTools];
}

function buildSystemPrompt(exposeStrategicTools) {
  return [
    "You are the NPC brain for TerraCivilization.",
    exposeStrategicTools
      ? "You operate through stateful interactive MCP UI tools and idempotent strategic MCP query tools."
      : "You operate through interactive MCP UI tools only.",
    "You must think explicitly in a structured way before each tool call.",
    "Do not assume hidden state. Use only returned interaction_state and tool payloads.",
    "",
    "Game rules summary:",
    "1. The board is a spherical adjacency graph of cells. Most have 6 neighbors, 12 pentagon cells have 5 neighbors.",
    "2. There are 12 factions. Each faction starts with 16 pieces: 1 commander, 5 archers, 5 cavalry, 5 infantry.",
    "3. Commander does not move, but counts in capture formations. Losing the commander defeats the faction.",
    "4. Infantry can move 1 cell, standard-jump, and chain-jump.",
    "5. Cavalry can move 1 cell, standard-jump, chain-jump, and longer-jump. Cavalry cannot enter or jump across mountains.",
    "6. Archer can move 1 cell, standard-jump, chain-jump, and perform extended ranged capture patterns.",
    "7. Plains have no special effect. Forest protects targets from archer ranged capture only. Mountains block cavalry and extend archer ranged capture if the archer stands there.",
    "8. Captures resolve after the whole action finishes.",
    "9. Last undefeated faction wins.",
    "",
    "Interactive tool semantics:",
    "- terra.ui_begin_turn_review: get the current faction summary for this turn.",
    "- terra.ui_select_piece: inspect a piece through the real gameplay click path and get move_options.",
    "- terra.ui_preview_move: inspect a concrete move through the real gameplay click path and get preview data.",
    "- terra.ui_cancel_selection: cancel current selection or preview and return to a stable gameplay phase.",
    "- terra.ui_confirm_action: confirm the current already-previewed action and end the turn.",
    "",
    "Strategic context semantics:",
    "- Before your first UI decision, the supervising agent may provide an idempotent strategic_context assembled in parallel from terra.strategy tools.",
    "- Treat it as current-turn evidence for intent selection: faction material, frontline contacts, terrain control, enemy pressure, and transparent strategic options.",
    "- Do not treat a strategic option as a legal action. Use UI tools and local tactical cards to validate a concrete piece and destination.",
    exposeStrategicTools
      ? "- terra.strategy.* tools are available in addition to UI tools. They are read-only, accept {}, and can be called in any interaction phase. Use them deliberately to choose or revise an intent; do not repeatedly request an unchanged strategic view without a new question."
      : "",
    "",
    "Local tactical card semantics:",
    "- ui_select_piece move_options and ui_preview_move preview may include local_tactical_card.",
    "- Treat it as deterministic evidence for one concrete action: from/to terrain, forward graph topology, mobility after the move, friendly jump-anchor support, and enemy response pressure.",
    "- Positive approach_to_enemy means the move closes graph distance to the nearest enemy; it is not by itself a reason to advance.",
    "- For cavalry, inspect mobility.forward_blocked_by_mountain_cell_ids and mobility_delta_reachable_endpoints before treating a forward move as useful.",
    "- For combined arms, inspect friendly_synergy.moved_piece_can_be_jump_anchor_for_friendly_piece_ids and support_delta. These are static rule-verified opportunities, not promises about an enemy's next turn.",
    "- Never treat a missing, null, or truncated future-pressure field as proof that the destination is safe.",
    "",
    "What good reasoning looks like here:",
    "- First decide a turn goal, not just a legal move.",
    "- Then form a hypothesis about which piece or move could serve that goal.",
    "- Use tools to test that hypothesis.",
    "- If a hypothesis is weakened by returned data, say why and revise it.",
    "- Only confirm when the chosen move still matches the turn goal better than inspected alternatives.",
    "",
    "You are not required to inspect many pieces if one move already strongly matches the turn goal.",
    "You are not required to cancel just to satisfy a ritual. Cancel only when comparison is genuinely useful.",
    "A turn is not complete until terra.ui_confirm_action succeeds.",
    "",
    "Response contract:",
    "- Always return strict JSON.",
    "- For a tool step, return exactly:",
    '{"kind":"tool_call","turn_goal":"string","tactical_focus":"string","current_hypothesis":"string","evidence_summary":"string","why_not_previous_option":"string","tool_name":"<exact tool name>","arguments":{...},"purpose":"why this tool call is the right next step"}',
    "- You may return kind=final only after terra.ui_confirm_action has succeeded, or if the supervising agent already told you the turn was executed.",
    "- For a final decision after successful execution, return exactly:",
    '{"kind":"final","turn_goal":"string","decision_summary":"string","why_this_plan":"string"}',
    "- Do not wrap JSON in markdown fences.",
  ].filter(Boolean).join("\n");
}

function buildUserPrompt(availableTools, history, currentState, strategicContext, exposeStrategicTools) {
  return JSON.stringify({
    task: exposeStrategicTools
      ? "Play exactly one NPC turn using only the listed tools. Stateful terra.ui_* tools are dynamically exposed by Gameplay; read-only terra.strategy.* tools may be queried at any phase. Give explicit structured reasoning before each tool call."
      : "Play exactly one NPC turn using only the currently visible interactive UI tools, with explicit structured reasoning before each tool call.",
    available_tools: availableTools,
    current_interaction_state: currentState,
    strategic_context: strategicContext,
    interaction_rules: [
      "Call one tool at a time.",
      "Use exact tool names from available_tools only.",
      ...(exposeStrategicTools ? [
        "A terra.strategy.* call is an information-gathering step, not a move and does not change interaction phase.",
        "Use strategy queries to answer a concrete question about material, frontline, terrain control, enemy pressure, or strategic alternatives before selecting or revising a UI line.",
      ] : []),
      "Do not invent piece ids or destination cell ids.",
      "Base your reasoning on observed tool outputs, not on imagined topology.",
      "When a local_tactical_card is available, compare its mobility, friendly_synergy, enemy_interaction, and supporting raw fields before confirming a move.",
      "If you abandon a previously previewed or selected line, explain why in why_not_previous_option.",
      "You may confirm early if the preview clearly satisfies the turn goal.",
      "Do not output kind=final before terra.ui_confirm_action succeeds.",
    ],
    history,
    expected_output_schema: {
      kind: "tool_call | final",
      turn_goal: "string",
      tactical_focus: "string when kind=tool_call",
      current_hypothesis: "string when kind=tool_call",
      evidence_summary: "string when kind=tool_call",
      why_not_previous_option: "string when kind=tool_call",
      tool_name: "string when kind=tool_call",
      arguments: "object when kind=tool_call",
      purpose: "string when kind=tool_call",
      decision_summary: "string when kind=final",
      why_this_plan: "string when kind=final",
    },
  }, null, 2);
}

function filterStrategicTools(toolsResult) {
  return (toolsResult?.tools || [])
    .filter((tool) => STRATEGIC_TOOL_NAMES.includes(tool.name))
    .map((tool) => tool.name);
}

function snapshotKey(result) {
  const snapshot = result?.snapshot;
  if (!snapshot || !Number.isInteger(snapshot.turn_index) || !Number.isInteger(snapshot.current_faction_id) || typeof snapshot.interaction_phase !== "string") {
    return "";
  }
  return `${snapshot.turn_index}:${snapshot.current_faction_id}:${snapshot.interaction_phase}`;
}

async function collectStrategicContext(mcp, toolsResult) {
  const visibleStrategicTools = filterStrategicTools(toolsResult);
  const missing = STRATEGIC_TOOL_NAMES.filter((name) => !visibleStrategicTools.includes(name));
  if (missing.length > 0) {
    throw new Error(`Missing strategic MCP tools: ${missing.join(", ")}`);
  }

  const entries = await Promise.all(STRATEGIC_TOOL_NAMES.map(async (toolName) => ({
    tool_name: toolName,
    result: await mcp.callTool(toolName, {}),
  })));
  for (const entry of entries) {
    if (!entry.result?.ok) {
      throw new Error(`Strategic tool failed ${entry.tool_name}: ${JSON.stringify(entry.result)}`);
    }
  }

  const snapshotKeys = new Set(entries.map((entry) => snapshotKey(entry.result)));
  if (snapshotKeys.size !== 1 || !snapshotKeys.values().next().value) {
    throw new Error(`Strategic snapshot mismatch across parallel tools: ${JSON.stringify(entries)}`);
  }

  return {
    snapshot_key: snapshotKeys.values().next().value,
    parallel_tool_names: STRATEGIC_TOOL_NAMES,
    results: entries,
  };
}

async function askLlmForStep(args, systemPrompt, userPrompt) {
  if (!args.llmApiKey) {
    throw new Error("Missing LLM API key. Set TERRA_NPC_LLM_API_KEY or OPENAI_API_KEY.");
  }

  const requestBody = {
    model: args.llmModel,
    temperature: 0.35,
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

async function askLlmForToolTakeaway(args, toolName, purpose, result) {
  if (!args.llmApiKey) {
    throw new Error("Missing LLM API key. Set TERRA_NPC_LLM_API_KEY or OPENAI_API_KEY.");
  }

  const requestBody = {
    model: args.llmModel,
    temperature: 0.2,
    messages: [
      {
        role: "system",
        content: "You summarize deterministic game-tool results for an NPC decision loop. Return exactly one concise natural-language sentence, maximum 40 words. State only the most decision-useful fact supported by the result. Do not invent hidden state, do not propose an action, and do not use markdown.",
      },
      {
        role: "user",
        content: JSON.stringify({
          tool_name: toolName,
          tool_purpose: purpose,
          result: sanitizeToolResult(result),
        }),
      },
    ],
  };

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
    throw new Error(`LLM takeaway HTTP ${response.status} ${response.statusText}: ${text}`);
  }

  const body = await response.json();
  const content = body?.choices?.[0]?.message?.content;
  if (typeof content !== "string" || !content.trim()) {
    throw new Error(`LLM takeaway response did not contain choices[0].message.content: ${JSON.stringify(body)}`);
  }

  return content.replace(/\s+/g, " ").trim().slice(0, 400);
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

function validateNonEmptyString(step, key) {
  if (typeof step[key] !== "string" || !step[key].trim()) {
    throw new Error(`LLM step missing non-empty string field ${key}: ${JSON.stringify(step)}`);
  }
}

function validateLlmStep(step, allowedToolNames) {
  if (!step || typeof step !== "object") {
    throw new Error(`LLM step is not an object: ${JSON.stringify(step)}`);
  }

  validateNonEmptyString(step, "turn_goal");

  if (step.kind === "final") {
    validateNonEmptyString(step, "decision_summary");
    validateNonEmptyString(step, "why_this_plan");
    return;
  }

  if (step.kind !== "tool_call") {
    throw new Error(`LLM step kind must be tool_call or final: ${JSON.stringify(step)}`);
  }

  validateNonEmptyString(step, "tactical_focus");
  validateNonEmptyString(step, "current_hypothesis");
  validateNonEmptyString(step, "evidence_summary");
  validateNonEmptyString(step, "why_not_previous_option");
  validateNonEmptyString(step, "purpose");

  if (typeof step.tool_name !== "string" || !allowedToolNames.has(step.tool_name)) {
    throw new Error(`LLM chose unavailable tool: ${JSON.stringify(step)}`);
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

  const systemPrompt = buildSystemPrompt(args.exposeStrategicTools);
  const history = [];
  let currentState = null;
  let executed = false;
  let finalResult = null;
  let confirmSucceeded = false;
  let strategicContext = null;

  for (let stepIndex = 1; stepIndex <= args.maxSteps; ++stepIndex) {
    const toolsResult = await mcp.listTools();
    const interactiveTools = filterInteractiveTools(toolsResult);
    const availableTools = filterAvailableTools(toolsResult, args.exposeStrategicTools);
    const allowedToolNames = new Set(availableTools.map((tool) => tool.name));

    if (availableTools.length <= 0) {
      throw new Error("No interactive UI tools are currently exposed by MCP.");
    }

    console.log(`[Agent] Visible tools: ${availableTools.map((tool) => tool.name).join(", ")}`);

    if (args.prefetchStrategicContext && !strategicContext && interactiveTools.some((tool) => tool.name === "terra.ui_begin_turn_review")) {
      strategicContext = await collectStrategicContext(mcp, toolsResult);
      console.log(`[Agent] Parallel strategic tools: ${strategicContext.parallel_tool_names.join(", ")}`);
      console.log(`[Agent] Strategic snapshot: ${strategicContext.snapshot_key}`);
    }

    const userPrompt = buildUserPrompt(availableTools, history, currentState, strategicContext, args.exposeStrategicTools);
    const llmStep = await askLlmForStep(args, systemPrompt, userPrompt);
    validateLlmStep(llmStep, allowedToolNames);

    if (llmStep.kind === "final") {
      if (!confirmSucceeded) {
        const rejection = "A turn is not complete until terra.ui_confirm_action succeeds. Return a currently visible tool_call instead of kind=final.";
        console.log(`[Agent] Rejected premature final: ${rejection}`);
        history.push({
          step: stepIndex,
          event: "supervisor_rejection",
          reason: rejection,
          llm_final: llmStep,
          visible_tools: availableTools.map((tool) => tool.name),
          interaction_state_after: currentState,
        });
        continue;
      }
      console.log(`[Agent] Turn Goal: ${llmStep.turn_goal}`);
      console.log(`[Agent] Final Summary: ${llmStep.decision_summary}`);
      console.log(`[Agent] Why This Plan: ${llmStep.why_this_plan}`);
      finalResult = llmStep;
      break;
    }

    console.log(`[Agent] Step ${stepIndex}: ${llmStep.tool_name}`);
    console.log(`[Agent] Turn Goal: ${llmStep.turn_goal}`);
    console.log(`[Agent] Tactical Focus: ${llmStep.tactical_focus}`);
    console.log(`[Agent] Hypothesis: ${llmStep.current_hypothesis}`);
    console.log(`[Agent] Evidence: ${llmStep.evidence_summary}`);
    console.log(`[Agent] Why Not Previous: ${llmStep.why_not_previous_option}`);
    console.log(`[Agent] Purpose: ${llmStep.purpose}`);
    console.log(`[Agent] Arguments: ${JSON.stringify(llmStep.arguments)}`);

    const toolResult = await mcp.callTool(llmStep.tool_name, llmStep.arguments);
    console.log(`[Agent] Tool result ${llmStep.tool_name}:`);
    console.log(JSON.stringify(toolResult, null, 2));

    const toolTakeaway = args.llmToolTakeaways
      ? await askLlmForToolTakeaway(args, llmStep.tool_name, llmStep.purpose, toolResult)
      : "";
    if (toolTakeaway) {
      console.log(`[Agent] LLM takeaway: ${toolTakeaway}`);
    }

    currentState = extractInteractionState(toolResult, currentState);
    history.push({
      step: stepIndex,
      visible_tools: availableTools.map((tool) => tool.name),
      turn_goal: llmStep.turn_goal,
      tactical_focus: llmStep.tactical_focus,
      current_hypothesis: llmStep.current_hypothesis,
      evidence_summary: llmStep.evidence_summary,
      why_not_previous_option: llmStep.why_not_previous_option,
      tool_name: llmStep.tool_name,
      purpose: llmStep.purpose,
      arguments: llmStep.arguments,
      result: sanitizeToolResult(toolResult),
      llm_tool_takeaway: toolTakeaway || undefined,
      interaction_state_after: currentState,
    });

    if (llmStep.tool_name === "terra.ui_confirm_action") {
      executed = Boolean(toolResult?.ok && toolResult?.executed);
      confirmSucceeded = executed;
      finalResult = {
        kind: "final",
        turn_goal: llmStep.turn_goal,
        decision_summary: executed
          ? "Turn executed successfully through interactive tools."
          : "Confirm tool was called but the action did not execute successfully.",
        why_this_plan: llmStep.current_hypothesis,
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
