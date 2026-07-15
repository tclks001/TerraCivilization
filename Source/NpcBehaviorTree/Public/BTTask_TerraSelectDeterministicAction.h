#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_TerraSelectDeterministicAction.generated.h"

UCLASS()
class NPCBEHAVIORTREE_API UBTTask_TerraSelectDeterministicAction : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_TerraSelectDeterministicAction();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
