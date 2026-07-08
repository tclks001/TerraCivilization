#include "NpcMcp.h"

#include "IModelContextProtocolModule.h"
#include "IModelContextProtocolTool.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerraNpcMcp, Log, All);

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpPingGameplayTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpGetTurnContextTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpListLegalActionsTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpEvaluateActionRiskTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpSubmitActionProposalTool();

IMPLEMENT_MODULE(FNpcMcpModule, NpcMcp)

namespace
{
    IModelContextProtocolModule* LoadModelContextProtocolModule_()
    {
        return FModuleManager::Get().LoadModulePtr<IModelContextProtocolModule>(TEXT("ModelContextProtocol"));
    }
}

void FNpcMcpModule::StartupModule()
{
    RegisterTools_();
    StartServerIfRequested_();
}

void FNpcMcpModule::ShutdownModule()
{
    UnregisterTools_();
}

void FNpcMcpModule::RegisterTools_()
{
    IModelContextProtocolModule* McpModule = LoadModelContextProtocolModule_();
    if (!McpModule)
    {
        UE_LOG(LogTerraNpcMcp, Error, TEXT("[NpcMcp] Failed to load ModelContextProtocol module. Check that the ModelContextProtocol plugin is enabled for this target."));
        return;
    }

    RegisteredTools.Add(MakeTerraNpcMcpPingGameplayTool());
    RegisteredTools.Add(MakeTerraNpcMcpGetTurnContextTool());
    RegisteredTools.Add(MakeTerraNpcMcpListLegalActionsTool());
    RegisteredTools.Add(MakeTerraNpcMcpEvaluateActionRiskTool());
    RegisteredTools.Add(MakeTerraNpcMcpSubmitActionProposalTool());

    for (const TSharedRef<IModelContextProtocolTool>& Tool : RegisteredTools)
    {
        const bool bAdded = McpModule->AddTool(Tool);
        UE_LOG(LogTerraNpcMcp, Log, TEXT("[NpcMcp] Register tool %s: %s"), *Tool->GetName(), bAdded ? TEXT("ok") : TEXT("duplicate"));
    }
}

void FNpcMcpModule::UnregisterTools_()
{
    IModelContextProtocolModule* McpModule = IModelContextProtocolModule::Get();
    if (McpModule)
    {
        for (const TSharedRef<IModelContextProtocolTool>& Tool : RegisteredTools)
        {
            McpModule->RemoveTool(Tool);
        }
    }

    RegisteredTools.Reset();
}

void FNpcMcpModule::StartServerIfRequested_()
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("TerraNpcMcpStartServer")))
    {
        UE_LOG(LogTerraNpcMcp, Log, TEXT("[NpcMcp] Runtime MCP server disabled. Pass -TerraNpcMcpStartServer to enable."));
        return;
    }

    uint32 Port = 8765;
    FParse::Value(FCommandLine::Get(), TEXT("TerraNpcMcpPort="), Port);

    FString UrlPath = TEXT("/terra-npc-mcp");
    FParse::Value(FCommandLine::Get(), TEXT("TerraNpcMcpPath="), UrlPath);
    if (!UrlPath.StartsWith(TEXT("/")))
    {
        UrlPath.InsertAt(0, TEXT("/"));
    }

    IModelContextProtocolModule* McpModule = LoadModelContextProtocolModule_();
    if (!McpModule)
    {
        UE_LOG(LogTerraNpcMcp, Error, TEXT("[NpcMcp] Cannot start runtime MCP server because ModelContextProtocol module failed to load."));
        return;
    }

    McpModule->StartServer(Port, UrlPath);

    UE_LOG(LogTerraNpcMcp, Display, TEXT("[NpcMcp] Runtime MCP server requested at http://127.0.0.1:%u%s"), Port, *UrlPath);
}
