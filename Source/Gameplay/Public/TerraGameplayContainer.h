#pragma once

#include "CoreMinimal.h"
#include "TerraGameplayTypes.h"

class GAMEPLAY_API FTerraGameplayContainer
{
public:
    void Initialize(const TArray<FTerraGameplayCellState>& InCells);

    bool IsInitialized() const { return bInitialized; }
    int32 GetCurrentFactionId() const { return CurrentFactionId; }
    int32 GetTurnIndex() const { return TurnIndex; }
    int32 GetSelectedPieceId() const { return SelectedPieceId; }
    ETerraGameplayInteractionPhase GetInteractionPhase() const { return InteractionPhase; }

    bool HandleCellClick(int32 CellId, TArray<int32>& OutDirtyCellIds);
    bool GetHighlightForCell(int32 CellId, FTerraGameplayCellHighlight& OutHighlight) const;
    int32 GetCurrentFactionBaseCellId() const;
    bool IsCurrentFactionPieceCell(int32 CellId) const;
    bool CollectFactionPieceCellIds(int32 FactionId, TArray<int32>& OutCellIds) const;
    bool CollectCurrentFactionPieceCellIds(TArray<int32>& OutCellIds) const;
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
    bool CanOrdinaryMove_(const FTerraGameplayPieceState& Piece, int32 TargetCellId) const;

    bool TrySelectPieceAtCell_(int32 CellId, TArray<int32>& OutDirtyCellIds);
    bool TryMoveSelectedPieceTo_(int32 TargetCellId, TArray<int32>& OutDirtyCellIds);
    bool TryEndTurnOnSelectedCell_(int32 CellId, TArray<int32>& OutDirtyCellIds);

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

    int32 CurrentFactionId = INDEX_NONE;
    int32 TurnIndex = 0;
    int32 SelectedPieceId = INDEX_NONE;
    int32 SelectedPieceStartCellId = INDEX_NONE;
    ETerraGameplayInteractionPhase InteractionPhase = ETerraGameplayInteractionPhase::Idle;
};
