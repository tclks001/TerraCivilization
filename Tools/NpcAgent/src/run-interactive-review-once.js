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
  node src/run-interactive-review-once.js --url http://127.0.0.1:8765/terra-npc-mcp

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
          name: "terra-npc-agent-interactive-review",
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
    throw new Error(`Required MCP tool is missing in current phase: ${toolName}`);
  }
}

function chooseFirstPiece(reviewResult) {
  const pieces = reviewResult?.pieces || [];
  const actionable = pieces.find((piece) => Number(piece.legal_action_count || 0) > 0);
  if (!actionable || !Number.isInteger(actionable.piece_id)) {
    throw new Error(`ui_begin_turn_review did not return an actionable piece: ${JSON.stringify(reviewResult)}`);
  }
  return actionable;
}

function chooseFirstMove(selectResult) {
  const moves = selectResult?.move_options || [];
  const move = moves[0];
  if (!move || !Number.isInteger(move.to_cell_id)) {
    throw new Error(`ui_select_piece did not return move_options: ${JSON.stringify(selectResult)}`);
  }
  return move;
}

async function refreshTools(mcp) {
  const tools = await mcp.listTools();
  return tools?.tools?.map((tool) => tool.name) || [];
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

  let toolNames = await refreshTools(mcp);
  console.log(`[Agent] Visible tools: ${toolNames.join(", ")}`);

  requireTool({ tools: toolNames.map((name) => ({ name })) }, "terra.ui_begin_turn_review");
  const review = await mcp.callTool("terra.ui_begin_turn_review", {});
  if (!review.ok) {
    throw new Error(`ui_begin_turn_review failed: ${JSON.stringify(review)}`);
  }
  console.log("[Agent] Begin review:");
  console.log(JSON.stringify(review, null, 2));

  const piece = chooseFirstPiece(review);

  toolNames = await refreshTools(mcp);
  console.log(`[Agent] Visible tools after begin: ${toolNames.join(", ")}`);
  requireTool({ tools: toolNames.map((name) => ({ name })) }, "terra.ui_select_piece");

  const selectResult = await mcp.callTool("terra.ui_select_piece", { piece_id: piece.piece_id });
  if (!selectResult.ok) {
    throw new Error(`ui_select_piece failed: ${JSON.stringify(selectResult)}`);
  }
  console.log("[Agent] Select piece:");
  console.log(JSON.stringify(selectResult, null, 2));

  const move = chooseFirstMove(selectResult);

  toolNames = await refreshTools(mcp);
  console.log(`[Agent] Visible tools after select: ${toolNames.join(", ")}`);
  requireTool({ tools: toolNames.map((name) => ({ name })) }, "terra.ui_preview_move");

  const previewResult = await mcp.callTool("terra.ui_preview_move", {
    piece_id: piece.piece_id,
    to_cell_id: move.to_cell_id,
  });
  if (!previewResult.ok) {
    throw new Error(`ui_preview_move failed: ${JSON.stringify(previewResult)}`);
  }
  console.log("[Agent] Preview move:");
  console.log(JSON.stringify(previewResult, null, 2));

  toolNames = await refreshTools(mcp);
  console.log(`[Agent] Visible tools after preview: ${toolNames.join(", ")}`);
  requireTool({ tools: toolNames.map((name) => ({ name })) }, "terra.ui_confirm_action");

  const confirmResult = await mcp.callTool("terra.ui_confirm_action", {});
  if (!confirmResult.ok || !confirmResult.executed) {
    throw new Error(`ui_confirm_action failed: ${JSON.stringify(confirmResult)}`);
  }
  console.log("[Agent] Confirm action:");
  console.log(JSON.stringify(confirmResult, null, 2));
}

main().catch((error) => {
  console.error(`[Agent] Failed: ${error.stack || error.message || error}`);
  process.exitCode = 1;
});
