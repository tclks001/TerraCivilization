#!/usr/bin/env node

const DEFAULT_MCP_URL = "http://127.0.0.1:8765/terra-npc-mcp";
const PROTOCOL_VERSION = "2025-06-18";

function parseArgs(argv) {
  const args = {
    url: process.env.TERRA_NPC_MCP_URL || DEFAULT_MCP_URL,
  };

  for (let i = 2; i < argv.length; ++i) {
    const arg = argv[i];
    if (arg === "--url" && argv[i + 1]) {
      args.url = argv[++i];
    } else if (arg.startsWith("--url=")) {
      args.url = arg.slice("--url=".length);
    } else if (arg === "--help" || arg === "-h") {
      args.help = true;
    }
  }

  return args;
}

function printHelp() {
  console.log(`Usage:
  npm run run-once
  node src/run-once.js --url http://127.0.0.1:8765/terra-npc-mcp

Environment:
  TERRA_NPC_MCP_URL  Override MCP endpoint URL.
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
          name: "terra-npc-agent",
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

function chooseFirstLegalAction(actionsResult) {
  const actions = actionsResult?.actions || [];
  if (actions.length <= 0) {
    throw new Error("terra.list_legal_actions returned no legal actions.");
  }

  const action = actions[0];
  if (!Number.isInteger(action.piece_id) || !Number.isInteger(action.to_cell_id)) {
    throw new Error(`First action does not contain integer piece_id/to_cell_id: ${JSON.stringify(action)}`);
  }

  return action;
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

  const action = chooseFirstLegalAction(actionsResult);
  console.log(`[Agent] Chosen first action: piece_id=${action.piece_id} to_cell_id=${action.to_cell_id}`);

  const proposal = await mcp.callTool("terra.submit_action_proposal", {
    piece_id: action.piece_id,
    to_cell_id: action.to_cell_id,
  });
  if (!proposal.ok || !proposal.accepted) {
    throw new Error(`Proposal rejected: ${JSON.stringify(proposal)}`);
  }
  console.log("[Agent] Proposal accepted.");

  const execution = await mcp.callTool("terra.execute_validated_action", {
    expected_turn_index: context.turn_index,
    expected_faction_id: context.current_faction_id,
    piece_id: action.piece_id,
    to_cell_id: action.to_cell_id,
  });

  console.log("[Agent] Execution result:");
  console.log(JSON.stringify(execution, null, 2));

  if (!execution.ok || !execution.executed) {
    process.exitCode = 2;
  }
}

main().catch((error) => {
  console.error(`[Agent] Failed: ${error.stack || error.message || error}`);
  process.exitCode = 1;
});
