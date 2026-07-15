#pragma once

#include "CoreMinimal.h"
#include "GameFramework\Pawn.h"
#include "TerraNpcBrainPawn.generated.h"

UCLASS()
class NPCBEHAVIORTREE_API ATerraNpcBrainPawn : public APawn
{
    GENERATED_BODY()

public:
    ATerraNpcBrainPawn();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Terra NPC|BT1", meta = (ClampMin = "0"))
    int32 ControlledFactionId = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terra NPC|BT1")
    TObjectPtr<class UBehaviorTree> BehaviorTreeAsset;

private:
    UPROPERTY(VisibleAnywhere, Category = "Terra NPC|BT1", meta = (NoEditInline))
    TObjectPtr<class USceneComponent> BrainRoot;
};
