# UE 5.8 AI Toolset Runtime MCP 调研报告

日期：2026-07-08  
范围：UE 5.8 `ModelContextProtocol` / Unreal MCP / AI Toolset Registry 在 cooked、release、Shipping runtime 中的可用性，以及 TerraCivilization 战棋 NPC 的大语言模型 MCP 驱动最小闭环。  
排除范围：不展开 AI Toolset Editor 的编辑器工作流、资产创建、Slate/Editor 操作工具集用法。

## 结论摘要

UE 5.8 的 Unreal MCP 可以拆成两层看：

1. `ModelContextProtocol` 和 `ModelContextProtocolEngine` 是 Runtime 模块。它们包含 MCP HTTP server、协议处理、工具注册接口、资源接口、console command、client config 生成逻辑。Cooked/Shipping 游戏进程技术上可以通过 `IModelContextProtocolModule::StartServer()` 托管 MCP server。
2. AI Toolset Registry 及 Epic 随附的各类 Toolset 插件基本是 Editor-only。`ToolsetRegistry` 本身是 `EditorOnly=true`，模块类型是 `Editor`；`AIModuleToolset`、`EditorToolset`、`GameplayTagsToolset`、`MCPClientToolset`、`AllToolsets` 等也都是 Editor-only 或依赖 Editor-only registry。它们不能作为 Release/Shipping runtime NPC AI 的工具层。
3. 因此，本项目如果要在游戏 Runtime 中做 NPC 大语言模型 MCP 驱动 AI，推荐使用 **MCP Runtime 核心 + 项目自定义 Runtime 工具**，不要依赖 UE 随附的 AI Toolset Registry 自动发现机制。
4. Runtime 启动不能依赖 Editor Preferences 的 auto-start。UE 5.8 源码中 `bAutoStartServer` 的检查发生在 `ModelContextProtocolEditor` 模块里；Shipping runtime 需要项目自己的 GameInstance / subsystem / module 显式调用 `StartServer()`。
5. MCP server 默认走 UE `HTTPServer`，默认监听地址来自 `HTTPServer.Listeners`，默认 `BindAddress = localhost`。保持本机监听是正确默认；不要在可发布版本里默认暴露到 LAN/WAN。

## 外部资料结论

Epic 官方文档《Unreal MCP》明确说明：

- Unreal MCP 是实验性功能，数据格式和 API 仍可能变化。
- MCP server 嵌入 Unreal 进程，通过本地 HTTP 连接给 MCP-compatible AI agent 调用。
- Toolsets/tools 不是 Unreal MCP 核心本身实现的，编辑器侧通常通过 `AllToolsets` / Toolset Registry 暴露。
- `ModelContextProtocol` 与 `ModelContextProtocolEngine` 是 runtime modules；`ModelContextProtocolEditor` 是 editor-only，负责 auto-start hook 以及把 Toolset Registry 发现的 toolsets 适配成 MCP tools。
- Cooked/Shipping game builds 可以通过调用 `IModelContextProtocolModule::StartServer()` 托管 MCP server。
- 对直接注册场景，可实现 `IModelContextProtocolTool` 并调用 `IModelContextProtocolModule::GetChecked().AddTool(Tool)`。

参考：

- Epic 官方文档：[Unreal MCP](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)
- MCP 官方规范：[Model Context Protocol specification](https://modelcontextprotocol.io/specification)

## UE 5.8 源码证据

本机源码路径：`C:\Program Files\Epic Games\UE_5.8\Engine\Source`，插件源码主要位于 `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental`。

### Unreal MCP 插件分层

`ModelContextProtocol.uplugin`：

- Friendly name 是 `Unreal MCP`，描述为 MCP server implementation for Unreal Engine。
- `NoRedist=true`，`IsExperimentalVersion=true`，发布产品要单独评估 EULA/分发合规。
- 模块：
  - `ModelContextProtocol`：`Type=Runtime`
  - `ModelContextProtocolEngine`：`Type=Runtime`
  - `ModelContextProtocolEditor`：`Type=Editor`
  - tests：`UncookedOnly` 或 `Editor`
- 依赖里包含 `ToolsetRegistry`，但 `ToolsetRegistry` 自身是 Editor-only，不能因此推断 Toolset Registry 可进 Shipping。

关键文件：

- `...\ModelContextProtocol\ModelContextProtocol.uplugin`
- `...\ModelContextProtocol\Source\ModelContextProtocol\ModelContextProtocol.Build.cs`
- `...\ModelContextProtocol\Source\ModelContextProtocolEngine\ModelContextProtocolEngine.Build.cs`

`ModelContextProtocol.Build.cs` 只依赖 `Core`、`Analytics`、`AnalyticsET`、`HTTPServer`、`JsonUtilities`、`Json`、`CoreUObject`，没有 Editor 依赖。

`ModelContextProtocolEngine.Build.cs` 默认也是 Runtime 可编译依赖；只有 `if (Target.bBuildEditor)` 分支才追加 `BlueprintGraph`、`JsonUtilitiesEditor`、`UnrealEd`。

### Runtime MCP API

`IModelContextProtocolModule` 提供核心 Runtime API：

- `GetTools()`
- `FindTool()`
- `AddTool()`
- `RemoveTool()`
- `OnRefreshTools()`
- `RefreshTools()`
- `GetServer()`
- `StartServer(uint32 Port, FString UrlPath)`
- `StopServer()`
- `AddResourceProvider()`
- `RemoveResourceProvider()`

关键文件：

- `...\ModelContextProtocol\Source\ModelContextProtocol\Public\IModelContextProtocolModule.h`
- `...\ModelContextProtocol\Source\ModelContextProtocol\Private\ModelContextProtocolModule.cpp`

`IModelContextProtocolTool` 是 Runtime 直接注册工具的核心接口：

- `GetName()`
- `GetDescription()`
- `GetInputJsonSchema()`
- `GetOutputJsonSchema()`
- `Run()`
- `RunAsync()`
- `CancelAsync()`

源码注释说明工具可通过 `IModelContextProtocolModule::GetChecked().AddTool` 注册，并且工具结果必须符合 MCP tool result 结构。

### Server 行为

`ModelContextProtocol.h` 中的默认值：

- 默认端口：`8000`
- 默认路径：`/mcp`
- 默认 server name：`unreal-mcp`
- 支持协议版本包括 `2025-11-25`、`2025-06-18`、`2024-11-05`

`FModelContextProtocolServer::StartServer` 会：

- 校验 URL path。
- 通过 `FHttpServerModule::Get().GetHttpRouter(Port)` 获取 HTTP router。
- 在同一个 path 上注册：
  - POST：主 MCP JSON-RPC 请求。
  - GET：当前实现返回 `BadMethod`，不支持单独 SSE endpoint。
  - DELETE：关闭 session。
- 调用 `FHttpServerModule::Get().StartAllListeners()`。
- 注册 ticker 驱动 MCP server tick。

`ModelContextProtocolServer.cpp` 中实现了 Origin 校验：

- 无 `Origin` header 的 non-browser client 允许。
- `localhost`、`127.0.0.1`、`[::1]` 允许。
- 其他 Origin 返回 403。

底层 `HTTPServer` 默认监听：

- `FHttpServerListenerConfig::BindAddress = "localhost"`。
- 可通过 `Engine.ini` 的 `[HTTPServer.Listeners] DefaultBindAddress` 或 per-port `ListenerOverrides` 改写。
- `HttpListener.cpp` 中 `"localhost"` 会映射到 loopback address，`"any"` 才会监听任意地址。

### Auto-start 只属于 Editor 集成

`UModelContextProtocolSettings` 是 `config=EditorPerProjectUserSettings`，包含：

- `ServerUrlPath`
- `ServerPortNumber`
- `bAutoStartServer`
- `bEnableToolSearch`

但 `ShouldAutoStartServer()` 的调用点在 `ModelContextProtocolEditor.cpp`：

- `FModelContextProtocolEditorModule::SetupEditorIntegration()` 中先注册 Toolset Registry adapter，再判断 `ShouldAutoStartServer()` 并调用 `StartServer()`。
- Runtime/Shipping 不加载 `ModelContextProtocolEditor`，因此不会自动执行该逻辑。

项目 Runtime 如果要启动 MCP server，应在自己的 Runtime 代码里显式调用：

```cpp
IModelContextProtocolModule::GetChecked().StartServer(8000, TEXT("/mcp"));
```

如果要允许命令行控制，可在项目模块里自行读取 `-TerraNpcMcpStartServer`、`-TerraNpcMcpPort=...`，不要依赖 Editor-only auto-start。

### Toolset Registry 与随附 Toolsets 不适合 Release Runtime

`ToolsetRegistry.uplugin`：

- `EditorOnly=true`
- 模块 `ToolsetRegistry`：`Type=Editor`
- `TargetAllowList=["Editor"]`
- 依赖 `PythonScriptPlugin`、`EditorScriptingUtilities`、`FileSandbox`

随附 Toolsets 示例：

- `AIModuleToolset.uplugin`：`EditorOnly=true`，模块 `Type=Editor`，`TargetAllowList=["Editor"]`
- `GameplayTagsToolset.uplugin`：`EditorOnly=true`，依赖 `GameplayTagsEditor`
- `ConfigSettingsToolset.uplugin`：`EditorOnly=true`
- `EditorToolset.uplugin`：`EditorOnly=true`
- `MCPClientToolset.uplugin`：`EditorOnly=true`，模块 `Type=Editor`，依赖 `ToolsetRegistry`、`UnrealEd`
- `AllToolsets.uplugin`：`EditorOnly=true`，聚合全部 Toolsets

`ModelContextProtocolToolsetRegistryAdapter.cpp` 也位于 `ModelContextProtocolEditor` 模块，并直接依赖：

- `Editor.h`
- `GEditor`
- `UToolsetRegistrySubsystem`
- `ToolsetRegistry`

结论：Release runtime 不能依赖 Toolset Registry 自动枚举和 adapter。项目应自行注册 `IModelContextProtocolTool`，或谨慎使用 `UModelContextProtocolToolLibrary` / `UModelContextProtocolToolAsyncAction` 这类 Runtime/legacy fallback，但优先推荐手写 C++ tool，便于控制 schema、线程、生命周期和 Gameplay 权威边界。

## 对 TerraCivilization 的架构建议

本项目当前 `Gameplay` 模块已经是 Runtime 模块，核心类是 `FTerraGameplayContainer`：

- 初始化：`Initialize`
- 回合状态：`GetCurrentFactionId()`、`GetTurnIndex()`、`IsMatchEnded()`
- 局势状态：`GetPieces()`、`GetFactions()`、`GetCellToPieceId()`
- 用户式行动入口：`HandleCellClick(int32 CellId, TArray<int32>& OutDirtyCellIds)`
- 内部规则查询已有雏形：
  - `CollectOrdinaryMoveTargets_`
  - `CollectJumpTargets_`
  - `CollectCaptureEntriesAfterHypotheticalMove_`
  - pending capture / highlight 机制
- 已有行动日志：
  - `BuildActionLogJson_`
  - `EmitActionLog_`

这说明最小闭环不需要先重写规则。推荐新增一个 Runtime AI 边界层，而不是让 LLM 直接改 Gameplay 状态。

建议新增模块或类：

- `NpcDecision` Runtime 模块，或放在 `Gameplay` 下的 `TerraNpcMcpDecisionSubsystem`。
- `FTerraGameplaySnapshot`：只读局势快照，带 `TurnIndex`、`CurrentFactionId`、cells、pieces、factions、action history 摘要。
- `FTerraNpcActionProposal`：LLM 输出的候选行动。
- `FTerraNpcActionValidator`：Gameplay 权威校验器。
- `FTerraNpcMcpTool_*`：若干 `IModelContextProtocolTool` 实现。
- `FTerraNpcDeterministicFallback`：LLM 超时或非法输出时的兜底走法。

不要让 MCP tool 直接执行最终行动。更稳的边界是：

1. MCP tools 只读局势、做确定性查询、返回候选列表或风险评分。
2. LLM 输出 `FTerraNpcActionProposal`。
3. Gameplay 层验证 proposal。
4. 验证通过后 Gameplay 执行。
5. 执行结果写入现有 action log，并补充 AI decision log。

## 建议的 Runtime MCP Tools

最小闭环只需要 5 个工具。

### `terra.get_turn_context`

输入：

```json
{
  "faction_id": 1
}
```

输出：

```json
{
  "turn_index": 12,
  "current_faction_id": 1,
  "match_ended": false,
  "my_piece_ids": [4, 5, 6],
  "enemy_piece_count": 9,
  "recent_actions_digest": []
}
```

用途：让 LLM 先确认当前是否轮到该 faction，以及有哪些棋子。

### `terra.list_legal_actions`

输入：

```json
{
  "faction_id": 1
}
```

输出：

```json
{
  "actions": [
    {
      "piece_id": 4,
      "from_cell": 100,
      "path_cell_ids": [100, 101],
      "action_kind": "ordinary_move",
      "would_capture_piece_ids": [9],
      "would_capture_cell_ids": [102]
    }
  ]
}
```

用途：由确定性代码枚举所有合法候选。LLM 不自行推导移动规则。

### `terra.evaluate_action_risk`

输入：

```json
{
  "faction_id": 1,
  "piece_id": 4,
  "path_cell_ids": [100, 101]
}
```

输出：

```json
{
  "legal": true,
  "capture_value": 3,
  "retaliation_threat_count": 1,
  "self_loss_risk": "medium",
  "commander_exposed": false
}
```

用途：计算“走到这里会不会被吃”、价值交换、是否暴露关键棋子。

### `terra.summarize_strategic_state`

输出：

```json
{
  "strongest_faction_id": 2,
  "weakest_faction_id": 3,
  "current_faction_rank": 2,
  "recent_hostility": [
    {
      "attacker_faction_id": 2,
      "target_faction_id": 1,
      "turn_index": 10
    }
  ]
}
```

用途：宏观局势查询，避免模型从完整日志里幻觉推理。

### `terra.submit_action_proposal`

输入：

```json
{
  "turn_index": 12,
  "faction_id": 1,
  "piece_id": 4,
  "path_cell_ids": [100, 101],
  "declared_intent": "capture_low_risk_enemy"
}
```

输出：

```json
{
  "accepted": true,
  "normalized_action": {
    "turn_index": 12,
    "faction_id": 1,
    "piece_id": 4,
    "path_cell_ids": [100, 101]
  },
  "validation_errors": []
}
```

用途：只提交 proposal，不直接改局。Gameplay 仍是最终执行方。

## 可验证最小闭环

目标：在不依赖 Editor Toolset Registry 的前提下，验证 Shipping-compatible Runtime MCP 路线能驱动一个 NPC 回合。

### 阶段 1：本地 Runtime MCP server 可见

实现内容：

1. 新增 Runtime 代码，在非 Shipping 或带显式命令行参数时启动 server：

```cpp
if (FParse::Param(FCommandLine::Get(), TEXT("TerraNpcMcpStartServer")))
{
    IModelContextProtocolModule::GetChecked().StartServer(8765, TEXT("/terra-npc-mcp"));
}
```

2. `Build.cs` 增加依赖：

```csharp
"ModelContextProtocol",
"ModelContextProtocolEngine"
```

3. 注册一个最小工具 `terra.ping_gameplay`：

```json
{
  "turn_index": 0,
  "current_faction_id": 0,
  "piece_count": 12
}
```

验收：

- 启动 PIE 或 Standalone，命令行带 `-TerraNpcMcpStartServer -TerraNpcMcpPort=8765`。
- MCP Inspector 连接 `http://127.0.0.1:8765/terra-npc-mcp`。
- `tools/list` 能看到 `terra.ping_gameplay`。
- `tools/call` 返回当前 Gameplay 数据。

### 阶段 2：只读战术工具闭环

实现内容：

1. 给 `FTerraGameplayContainer` 增加只读 public 查询接口，或新增 friend/adapter，避免 MCP tool 调 private 函数：
   - `BuildSnapshot()`
   - `CollectLegalNpcActions(FactionId)`
   - `EvaluateNpcActionRisk(Action)`
2. 工具注册：
   - `terra.get_turn_context`
   - `terra.list_legal_actions`
   - `terra.evaluate_action_risk`

验收：

- 对同一局势连续调用返回稳定结果。
- 单元测试或 automation test 验证：候选 action 都能被 Gameplay 原规则接受。
- 对非法 faction / stale turn / dead piece 返回结构化错误，不崩溃。

### 阶段 3：LLM proposal，不执行

实现内容：

1. 外部 LLM agent 通过 MCP tools 查询局势。
2. LLM 最终调用 `terra.submit_action_proposal`。
3. 游戏只记录 proposal，不执行。

验收：

- 日志出现：

```json
{
  "type": "npc_ai_proposal",
  "turn_index": 12,
  "faction_id": 1,
  "piece_id": 4,
  "path_cell_ids": [100, 101],
  "accepted": true
}
```

- 如果 LLM 提交非法 path，返回 `accepted=false` 和 `validation_errors`。
- 不管 LLM 输出什么，Gameplay 局势不会被 MCP tool 直接篡改。

### 阶段 4：执行一个 NPC 回合

实现内容：

1. Gameplay 回合切到 NPC faction。
2. `TerraNpcDecisionSubsystem` 请求外部 LLM agent 产生 proposal。
3. proposal 通过 validator 后，Gameplay 执行：
   - MVP 可先复用 `HandleCellClick` 序列：点击起点、目标、终点结束回合。
   - 后续建议补显式行动入口：`TryExecuteValidatedAction(FTerraNpcActionProposal)`，减少 UI click 语义耦合。
4. 执行后复用当前 `EmitActionLog_`，并追加 AI decision log。

验收：

- NPC 能完成一个合法行动并推进 `TurnIndex`。
- `ActionLog` 中出现与 proposal 一致的 `pieceId`、`pathCellIds`、capture 信息。
- LLM 超时、JSON 非法、proposal 失效时，fallback AI 能执行一个确定性合法行动或跳过。

### 阶段 5：Development/Shipping 打包验证

实现内容：

1. 打 Development packaged build。
2. 用命令行显式开启：

```text
TerraCivilization.exe -TerraNpcMcpStartServer -TerraNpcMcpPort=8765 -ini:Engine:[HTTPServer.Listeners]:DefaultBindAddress=localhost
```

3. MCP Inspector 连接本机端口。

验收：

- packaged build 中 `terra.ping_gameplay` 可调用。
- 不启用 `-TerraNpcMcpStartServer` 时，端口不开放。
- `DefaultBindAddress=localhost` 时，局域网其他机器不能访问。
- 若临时设置 `DefaultBindAddress=any`，必须有额外鉴权或调试开关；默认不允许。

## 风险与约束

### 实验性与 NoRedist

UE 5.8 Unreal MCP / Toolset 相关插件带有 `IsExperimentalVersion`，部分插件带 `NoRedist=true`。技术可用不等于可以无条件随产品分发。真正进入可发布版本前，需要单独确认 Epic EULA、插件分发策略和目标平台要求。

### 安全风险

MCP server 是本地 HTTP server。即使默认 loopback，也应做以下限制：

- 默认关闭，只通过命令行或开发配置打开。
- 默认绑定 `localhost`。
- tool 白名单，不暴露任意 UObject 编辑、文件系统、console command、Python。
- tool 参数 schema 严格，所有输入都二次校验。
- 不在 tool 返回里泄露用户路径、密钥、完整日志、未授权内容。
- Shipping 版本如必须启用，需要 session token 或本地 companion 进程鉴权；UE 5.8 默认 MCP server 没有应用层 token。

### 延迟与回合体验

LLM 决策应有硬超时。建议 MVP：

- 单个 NPC 回合预算：2-5 秒。
- 超时一次 fallback。
- 每回合最多一次重试。
- 缓存只读工具结果，避免 LLM 多次重复查询大局势。

### 可复现性

每次 AI 决策要记录：

- prompt / skill 版本号
- tool 调用摘要
- snapshot hash
- proposal JSON
- validator 结果
- fallback 是否触发

否则很难复盘“为什么 NPC 这样走”。

## 推荐路线

短期推荐：

1. 不启用 `AllToolsets` 做 runtime NPC AI。
2. 保留 `ModelContextProtocol`、`ModelContextProtocolEngine` 作为 Runtime MCP server。
3. 写项目自定义 `IModelContextProtocolTool`。
4. 从 `terra.ping_gameplay` + `terra.list_legal_actions` + `terra.submit_action_proposal` 做最小闭环。

中期推荐：

1. 从 `FTerraGameplayContainer` 抽出 public deterministic query API。
2. 新增 `TryExecuteValidatedAction`，避免 NPC 执行依赖 UI click 状态机。
3. 建立 AI decision log 和 deterministic fallback。
4. 将外部 LLM agent 作为可替换进程：OpenAI / Claude / local model 都只看 MCP tools 和最终 schema。

长期推荐：

1. 将 MCP 工具视为“NPC 决策解释层”，不要把规则权威交给 LLM。
2. 如果后续需要离线或无网络版本，保留同一套 deterministic query API，替换 LLM planner 即可。
3. 如果希望多阵营有性格差异，把性格和战略倾向放在 skill/prompt 与 faction config 中；规则仍由工具计算。

