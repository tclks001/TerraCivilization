#include "NpcMcp.h"

#include "IModelContextProtocolModule.h"
#include "IModelContextProtocolTool.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "TerraNpcMcpGameplayBridge.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerraNpcMcp, Log, All);

static FNpcMcpModule* GTerraNpcMcpModule = nullptr;

TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpPingGameplayTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpGetTurnContextTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpListLegalActionsTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpEvaluateActionRiskTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpSubmitActionProposalTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpExecuteValidatedActionTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpUiBeginTurnReviewTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpUiSelectPieceTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpUiPreviewMoveTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpUiCancelSelectionTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpUiConfirmActionTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpStrategicFactionStateTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpStrategicFrontlineTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpStrategicTerrainTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpStrategicEnemyPressureTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpStrategicOptionsTool();
TSharedRef<IModelContextProtocolTool> MakeTerraNpcMcpInspectLocalTopologyTool();

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
    GTerraNpcMcpModule = this;
    RegisterTools_();
    StartServerIfRequested_();
}

void FNpcMcpModule::ShutdownModule()
{
    UnregisterTools_();
    GTerraNpcMcpModule = nullptr;
}

void FNpcMcpModule::RefreshInteractiveToolAvailability()
{
    if (GTerraNpcMcpModule)
    {
        GTerraNpcMcpModule->RefreshInteractiveToolAvailability_();
    }
}

void FNpcMcpModule::RegisterTools_()
{
    IModelContextProtocolModule* McpModule = LoadModelContextProtocolModule_();
    if (!McpModule)
    {
        UE_LOG(LogTerraNpcMcp, Error, TEXT("[NpcMcp] Failed to load ModelContextProtocol module. Check that the ModelContextProtocol plugin is enabled for this target."));
        return;
    }

    AllToolsByName.Add(TEXT("terra.ping_gameplay"), MakeTerraNpcMcpPingGameplayTool());
    AllToolsByName.Add(TEXT("terra.get_turn_context"), MakeTerraNpcMcpGetTurnContextTool());
    AllToolsByName.Add(TEXT("terra.list_legal_actions"), MakeTerraNpcMcpListLegalActionsTool());
    AllToolsByName.Add(TEXT("terra.evaluate_action_risk"), MakeTerraNpcMcpEvaluateActionRiskTool());
    AllToolsByName.Add(TEXT("terra.submit_action_proposal"), MakeTerraNpcMcpSubmitActionProposalTool());
    AllToolsByName.Add(TEXT("terra.execute_validated_action"), MakeTerraNpcMcpExecuteValidatedActionTool());
    AllToolsByName.Add(TEXT("terra.ui_begin_turn_review"), MakeTerraNpcMcpUiBeginTurnReviewTool());
    AllToolsByName.Add(TEXT("terra.ui_select_piece"), MakeTerraNpcMcpUiSelectPieceTool());
    AllToolsByName.Add(TEXT("terra.ui_preview_move"), MakeTerraNpcMcpUiPreviewMoveTool());
    AllToolsByName.Add(TEXT("terra.ui_cancel_selection"), MakeTerraNpcMcpUiCancelSelectionTool());
    AllToolsByName.Add(TEXT("terra.ui_confirm_action"), MakeTerraNpcMcpUiConfirmActionTool());
    AllToolsByName.Add(TEXT("terra.strategy.summarize_faction_state"), MakeTerraNpcMcpStrategicFactionStateTool());
    AllToolsByName.Add(TEXT("terra.strategy.describe_frontline"), MakeTerraNpcMcpStrategicFrontlineTool());
    AllToolsByName.Add(TEXT("terra.strategy.find_terrain_control_points"), MakeTerraNpcMcpStrategicTerrainTool());
    AllToolsByName.Add(TEXT("terra.strategy.find_enemy_pressure"), MakeTerraNpcMcpStrategicEnemyPressureTool());
    AllToolsByName.Add(TEXT("terra.strategy.describe_strategic_options"), MakeTerraNpcMcpStrategicOptionsTool());
    AllToolsByName.Add(TEXT("terra.inspect_local_topology"), MakeTerraNpcMcpInspectLocalTopologyTool());

    const TArray<FString> AlwaysOnToolNames = {
        TEXT("terra.ping_gameplay"),
        TEXT("terra.get_turn_context"),
        TEXT("terra.list_legal_actions"),
        TEXT("terra.evaluate_action_risk"),
        TEXT("terra.submit_action_proposal"),
        TEXT("terra.execute_validated_action"),
        TEXT("terra.strategy.summarize_faction_state"),
        TEXT("terra.strategy.describe_frontline"),
        TEXT("terra.strategy.find_terrain_control_points"),
        TEXT("terra.strategy.find_enemy_pressure"),
        TEXT("terra.strategy.describe_strategic_options"),
        TEXT("terra.inspect_local_topology")
    };

    for (const FString& ToolName : AlwaysOnToolNames)
    {
        const TSharedRef<IModelContextProtocolTool>* Tool = AllToolsByName.Find(ToolName);
        if (!Tool)
        {
            continue;
        }
        RegisteredTools.Add(*Tool);
        RegisteredToolNames.Add(ToolName);
        const bool bAdded = McpModule->AddTool(*Tool);
        UE_LOG(LogTerraNpcMcp, Log, TEXT("[NpcMcp] Register tool %s: %s"), *ToolName, bAdded ? TEXT("ok") : TEXT("duplicate"));
    }

    RefreshInteractiveToolAvailability_();
}

void FNpcMcpModule::RefreshInteractiveToolAvailability_()
{
    IModelContextProtocolModule* McpModule = LoadModelContextProtocolModule_();
    if (!McpModule)
    {
        return;
    }

    TSet<FString> DesiredInteractiveToolNames;
    if (const FTerraGameplayContainer* GameplayContainer = FTerraNpcMcpGameplayBridge::GetGameplayContainer();
        GameplayContainer && GameplayContainer->IsInitialized() && !GameplayContainer->IsMatchEnded())
    {
        switch (GameplayContainer->GetInteractionPhase())
        {
        case ETerraGameplayInteractionPhase::Idle:
            DesiredInteractiveToolNames.Add(TEXT("terra.ui_begin_turn_review"));
            DesiredInteractiveToolNames.Add(TEXT("terra.ui_select_piece"));
            break;
        case ETerraGameplayInteractionPhase::PieceSelected:
            DesiredInteractiveToolNames.Add(TEXT("terra.ui_select_piece"));
            DesiredInteractiveToolNames.Add(TEXT("terra.ui_preview_move"));
            DesiredInteractiveToolNames.Add(TEXT("terra.ui_cancel_selection"));
            break;
        case ETerraGameplayInteractionPhase::PieceMovedCanEndTurn:
            DesiredInteractiveToolNames.Add(TEXT("terra.ui_confirm_action"));
            DesiredInteractiveToolNames.Add(TEXT("terra.ui_cancel_selection"));
            break;
        case ETerraGameplayInteractionPhase::PieceJumpingCanContinue:
            DesiredInteractiveToolNames.Add(TEXT("terra.ui_preview_move"));
            DesiredInteractiveToolNames.Add(TEXT("terra.ui_confirm_action"));
            DesiredInteractiveToolNames.Add(TEXT("terra.ui_cancel_selection"));
            break;
        default:
            break;
        }
    }

    const TArray<FString> InteractiveToolNames = {
        TEXT("terra.ui_begin_turn_review"),
        TEXT("terra.ui_select_piece"),
        TEXT("terra.ui_preview_move"),
        TEXT("terra.ui_cancel_selection"),
        TEXT("terra.ui_confirm_action")
    };

    for (const FString& ToolName : InteractiveToolNames)
    {
        const bool bShouldBeRegistered = DesiredInteractiveToolNames.Contains(ToolName);
        const bool bIsRegistered = RegisteredToolNames.Contains(ToolName);
        const TSharedRef<IModelContextProtocolTool>* Tool = AllToolsByName.Find(ToolName);
        if (!Tool)
        {
            continue;
        }

        if (bShouldBeRegistered && !bIsRegistered)
        {
            RegisteredTools.Add(*Tool);
            RegisteredToolNames.Add(ToolName);
            const bool bAdded = McpModule->AddTool(*Tool);
            UE_LOG(LogTerraNpcMcp, Log, TEXT("[NpcMcp] Enable interactive tool %s: %s"), *ToolName, bAdded ? TEXT("ok") : TEXT("duplicate"));
        }
        else if (!bShouldBeRegistered && bIsRegistered)
        {
            McpModule->RemoveTool(*Tool);
            RegisteredToolNames.Remove(ToolName);
            RegisteredTools.RemoveSingle(*Tool);
            UE_LOG(LogTerraNpcMcp, Log, TEXT("[NpcMcp] Disable interactive tool %s"), *ToolName);
        }
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
    RegisteredToolNames.Reset();
    AllToolsByName.Reset();
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
