# Unreal MCP Runtime 使用手册模板

日期：2026-07-08

范围：UE 5.8 `ModelContextProtocol` Runtime 用法。本文只覆盖游戏 Runtime 中自定义 MCP server 和 tool 的最小集成方式，不覆盖 Editor AI Toolset Registry。

## 1. 插件启用

在 `.uproject` 中启用 `ModelContextProtocol` 插件：

```json
{
  "Name": "ModelContextProtocol",
  "Enabled": true
}
```

注意：

- `ModelContextProtocol` / `ModelContextProtocolEngine` 是 Runtime 模块。
- UE 随附的 `ToolsetRegistry` 以及多数 AI Toolset 插件是 Editor-only，不要作为 Shipping runtime 的 tool 注册机制。
- Runtime 中建议直接实现并注册 `IModelContextProtocolTool`。

## 2. Runtime 模块依赖

建议新建一个独立 Runtime 模块承载 MCP 集成，例如 `GameMcp`。

`.uproject`：

```json
{
  "Name": "GameMcp",
  "Type": "Runtime",
  "LoadingPhase": "Default"
}
```

`GameMcp.Build.cs`：

```csharp
using UnrealBuildTool;

public class GameMcp : ModuleRules
{
    public GameMcp(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Json",
            "JsonUtilities",
            "ModelContextProtocol"
        });
    }
}
```

`JsonUtilities` 通常需要加入，因为 `FModelContextProtocolToolResult` 依赖 `FJsonObjectWrapper`。

## 3. 显式加载 MCP 模块

Runtime 中不要假设 `ModelContextProtocol` 已经加载。推荐显式加载：

```cpp
#include "IModelContextProtocolModule.h"
#include "Modules/ModuleManager.h"

IModelContextProtocolModule* LoadMcpModule()
{
    return FModuleManager::Get().LoadModulePtr<IModelContextProtocolModule>(
        TEXT("ModelContextProtocol"));
}
```

避免只用：

```cpp
IModelContextProtocolModule::Get()
```

`Get()` 只查询已加载模块，不触发加载。

## 4. MCP Server 启动

建议默认关闭 Runtime MCP server，通过命令行显式开启：

```cpp
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void StartMcpServerIfRequested()
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("GameMcpStartServer")))
    {
        return;
    }

    uint32 Port = 8765;
    FParse::Value(FCommandLine::Get(), TEXT("GameMcpPort="), Port);

    FString UrlPath = TEXT("/game-mcp");
    FParse::Value(FCommandLine::Get(), TEXT("GameMcpPath="), UrlPath);
    if (!UrlPath.StartsWith(TEXT("/")))
    {
        UrlPath.InsertAt(0, TEXT("/"));
    }

    if (IModelContextProtocolModule* McpModule = LoadMcpModule())
    {
        McpModule->StartServer(Port, UrlPath);
    }
}
```

启动参数示例：

```text
-GameMcpStartServer -GameMcpPort=8765 -GameMcpPath=/game-mcp
```

安全建议：

- 默认不启动 server。
- 默认只用于本机调试。
- 不默认暴露到 LAN/WAN。
- 不提供任意文件系统、console command、UObject 编辑等高风险 tool。

## 5. Tool 实现

实现 `IModelContextProtocolTool`：

```cpp
#include "IModelContextProtocolTool.h"
#include "ModelContextProtocolToolResults.h"

class FExamplePingTool final : public IModelContextProtocolTool
{
public:
    virtual FString GetName() const override
    {
        return TEXT("game.ping");
    }

    virtual FString GetDescription() const override
    {
        return TEXT("Returns a minimal runtime MCP health check.");
    }

    virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override
    {
        TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        Schema->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());
        Schema->SetBoolField(TEXT("additionalProperties"), false);
        return Schema;
    }

    virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("ok"), true);
        Result->SetStringField(TEXT("message"), TEXT("pong"));

        TSharedPtr<FJsonValue> StructuredContent =
            MakeShared<FJsonValueObject>(Result);
        return UE::ModelContextProtocol::MakeStructuredContentResult(StructuredContent);
    }
};
```

注意：

- `GetInputJsonSchema()` 应尽量严格，避免接受任意参数。
- `Run()` 内仍要二次校验参数，不能只信 schema。
- 结构化返回推荐使用 `MakeStructuredContentResult(TSharedPtr<FJsonValue>)`。
- 如果直接传 `MakeShared<FJsonValueObject>(Result)`，C++ 可能误选模板重载；先显式转成 `TSharedPtr<FJsonValue>` 更稳。

## 6. Tool 注册与注销

在模块中保存 tool 引用，并注册到 MCP module：

```cpp
class FGameMcpModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        RegisterTools();
        StartMcpServerIfRequested();
    }

    virtual void ShutdownModule() override
    {
        UnregisterTools();
    }

private:
    void RegisterTools()
    {
        IModelContextProtocolModule* McpModule = LoadMcpModule();
        if (!McpModule)
        {
            return;
        }

        RegisteredTools.Add(MakeShared<FExamplePingTool>());

        for (const TSharedRef<IModelContextProtocolTool>& Tool : RegisteredTools)
        {
            McpModule->AddTool(Tool);
        }
    }

    void UnregisterTools()
    {
        if (IModelContextProtocolModule* McpModule = IModelContextProtocolModule::Get())
        {
            for (const TSharedRef<IModelContextProtocolTool>& Tool : RegisteredTools)
            {
                McpModule->RemoveTool(Tool);
            }
        }

        RegisteredTools.Reset();
    }

private:
    TArray<TSharedRef<IModelContextProtocolTool>> RegisteredTools;
};

IMPLEMENT_MODULE(FGameMcpModule, GameMcp)
```

相关 API：

```cpp
IModelContextProtocolModule::AddTool(...)
IModelContextProtocolModule::RemoveTool(...)
IModelContextProtocolModule::StartServer(...)
IModelContextProtocolModule::StopServer()
IModelContextProtocolModule::FindTool(...)
IModelContextProtocolModule::GetTools()
```

## 7. Gameplay/Runtime 状态访问建议

Tool 不建议直接搜索世界内对象或修改游戏状态。更稳的方式是提供一个明确的 runtime bridge 或 subsystem：

```cpp
class FGameMcpRuntimeBridge
{
public:
    static void RegisterRuntimeState(const FMyRuntimeState* InState);
    static void UnregisterRuntimeState(const FMyRuntimeState* InState);
    static const FMyRuntimeState* GetRuntimeState();
};
```

原则：

- Tool 层优先只读。
- 写操作走 proposal/validate/execute 边界。
- Gameplay 或 domain system 保持规则权威。
- MCP tool 只负责查询、解释、提交候选，不直接绕过规则系统。

## 8. MCP Inspector 验证

启动游戏或 Editor：

```text
-GameMcpStartServer -GameMcpPort=8765 -GameMcpPath=/game-mcp
```

启动 Inspector：

```powershell
npx @modelcontextprotocol/inspector@latest
```

在 Inspector 中连接：

```text
Transport Type: Streamable HTTP
URL: http://127.0.0.1:8765/game-mcp
```

验证：

1. 点击 `Connect`。
2. 打开 `Tools`。
3. `List Tools`。
4. 选择 `game.ping`。
5. 输入 `{}`。
6. `Run Tool`。

期望返回：

```json
{
  "ok": true,
  "message": "pong"
}
```

## 9. 常见问题

### MCP module 不可用

现象：

```text
ModelContextProtocol module is not available
```

处理：

- 确认 `.uproject` 启用了 `ModelContextProtocol`。
- 确认模块 `Build.cs` 依赖 `ModelContextProtocol`。
- 使用 `FModuleManager::LoadModulePtr<IModelContextProtocolModule>()` 显式加载。

### 链接 `FJsonObjectWrapper` 失败

处理：

- 在 `.Build.cs` 中加入 `JsonUtilities`。

### Inspector 连接失败

检查：

- 游戏进程是否带了启动参数。
- URL 是否包含 path，例如 `/game-mcp`。
- 是否选择 `Streamable HTTP`，而不是 `STDIO`。
- 端口是否被占用。

### 工具列表为空

检查：

- `AddTool()` 是否成功。
- tool 名称是否重复。
- MCP module 是否在注册 tool 前成功加载。

## 10. 推荐分阶段路线

1. `ping` tool：验证 MCP server 和 tool 注册。
2. 只读 query tools：查询 runtime 状态和确定性规则结果。
3. proposal tool：提交候选动作，只校验不执行。
4. execute tool：在严格 validator 之后执行动作。
5. decision log：记录 prompt/tool/proposal/validator/fallback，保证可复盘。
