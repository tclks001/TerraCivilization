#include "TerraNpcAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "NpcBehaviorTree.h"
#include "TerraNpcBrainPawn.h"

ATerraNpcAIController::ATerraNpcAIController()
{
    bWantsPlayerState = false;
}

void ATerraNpcAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    const ATerraNpcBrainPawn* Brain = Cast<ATerraNpcBrainPawn>(InPawn);
    UBehaviorTree* TreeToRun = Brain && Brain->BehaviorTreeAsset ? Brain->BehaviorTreeAsset : BehaviorTreeAsset;
    if (!TreeToRun)
    {
        UE_LOG(LogTerraNpcBT, Warning, TEXT("Brain=%s has no BehaviorTreeAsset."), *GetNameSafe(InPawn));
        return;
    }

    if (!RunBehaviorTree(TreeToRun))
    {
        UE_LOG(LogTerraNpcBT, Error, TEXT("Brain=%s failed to run BehaviorTree=%s."), *GetNameSafe(InPawn), *GetNameSafe(TreeToRun));
    }
}
