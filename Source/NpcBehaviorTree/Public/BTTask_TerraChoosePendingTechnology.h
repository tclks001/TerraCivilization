#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_TerraChoosePendingTechnology.generated.h"

UCLASS()
class NPCBEHAVIORTREE_API UBTTask_TerraChoosePendingTechnology : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_TerraChoosePendingTechnology();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
