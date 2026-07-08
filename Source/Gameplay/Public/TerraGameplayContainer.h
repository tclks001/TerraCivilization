#pragma once

#include "CoreMinimal.h"
#include "TerraGameplayTypes.h"

class GAMEPLAY_API FTerraGameplayContainer
{
public:
    struct FLegalActionQuery
    {
        int32 PieceId = INDEX_NONE;
        int32 FromCellId = INDEX_NONE;
        int32 ToCellId = INDEX_NONE;
        bool bIsJump = false;
        TArray<FTerraGameplayCaptureEntry> CaptureEntries;
    };

    struct FActionRiskQuery
    {
        bool bValidAction = false;
        bool bDestinationThreatened = false;
        int32 ThreatCount = 0;
        TArray<int32> ThreateningPieceIds;
        TArray<int32> ThreateningFactionIds;
    };

    struct FValidatedActionExecutionResult
    {
        bool bAccepted = false;
        bool bExecuted = false;
        FString RejectReason;
        int32 TurnIndexBefore = INDEX_NONE;
        int32 TurnIndexAfter = INDEX_NONE;
        int32 FactionIdBefore = INDEX_NONE;
        int32 FactionIdAfter = INDEX_NONE;
        int32 PieceId = INDEX_NONE;
        int32 FromCellId = INDEX_NONE;
        int32 ToCellId = INDEX_NONE;
        bool bWasJump = false;
        TArray<FTerraGameplayCaptureEntry> CaptureEntries;
        TArray<int32> DirtyCellIds;
    };

    struct FInteractionUndoSnapshot
    {
        TArray<FTerraGameplayPieceState> Pieces;
        TArray<int32> CellToPieceId;
        TMap<int32, FTerraGameplayCellHighlight> GameplayHighlights;
        TSet<int32> OrdinaryMoveTargetCellIds;
        TSet<int32> JumpTargetCellIds;
        TMap<int32, TSet<int32>> ActionTargetCellIdToCaptureCellIds;
        TMap<int32, FTerraGameplayCaptureEntry> PendingCaptureEntriesByPieceId;
        TSet<int32> PendingCapturePieceIds;
        TSet<int32> PendingCaptureCellIds;
        TArray<int32> CurrentActionPathCellIds;
        int32 SelectedPieceId = INDEX_NONE;
        int32 SelectedPieceStartCellId = INDEX_NONE;
        int32 LastJumpStartCellId = INDEX_NONE;
        ETerraGameplayInteractionPhase InteractionPhase = ETerraGameplayInteractionPhase::Idle;
    };

    void Initialize(const TArray<FTerraGameplayCellState>& InCells);

    bool IsInitialized() const { return bInitialized; }
    int32 GetCurrentFactionId() const { return CurrentFactionId; }
    int32 GetTurnIndex() const { return TurnIndex; }
    int32 GetSelectedPieceId() const { return SelectedPieceId; }
    ETerraGameplayInteractionPhase GetInteractionPhase() const { return InteractionPhase; }
    bool IsMatchEnded() const { return bMatchEnded; }
    int32 GetWinningFactionId() const { return WinningFactionId; }

    bool HandleCellClick(int32 CellId, TArray<int32>& OutDirtyCellIds);
    bool UndoCurrentInteraction(TArray<int32>& OutDirtyCellIds);
    void SetDebugKeepSameFactionOnEndTurn(bool bInKeepSameFaction) { bDebugKeepSameFactionOnEndTurn = bInKeepSameFaction; }
    bool GetHighlightForCell(int32 CellId, FTerraGameplayCellHighlight& OutHighlight) const;
    int32 GetCurrentFactionBaseCellId() const;
    bool IsCurrentFactionPieceCell(int32 CellId) const;
    bool IsCurrentActionTargetCell(int32 CellId) const;
    bool IsCapturePreviewCellForActionTarget(int32 CaptureCellId, int32 ActionTargetCellId) const;
    bool CollectCapturePreviewCellIdsForActionTarget(int32 ActionTargetCellId, TArray<int32>& OutCellIds) const;
    bool CollectFactionPieceCellIds(int32 FactionId, TArray<int32>& OutCellIds) const;
    bool CollectCurrentFactionPieceCellIds(TArray<int32>& OutCellIds) const;
    bool CollectCurrentFactionSelectablePieceIds(TArray<int32>& OutPieceIds) const;
    bool CollectPendingCaptureEntries(TArray<FTerraGameplayCaptureEntry>& OutCaptureEntries) const;
    bool CollectCurrentFactionLegalActions(TArray<FLegalActionQuery>& OutActions) const;
    bool EvaluateCurrentFactionActionRisk(int32 PieceId, int32 ToCellId, FActionRiskQuery& OutRisk) const;
    bool IsCurrentFactionLegalAction(int32 PieceId, int32 ToCellId, FLegalActionQuery& OutAction) const;
    bool TryExecuteValidatedAction(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FValidatedActionExecutionResult& OutResult);
    bool TryGetPieceCellId(int32 PieceId, int32& OutCellId) const;
    const TArray<FTerraGameplayPieceState>& GetPieces() const { return Pieces; }
    const TArray<FTerraGameplayFactionState>& GetFactions() const { return Factions; }
    const TArray<int32>& GetCellToPieceId() const { return CellToPieceId; }

private:
    void ResetRuntimeState_();
    void BuildInitialPieces_();
    bool AddPiece_(int32 FactionId, int32 CellId, ETerraGameplayPieceType PieceType, int32& OutPieceId);
    bool AddPieceIfFree_(int32 FactionId, int32 CellId, ETerraGameplayPieceType PieceType, int32& OutPieceId);

    bool IsValidCellId_(int32 CellId) const;
    bool IsValidPieceId_(int32 PieceId) const;
    bool IsCellEmpty_(int32 CellId) const;
    bool IsNeighbor_(int32 FromCellId, int32 ToCellId) const;
    int32 GetPieceIdAtCell_(int32 CellId) const;
    FTerraGameplayPieceState* GetMutablePiece_(int32 PieceId);
    const FTerraGameplayPieceState* GetPiece_(int32 PieceId) const;
    bool IsCurrentFactionPiece_(const FTerraGameplayPieceState& Piece) const;
    bool IsPieceSelectable_(const FTerraGameplayPieceState& Piece) const;
    bool IsPieceSelectableForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId) const;
    bool CanEnterTerrain_(const FTerraGameplayPieceState& Piece, int32 TargetCellId) const;
    bool CanOrdinaryMove_(const FTerraGameplayPieceState& Piece, int32 TargetCellId) const;
    bool CanOrdinaryMoveForFaction_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, int32 ActingFactionId) const;
    bool StepForwardBranches_(int32 PrevCellId, int32 CurCellId, TArray<int32>& OutNextCellIds) const;
    void CollectOrdinaryMoveTargets_(const FTerraGameplayPieceState& Piece, TSet<int32>& OutTargetCellIds) const;
    void CollectOrdinaryMoveTargetsForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, TSet<int32>& OutTargetCellIds) const;
    void CollectJumpTargets_(const FTerraGameplayPieceState& Piece, TSet<int32>& OutTargetCellIds) const;
    void CollectJumpTargetsForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, int32 BlockedReturnCellId, TSet<int32>& OutTargetCellIds) const;
    void CollectCaptureEntriesAfterHypotheticalMove_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, TMap<int32, FTerraGameplayCaptureEntry>& OutCaptureEntriesByCellId) const;
    void CollectCaptureEntriesAfterHypotheticalMoveForFaction_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, int32 ActingFactionId, TMap<int32, FTerraGameplayCaptureEntry>& OutCaptureEntriesByCellId) const;
    void CollectCaptureCellsAfterHypotheticalMove_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, TSet<int32>& OutCaptureCellIds) const;
    void RebuildActionTargetCapturePreviews_(TArray<int32>& OutDirtyCellIds);
    bool HasCapturePreviewForActionTarget_(int32 ActionTargetCellId) const;
    void LockPendingCapturesForActionTarget_(int32 ActionTargetCellId, TArray<int32>& OutDirtyCellIds);
    void RebuildPendingCapturesForSelectedPieceCell_(TArray<int32>& OutDirtyCellIds);
    void ResolvePendingCaptures_(TArray<int32>& OutDirtyCellIds);
    void EliminateFaction_(int32 FactionId, TArray<int32>& OutDirtyCellIds);
    void EvaluateWinStateAfterCaptures_();
    void FinalizeTurnAfterResolution_();
    void InitializeActionLogFilePath_();
    void PushUndoSnapshot_();
    void ClearUndoSnapshots_();
    TArray<FTerraGameplayCaptureEntry> GetSortedPendingCaptureEntries_() const;
    FString BuildActionLogJson_(int32 ActionTurnIndex, int32 PlayerId, int32 PieceId, const TArray<int32>& PathCellIds, const TArray<FTerraGameplayCaptureEntry>& CaptureEntries) const;
    void AppendActionLogToFile_(const FString& ActionLogJson) const;
    void EmitActionLog_(int32 ActionTurnIndex, int32 PlayerId, int32 PieceId, const TArray<int32>& PathCellIds, const TArray<FTerraGameplayCaptureEntry>& CaptureEntries) const;

    bool TrySelectPieceAtCell_(int32 CellId, TArray<int32>& OutDirtyCellIds);
    bool TryMoveSelectedPieceToOrdinaryTarget_(int32 TargetCellId, TArray<int32>& OutDirtyCellIds);
    bool TryJumpSelectedPieceTo_(int32 TargetCellId, TArray<int32>& OutDirtyCellIds);
    bool TryEndTurnOnSelectedCell_(int32 CellId, TArray<int32>& OutDirtyCellIds);

    void RefreshSelectedPieceHighlights_(bool bIncludeOrdinaryMoves, TArray<int32>& OutDirtyCellIds);
    void SetSingleHighlight_(int32 CellId, const FLinearColor& Color, float Intensity, TArray<int32>& OutDirtyCellIds);
    void ClearGameplayHighlights_(TArray<int32>& OutDirtyCellIds);
    void AddDirtyCell_(int32 CellId, TArray<int32>& OutDirtyCellIds) const;
    void AdvanceTurn_();

private:
    bool bInitialized = false;

    TArray<FTerraGameplayCellState> Cells;
    TArray<FTerraGameplayPieceState> Pieces;
    TArray<FTerraGameplayFactionState> Factions;
    TArray<int32> CellToPieceId;
    TMap<int32, FTerraGameplayCellHighlight> GameplayHighlights;
    TSet<int32> OrdinaryMoveTargetCellIds;
    TSet<int32> JumpTargetCellIds;
    TMap<int32, TSet<int32>> ActionTargetCellIdToCaptureCellIds;
    TMap<int32, FTerraGameplayCaptureEntry> PendingCaptureEntriesByPieceId;
    TSet<int32> PendingCapturePieceIds;
    TSet<int32> PendingCaptureCellIds;

    bool bDebugKeepSameFactionOnEndTurn = false;
    bool bMatchEnded = false;
    int32 CurrentFactionId = INDEX_NONE;
    int32 TurnIndex = 0;
    int32 WinningFactionId = INDEX_NONE;
    int32 SelectedPieceId = INDEX_NONE;
    int32 SelectedPieceStartCellId = INDEX_NONE;
    int32 LastJumpStartCellId = INDEX_NONE;
    FString ActionLogFilePath;
    TArray<int32> CurrentActionPathCellIds;
    TArray<FInteractionUndoSnapshot> InteractionUndoSnapshots;
    ETerraGameplayInteractionPhase InteractionPhase = ETerraGameplayInteractionPhase::Idle;
};




