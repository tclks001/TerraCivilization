#pragma once

#include "CoreMinimal.h"
#include "TerraGameplayContainer.h"

class NPCMCP_API FTerraNpcMcpGameplayBridge
{
public:
    struct FInteractionStateSnapshot
    {
        int32 TurnIndex = INDEX_NONE;
        int32 CurrentFactionId = INDEX_NONE;
        ETerraGameplayInteractionPhase InteractionPhase = ETerraGameplayInteractionPhase::Idle;
        int32 SelectedPieceId = INDEX_NONE;
        int32 SelectedPieceCellId = INDEX_NONE;
        TArray<int32> ContinueJumpTargetCellIds;
        bool bCanSelectPieceNow = false;
        bool bCanPreviewTargetNow = false;
        bool bCanConfirmNow = false;
        bool bCanCancelNow = false;
    };

    struct FUiReviewResult
    {
        bool bOk = false;
        FString Error;
        FInteractionStateSnapshot InteractionState;
    };

    using FExecuteValidatedActionDelegate = TFunction<bool(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult)>;
    using FUiBeginTurnReviewDelegate = TFunction<bool(FUiReviewResult& OutResult)>;
    using FUiSelectPieceDelegate = TFunction<bool(int32 PieceId, FUiReviewResult& OutResult)>;
    using FUiPreviewMoveDelegate = TFunction<bool(int32 PieceId, int32 ToCellId, FUiReviewResult& OutResult)>;
    using FUiCancelSelectionDelegate = TFunction<bool(FUiReviewResult& OutResult)>;
    using FUiConfirmActionDelegate = TFunction<bool(FUiReviewResult& OutResult, FTerraGameplayContainer::FValidatedActionExecutionResult& OutExecutionResult)>;

    static void RegisterGameplayContainer(FTerraGameplayContainer* InGameplayContainer);
    static void UnregisterGameplayContainer(const FTerraGameplayContainer* InGameplayContainer);
    static const FTerraGameplayContainer* GetGameplayContainer();
    static FTerraGameplayContainer* GetMutableGameplayContainer();
    static void RegisterExecuteValidatedActionDelegate(FExecuteValidatedActionDelegate InDelegate);
    static void UnregisterExecuteValidatedActionDelegate();
    static bool TryExecuteValidatedAction(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult);
    static void RegisterUiBeginTurnReviewDelegate(FUiBeginTurnReviewDelegate InDelegate);
    static void RegisterUiSelectPieceDelegate(FUiSelectPieceDelegate InDelegate);
    static void RegisterUiPreviewMoveDelegate(FUiPreviewMoveDelegate InDelegate);
    static void RegisterUiCancelSelectionDelegate(FUiCancelSelectionDelegate InDelegate);
    static void RegisterUiConfirmActionDelegate(FUiConfirmActionDelegate InDelegate);
    static void UnregisterUiReviewDelegates();
    static bool TryUiBeginTurnReview(FUiReviewResult& OutResult);
    static bool TryUiSelectPiece(int32 PieceId, FUiReviewResult& OutResult);
    static bool TryUiPreviewMove(int32 PieceId, int32 ToCellId, FUiReviewResult& OutResult);
    static bool TryUiCancelSelection(FUiReviewResult& OutResult);
    static bool TryUiConfirmAction(FUiReviewResult& OutResult, FTerraGameplayContainer::FValidatedActionExecutionResult& OutExecutionResult);
};
