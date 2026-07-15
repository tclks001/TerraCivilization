#include "BTService_TerraUpdateTurnContext.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NpcBehaviorTree.h"
#include "TerraNpcBehaviorTreeSubsystem.h"
#include "TerraNpcBrainPawn.h"

namespace TerraNpcBTKeys
{
    static const FName ControlledFactionId(TEXT("ControlledFactionId"));
    static const FName CurrentFactionId(TEXT("CurrentFactionId"));
    static const FName TurnIndex(TEXT("TurnIndex"));
    static const FName GameplayReady(TEXT("bGameplayReady"));
    static const FName MatchEnded(TEXT("bMatchEnded"));
    static const FName IsMyTurn(TEXT("bIsMyTurn"));
    static const FName LastError(TEXT("LastError"));
}

UBTService_TerraUpdateTurnContext::UBTService_TerraUpdateTurnContext()
{
    NodeName = TEXT("Update Terra Turn Context");
    Interval = 0.25f;
    RandomDeviation = 0.0f;
}

void UBTService_TerraUpdateTurnContext::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    AAIController* Controller = OwnerComp.GetAIOwner();
    ATerraNpcBrainPawn* Brain = Controller ? Cast<ATerraNpcBrainPawn>(Controller->GetPawn()) : nullptr;
    UWorld* World = OwnerComp.GetWorld();
    UTerraNpcBehaviorTreeSubsystem* Subsystem = World ? World->GetSubsystem<UTerraNpcBehaviorTreeSubsystem>() : nullptr;
    if (!Blackboard || !Brain || !Subsystem)
    {
        return;
    }

    FTerraNpcBehaviorTreeTurnContext Context;
    const bool bReady = Subsystem->QueryTurnContext(Context);
    Blackboard->SetValueAsInt(TerraNpcBTKeys::ControlledFactionId, Brain->ControlledFactionId);
    Blackboard->SetValueAsBool(TerraNpcBTKeys::GameplayReady, bReady && Context.bGameplayReady);
    Blackboard->SetValueAsBool(TerraNpcBTKeys::MatchEnded, bReady && Context.bMatchEnded);
    Blackboard->SetValueAsInt(TerraNpcBTKeys::CurrentFactionId, Context.CurrentFactionId);
    Blackboard->SetValueAsInt(TerraNpcBTKeys::TurnIndex, Context.TurnIndex);
    Blackboard->SetValueAsBool(TerraNpcBTKeys::IsMyTurn,
        bReady
        && Context.bTurnActivationReady
        && !Context.bMatchEnded
        && Context.CurrentFactionId == Brain->ControlledFactionId);
    if (!bReady)
    {
        Blackboard->SetValueAsString(TerraNpcBTKeys::LastError, TEXT("gameplay_unavailable"));
    }
}
