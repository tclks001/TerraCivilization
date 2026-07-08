# UE 5.8 NPC MCP Runtime 阶段 1 设计稿

日期：2026-07-08

## 目标

阶段 1 只验证 Runtime MCP 最小通路：

1. 游戏 Runtime 能注册项目自定义 MCP tool。
2. MCP server 默认关闭，只能通过命令行显式启动。
3. 外部 MCP client 能调用 `terra.ping_gameplay`。
4. `terra.ping_gameplay` 能读到真实 `FTerraGameplayContainer` 的只读状态。

本阶段不做 LLM 决策、不做走法枚举、不执行 NPC 行动。

## 模块边界

新增 Runtime 模块 `NpcMcp`：

- 依赖 `ModelContextProtocol` 和 `Gameplay`。
- 不依赖 UE Editor-only Toolset Registry。
- 只注册项目白名单工具。
- 通过 `FTerraNpcMcpGameplayBridge` 持有当前 Gameplay 容器的只读指针。

`Gameplay` 仍然是规则权威；`NpcMcp` 本阶段只读。

## 启动方式

默认不开放 MCP 端口。

启动参数：

```text
-TerraNpcMcpStartServer -TerraNpcMcpPort=8765 -TerraNpcMcpPath=/terra-npc-mcp
```

默认端口：`8765`

默认路径：`/terra-npc-mcp`

## 工具

### `terra.ping_gameplay`

输入：空对象。

输出字段：

- `ok`
- `gameplay_registered`
- `gameplay_initialized`
- `turn_index`
- `current_faction_id`
- `piece_count`
- `alive_piece_count`
- `faction_count`
- `alive_faction_count`
- `match_ended`
- `winning_faction_id`

用途：验证 MCP runtime、项目工具注册、Gameplay 桥接和只读状态查询都已连通。

## 验收

1. 不带 `-TerraNpcMcpStartServer` 启动时，日志显示 Runtime MCP server disabled。
2. 带启动参数运行时，日志显示 MCP server 地址。
3. MCP Inspector 连接：

```text
http://127.0.0.1:8765/terra-npc-mcp
```

4. 工具列表包含 `terra.ping_gameplay`。
5. 创建/重建棋盘后调用工具，`gameplay_registered=true` 且能返回当前回合、阵营、棋子数。

## MCP Inspector 操作步骤

参考：

- 官方文档：[MCP Inspector](https://modelcontextprotocol.io/docs/tools/inspector)
- 官方仓库：[modelcontextprotocol/inspector](https://github.com/modelcontextprotocol/inspector)

### 1. 启动 Unreal Editor 或游戏进程

阶段 1 的 MCP server 默认关闭，必须显式传入启动参数。

在 Editor 里可以通过命令行启动：

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" `
  "C:\workspace\TerraCivilization\TerraCivilization.uproject" `
  -TerraNpcMcpStartServer `
  -TerraNpcMcpPort=8765 `
  -TerraNpcMcpPath=/terra-npc-mcp
```

期望日志包含：

```text
[NpcMcp] Register tool terra.ping_gameplay: ok
[NpcMcp] Runtime MCP server requested at http://127.0.0.1:8765/terra-npc-mcp
```

如果没有传 `-TerraNpcMcpStartServer`，期望日志包含：

```text
[NpcMcp] Runtime MCP server disabled. Pass -TerraNpcMcpStartServer to enable.
```

### 2. 确认 Gameplay 容器已经创建

打开地图并让 `APlanetTessellatedMesh` 完成棋盘/Gameplay rebuild。

如果还没有创建 Gameplay 容器，`terra.ping_gameplay` 仍然可以调用，但会返回：

```json
{
  "ok": true,
  "gameplay_registered": false,
  "gameplay_initialized": false
}
```

当棋盘创建完成后，期望：

```json
{
  "ok": true,
  "gameplay_registered": true,
  "gameplay_initialized": true
}
```

### 3. 启动 MCP Inspector

另开一个 PowerShell 窗口，运行：

```powershell
npx @modelcontextprotocol/inspector@latest
```

第一次运行会下载 npm 包。Inspector 启动后，终端通常会输出一个带 token 的本地 URL，例如：

```text
Open inspector with token pre-filled:
   http://localhost:6274/?MCP_PROXY_AUTH_TOKEN=...
```

用浏览器打开这个 URL。不要手动丢掉 query string 里的 `MCP_PROXY_AUTH_TOKEN`，否则 Inspector UI 可能无法连接自己的 proxy。

### 4. 在 Inspector 里连接 UE MCP server

在 Inspector 左侧连接面板中填写：

```text
Transport Type: Streamable HTTP
URL: http://127.0.0.1:8765/terra-npc-mcp
```

然后点击 `Connect`。

注意：

- 不要选择 `STDIO`。UE 进程里的 MCP server 已经在运行，Inspector 不负责启动它。
- 不要选择 `SSE`。UE 5.8 `ModelContextProtocolServer` 这里使用 POST 到 MCP path 的 Streamable HTTP 风格通路。
- URL 必须包含 path：`/terra-npc-mcp`。只填 `http://127.0.0.1:8765` 会连不到 MCP endpoint。
- 阶段 1 没有额外 auth header，Inspector 里不需要填写 Bearer token。Inspector 自己 UI/proxy 的 token 和 UE MCP server 不是一回事。

### 5. 调用 `terra.ping_gameplay`

连接成功后：

1. 打开 `Tools` tab。
2. 点击 `List Tools` 或刷新工具列表。
3. 选择 `terra.ping_gameplay`。
4. 输入参数保持空对象：

```json
{}
```

5. 点击 `Run Tool`。

期望返回类似：

```json
{
  "ok": true,
  "gameplay_registered": true,
  "gameplay_initialized": true,
  "turn_index": 0,
  "current_faction_id": 0,
  "piece_count": 48,
  "alive_piece_count": 48,
  "faction_count": 12,
  "alive_faction_count": 12,
  "match_ended": false,
  "winning_faction_id": -1
}
```

具体数量以当前地图和 Gameplay 初始化逻辑为准。

### 6. 常见问题

#### Inspector 显示连接失败

检查 UE 进程是否真的带了启动参数：

```text
-TerraNpcMcpStartServer -TerraNpcMcpPort=8765 -TerraNpcMcpPath=/terra-npc-mcp
```

再检查 UE 日志是否出现：

```text
[NpcMcp] Runtime MCP server requested at http://127.0.0.1:8765/terra-npc-mcp
```

#### 只填端口连接失败

Inspector URL 必须是：

```text
http://127.0.0.1:8765/terra-npc-mcp
```

不要填：

```text
http://127.0.0.1:8765
```

#### 工具列表没有 `terra.ping_gameplay`

检查 UE 日志是否出现：

```text
[NpcMcp] Register tool terra.ping_gameplay: ok
```

如果出现 `duplicate`，说明工具名已经被注册过，通常是模块刷新或多实例导致；重启 Editor 后再测。

#### `gameplay_registered=false`

说明 MCP server 和 tool 已经工作，但 `APlanetTessellatedMesh` 还没有创建并注册 `FTerraGameplayContainer`。

处理方式：

1. 确认当前关卡里有 `APlanetTessellatedMesh`。
2. 触发一次地形/棋盘 rebuild。
3. 再次调用 `terra.ping_gameplay`。

#### 端口被占用

换一个端口启动 UE：

```powershell
-TerraNpcMcpPort=8766
```

Inspector URL 同步改成：

```text
http://127.0.0.1:8766/terra-npc-mcp
```

#### 局域网其他机器连不上

这是阶段 1 的预期行为。UE HTTPServer 默认绑定 localhost，本阶段只验证本机 MCP client。不要为了阶段 1 把监听地址改成 `any`。

## 下一阶段

阶段 2 增加 deterministic query tools：

- `terra.get_turn_context`
- `terra.list_legal_actions`
- `terra.evaluate_action_risk`
- `terra.submit_action_proposal`

同时新增 Gameplay 层 public query API，避免 MCP 直接访问私有规则 helper。
