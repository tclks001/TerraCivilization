#include "BTTask_TerraExecuteValidatedAction.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "TerraGameplayContainer.h"
#include "NpcBehaviorTree.h"
#include "TerraNpcBehaviorTreeSubsystem.h"

namespace TerraNpcBTKeys
{
    static const FName ControlledFactionId(TEXT("ControlledFactionId"));
    static const FName CurrentFactionId(TEXT("CurrentFactionId"));
    static const FName TurnIndex(TEXT("TurnIndex"));
    static const FName SelectedPieceId(TEXT("SelectedPieceId"));
    static const FName SelectedToCellId(TEXT("SelectedToCellId"));
    static const FName LastError(TEXT("LastError"));
}

UBTTask_TerraExecuteValidatedAction::UBTTask_TerraExecuteValidatedAction()
{
    NodeName = TEXT("Execute Terra Validated Action");
}

EBTNodeResult::Type UBTTask_TerraExecuteValidatedAction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    UWorld* World = OwnerComp.GetWorld();
    UTerraNpcBehaviorTreeSubsystem* Subsystem = World ? World->GetSubsystem<UTerraNpcBehaviorTreeSubsystem>() : nullptr;
    if (!Blackboard || !Subsystem)
    {
        return EBTNodeResult::Failed;
    }

    const int32 TurnIndex = Blackboard->GetValueAsInt(TerraNpcBTKeys::TurnIndex);
    const int32 FactionId = Blackboard->GetValueAsInt(TerraNpcBTKeys::CurrentFactionId);
    const int32 ControlledFactionId = Blackboard->GetValueAsInt(TerraNpcBTKeys::ControlledFactionId);
    const int32 PieceId = Blackboard->GetValueAsInt(TerraNpcBTKeys::SelectedPieceId);
    const int32 ToCellId = Blackboard->GetValueAsInt(TerraNpcBTKeys::SelectedToCellId);
    if (FactionId != ControlledFactionId || PieceId == INDEX_NONE || ToCellId == INDEX_NONE)
    {
        Blackboard->SetValueAsString(TerraNpcBTKeys::LastError, TEXT("invalid_execution_context"));
        return EBTNodeResult::Failed;
    }

    FTerraGameplayContainer::FValidatedActionExecutionResult Result;
    FString Error;
    if (!Subsystem->ExecuteValidatedAction(TurnIndex, FactionId, PieceId, ToCellId, Result, Error))
    {
        Blackboard->SetValueAsString(TerraNpcBTKeys::LastError, Error);
        UE_LOG(LogTerraNpcBT, Warning, TEXT("Execute=Rejected Faction=%d Turn=%d Piece=%d To=%d Error=%s"), FactionId, TurnIndex, PieceId, ToCellId, *Error);
        return EBTNodeResult::Failed;
    }

    UE_LOG(LogTerraNpcBT, Log, TEXT("Execute=Succeeded Faction=%d Turn=%d Piece=%d To=%d TurnAfter=%d"), FactionId, TurnIndex, PieceId, ToCellId, Result.TurnIndexAfter);
    return EBTNodeResult::Succeeded;
}
