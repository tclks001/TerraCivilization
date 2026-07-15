#include "TerraNpcBrainPawn.h"

#include "Components/SceneComponent.h"
#include "TerraNpcAIController.h"

ATerraNpcBrainPawn::ATerraNpcBrainPawn()
{
    BrainRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BrainRoot"));
    RootComponent = BrainRoot;
    PrimaryActorTick.bCanEverTick = false;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = ATerraNpcAIController::StaticClass();
    SetActorHiddenInGame(true);
    SetCanBeDamaged(false);
}
