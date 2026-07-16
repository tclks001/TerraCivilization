#include "BTTask_TerraChoosePendingTechnology.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "NpcBehaviorTree.h"
#include "TerraNpcBehaviorTreeSubsystem.h"

namespace TerraNpcBTKeys
{
    static const FName ControlledFactionId(TEXT("ControlledFactionId"));
    static const FName CurrentFactionId(TEXT("CurrentFactionId"));
    static const FName LastError(TEXT("LastError"));
}

UBTTask_TerraChoosePendingTechnology::UBTTask_TerraChoosePendingTechnology()
{
    NodeName = TEXT("Choose Pending Technology");
}

EBTNodeResult::Type UBTTask_TerraChoosePendingTechnology::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    UWorld* World = OwnerComp.GetWorld();
    UTerraNpcBehaviorTreeSubsystem* Subsystem = World ? World->GetSubsystem<UTerraNpcBehaviorTreeSubsystem>() : nullptr;
    if (!Blackboard || !Subsystem)
    {
        return EBTNodeResult::Failed;
    }

    const int32 FactionId = Blackboard->GetValueAsInt(TerraNpcBTKeys::CurrentFactionId);
    const int32 ControlledFactionId = Blackboard->GetValueAsInt(TerraNpcBTKeys::ControlledFactionId);
    if (FactionId != ControlledFactionId)
    {
        Blackboard->SetValueAsString(TerraNpcBTKeys::LastError, TEXT("invalid_technology_choice_context"));
        return EBTNodeResult::Failed;
    }

    TArray<ETerraGameplayTechnologyId> Choices;
    FString Error;
    if (!Subsystem->QueryPendingTechnologyChoices(FactionId, Choices, Error))
    {
        Blackboard->SetValueAsString(TerraNpcBTKeys::LastError, Error);
        UE_LOG(LogTerraNpcBT, Warning, TEXT("TechnologyChoice=QueryFailed Faction=%d Error=%s"), FactionId, *Error);
        return EBTNodeResult::Failed;
    }

    Choices.Sort([](ETerraGameplayTechnologyId Left, ETerraGameplayTechnologyId Right)
    {
        return static_cast<uint8>(Left) < static_cast<uint8>(Right);
    });

    const ETerraGameplayTechnologyId SelectedTechnology = Choices[0];
    if (!Subsystem->ChoosePendingTechnology(FactionId, SelectedTechnology, Error))
    {
        Blackboard->SetValueAsString(TerraNpcBTKeys::LastError, Error);
        UE_LOG(LogTerraNpcBT, Warning, TEXT("TechnologyChoice=Rejected Faction=%d Technology=%d Error=%s"), FactionId, static_cast<int32>(SelectedTechnology), *Error);
        return EBTNodeResult::Failed;
    }

    Blackboard->SetValueAsString(TerraNpcBTKeys::LastError, FString());
    UE_LOG(LogTerraNpcBT, Log, TEXT("TechnologyChoice=Succeeded Faction=%d Technology=%d"), FactionId, static_cast<int32>(SelectedTechnology));
    return EBTNodeResult::Succeeded;
}
