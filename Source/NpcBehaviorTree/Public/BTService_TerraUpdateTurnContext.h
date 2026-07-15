#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_TerraUpdateTurnContext.generated.h"

UCLASS()
class NPCBEHAVIORTREE_API UBTService_TerraUpdateTurnContext : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_TerraUpdateTurnContext();

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
