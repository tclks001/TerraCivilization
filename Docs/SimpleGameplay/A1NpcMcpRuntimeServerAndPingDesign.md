# UE 5.8 NPC MCP Runtime 阶段 1 设计稿

日期：2026-07-08

## 目标

阶段 1 只验证 Runtime MCP 最小通路：

1. 游戏 Runtime 能注册项目自定义 MCP tool。
2. MCP server 默认关闭，只能通过命令行显式启动。
3. 外部 MCP client 能调用 `terra.ping_gameplay`。
4. `terra.ping_gameplay` 能读到真实 `FTerraGameplayContainer` 的只读状态。

本阶段不做 LLM 决策、不做走法枚举、不执行 NPC 行动。

## 核心结论

UE 5.8 的 Editor AI Toolset Registry 不能作为 Release runtime NPC AI 的工具注册层。阶段 1 采用：

```text
NpcMcp Runtime 模块
  -> 显式加载 ModelContextProtocol runtime module
  -> 直接注册 IModelContextProtocolTool
  -> 按命令行开关启动 MCP HTTP server
  -> 通过项目桥接层只读访问 Gameplay 容器
```

这条路径不依赖 Editor-only Toolset Registry，也不要求 AI Toolset Editor 工作流。

## 模块边界

新增 Runtime 模块 `NpcMcp`：

- 依赖 `ModelContextProtocol`、`Gameplay`、`Json`、`JsonUtilities`。
- 不依赖 UE Editor-only Toolset Registry。
- 只注册项目白名单工具。
- 不直接修改 Gameplay 状态。
- 通过 `FTerraNpcMcpGameplayBridge` 持有当前 Gameplay 容器的只读指针。

`Gameplay` 仍然是规则权威；`NpcMcp` 本阶段只读。

## C++ 实现框架

### 1. 模块挂载

已在 `.uproject` 中新增 Runtime 模块：

```json
{
  "Name": "NpcMcp",
  "Type": "Runtime",
  "LoadingPhase": "Default"
}
```

已在目标文件中加入：

```csharp
ExtraModuleNames.Add("NpcMcp");
```

已在主游戏模块依赖中加入 `NpcMcp`，让 `APlanetTessellatedMesh` 能调用桥接 API：

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "...",
    "Gameplay",
    "PiecePresentation",
    "NpcMcp"
});
```

### 2. `NpcMcp.Build.cs`

`NpcMcp` 的依赖：

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "Gameplay",
    "Json",
    "JsonUtilities",
    "ModelContextProtocol"
});
```

注意点：

- `ModelContextProtocol` 提供 `IModelContextProtocolModule`、`IModelContextProtocolTool` 和 tool result API。
- `Json` 用于手写输入/输出 schema 和 structured JSON。
- `JsonUtilities` 是 `FModelContextProtocolToolResult` 依赖的 `FJsonObjectWrapper` 所需链接模块；缺少它会出现 `FJsonObjectWrapper` 链接错误。
- 本阶段没有依赖 `ModelContextProtocolEngine`，因为没有使用 Engine settings 类或 Editor preferences。

### 3. `FNpcMcpModule`

文件：

- `Source/NpcMcp/Public/NpcMcp.h`
- `Source/NpcMcp/Private/NpcMcp.cpp`

模块生命周期：

```cpp
void FNpcMcpModule::StartupModule()
{
    RegisterTools_();
    StartServerIfRequested_();
}

void FNpcMcpModule::ShutdownModule()
{
    UnregisterTools_();
}
```

实现思路：

1. 模块启动时注册项目工具。
2. 如果命令行带 `-TerraNpcMcpStartServer`，启动 MCP server。
3. 模块关闭时注销已注册工具。

### 4. 显式加载 MCP runtime 模块

最初使用过：

```cpp
IModelContextProtocolModule::Get()
```

这个 API 只查询“已经加载”的模块，不会触发加载。在 Standalone PIE 中，`NpcMcp` 启动时 `ModelContextProtocol` 可能还没有加载，会导致：

```text
[NpcMcp] ModelContextProtocol module is not available.
```

阶段 1 已修正为：

```cpp
IModelContextProtocolModule* McpModule =
    FModuleManager::Get().LoadModulePtr<IModelContextProtocolModule>(TEXT("ModelContextProtocol"));
```

设计原则：

- Runtime 功能模块不要假设插件模块已经被加载。
- `RegisterTools_()` 和 `StartServerIfRequested_()` 都通过同一个 helper 显式加载 MCP 模块。
- 如果加载失败，记录 error log 并停止启动，不调用 `GetChecked()`。

### 5. 工具注册

使用 UE 5.8 Unreal MCP runtime API：

```cpp
IModelContextProtocolModule::AddTool(const TSharedRef<IModelContextProtocolTool>& Tool)
IModelContextProtocolModule::RemoveTool(const TSharedRef<IModelContextProtocolTool>& Tool)
```

阶段 1 注册：

```text
terra.ping_gameplay
```

工具对象保存在 `FNpcMcpModule::RegisteredTools` 中，模块关闭时逐个 `RemoveTool`。

### 6. MCP server 启动

启动参数：

```text
-TerraNpcMcpStartServer -TerraNpcMcpPort=8765 -TerraNpcMcpPath=/terra-npc-mcp
```

解析 API：

```cpp
FParse::Param(FCommandLine::Get(), TEXT("TerraNpcMcpStartServer"))
FParse::Value(FCommandLine::Get(), TEXT("TerraNpcMcpPort="), Port)
FParse::Value(FCommandLine::Get(), TEXT("TerraNpcMcpPath="), UrlPath)
```

启动 API：

```cpp
IModelContextProtocolModule::StartServer(uint32 Port, const FString& UrlPath)
```

默认值：

```text
Port: 8765
Path: /terra-npc-mcp
```

安全边界：

- 默认不启动 server。
- 必须显式传 `-TerraNpcMcpStartServer`。
- 阶段 1 只面向本机调试，保持 UE HTTPServer 默认 localhost 绑定。
- 不暴露文件系统、console command、UObject 任意编辑能力。

### 7. Gameplay 桥接

文件：

- `Source/NpcMcp/Public/TerraNpcMcpGameplayBridge.h`
- `Source/NpcMcp/Private/TerraNpcMcpGameplayBridge.cpp`

桥接 API：

```cpp
class FTerraNpcMcpGameplayBridge
{
public:
    static void RegisterGameplayContainer(const FTerraGameplayContainer* InGameplayContainer);
    static void UnregisterGameplayContainer(const FTerraGameplayContainer* InGameplayContainer);
    static const FTerraGameplayContainer* GetGameplayContainer();
};
```

实现方式：

- 内部保存一个 `const FTerraGameplayContainer*`。
- 用 `FCriticalSection` + `FScopeLock` 保护读写。
- 只暴露 const 指针，阶段 1 不允许 MCP 工具修改 Gameplay。

为什么不用全局 UObject 查找：

- 当前 `FTerraGameplayContainer` 是 `APlanetTessellatedMesh` 持有的 `TUniquePtr`，不是 UObject。
- 主动注册/注销生命周期更明确。
- 后续阶段可以把这个桥接替换成 World subsystem 或 GameInstance subsystem，但 tool 层不需要大改。

### 8. Gameplay 容器生命周期接入

文件：

- `Source/TerraCivilization/Private/Render/PlanetTessellatedMesh.cpp`

接入点：

1. `APlanetTessellatedMesh::~APlanetTessellatedMesh()`
   - 如果 `GameplayContainer` 仍有效，注销桥接。

2. `APlanetTessellatedMesh::RebuildGameplay_()`
   - 当 `CellTopology` 或 `Generator` 无效时，先注销旧容器，再 `Reset()`。
   - 当 WorldGen cell count mismatch 时，先注销旧容器，再 `Reset()`。
   - 正常重建前，如果已有旧容器，先注销。
   - 新容器 `Initialize()` 后注册：

```cpp
GameplayContainer = MakeUnique<FTerraGameplayContainer>();
GameplayContainer->Initialize(GameplayCells);
GameplayContainer->SetDebugKeepSameFactionOnEndTurn(bG3DebugKeepSameFactionOnEndTurn);
FTerraNpcMcpGameplayBridge::RegisterGameplayContainer(GameplayContainer.Get());
```

生命周期约束：

- 桥接指针只在容器有效期间存在。
- 容器销毁或重建前必须注销。
- 阶段 1 不在 `BeginDestroy()` 手动 reset `TUniquePtr`，仍遵守现有生命周期约定。

### 9. `terra.ping_gameplay` 工具实现

文件：

- `Source/NpcMcp/Private/TerraNpcMcpPingGameplayTool.cpp`

实现类：

```cpp
class FTerraNpcMcpPingGameplayTool final : public IModelContextProtocolTool
```

实现的 MCP tool API：

```cpp
FString GetName() const
FString GetDescription() const
TSharedPtr<FJsonObject> GetInputJsonSchema() const
TSharedPtr<FJsonObject> GetOutputJsonSchema() const
FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params)
```

输入 schema：

```json
{
  "type": "object",
  "properties": {},
  "additionalProperties": false
}
```

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

读取的 Gameplay API：

```cpp
FTerraGameplayContainer::IsInitialized()
FTerraGameplayContainer::GetTurnIndex()
FTerraGameplayContainer::GetCurrentFactionId()
FTerraGameplayContainer::GetPieces()
FTerraGameplayContainer::GetFactions()
FTerraGameplayContainer::IsMatchEnded()
FTerraGameplayContainer::GetWinningFactionId()
```

返回结果 API：

```cpp
UE::ModelContextProtocol::MakeStructuredContentResult(TSharedPtr<FJsonValue>)
```

注意点：

- `MakeStructuredContentResult(MakeShared<FJsonValueObject>(Result))` 会被 C++ 模板错误匹配到 UStruct 路径。
- 当前实现先显式转成 `TSharedPtr<FJsonValue>`，确保调用 JSON structuredContent 重载：

```cpp
TSharedPtr<FJsonValue> StructuredContent = MakeShared<FJsonValueObject>(Result);
return UE::ModelContextProtocol::MakeStructuredContentResult(StructuredContent);
```

### 10. 日志

成功注册工具：

```text
[NpcMcp] Register tool terra.ping_gameplay: ok
```

默认未启动 server：

```text
[NpcMcp] Runtime MCP server disabled. Pass -TerraNpcMcpStartServer to enable.
```

显式启动 server：

```text
[NpcMcp] Runtime MCP server requested at http://127.0.0.1:8765/terra-npc-mcp
```

MCP 模块加载失败：

```text
[NpcMcp] Failed to load ModelContextProtocol module. Check that the ModelContextProtocol plugin is enabled for this target.
```

## 当前文件清单

新增：

- `Source/NpcMcp/NpcMcp.Build.cs`
- `Source/NpcMcp/Public/NpcMcp.h`
- `Source/NpcMcp/Public/TerraNpcMcpGameplayBridge.h`
- `Source/NpcMcp/Private/NpcMcp.cpp`
- `Source/NpcMcp/Private/TerraNpcMcpGameplayBridge.cpp`
- `Source/NpcMcp/Private/TerraNpcMcpPingGameplayTool.cpp`
- `Docs/SimpleGameplay/A1NpcMcpRuntimeServerAndPingDesign.md`

修改：

- `TerraCivilization.uproject`
- `Source/TerraCivilization.Target.cs`
- `Source/TerraCivilizationEditor.Target.cs`
- `Source/TerraCivilization/TerraCivilization.Build.cs`
- `Source/TerraCivilization/Private/Render/PlanetTessellatedMesh.cpp`

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

输出示例：

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

Standalone PIE 日志里的 command line 也应该包含：

```text
-TerraNpcMcpStartServer -TerraNpcMcpPort=8765 -TerraNpcMcpPath=/terra-npc-mcp
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

#### 日志显示 `ModelContextProtocol module is not available`

这是早期实现的问题：`IModelContextProtocolModule::Get()` 不会触发模块加载。当前实现已改为 `FModuleManager::LoadModulePtr`。

如果仍然出现类似错误，检查：

1. `TerraCivilization.uproject` 中 `ModelContextProtocol` 插件是否启用。
2. 当前 target 是否包含 `NpcMcp` 模块。
3. 是否重新编译了 Editor target。

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

## 对阶段 2 的参考约束

阶段 2 增加 deterministic query tools：

- `terra.get_turn_context`
- `terra.list_legal_actions`
- `terra.evaluate_action_risk`
- `terra.submit_action_proposal`

建议沿用阶段 1 框架：

1. 所有新 tool 继续实现 `IModelContextProtocolTool`，由 `FNpcMcpModule::RegisterTools_()` 白名单注册。
2. 所有 tool 输入必须有严格 JSON schema，并在 `Run()` 中做二次校验。
3. 所有 Gameplay 查询先抽成 `FTerraGameplayContainer` public deterministic query API，不让 MCP tool 调私有 helper。
4. `submit_action_proposal` 只提交 proposal，真正执行仍由 Gameplay validator 决定。
5. MCP tool 默认只读；任何写操作必须走明确的 proposal/validate/execute 边界。
6. 日志继续记录 tool 名称、输入摘要、snapshot hash、validator 结果，方便复盘。
