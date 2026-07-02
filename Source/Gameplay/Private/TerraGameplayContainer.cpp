#include "TerraGameplayContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerraGameplay, Log, All);

namespace
{
    constexpr FLinearColor GSelectedPieceCellColor(1.0f, 1.0f, 0.0f, 1.0f);
    constexpr FLinearColor GMovedPieceCanEndTurnColor(0.0f, 0.35f, 1.0f, 1.0f);
    constexpr FLinearColor GActionTargetCellColor(0.35f, 0.80f, 1.0f, 1.0f);
    constexpr FLinearColor GCapturePreviewCellColor(0.65f, 0.02f, 0.02f, 1.0f);
}

void FTerraGameplayContainer::Initialize(const TArray<FTerraGameplayCellState>& InCells)
{
    Cells = InCells;
    ResetRuntimeState_();
    BuildInitialPieces_();

    bInitialized = true;
    CurrentFactionId = Factions.Num() > 0 ? Factions[0].FactionId : INDEX_NONE;

    UE_LOG(LogTerraGameplay, Log,
        TEXT("[Gameplay][G2] Initialized. Cells=%d Factions=%d Pieces=%d CurrentFaction=%d"),
        Cells.Num(),
        Factions.Num(),
        Pieces.Num(),
        CurrentFactionId);
}

bool FTerraGameplayContainer::HandleCellClick(int32 CellId, TArray<int32>& OutDirtyCellIds)
{
    OutDirtyCellIds.Reset();

    if (!bInitialized || !IsValidCellId_(CellId) || CurrentFactionId == INDEX_NONE)
    {
        return false;
    }

    switch (InteractionPhase)
    {
    case ETerraGameplayInteractionPhase::Idle:
        return TrySelectPieceAtCell_(CellId, OutDirtyCellIds);

    case ETerraGameplayInteractionPhase::PieceSelected:
        if (TryMoveSelectedPieceToOrdinaryTarget_(CellId, OutDirtyCellIds))
        {
            return true;
        }
        if (TryJumpSelectedPieceTo_(CellId, OutDirtyCellIds))
        {
            return true;
        }
        return TrySelectPieceAtCell_(CellId, OutDirtyCellIds);

    case ETerraGameplayInteractionPhase::PieceMovedCanEndTurn:
        return TryEndTurnOnSelectedCell_(CellId, OutDirtyCellIds);

    case ETerraGameplayInteractionPhase::PieceJumpingCanContinue:
        if (TryEndTurnOnSelectedCell_(CellId, OutDirtyCellIds))
        {
            return true;
        }
        return TryJumpSelectedPieceTo_(CellId, OutDirtyCellIds);

    default:
        return false;
    }
}

bool FTerraGameplayContainer::GetHighlightForCell(int32 CellId, FTerraGameplayCellHighlight& OutHighlight) const
{
    if (const FTerraGameplayCellHighlight* Highlight = GameplayHighlights.Find(CellId))
    {
        OutHighlight = *Highlight;
        return Highlight->IsActive();
    }

    OutHighlight = FTerraGameplayCellHighlight();
    return false;
}

int32 FTerraGameplayContainer::GetCurrentFactionBaseCellId() const
{
    return Factions.IsValidIndex(CurrentFactionId) ? Factions[CurrentFactionId].BaseCellId : INDEX_NONE;
}

bool FTerraGameplayContainer::IsCurrentFactionPieceCell(int32 CellId) const
{
    if (InteractionPhase != ETerraGameplayInteractionPhase::Idle)
    {
        return false;
    }

    const int32 PieceId = GetPieceIdAtCell_(CellId);
    const FTerraGameplayPieceState* Piece = GetPiece_(PieceId);
    return Piece && IsCurrentFactionPiece_(*Piece);
}

bool FTerraGameplayContainer::IsCurrentActionTargetCell(int32 CellId) const
{
    return OrdinaryMoveTargetCellIds.Contains(CellId) || JumpTargetCellIds.Contains(CellId);
}

bool FTerraGameplayContainer::IsCapturePreviewCellForActionTarget(int32 CaptureCellId, int32 ActionTargetCellId) const
{
    const TSet<int32>* CaptureCellIds = ActionTargetCellIdToCaptureCellIds.Find(ActionTargetCellId);
    return CaptureCellIds && CaptureCellIds->Contains(CaptureCellId);
}

bool FTerraGameplayContainer::CollectCapturePreviewCellIdsForActionTarget(int32 ActionTargetCellId, TArray<int32>& OutCellIds) const
{
    OutCellIds.Reset();
    const TSet<int32>* CaptureCellIds = ActionTargetCellIdToCaptureCellIds.Find(ActionTargetCellId);
    if (!CaptureCellIds)
    {
        return false;
    }

    for (const int32 CaptureCellId : *CaptureCellIds)
    {
        if (IsValidCellId_(CaptureCellId))
        {
            OutCellIds.AddUnique(CaptureCellId);
        }
    }
    return OutCellIds.Num() > 0;
}

bool FTerraGameplayContainer::CollectFactionPieceCellIds(int32 FactionId, TArray<int32>& OutCellIds) const
{
    OutCellIds.Reset();
    if (!Factions.IsValidIndex(FactionId) || !Factions[FactionId].bAlive)
    {
        return false;
    }

    for (const FTerraGameplayPieceState& Piece : Pieces)
    {
        if (Piece.bAlive && Piece.OwnerFactionId == FactionId && IsValidCellId_(Piece.CellId))
        {
            OutCellIds.AddUnique(Piece.CellId);
        }
    }
    return OutCellIds.Num() > 0;
}

bool FTerraGameplayContainer::CollectCurrentFactionPieceCellIds(TArray<int32>& OutCellIds) const
{
    return CollectFactionPieceCellIds(CurrentFactionId, OutCellIds);
}

bool FTerraGameplayContainer::TryGetPieceCellId(int32 PieceId, int32& OutCellId) const
{
    OutCellId = INDEX_NONE;
    const FTerraGameplayPieceState* Piece = GetPiece_(PieceId);
    if (!Piece || !Piece->bAlive || !IsValidCellId_(Piece->CellId))
    {
        return false;
    }

    OutCellId = Piece->CellId;
    return true;
}

void FTerraGameplayContainer::ResetRuntimeState_()
{
    Pieces.Reset();
    Factions.Reset();
    GameplayHighlights.Reset();
    OrdinaryMoveTargetCellIds.Reset();
    JumpTargetCellIds.Reset();
    ActionTargetCellIdToCaptureCellIds.Reset();
    PendingCapturePieceIds.Reset();
    PendingCaptureCellIds.Reset();
    CellToPieceId.Init(INDEX_NONE, Cells.Num());

    CurrentFactionId = INDEX_NONE;
    TurnIndex = 0;
    SelectedPieceId = INDEX_NONE;
    SelectedPieceStartCellId = INDEX_NONE;
    LastJumpStartCellId = INDEX_NONE;
    InteractionPhase = ETerraGameplayInteractionPhase::Idle;
    bInitialized = false;
}

void FTerraGameplayContainer::BuildInitialPieces_()
{
    TArray<int32> PentagonCellIds;
    for (const FTerraGameplayCellState& Cell : Cells)
    {
        if (Cell.bIsPentagon)
        {
            PentagonCellIds.Add(Cell.CellId);
        }
    }

    PentagonCellIds.Sort();

    if (PentagonCellIds.Num() != 12)
    {
        UE_LOG(LogTerraGameplay, Warning,
            TEXT("[Gameplay][G2] Expected 12 pentagon bases, got %d."),
            PentagonCellIds.Num());
    }

    for (int32 FactionIndex = 0; FactionIndex < PentagonCellIds.Num(); ++FactionIndex)
    {
        const int32 BaseCellId = PentagonCellIds[FactionIndex];
        if (!IsValidCellId_(BaseCellId))
        {
            continue;
        }

        FTerraGameplayFactionState& Faction = Factions.AddDefaulted_GetRef();
        Faction.FactionId = FactionIndex;
        Faction.BaseCellId = BaseCellId;
        Faction.bAlive = true;

        const FTerraGameplayCellState& BaseCell = Cells[BaseCellId];
        TArray<int32> ArcherCells;
        TArray<int32> CavalryCells;
        TArray<int32> InfantryCells;
        ArcherCells.Reserve(5);
        CavalryCells.Reserve(5);
        InfantryCells.Reserve(5);

        for (int32 NeighborSlot = 0; NeighborSlot < 5; ++NeighborSlot)
        {
            const int32 ArcherCellId = BaseCell.NeighborCellIds[NeighborSlot];
            if (!IsValidCellId_(ArcherCellId))
            {
                continue;
            }

            ArcherCells.Add(ArcherCellId);

            const FTerraGameplayCellState& ArcherCell = Cells[ArcherCellId];
            int32 BackToBaseIndex = INDEX_NONE;
            int32 NeighborCount = 0;
            for (int32 I = 0; I < 6; ++I)
            {
                const int32 NeighborCellId = ArcherCell.NeighborCellIds[I];
                if (IsValidCellId_(NeighborCellId))
                {
                    ++NeighborCount;
                }
                if (NeighborCellId == BaseCellId)
                {
                    BackToBaseIndex = I;
                }
            }

            if (NeighborCount != 6 || BackToBaseIndex == INDEX_NONE)
            {
                CavalryCells.Add(INDEX_NONE);
                continue;
            }

            const int32 CavalryCellId = ArcherCell.NeighborCellIds[(BackToBaseIndex + 3) % 6];
            CavalryCells.Add(CavalryCellId);
        }

        for (int32 I = 0; I < CavalryCells.Num(); ++I)
        {
            const int32 CavalryAId = CavalryCells[I];
            const int32 CavalryBId = CavalryCells[(I + 1) % CavalryCells.Num()];
            if (!IsValidCellId_(CavalryAId) || !IsValidCellId_(CavalryBId))
            {
                continue;
            }

            const FTerraGameplayCellState& CavalryA = Cells[CavalryAId];
            const FTerraGameplayCellState& CavalryB = Cells[CavalryBId];

            int32 InfantryCellId = INDEX_NONE;
            for (int32 SlotA = 0; SlotA < 6; ++SlotA)
            {
                const int32 CandidateCellId = CavalryA.NeighborCellIds[SlotA];
                if (!IsValidCellId_(CandidateCellId)
                    || CandidateCellId == BaseCellId
                    || ArcherCells.Contains(CandidateCellId)
                    || CavalryCells.Contains(CandidateCellId)
                    || InfantryCells.Contains(CandidateCellId)
                    || !IsCellEmpty_(CandidateCellId))
                {
                    continue;
                }

                for (int32 SlotB = 0; SlotB < 6; ++SlotB)
                {
                    if (CavalryB.NeighborCellIds[SlotB] == CandidateCellId)
                    {
                        InfantryCellId = CandidateCellId;
                        break;
                    }
                }

                if (InfantryCellId != INDEX_NONE)
                {
                    break;
                }
            }

            if (InfantryCellId != INDEX_NONE)
            {
                InfantryCells.Add(InfantryCellId);
            }
        }

        int32 FlagPieceId = INDEX_NONE;
        AddPieceIfFree_(FactionIndex, BaseCellId, ETerraGameplayPieceType::Flag, FlagPieceId);
        Faction.FlagPieceId = FlagPieceId;

        for (const int32 ArcherCellId : ArcherCells)
        {
            int32 ArcherPieceId = INDEX_NONE;
            AddPieceIfFree_(FactionIndex, ArcherCellId, ETerraGameplayPieceType::Archer, ArcherPieceId);
        }

        for (const int32 CavalryCellId : CavalryCells)
        {
            int32 CavalryPieceId = INDEX_NONE;
            AddPieceIfFree_(FactionIndex, CavalryCellId, ETerraGameplayPieceType::Cavalry, CavalryPieceId);
        }

        for (const int32 InfantryCellId : InfantryCells)
        {
            int32 InfantryPieceId = INDEX_NONE;
            AddPieceIfFree_(FactionIndex, InfantryCellId, ETerraGameplayPieceType::Infantry, InfantryPieceId);
        }
    }
}

bool FTerraGameplayContainer::AddPiece_(int32 FactionId, int32 CellId, ETerraGameplayPieceType PieceType, int32& OutPieceId)
{
    OutPieceId = INDEX_NONE;
    if (!IsValidCellId_(CellId) || !IsCellEmpty_(CellId))
    {
        return false;
    }

    FTerraGameplayPieceState& Piece = Pieces.AddDefaulted_GetRef();
    Piece.PieceId = Pieces.Num() - 1;
    Piece.OwnerFactionId = FactionId;
    Piece.CellId = CellId;
    Piece.PieceType = PieceType;
    Piece.bAlive = true;
    Piece.bCanMove = PieceType != ETerraGameplayPieceType::Flag;

    CellToPieceId[CellId] = Piece.PieceId;
    OutPieceId = Piece.PieceId;
    return true;
}

bool FTerraGameplayContainer::AddPieceIfFree_(int32 FactionId, int32 CellId, ETerraGameplayPieceType PieceType, int32& OutPieceId)
{
    if (AddPiece_(FactionId, CellId, PieceType, OutPieceId))
    {
        return true;
    }

    UE_LOG(LogTerraGameplay, Warning,
        TEXT("[Gameplay][G2] Skip initial piece. Faction=%d Cell=%d Type=%d"),
        FactionId,
        CellId,
        static_cast<int32>(PieceType));
    return false;
}

bool FTerraGameplayContainer::IsValidCellId_(int32 CellId) const
{
    return CellId >= 0 && CellId < Cells.Num();
}

bool FTerraGameplayContainer::IsValidPieceId_(int32 PieceId) const
{
    return PieceId >= 0 && PieceId < Pieces.Num();
}

bool FTerraGameplayContainer::IsCellEmpty_(int32 CellId) const
{
    return IsValidCellId_(CellId) && CellToPieceId.IsValidIndex(CellId) && CellToPieceId[CellId] == INDEX_NONE;
}

bool FTerraGameplayContainer::IsNeighbor_(int32 FromCellId, int32 ToCellId) const
{
    if (!IsValidCellId_(FromCellId) || !IsValidCellId_(ToCellId))
    {
        return false;
    }

    for (int32 I = 0; I < 6; ++I)
    {
        if (Cells[FromCellId].NeighborCellIds[I] == ToCellId)
        {
            return true;
        }
    }
    return false;
}

int32 FTerraGameplayContainer::GetPieceIdAtCell_(int32 CellId) const
{
    return CellToPieceId.IsValidIndex(CellId) ? CellToPieceId[CellId] : INDEX_NONE;
}

FTerraGameplayPieceState* FTerraGameplayContainer::GetMutablePiece_(int32 PieceId)
{
    return IsValidPieceId_(PieceId) ? &Pieces[PieceId] : nullptr;
}

const FTerraGameplayPieceState* FTerraGameplayContainer::GetPiece_(int32 PieceId) const
{
    return IsValidPieceId_(PieceId) ? &Pieces[PieceId] : nullptr;
}

bool FTerraGameplayContainer::IsCurrentFactionPiece_(const FTerraGameplayPieceState& Piece) const
{
    return Piece.bAlive && Piece.OwnerFactionId == CurrentFactionId;
}

bool FTerraGameplayContainer::IsPieceSelectable_(const FTerraGameplayPieceState& Piece) const
{
    return IsCurrentFactionPiece_(Piece)
        && Piece.bCanMove
        && Piece.PieceType != ETerraGameplayPieceType::Flag;
}

bool FTerraGameplayContainer::CanEnterTerrain_(const FTerraGameplayPieceState& Piece, int32 TargetCellId) const
{
    if (!IsValidCellId_(TargetCellId))
    {
        return false;
    }

    if (Piece.PieceType == ETerraGameplayPieceType::Cavalry
        && Cells[TargetCellId].TerrainType == ETerraGameplayTerrainType::Mountain)
    {
        return false;
    }

    return true;
}

bool FTerraGameplayContainer::CanOrdinaryMove_(const FTerraGameplayPieceState& Piece, int32 TargetCellId) const
{
    return IsPieceSelectable_(Piece)
        && IsValidCellId_(TargetCellId)
        && IsCellEmpty_(TargetCellId)
        && IsNeighbor_(Piece.CellId, TargetCellId)
        && CanEnterTerrain_(Piece, TargetCellId);
}

bool FTerraGameplayContainer::StepForwardBranches_(int32 PrevCellId, int32 CurCellId, TArray<int32>& OutNextCellIds) const
{
    OutNextCellIds.Reset();
    if (!IsValidCellId_(PrevCellId) || !IsValidCellId_(CurCellId))
    {
        return false;
    }

    const FTerraGameplayCellState& CurCell = Cells[CurCellId];
    const int32 NeighborCount = CurCell.bIsPentagon ? 5 : 6;
    int32 PrevNeighborIndex = INDEX_NONE;
    for (int32 I = 0; I < NeighborCount; ++I)
    {
        if (CurCell.NeighborCellIds[I] == PrevCellId)
        {
            PrevNeighborIndex = I;
            break;
        }
    }

    if (PrevNeighborIndex == INDEX_NONE)
    {
        return false;
    }

    if (CurCell.bIsPentagon)
    {
        const int32 NextA = CurCell.NeighborCellIds[(PrevNeighborIndex + 2) % 5];
        const int32 NextB = CurCell.NeighborCellIds[(PrevNeighborIndex + 3) % 5];
        if (IsValidCellId_(NextA))
        {
            OutNextCellIds.AddUnique(NextA);
        }
        if (IsValidCellId_(NextB))
        {
            OutNextCellIds.AddUnique(NextB);
        }
    }
    else
    {
        const int32 Next = CurCell.NeighborCellIds[(PrevNeighborIndex + 3) % 6];
        if (IsValidCellId_(Next))
        {
            OutNextCellIds.AddUnique(Next);
        }
    }

    return OutNextCellIds.Num() > 0;
}

void FTerraGameplayContainer::CollectOrdinaryMoveTargets_(const FTerraGameplayPieceState& Piece, TSet<int32>& OutTargetCellIds) const
{
    OutTargetCellIds.Reset();
    if (!IsPieceSelectable_(Piece))
    {
        return;
    }

    for (int32 I = 0; I < 6; ++I)
    {
        const int32 NeighborCellId = Cells[Piece.CellId].NeighborCellIds[I];
        if (CanOrdinaryMove_(Piece, NeighborCellId))
        {
            OutTargetCellIds.Add(NeighborCellId);
        }
    }
}

void FTerraGameplayContainer::CollectJumpTargets_(const FTerraGameplayPieceState& Piece, TSet<int32>& OutTargetCellIds) const
{
    OutTargetCellIds.Reset();
    if (!IsPieceSelectable_(Piece))
    {
        return;
    }

    for (int32 I = 0; I < 6; ++I)
    {
        const int32 MiddleCellId = Cells[Piece.CellId].NeighborCellIds[I];
        if (!IsValidCellId_(MiddleCellId) || IsCellEmpty_(MiddleCellId))
        {
            continue;
        }

        if (Piece.PieceType == ETerraGameplayPieceType::Cavalry
            && Cells[MiddleCellId].TerrainType == ETerraGameplayTerrainType::Mountain)
        {
            continue;
        }

        TArray<int32> FirstLandingCandidates;
        if (!StepForwardBranches_(Piece.CellId, MiddleCellId, FirstLandingCandidates))
        {
            continue;
        }

        for (const int32 FirstLandingCellId : FirstLandingCandidates)
        {
            if (FirstLandingCellId == LastJumpStartCellId
                || !IsCellEmpty_(FirstLandingCellId)
                || !CanEnterTerrain_(Piece, FirstLandingCellId))
            {
                continue;
            }

            OutTargetCellIds.Add(FirstLandingCellId);

            if (Piece.PieceType != ETerraGameplayPieceType::Cavalry)
            {
                continue;
            }

            TArray<int32> SecondLandingCandidates;
            if (!StepForwardBranches_(MiddleCellId, FirstLandingCellId, SecondLandingCandidates))
            {
                continue;
            }

            for (const int32 SecondLandingCellId : SecondLandingCandidates)
            {
                if (SecondLandingCellId != LastJumpStartCellId
                    && IsCellEmpty_(SecondLandingCellId)
                    && CanEnterTerrain_(Piece, SecondLandingCellId))
                {
                    OutTargetCellIds.Add(SecondLandingCellId);
                }
            }
        }
    }
}

void FTerraGameplayContainer::CollectCaptureCellsAfterHypotheticalMove_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, TSet<int32>& OutCaptureCellIds) const
{
    OutCaptureCellIds.Reset();
    if (!IsPieceSelectable_(Piece) || !IsValidCellId_(TargetCellId))
    {
        return;
    }

    auto GetHypotheticalPieceAtCell = [this, &Piece, TargetCellId](int32 CellId) -> const FTerraGameplayPieceState*
    {
        if (!IsValidCellId_(CellId))
        {
            return nullptr;
        }
        if (CellId == TargetCellId)
        {
            return &Piece;
        }
        if (CellId == Piece.CellId)
        {
            return nullptr;
        }
        return GetPiece_(GetPieceIdAtCell_(CellId));
    };

    auto IsHypotheticalFriendlyAtCell = [this, &GetHypotheticalPieceAtCell](int32 CellId) -> bool
    {
        const FTerraGameplayPieceState* CandidatePiece = GetHypotheticalPieceAtCell(CellId);
        return CandidatePiece && CandidatePiece->bAlive && CandidatePiece->OwnerFactionId == CurrentFactionId;
    };

    auto TryAddHypotheticalEnemyAtCell = [this, &GetHypotheticalPieceAtCell, &OutCaptureCellIds](int32 CellId)
    {
        const FTerraGameplayPieceState* CandidatePiece = GetHypotheticalPieceAtCell(CellId);
        if (CandidatePiece && CandidatePiece->bAlive && CandidatePiece->OwnerFactionId != CurrentFactionId)
        {
            OutCaptureCellIds.Add(CellId);
        }
    };

    auto ScanArcherRemoteCapture = [this, &GetHypotheticalPieceAtCell, &OutCaptureCellIds](int32 ArcherCellId, int32 FrontCellId)
    {
        const FTerraGameplayPieceState* ArcherPiece = GetHypotheticalPieceAtCell(ArcherCellId);
        const FTerraGameplayPieceState* FrontPiece = GetHypotheticalPieceAtCell(FrontCellId);
        if (!ArcherPiece || !ArcherPiece->bAlive
            || ArcherPiece->OwnerFactionId != CurrentFactionId
            || ArcherPiece->PieceType != ETerraGameplayPieceType::Archer
            || !FrontPiece || !FrontPiece->bAlive
            || FrontPiece->OwnerFactionId != CurrentFactionId)
        {
            return;
        }

        const int32 MaxDistanceFromArcher = Cells[ArcherCellId].TerrainType == ETerraGameplayTerrainType::Mountain ? 4 : 3;

        struct FRemoteCaptureScanCursor
        {
            int32 PrevCellId = INDEX_NONE;
            int32 CurCellId = INDEX_NONE;
            int32 DistanceFromArcher = 0;
        };

        TArray<int32> InitialCandidates;
        if (!StepForwardBranches_(ArcherCellId, FrontCellId, InitialCandidates))
        {
            return;
        }

        TArray<FRemoteCaptureScanCursor> Cursors;
        for (const int32 CandidateCellId : InitialCandidates)
        {
            Cursors.Add({FrontCellId, CandidateCellId, 2});
        }

        while (Cursors.Num() > 0)
        {
            const FRemoteCaptureScanCursor Cursor = Cursors.Pop(EAllowShrinking::No);
            if (!IsValidCellId_(Cursor.CurCellId) || Cursor.DistanceFromArcher > MaxDistanceFromArcher)
            {
                continue;
            }

            bool bContinueThisBranch = false;
            const FTerraGameplayPieceState* CandidatePiece = GetHypotheticalPieceAtCell(Cursor.CurCellId);
            if (!CandidatePiece || !CandidatePiece->bAlive)
            {
                bContinueThisBranch = true;
            }
            else if (CandidatePiece->OwnerFactionId == CurrentFactionId)
            {
                bContinueThisBranch = false;
            }
            else if (Cursor.DistanceFromArcher <= 2)
            {
                bContinueThisBranch = false;
            }
            else if (Cells[Cursor.CurCellId].TerrainType == ETerraGameplayTerrainType::Forest)
            {
                bContinueThisBranch = true;
            }
            else
            {
                OutCaptureCellIds.Add(Cursor.CurCellId);
                bContinueThisBranch = false;
            }

            if (!bContinueThisBranch || Cursor.DistanceFromArcher >= MaxDistanceFromArcher)
            {
                continue;
            }

            TArray<int32> NextCandidates;
            if (StepForwardBranches_(Cursor.PrevCellId, Cursor.CurCellId, NextCandidates))
            {
                for (const int32 NextCellId : NextCandidates)
                {
                    Cursors.Add({Cursor.CurCellId, NextCellId, Cursor.DistanceFromArcher + 1});
                }
            }
        }
    };

    for (int32 NeighborSlot = 0; NeighborSlot < 6; ++NeighborSlot)
    {
        const int32 FirstCellId = Cells[TargetCellId].NeighborCellIds[NeighborSlot];
        if (!IsHypotheticalFriendlyAtCell(FirstCellId))
        {
            continue;
        }

        TArray<int32> EnemyCandidates;
        if (StepForwardBranches_(TargetCellId, FirstCellId, EnemyCandidates))
        {
            for (const int32 EnemyCellId : EnemyCandidates)
            {
                TryAddHypotheticalEnemyAtCell(EnemyCellId);
            }
        }

        TArray<int32> EnemyCandidatesWithActionPieceAsFront;
        if (StepForwardBranches_(FirstCellId, TargetCellId, EnemyCandidatesWithActionPieceAsFront))
        {
            for (const int32 EnemyCellId : EnemyCandidatesWithActionPieceAsFront)
            {
                TryAddHypotheticalEnemyAtCell(EnemyCellId);
            }
        }

        ScanArcherRemoteCapture(TargetCellId, FirstCellId);
        ScanArcherRemoteCapture(FirstCellId, TargetCellId);
    }
}

void FTerraGameplayContainer::RebuildActionTargetCapturePreviews_(TArray<int32>& OutDirtyCellIds)
{
    ActionTargetCellIdToCaptureCellIds.Reset();

    const FTerraGameplayPieceState* Piece = GetPiece_(SelectedPieceId);
    if (!Piece || !Piece->bAlive)
    {
        return;
    }

    TSet<int32> AllActionTargetCellIds;
    for (const int32 TargetCellId : OrdinaryMoveTargetCellIds)
    {
        AllActionTargetCellIds.Add(TargetCellId);
    }
    for (const int32 TargetCellId : JumpTargetCellIds)
    {
        AllActionTargetCellIds.Add(TargetCellId);
    }

    for (const int32 TargetCellId : AllActionTargetCellIds)
    {
        TSet<int32> CaptureCellIds;
        CollectCaptureCellsAfterHypotheticalMove_(*Piece, TargetCellId, CaptureCellIds);
        if (CaptureCellIds.Num() == 0)
        {
            continue;
        }

        TSet<int32>& StoredCaptureCellIds = ActionTargetCellIdToCaptureCellIds.FindOrAdd(TargetCellId);
        for (const int32 CaptureCellId : CaptureCellIds)
        {
            StoredCaptureCellIds.Add(CaptureCellId);
            SetSingleHighlight_(CaptureCellId, GCapturePreviewCellColor, 1.0f, OutDirtyCellIds);
        }
    }
}

bool FTerraGameplayContainer::HasCapturePreviewForActionTarget_(int32 ActionTargetCellId) const
{
    const TSet<int32>* CaptureCellIds = ActionTargetCellIdToCaptureCellIds.Find(ActionTargetCellId);
    return CaptureCellIds && CaptureCellIds->Num() > 0;
}

void FTerraGameplayContainer::LockPendingCapturesForActionTarget_(int32 ActionTargetCellId, TArray<int32>& OutDirtyCellIds)
{
    PendingCapturePieceIds.Reset();
    PendingCaptureCellIds.Reset();

    const TSet<int32>* CaptureCellIds = ActionTargetCellIdToCaptureCellIds.Find(ActionTargetCellId);
    if (!CaptureCellIds)
    {
        return;
    }

    for (const int32 CaptureCellId : *CaptureCellIds)
    {
        const int32 CapturePieceId = GetPieceIdAtCell_(CaptureCellId);
        const FTerraGameplayPieceState* CapturePiece = GetPiece_(CapturePieceId);
        if (CapturePiece && CapturePiece->bAlive && CapturePiece->OwnerFactionId != CurrentFactionId)
        {
            PendingCapturePieceIds.Add(CapturePieceId);
            PendingCaptureCellIds.Add(CaptureCellId);
            SetSingleHighlight_(CaptureCellId, GCapturePreviewCellColor, 1.0f, OutDirtyCellIds);
        }
    }
}

void FTerraGameplayContainer::RebuildPendingCapturesForSelectedPieceCell_(TArray<int32>& OutDirtyCellIds)
{
    PendingCapturePieceIds.Reset();
    PendingCaptureCellIds.Reset();

    const FTerraGameplayPieceState* Piece = GetPiece_(SelectedPieceId);
    if (!Piece || !Piece->bAlive || !IsValidCellId_(Piece->CellId))
    {
        return;
    }

    TSet<int32> CaptureCellIds;
    CollectCaptureCellsAfterHypotheticalMove_(*Piece, Piece->CellId, CaptureCellIds);
    for (const int32 CaptureCellId : CaptureCellIds)
    {
        const int32 CapturePieceId = GetPieceIdAtCell_(CaptureCellId);
        const FTerraGameplayPieceState* CapturePiece = GetPiece_(CapturePieceId);
        if (CapturePiece && CapturePiece->bAlive && CapturePiece->OwnerFactionId != CurrentFactionId)
        {
            PendingCapturePieceIds.Add(CapturePieceId);
            PendingCaptureCellIds.Add(CaptureCellId);
            SetSingleHighlight_(CaptureCellId, GCapturePreviewCellColor, 1.0f, OutDirtyCellIds);
        }
    }
}

void FTerraGameplayContainer::ResolvePendingCaptures_(TArray<int32>& OutDirtyCellIds)
{
    for (const int32 CapturePieceId : PendingCapturePieceIds)
    {
        FTerraGameplayPieceState* CapturePiece = GetMutablePiece_(CapturePieceId);
        if (!CapturePiece || !CapturePiece->bAlive)
        {
            continue;
        }

        const int32 CaptureCellId = CapturePiece->CellId;
        if (IsValidCellId_(CaptureCellId) && GetPieceIdAtCell_(CaptureCellId) == CapturePieceId)
        {
            CellToPieceId[CaptureCellId] = INDEX_NONE;
        }
        CapturePiece->CellId = INDEX_NONE;
        CapturePiece->bAlive = false;
        AddDirtyCell_(CaptureCellId, OutDirtyCellIds);
    }

    PendingCapturePieceIds.Reset();
    PendingCaptureCellIds.Reset();
}

void FTerraGameplayContainer::RefreshSelectedPieceHighlights_(bool bIncludeOrdinaryMoves, TArray<int32>& OutDirtyCellIds)
{
    const FTerraGameplayPieceState* Piece = GetPiece_(SelectedPieceId);
    if (!Piece || !Piece->bAlive)
    {
        return;
    }

    OrdinaryMoveTargetCellIds.Reset();
    JumpTargetCellIds.Reset();
    if (bIncludeOrdinaryMoves)
    {
        CollectOrdinaryMoveTargets_(*Piece, OrdinaryMoveTargetCellIds);
    }
    CollectJumpTargets_(*Piece, JumpTargetCellIds);

    SetSingleHighlight_(Piece->CellId, GSelectedPieceCellColor, 1.0f, OutDirtyCellIds);
    for (const int32 TargetCellId : OrdinaryMoveTargetCellIds)
    {
        SetSingleHighlight_(TargetCellId, GActionTargetCellColor, 1.0f, OutDirtyCellIds);
    }
    for (const int32 TargetCellId : JumpTargetCellIds)
    {
        SetSingleHighlight_(TargetCellId, GActionTargetCellColor, 1.0f, OutDirtyCellIds);
    }

    RebuildActionTargetCapturePreviews_(OutDirtyCellIds);
}

bool FTerraGameplayContainer::TrySelectPieceAtCell_(int32 CellId, TArray<int32>& OutDirtyCellIds)
{
    const int32 PieceId = GetPieceIdAtCell_(CellId);
    const FTerraGameplayPieceState* Piece = GetPiece_(PieceId);
    if (!Piece || !IsPieceSelectable_(*Piece))
    {
        return false;
    }

    ClearGameplayHighlights_(OutDirtyCellIds);

    PendingCapturePieceIds.Reset();
    PendingCaptureCellIds.Reset();
    ActionTargetCellIdToCaptureCellIds.Reset();
    SelectedPieceId = PieceId;
    SelectedPieceStartCellId = Piece->CellId;
    LastJumpStartCellId = INDEX_NONE;
    InteractionPhase = ETerraGameplayInteractionPhase::PieceSelected;
    RefreshSelectedPieceHighlights_(true, OutDirtyCellIds);

    UE_LOG(LogTerraGameplay, Log,
        TEXT("[Gameplay][G3] Select Piece=%d Type=%d Cell=%d Faction=%d OrdinaryTargets=%d JumpTargets=%d"),
        Piece->PieceId,
        static_cast<int32>(Piece->PieceType),
        Piece->CellId,
        CurrentFactionId,
        OrdinaryMoveTargetCellIds.Num(),
        JumpTargetCellIds.Num());
    return true;
}

bool FTerraGameplayContainer::TryMoveSelectedPieceToOrdinaryTarget_(int32 TargetCellId, TArray<int32>& OutDirtyCellIds)
{
    FTerraGameplayPieceState* Piece = GetMutablePiece_(SelectedPieceId);
    if (!Piece || !OrdinaryMoveTargetCellIds.Contains(TargetCellId) || !CanOrdinaryMove_(*Piece, TargetCellId))
    {
        return false;
    }

    const int32 FromCellId = Piece->CellId;
    const bool bHasPendingCaptures = HasCapturePreviewForActionTarget_(TargetCellId);

    ClearGameplayHighlights_(OutDirtyCellIds);
    AddDirtyCell_(FromCellId, OutDirtyCellIds);
    AddDirtyCell_(TargetCellId, OutDirtyCellIds);

    CellToPieceId[FromCellId] = INDEX_NONE;
    CellToPieceId[TargetCellId] = Piece->PieceId;
    Piece->CellId = TargetCellId;
    LastJumpStartCellId = INDEX_NONE;

    InteractionPhase = ETerraGameplayInteractionPhase::PieceMovedCanEndTurn;
    if (bHasPendingCaptures)
    {
        LockPendingCapturesForActionTarget_(TargetCellId, OutDirtyCellIds);
    }
    ActionTargetCellIdToCaptureCellIds.Reset();
    SetSingleHighlight_(TargetCellId, GMovedPieceCanEndTurnColor, 1.0f, OutDirtyCellIds);

    UE_LOG(LogTerraGameplay, Log,
        TEXT("[Gameplay][G4] OrdinaryMove Piece=%d Faction=%d From=%d To=%d PendingCaptures=%d. Click same cell to end turn."),
        Piece->PieceId,
        CurrentFactionId,
        FromCellId,
        TargetCellId,
        PendingCapturePieceIds.Num());
    return true;
}

bool FTerraGameplayContainer::TryJumpSelectedPieceTo_(int32 TargetCellId, TArray<int32>& OutDirtyCellIds)
{
    FTerraGameplayPieceState* Piece = GetMutablePiece_(SelectedPieceId);
    if (!Piece || !JumpTargetCellIds.Contains(TargetCellId) || !IsCellEmpty_(TargetCellId) || !CanEnterTerrain_(*Piece, TargetCellId))
    {
        return false;
    }

    const int32 FromCellId = Piece->CellId;

    ClearGameplayHighlights_(OutDirtyCellIds);
    AddDirtyCell_(FromCellId, OutDirtyCellIds);
    AddDirtyCell_(TargetCellId, OutDirtyCellIds);

    CellToPieceId[FromCellId] = INDEX_NONE;
    CellToPieceId[TargetCellId] = Piece->PieceId;
    Piece->CellId = TargetCellId;
    LastJumpStartCellId = FromCellId;

    InteractionPhase = ETerraGameplayInteractionPhase::PieceJumpingCanContinue;
    RefreshSelectedPieceHighlights_(false, OutDirtyCellIds);
    RebuildPendingCapturesForSelectedPieceCell_(OutDirtyCellIds);

    UE_LOG(LogTerraGameplay, Log,
        TEXT("[Gameplay][G4] Jump Piece=%d Faction=%d From=%d To=%d CurrentNodeCaptures=%d ContinueJumpTargets=%d"),
        Piece->PieceId,
        CurrentFactionId,
        FromCellId,
        TargetCellId,
        PendingCapturePieceIds.Num(),
        JumpTargetCellIds.Num());
    return true;
}

bool FTerraGameplayContainer::TryEndTurnOnSelectedCell_(int32 CellId, TArray<int32>& OutDirtyCellIds)
{
    FTerraGameplayPieceState* Piece = GetMutablePiece_(SelectedPieceId);
    if (!Piece || Piece->CellId != CellId)
    {
        return false;
    }

    ClearGameplayHighlights_(OutDirtyCellIds);
    if (InteractionPhase == ETerraGameplayInteractionPhase::PieceJumpingCanContinue)
    {
        RebuildPendingCapturesForSelectedPieceCell_(OutDirtyCellIds);
    }
    const int32 CaptureCount = PendingCapturePieceIds.Num();
    ResolvePendingCaptures_(OutDirtyCellIds);
    ClearGameplayHighlights_(OutDirtyCellIds);
    AddDirtyCell_(CellId, OutDirtyCellIds);

    const int32 EndedFactionId = CurrentFactionId;
    SelectedPieceId = INDEX_NONE;
    SelectedPieceStartCellId = INDEX_NONE;
    LastJumpStartCellId = INDEX_NONE;
    ActionTargetCellIdToCaptureCellIds.Reset();
    InteractionPhase = ETerraGameplayInteractionPhase::Idle;
    AdvanceTurn_();

    UE_LOG(LogTerraGameplay, Log,
        TEXT("[Gameplay][G4] EndTurn. EndedFaction=%d NewFaction=%d TurnIndex=%d Captures=%d KeepSameFaction=%d"),
        EndedFactionId,
        CurrentFactionId,
        TurnIndex,
        CaptureCount,
        bDebugKeepSameFactionOnEndTurn ? 1 : 0);
    return true;
}

void FTerraGameplayContainer::SetSingleHighlight_(int32 CellId, const FLinearColor& Color, float Intensity, TArray<int32>& OutDirtyCellIds)
{
    if (!IsValidCellId_(CellId))
    {
        return;
    }

    FTerraGameplayCellHighlight& Highlight = GameplayHighlights.FindOrAdd(CellId);
    Highlight.Color = Color;
    Highlight.Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    AddDirtyCell_(CellId, OutDirtyCellIds);
}

void FTerraGameplayContainer::ClearGameplayHighlights_(TArray<int32>& OutDirtyCellIds)
{
    for (const TPair<int32, FTerraGameplayCellHighlight>& Pair : GameplayHighlights)
    {
        AddDirtyCell_(Pair.Key, OutDirtyCellIds);
    }
    GameplayHighlights.Reset();
    OrdinaryMoveTargetCellIds.Reset();
    JumpTargetCellIds.Reset();
}

void FTerraGameplayContainer::AddDirtyCell_(int32 CellId, TArray<int32>& OutDirtyCellIds) const
{
    if (IsValidCellId_(CellId))
    {
        OutDirtyCellIds.AddUnique(CellId);
    }
}

void FTerraGameplayContainer::AdvanceTurn_()
{
    if (Factions.Num() == 0)
    {
        CurrentFactionId = INDEX_NONE;
        ++TurnIndex;
        return;
    }

    if (bDebugKeepSameFactionOnEndTurn)
    {
        ++TurnIndex;
        return;
    }

    const int32 StartFactionId = CurrentFactionId;
    int32 CandidateFactionId = CurrentFactionId;
    do
    {
        CandidateFactionId = (CandidateFactionId + 1) % Factions.Num();
        if (Factions[CandidateFactionId].bAlive)
        {
            CurrentFactionId = CandidateFactionId;
            break;
        }
    } while (CandidateFactionId != StartFactionId);

    ++TurnIndex;
}