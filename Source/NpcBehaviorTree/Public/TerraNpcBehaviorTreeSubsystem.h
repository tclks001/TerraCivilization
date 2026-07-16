#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TerraGameplayContainer.h"
#include "TerraNpcBehaviorTreeSubsystem.generated.h"

class UPlanetGameplayComponent;

USTRUCT()
struct NPCBEHAVIORTREE_API FTerraNpcBehaviorTreeTurnContext
{
    GENERATED_BODY()

    bool bGameplayReady = false;
    bool bTurnActivationReady = false;
    bool bMatchEnded = false;
    bool bNeedsTechnologyChoice = false;
    int32 CurrentFactionId = INDEX_NONE;
    int32 TurnIndex = INDEX_NONE;
};

UCLASS()
class NPCBEHAVIORTREE_API UTerraNpcBehaviorTreeSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    bool QueryTurnContext(FTerraNpcBehaviorTreeTurnContext& OutContext);
    bool CollectCurrentFactionLegalActions(TArray<FTerraGameplayContainer::FLegalActionQuery>& OutActions, FString& OutError);
    bool ExecuteValidatedAction(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult, FString& OutError);
    bool QueryPendingTechnologyChoices(int32 FactionId, TArray<ETerraGameplayTechnologyId>& OutChoices, FString& OutError);
    bool ChoosePendingTechnology(int32 FactionId, ETerraGameplayTechnologyId TechnologyId, FString& OutError);

private:
    UPlanetGameplayComponent* ResolveGameplayComponent();

    TWeakObjectPtr<UPlanetGameplayComponent> CachedGameplayComponent;
};
