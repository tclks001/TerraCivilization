#include "TerraNpcMcpGameplayBridge.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "TerraGameplayContainer.h"

namespace
{
    FCriticalSection GTerraNpcMcpGameplayBridgeMutex;
    FTerraGameplayContainer* GTerraNpcMcpGameplayContainer = nullptr;
    FTerraNpcMcpGameplayBridge::FExecuteValidatedActionDelegate GTerraNpcMcpExecuteValidatedActionDelegate;
    FTerraNpcMcpGameplayBridge::FUiBeginTurnReviewDelegate GTerraNpcMcpUiBeginTurnReviewDelegate;
    FTerraNpcMcpGameplayBridge::FUiSelectPieceDelegate GTerraNpcMcpUiSelectPieceDelegate;
    FTerraNpcMcpGameplayBridge::FUiPreviewMoveDelegate GTerraNpcMcpUiPreviewMoveDelegate;
    FTerraNpcMcpGameplayBridge::FUiCancelSelectionDelegate GTerraNpcMcpUiCancelSelectionDelegate;
    FTerraNpcMcpGameplayBridge::FUiConfirmActionDelegate GTerraNpcMcpUiConfirmActionDelegate;
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

void FTerraNpcMcpGameplayBridge::RegisterUiBeginTurnReviewDelegate(FUiBeginTurnReviewDelegate InDelegate)
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    GTerraNpcMcpUiBeginTurnReviewDelegate = MoveTemp(InDelegate);
}

void FTerraNpcMcpGameplayBridge::RegisterUiSelectPieceDelegate(FUiSelectPieceDelegate InDelegate)
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    GTerraNpcMcpUiSelectPieceDelegate = MoveTemp(InDelegate);
}

void FTerraNpcMcpGameplayBridge::RegisterUiPreviewMoveDelegate(FUiPreviewMoveDelegate InDelegate)
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    GTerraNpcMcpUiPreviewMoveDelegate = MoveTemp(InDelegate);
}

void FTerraNpcMcpGameplayBridge::RegisterUiCancelSelectionDelegate(FUiCancelSelectionDelegate InDelegate)
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    GTerraNpcMcpUiCancelSelectionDelegate = MoveTemp(InDelegate);
}

void FTerraNpcMcpGameplayBridge::RegisterUiConfirmActionDelegate(FUiConfirmActionDelegate InDelegate)
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    GTerraNpcMcpUiConfirmActionDelegate = MoveTemp(InDelegate);
}

void FTerraNpcMcpGameplayBridge::UnregisterUiReviewDelegates()
{
    FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
    GTerraNpcMcpUiBeginTurnReviewDelegate.Reset();
    GTerraNpcMcpUiSelectPieceDelegate.Reset();
    GTerraNpcMcpUiPreviewMoveDelegate.Reset();
    GTerraNpcMcpUiCancelSelectionDelegate.Reset();
    GTerraNpcMcpUiConfirmActionDelegate.Reset();
}

bool FTerraNpcMcpGameplayBridge::TryUiBeginTurnReview(FUiReviewResult& OutResult)
{
    FUiBeginTurnReviewDelegate DelegateCopy;
    {
        FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
        DelegateCopy = GTerraNpcMcpUiBeginTurnReviewDelegate;
    }
    if (DelegateCopy)
    {
        return DelegateCopy(OutResult);
    }
    OutResult = FUiReviewResult();
    OutResult.bOk = false;
    OutResult.Error = TEXT("ui_begin_turn_review_delegate_unavailable");
    return false;
}

bool FTerraNpcMcpGameplayBridge::TryUiSelectPiece(int32 PieceId, FUiReviewResult& OutResult)
{
    FUiSelectPieceDelegate DelegateCopy;
    {
        FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
        DelegateCopy = GTerraNpcMcpUiSelectPieceDelegate;
    }
    if (DelegateCopy)
    {
        return DelegateCopy(PieceId, OutResult);
    }
    OutResult = FUiReviewResult();
    OutResult.bOk = false;
    OutResult.Error = TEXT("ui_select_piece_delegate_unavailable");
    return false;
}

bool FTerraNpcMcpGameplayBridge::TryUiPreviewMove(int32 PieceId, int32 ToCellId, FUiReviewResult& OutResult)
{
    FUiPreviewMoveDelegate DelegateCopy;
    {
        FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
        DelegateCopy = GTerraNpcMcpUiPreviewMoveDelegate;
    }
    if (DelegateCopy)
    {
        return DelegateCopy(PieceId, ToCellId, OutResult);
    }
    OutResult = FUiReviewResult();
    OutResult.bOk = false;
    OutResult.Error = TEXT("ui_preview_move_delegate_unavailable");
    return false;
}

bool FTerraNpcMcpGameplayBridge::TryUiCancelSelection(FUiReviewResult& OutResult)
{
    FUiCancelSelectionDelegate DelegateCopy;
    {
        FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
        DelegateCopy = GTerraNpcMcpUiCancelSelectionDelegate;
    }
    if (DelegateCopy)
    {
        return DelegateCopy(OutResult);
    }
    OutResult = FUiReviewResult();
    OutResult.bOk = false;
    OutResult.Error = TEXT("ui_cancel_selection_delegate_unavailable");
    return false;
}

bool FTerraNpcMcpGameplayBridge::TryUiConfirmAction(FUiReviewResult& OutResult, FTerraGameplayContainer::FValidatedActionExecutionResult& OutExecutionResult)
{
    FUiConfirmActionDelegate DelegateCopy;
    {
        FScopeLock Lock(&GTerraNpcMcpGameplayBridgeMutex);
        DelegateCopy = GTerraNpcMcpUiConfirmActionDelegate;
    }
    if (DelegateCopy)
    {
        return DelegateCopy(OutResult, OutExecutionResult);
    }
    OutResult = FUiReviewResult();
    OutResult.bOk = false;
    OutResult.Error = TEXT("ui_confirm_action_delegate_unavailable");
    OutExecutionResult = FTerraGameplayContainer::FValidatedActionExecutionResult();
    return false;
}
