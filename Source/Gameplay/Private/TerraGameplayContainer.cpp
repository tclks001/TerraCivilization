#include "TerraGameplayContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerraGameplay, Log, All);

namespace
{
    constexpr FLinearColor GSelectedPieceCellColor(1.0f, 1.0f, 0.0f, 1.0f);
    constexpr FLinearColor GMovedPieceCanEndTurnColor(0.0f, 0.35f, 1.0f, 1.0f);
    constexpr FLinearColor GActionTargetCellColor(0.35f, 0.80f, 1.0f, 1.0f);
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

        int32 FlagPieceId = INDEX_NONE;
        AddPieceIfFree_(FactionIndex, BaseCellId, ETerraGameplayPieceType::Flag, FlagPieceId);
        Faction.FlagPieceId = FlagPieceId;

        const FTerraGameplayCellState& BaseCell = Cells[BaseCellId];
        TArray<int32> InfantryCells;
        TArray<int32> CavalryCells;
        InfantryCells.Reserve(5);
        CavalryCells.Reserve(5);

        for (int32 NeighborSlot = 0; NeighborSlot < 5; ++NeighborSlot)
        {
            const int32 InfantryCellId = BaseCell.NeighborCellIds[NeighborSlot];
            if (!IsValidCellId_(InfantryCellId))
            {
                continue;
            }

            int32 InfantryPieceId = INDEX_NONE;
            AddPieceIfFree_(FactionIndex, InfantryCellId, ETerraGameplayPieceType::Infantry, InfantryPieceId);
            InfantryCells.Add(InfantryCellId);

            const FTerraGameplayCellState& InfantryCell = Cells[InfantryCellId];
            int32 BackToBaseIndex = INDEX_NONE;
            int32 NeighborCount = 0;
            for (int32 I = 0; I < 6; ++I)
            {
                const int32 NeighborCellId = InfantryCell.NeighborCellIds[I];
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

            const int32 CavalryCellId = InfantryCell.NeighborCellIds[(BackToBaseIndex + 3) % 6];
            CavalryCells.Add(CavalryCellId);

            int32 CavalryPieceId = INDEX_NONE;
            AddPieceIfFree_(FactionIndex, CavalryCellId, ETerraGameplayPieceType::Cavalry, CavalryPieceId);
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

            int32 ArcherCellId = INDEX_NONE;
            for (int32 SlotA = 0; SlotA < 6; ++SlotA)
            {
                const int32 CandidateCellId = CavalryA.NeighborCellIds[SlotA];
                if (!IsValidCellId_(CandidateCellId)
                    || CandidateCellId == BaseCellId
                    || InfantryCells.Contains(CandidateCellId)
                    || CavalryCells.Contains(CandidateCellId)
                    || !IsCellEmpty_(CandidateCellId))
                {
                    continue;
                }

                for (int32 SlotB = 0; SlotB < 6; ++SlotB)
                {
                    if (CavalryB.NeighborCellIds[SlotB] == CandidateCellId)
                    {
                        ArcherCellId = CandidateCellId;
                        break;
                    }
                }

                if (ArcherCellId != INDEX_NONE)
                {
                    break;
                }
            }

            int32 ArcherPieceId = INDEX_NONE;
            AddPieceIfFree_(FactionIndex, ArcherCellId, ETerraGameplayPieceType::Archer, ArcherPieceId);
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

    ClearGameplayHighlights_(OutDirtyCellIds);
    AddDirtyCell_(FromCellId, OutDirtyCellIds);
    AddDirtyCell_(TargetCellId, OutDirtyCellIds);

    CellToPieceId[FromCellId] = INDEX_NONE;
    CellToPieceId[TargetCellId] = Piece->PieceId;
    Piece->CellId = TargetCellId;
    LastJumpStartCellId = INDEX_NONE;

    InteractionPhase = ETerraGameplayInteractionPhase::PieceMovedCanEndTurn;
    SetSingleHighlight_(TargetCellId, GMovedPieceCanEndTurnColor, 1.0f, OutDirtyCellIds);

    UE_LOG(LogTerraGameplay, Log,
        TEXT("[Gameplay][G3] OrdinaryMove Piece=%d Faction=%d From=%d To=%d. Click same cell to end turn."),
        Piece->PieceId,
        CurrentFactionId,
        FromCellId,
        TargetCellId);
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

    UE_LOG(LogTerraGameplay, Log,
        TEXT("[Gameplay][G3] Jump Piece=%d Faction=%d From=%d To=%d. ContinueJumpTargets=%d"),
        Piece->PieceId,
        CurrentFactionId,
        FromCellId,
        TargetCellId,
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
    AddDirtyCell_(CellId, OutDirtyCellIds);

    const int32 EndedFactionId = CurrentFactionId;
    SelectedPieceId = INDEX_NONE;
    SelectedPieceStartCellId = INDEX_NONE;
    LastJumpStartCellId = INDEX_NONE;
    InteractionPhase = ETerraGameplayInteractionPhase::Idle;
    AdvanceTurn_();

    UE_LOG(LogTerraGameplay, Log,
        TEXT("[Gameplay][G3] EndTurn. EndedFaction=%d NewFaction=%d TurnIndex=%d KeepSameFaction=%d"),
        EndedFactionId,
        CurrentFactionId,
        TurnIndex,
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
