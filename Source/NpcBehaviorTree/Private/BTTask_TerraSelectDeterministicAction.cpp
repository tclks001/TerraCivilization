#include "BTTask_TerraSelectDeterministicAction.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "TerraGameplayContainer.h"
#include "NpcBehaviorTree.h"
#include "TerraNpcBehaviorTreeSubsystem.h"

namespace TerraNpcBTKeys
{
    static const FName SelectedPieceId(TEXT("SelectedPieceId"));
    static const FName SelectedToCellId(TEXT("SelectedToCellId"));
    static const FName DecisionSource(TEXT("DecisionSource"));
    static const FName LastError(TEXT("LastError"));
}

UBTTask_TerraSelectDeterministicAction::UBTTask_TerraSelectDeterministicAction()
{
    NodeName = TEXT("Select Deterministic Action");
}

EBTNodeResult::Type UBTTask_TerraSelectDeterministicAction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    UWorld* World = OwnerComp.GetWorld();
    UTerraNpcBehaviorTreeSubsystem* Subsystem = World ? World->GetSubsystem<UTerraNpcBehaviorTreeSubsystem>() : nullptr;
    if (!Blackboard || !Subsystem)
    {
        return EBTNodeResult::Failed;
    }

    TArray<FTerraGameplayContainer::FLegalActionQuery> Actions;
    FString Error;
    if (!Subsystem->CollectCurrentFactionLegalActions(Actions, Error) || Actions.IsEmpty())
    {
        Blackboard->SetValueAsString(TerraNpcBTKeys::LastError, Error.IsEmpty() ? TEXT("no_legal_actions") : Error);
        return EBTNodeResult::Failed;
    }

    Actions.Sort([](const FTerraGameplayContainer::FLegalActionQuery& A, const FTerraGameplayContainer::FLegalActionQuery& B)
    {
        if (A.CaptureEntries.Num() != B.CaptureEntries.Num()) return A.CaptureEntries.Num() > B.CaptureEntries.Num();
        if (A.bIsJump != B.bIsJump) return A.bIsJump;
        if (A.PieceId != B.PieceId) return A.PieceId < B.PieceId;
        return A.ToCellId < B.ToCellId;
    });

    const FTerraGameplayContainer::FLegalActionQuery& Selected = Actions[0];
    Blackboard->SetValueAsInt(TerraNpcBTKeys::SelectedPieceId, Selected.PieceId);
    Blackboard->SetValueAsInt(TerraNpcBTKeys::SelectedToCellId, Selected.ToCellId);
    Blackboard->SetValueAsInt(TerraNpcBTKeys::DecisionSource, 0);
    Blackboard->SetValueAsString(TerraNpcBTKeys::LastError, FString());
    UE_LOG(LogTerraNpcBT, Log, TEXT("Decision=Deterministic Piece=%d To=%d Captures=%d Jump=%d"), Selected.PieceId, Selected.ToCellId, Selected.CaptureEntries.Num(), Selected.bIsJump ? 1 : 0);
    return EBTNodeResult::Succeeded;
}
