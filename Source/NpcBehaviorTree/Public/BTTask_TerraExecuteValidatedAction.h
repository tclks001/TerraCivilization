#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_TerraExecuteValidatedAction.generated.h"

UCLASS()
class NPCBEHAVIORTREE_API UBTTask_TerraExecuteValidatedAction : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_TerraExecuteValidatedAction();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
