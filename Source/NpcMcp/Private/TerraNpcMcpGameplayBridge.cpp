#include "TerraNpcMcpGameplayBridge.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "TerraGameplayContainer.h"

namespace
{
    FCriticalSection GTerraNpcMcpGameplayBridgeMutex;
    FTerraGameplayContainer* GTerraNpcMcpGameplayContainer = nullptr;
    FTerraNpcMcpGameplayBridge::FExecuteValidatedActionDelegate GTerraNpcMcpExecuteValidatedActionDelegate;
}

void FTerraNpcMcpGameplayBridge::RegisterGameplayContainer(FTerraGameplayContainer* InGameplayContainer)
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    GTerraNpcMcpGameplayContainer = InGameplayContainer;
}

void FTerraNpcMcpGameplayBridge::UnregisterGameplayContainer(const FTerraGameplayContainer* InGameplayContainer)
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    if (GTerraNpcMcpGameplayContainer == InGameplayContainer)
    {
        GTerraNpcMcpGameplayContainer = nullptr;
    }
}

const FTerraGameplayContainer* FTerraNpcMcpGameplayBridge::GetGameplayContainer()
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    return GTerraNpcMcpGameplayContainer;
}

FTerraGameplayContainer* FTerraNpcMcpGameplayBridge::GetMutableGameplayContainer()
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    return GTerraNpcMcpGameplayContainer;
}

void FTerraNpcMcpGameplayBridge::RegisterExecuteValidatedActionDelegate(FExecuteValidatedActionDelegate InDelegate)
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    GTerraNpcMcpExecuteValidatedActionDelegate = MoveTemp(InDelegate);
}

void FTerraNpcMcpGameplayBridge::UnregisterExecuteValidatedActionDelegate()
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    GTerraNpcMcpExecuteValidatedActionDelegate.Reset();
}

bool FTerraNpcMcpGameplayBridge::TryExecuteValidatedAction(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult)
{
    FExecuteValidatedActionDelegate DelegateCopy;
    {
        FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
        DelegateCopy = GTerraNpcMcpExecuteValidatedActionDelegate;
    }

    if (DelegateCopy)
    {
        return DelegateCopy(ExpectedTurnIndex, ExpectedFactionId, PieceId, ToCellId, OutResult);
    }

    OutResult = FTerraGameplayContainer::FValidatedActionExecutionResult();
    OutResult.bAccepted = false;
    OutResult.bExecuted = false;
    OutResult.RejectReason = TEXT("execute_delegate_unavailable");
    OutResult.TurnIndexBefore = GTerraNpcMcpGameplayContainer ? GTerraNpcMcpGameplayContainer->GetTurnIndex() : INDEX_NONE;
    OutResult.TurnIndexAfter = OutResult.TurnIndexBefore;
    OutResult.FactionIdBefore = GTerraNpcMcpGameplayContainer ? GTerraNpcMcpGameplayContainer->GetCurrentFactionId() : INDEX_NONE;
    OutResult.FactionIdAfter = OutResult.FactionIdBefore;
    OutResult.PieceId = PieceId;
    OutResult.ToCellId = ToCellId;
    return false;
}
