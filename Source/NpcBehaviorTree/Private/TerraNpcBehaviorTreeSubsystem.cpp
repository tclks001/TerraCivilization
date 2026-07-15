#include "TerraNpcBehaviorTreeSubsystem.h"

#include "TerraGameplayContainer.h"
#include "NpcBehaviorTree.h"
#include "Render/PlanetGameplayComponent.h"
#include "Render/PlanetTessellatedMesh.h"
#include "EngineUtils.h"

UPlanetGameplayComponent* UTerraNpcBehaviorTreeSubsystem::ResolveGameplayComponent()
{
    if (CachedGameplayComponent.IsValid())
    {
        return CachedGameplayComponent.Get();
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    for (TActorIterator<APlanetTessellatedMesh> It(World); It; ++It)
    {
        if (UPlanetGameplayComponent* Component = It->GetPlanetGameplayComponent())
        {
            CachedGameplayComponent = Component;
            return Component;
        }
    }

    return nullptr;
}

bool UTerraNpcBehaviorTreeSubsystem::QueryTurnContext(FTerraNpcBehaviorTreeTurnContext& OutContext)
{
    OutContext = FTerraNpcBehaviorTreeTurnContext();

    UPlanetGameplayComponent* Component = ResolveGameplayComponent();
    const FTerraGameplayContainer* Gameplay = Component ? Component->GetGameplayContainer() : nullptr;
    if (!Gameplay || !Gameplay->IsInitialized())
    {
        return false;
    }

    OutContext.bGameplayReady = true;
    OutContext.bTurnActivationReady = Component->IsTurnActivationReady();
    OutContext.bMatchEnded = Gameplay->IsMatchEnded();
    OutContext.CurrentFactionId = Gameplay->GetCurrentFactionId();
    OutContext.TurnIndex = Gameplay->GetTurnIndex();
    return true;
}

bool UTerraNpcBehaviorTreeSubsystem::CollectCurrentFactionLegalActions(TArray<FTerraGameplayContainer::FLegalActionQuery>& OutActions, FString& OutError)
{
    OutActions.Reset();
    OutError.Reset();

    UPlanetGameplayComponent* Component = ResolveGameplayComponent();
    const FTerraGameplayContainer* Gameplay = Component ? Component->GetGameplayContainer() : nullptr;
    if (!Gameplay || !Gameplay->IsInitialized())
    {
        OutError = TEXT("gameplay_unavailable");
        return false;
    }

    if (Gameplay->IsMatchEnded())
    {
        OutError = TEXT("match_ended");
        return false;
    }

    if (!Gameplay->CollectCurrentFactionLegalActions(OutActions))
    {
        OutError = TEXT("legal_action_query_failed");
        return false;
    }

    return true;
}

bool UTerraNpcBehaviorTreeSubsystem::ExecuteValidatedAction(
    int32 ExpectedTurnIndex,
    int32 ExpectedFactionId,
    int32 PieceId,
    int32 ToCellId,
    FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult,
    FString& OutError)
{
    OutError.Reset();

    UPlanetGameplayComponent* Component = ResolveGameplayComponent();
    if (!Component)
    {
        OutError = TEXT("gameplay_unavailable");
        return false;
    }

    if (!Component->TryExecuteNpcValidatedAction(ExpectedTurnIndex, ExpectedFactionId, PieceId, ToCellId, OutResult))
    {
        OutError = OutResult.RejectReason.IsEmpty() ? TEXT("validated_action_rejected") : OutResult.RejectReason;
        return false;
    }

    return OutResult.bExecuted;
}
