#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TerraNpcAIController.generated.h"

class UBehaviorTree;

UCLASS()
class NPCBEHAVIORTREE_API ATerraNpcAIController : public AAIController
{
    GENERATED_BODY()

public:
    ATerraNpcAIController();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terra NPC|BT1")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

protected:
    virtual void OnPossess(APawn* InPawn) override;
};
