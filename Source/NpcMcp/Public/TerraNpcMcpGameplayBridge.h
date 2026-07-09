#pragma once

#include "CoreMinimal.h"
#include "TerraGameplayContainer.h"

class NPCMCP_API FTerraNpcMcpGameplayBridge
{
public:
    using FExecuteValidatedActionDelegate = TFunction<bool(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult)>;

    static void RegisterGameplayContainer(FTerraGameplayContainer* InGameplayContainer);
    static void UnregisterGameplayContainer(const FTerraGameplayContainer* InGameplayContainer);
    static const FTerraGameplayContainer* GetGameplayContainer();
    static FTerraGameplayContainer* GetMutableGameplayContainer();
    static void RegisterExecuteValidatedActionDelegate(FExecuteValidatedActionDelegate InDelegate);
    static void UnregisterExecuteValidatedActionDelegate();
    static bool TryExecuteValidatedAction(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult);
};
