#include "TerraGameplayContainer.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"

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
    InitializeActionLogFilePath_();
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

    if (!bInitialized || bMatchEnded || !IsValidCellId_(CellId) || CurrentFactionId == INDEX_NONE)
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

bool FTerraGameplayContainer::CollectCurrentFactionSelectablePieceIds(TArray<int32>& OutPieceIds) const
{
    OutPieceIds.Reset();
    if (!bInitialized || bMatchEnded || CurrentFactionId == INDEX_NONE)
    {
        return false;
    }

    for (const FTerraGameplayPieceState& Piece : Pieces)
    {
        if (IsPieceSelectable_(Piece))
        {
            OutPieceIds.Add(Piece.PieceId);
        }
    }

    OutPieceIds.Sort();
    return OutPieceIds.Num() > 0;
}

bool FTerraGameplayContainer::CollectPendingCaptureEntries(TArray<FTerraGameplayCaptureEntry>& OutCaptureEntries) const
{
    OutCaptureEntries = GetSortedPendingCaptureEntries_();
    return OutCaptureEntries.Num() > 0;
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

bool FTerraGameplayContainer::GetCellTerrainType(int32 CellId, ETerraGameplayTerrainType& OutTerrainType) const
{
    OutTerrainType = ETerraGameplayTerrainType::Plain;
    if (!IsValidCellId_(CellId))
    {
        return false;
    }

    OutTerrainType = Cells[CellId].TerrainType;
    return true;
}

bool FTerraGameplayContainer::CollectAdjacentPieceIds(int32 CellId, int32 PerspectiveFactionId, TArray<int32>& OutFriendlyPieceIds, TArray<int32>& OutEnemyPieceIds) const
{
    OutFriendlyPieceIds.Reset();
    OutEnemyPieceIds.Reset();

    if (!IsValidCellId_(CellId))
    {
        return false;
    }

    for (int32 I = 0; I < 6; ++I)
    {
        const int32 NeighborCellId = Cells[CellId].NeighborCellIds[I];
        if (!IsValidCellId_(NeighborCellId))
        {
            continue;
        }

        const int32 NeighborPieceId = GetPieceIdAtCell_(NeighborCellId);
        const FTerraGameplayPieceState* NeighborPiece = GetPiece_(NeighborPieceId);
        if (!NeighborPiece || !NeighborPiece->bAlive)
        {
            continue;
        }

        if (NeighborPiece->OwnerFactionId == PerspectiveFactionId)
        {
            OutFriendlyPieceIds.AddUnique(NeighborPieceId);
        }
        else
        {
            OutEnemyPieceIds.AddUnique(NeighborPieceId);
        }
    }

    OutFriendlyPieceIds.Sort();
    OutEnemyPieceIds.Sort();
    return OutFriendlyPieceIds.Num() > 0 || OutEnemyPieceIds.Num() > 0;
}

bool FTerraGameplayContainer::FindNearestEnemyDistance(int32 CellId, int32 PerspectiveFactionId, int32& OutDistance) const
{
    OutDistance = INDEX_NONE;
    if (!IsValidCellId_(CellId))
    {
        return false;
    }

    if (const int32 PieceIdAtOrigin = GetPieceIdAtCell_(CellId);
        PieceIdAtOrigin != INDEX_NONE)
    {
        const FTerraGameplayPieceState* OriginPiece = GetPiece_(PieceIdAtOrigin);
        if (OriginPiece && OriginPiece->bAlive && OriginPiece->OwnerFactionId != PerspectiveFactionId)
        {
            OutDistance = 0;
            return true;
        }
    }

    TArray<int32> Queue;
    Queue.Add(CellId);

    TArray<int32> Distances;
    Distances.Init(INDEX_NONE, Cells.Num());
    Distances[CellId] = 0;

    for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
    {
        const int32 CurrentCellId = Queue[QueueIndex];
        const int32 CurrentDistance = Distances[CurrentCellId];

        for (int32 I = 0; I < 6; ++I)
        {
            const int32 NeighborCellId = Cells[CurrentCellId].NeighborCellIds[I];
            if (!IsValidCellId_(NeighborCellId) || Distances[NeighborCellId] != INDEX_NONE)
            {
                continue;
            }

            Distances[NeighborCellId] = CurrentDistance + 1;
            Queue.Add(NeighborCellId);

            const int32 NeighborPieceId = GetPieceIdAtCell_(NeighborCellId);
            const FTerraGameplayPieceState* NeighborPiece = GetPiece_(NeighborPieceId);
            if (NeighborPiece && NeighborPiece->bAlive && NeighborPiece->OwnerFactionId != PerspectiveFactionId)
            {
                OutDistance = Distances[NeighborCellId];
                return true;
            }
        }
    }

    return false;
}

bool FTerraGameplayContainer::QueryCurrentFactionPieceTurnSurvey(int32 PieceId, FPieceTurnSurvey& OutSurvey) const
{
    OutSurvey = FPieceTurnSurvey();

    if (!bInitialized || bMatchEnded || CurrentFactionId == INDEX_NONE)
    {
        return false;
    }

    const FTerraGameplayPieceState* Piece = GetPiece_(PieceId);
    if (!Piece || !Piece->bAlive || !IsPieceSelectable_(*Piece))
    {
        return false;
    }

    TMap<int32, int32> BestCaptureCountByLandingCell;

    TSet<int32> OrdinaryTargets;
    CollectOrdinaryMoveTargets_(*Piece, OrdinaryTargets);
    for (const int32 TargetCellId : OrdinaryTargets)
    {
        TMap<int32, FTerraGameplayCaptureEntry> CaptureEntriesByCellId;
        CollectCaptureEntriesAfterHypotheticalMove_(*Piece, TargetCellId, CaptureEntriesByCellId);
        BestCaptureCountByLandingCell.FindOrAdd(TargetCellId) = FMath::Max(BestCaptureCountByLandingCell.FindRef(TargetCellId), CaptureEntriesByCellId.Num());
    }

    TArray<int32> RootBoard = CellToPieceId;

    TFunction<void(int32, int32, const TArray<int32>&, TSet<int32>&)> ExploreJumpChains;
    ExploreJumpChains = [this, Piece, &BestCaptureCountByLandingCell, &ExploreJumpChains](int32 CurrentCellId, int32 BlockedReturnCellId, const TArray<int32>& HypotheticalBoard, TSet<int32>& VisitedLandingCells)
    {
        FTerraGameplayPieceState HypotheticalPiece = *Piece;
        HypotheticalPiece.CellId = CurrentCellId;

        TSet<int32> JumpTargets;
        CollectJumpTargetsForHypotheticalBoard_(HypotheticalPiece, CurrentFactionId, CurrentCellId, BlockedReturnCellId, HypotheticalBoard, JumpTargets);
        for (const int32 TargetCellId : JumpTargets)
        {
            if (VisitedLandingCells.Contains(TargetCellId))
            {
                continue;
            }

            TArray<int32> NextBoard = HypotheticalBoard;
            if (IsValidCellId_(CurrentCellId))
            {
                NextBoard[CurrentCellId] = INDEX_NONE;
            }
            if (IsValidCellId_(TargetCellId))
            {
                NextBoard[TargetCellId] = Piece->PieceId;
            }

            TMap<int32, FTerraGameplayCaptureEntry> CaptureEntriesByCellId;
            CollectCaptureEntriesAfterHypotheticalMove_(HypotheticalPiece, TargetCellId, CaptureEntriesByCellId);
            BestCaptureCountByLandingCell.FindOrAdd(TargetCellId) = FMath::Max(BestCaptureCountByLandingCell.FindRef(TargetCellId), CaptureEntriesByCellId.Num());

            VisitedLandingCells.Add(TargetCellId);
            ExploreJumpChains(TargetCellId, CurrentCellId, NextBoard, VisitedLandingCells);
            VisitedLandingCells.Remove(TargetCellId);
        }
    };

    TSet<int32> VisitedLandingCells;
    VisitedLandingCells.Add(Piece->CellId);
    ExploreJumpChains(Piece->CellId, INDEX_NONE, RootBoard, VisitedLandingCells);

    for (const TPair<int32, int32>& Pair : BestCaptureCountByLandingCell)
    {
        OutSurvey.ReachableActionCount++;
        OutSurvey.MaxCaptureCount = FMath::Max(OutSurvey.MaxCaptureCount, Pair.Value);
        if (Pair.Value > 0)
        {
            OutSurvey.bCanCaptureNow = true;
        }
    }

    for (const FTerraGameplayFactionState& Faction : Factions)
    {
        if (!Faction.bAlive || Faction.FactionId == CurrentFactionId)
        {
            continue;
        }

        for (const FTerraGameplayPieceState& EnemyPiece : Pieces)
        {
            if (!EnemyPiece.bAlive || EnemyPiece.OwnerFactionId != Faction.FactionId)
            {
                continue;
            }

            TSet<int32> EnemyOrdinaryTargets;
            CollectOrdinaryMoveTargetsForFaction_(EnemyPiece, Faction.FactionId, EnemyOrdinaryTargets);
            TSet<int32> JumpTargets;
            CollectJumpTargetsForFaction_(EnemyPiece, Faction.FactionId, INDEX_NONE, JumpTargets);
            if (EnemyOrdinaryTargets.Contains(Piece->CellId) || JumpTargets.Contains(Piece->CellId))
            {
                OutSurvey.bThreatenedIfHold = true;
                OutSurvey.HoldThreatCount++;
                OutSurvey.ThreateningPieceIds.AddUnique(EnemyPiece.PieceId);
            }
        }
    }

    OutSurvey.ThreateningPieceIds.Sort();
    return true;
}

bool FTerraGameplayContainer::BuildLocalTacticalSituationCard(const FLegalActionQuery& Action, FLocalTacticalSituationCard& OutCard) const
{
    OutCard = FLocalTacticalSituationCard();
    if (!bInitialized || bMatchEnded || CurrentFactionId == INDEX_NONE || !IsValidCellId_(Action.FromCellId) || !IsValidCellId_(Action.ToCellId))
    {
        return false;
    }

    const FTerraGameplayPieceState* MovingPiece = GetPiece_(Action.PieceId);
    if (!MovingPiece || !MovingPiece->bAlive || MovingPiece->OwnerFactionId != CurrentFactionId || MovingPiece->CellId != Action.FromCellId)
    {
        return false;
    }

    OutCard.PieceId = Action.PieceId;
    OutCard.PieceType = MovingPiece->PieceType;
    OutCard.FromCellId = Action.FromCellId;
    OutCard.ToCellId = Action.ToCellId;
    OutCard.bIsJump = Action.bIsJump;
    OutCard.CaptureCount = Action.CaptureEntries.Num();
    OutCard.FromTerrainType = Cells[Action.FromCellId].TerrainType;
    OutCard.ToTerrainType = Cells[Action.ToCellId].TerrainType;

    CollectAdjacentPieceIds(Action.FromCellId, CurrentFactionId, OutCard.FromNearbyFriendlyPieceIds, OutCard.FromNearbyEnemyPieceIds);
    FindNearestEnemyDistance(Action.FromCellId, CurrentFactionId, OutCard.NearestEnemyDistanceFrom);

    TArray<int32> HypotheticalBoard;
    BuildHypotheticalBoardForAction_(Action, HypotheticalBoard);
    CollectAdjacentPieceIdsForBoard_(Action.ToCellId, CurrentFactionId, HypotheticalBoard, OutCard.ToNearbyFriendlyPieceIds, OutCard.ToNearbyEnemyPieceIds);
    FindNearestEnemyDistanceForBoard_(Action.ToCellId, CurrentFactionId, HypotheticalBoard, OutCard.NearestEnemyDistanceTo);
    OutCard.SupportDelta = OutCard.ToNearbyFriendlyPieceIds.Num() - OutCard.FromNearbyFriendlyPieceIds.Num();

    FActionRiskQuery Risk;
    if (EvaluateCurrentFactionActionRisk(Action.PieceId, Action.ToCellId, Risk))
    {
        OutCard.bDestinationThreatened = Risk.bDestinationThreatened;
        OutCard.ThreatCount = Risk.ThreatCount;
        OutCard.ThreateningPieceIds = Risk.ThreateningPieceIds;
        OutCard.bEnemyResponseThreatensMovedPiece = Risk.bDestinationThreatened;
        OutCard.EnemyResponseThreateningPieceIds = Risk.ThreateningPieceIds;
        for (const int32 ThreateningPieceId : Risk.ThreateningPieceIds)
        {
            if (const FTerraGameplayPieceState* ThreateningPiece = GetPiece_(ThreateningPieceId);
                ThreateningPiece && ThreateningPiece->PieceType == ETerraGameplayPieceType::Archer)
            {
                OutCard.EnemyArcherLineThreateningPieceIds.Add(ThreateningPieceId);
            }
        }
    }

    TArray<int32> DistanceFromActionStart;
    DistanceFromActionStart.Init(INDEX_NONE, Cells.Num());
    TArray<int32> DistanceQueue;
    DistanceQueue.Add(Action.FromCellId);
    DistanceFromActionStart[Action.FromCellId] = 0;
    for (int32 QueueIndex = 0; QueueIndex < DistanceQueue.Num(); ++QueueIndex)
    {
        const int32 CurrentCellId = DistanceQueue[QueueIndex];
        for (int32 I = 0; I < 6; ++I)
        {
            const int32 NeighborCellId = Cells[CurrentCellId].NeighborCellIds[I];
            if (!IsValidCellId_(NeighborCellId) || DistanceFromActionStart[NeighborCellId] != INDEX_NONE)
            {
                continue;
            }
            DistanceFromActionStart[NeighborCellId] = DistanceFromActionStart[CurrentCellId] + 1;
            DistanceQueue.Add(NeighborCellId);
        }
    }

    for (int32 I = 0; I < 6; ++I)
    {
        const int32 NeighborCellId = Cells[Action.ToCellId].NeighborCellIds[I];
        if (!IsValidCellId_(NeighborCellId))
        {
            continue;
        }

        OutCard.ToCellNeighborCount++;
        const int32 DistanceToTarget = DistanceFromActionStart.IsValidIndex(Action.ToCellId) ? DistanceFromActionStart[Action.ToCellId] : INDEX_NONE;
        if (DistanceToTarget > 0 && DistanceFromActionStart[NeighborCellId] == DistanceToTarget - 1)
        {
            OutCard.ApproachCellIds.AddUnique(NeighborCellId);
            continue;
        }

        OutCard.ForwardNeighborCellIds.Add(NeighborCellId);
        const int32 NeighborPieceId = HypotheticalBoard.IsValidIndex(NeighborCellId) ? HypotheticalBoard[NeighborCellId] : INDEX_NONE;
        const FTerraGameplayPieceState* NeighborPiece = GetPiece_(NeighborPieceId);
        if (!NeighborPiece || !NeighborPiece->bAlive)
        {
            OutCard.ForwardEmptyCellCount++;
        }
        else if (NeighborPiece->OwnerFactionId == CurrentFactionId)
        {
            OutCard.ForwardFriendlyPieceIds.AddUnique(NeighborPieceId);
        }
        else
        {
            OutCard.ForwardEnemyPieceIds.AddUnique(NeighborPieceId);
        }

        if (MovingPiece->PieceType == ETerraGameplayPieceType::Cavalry
            && Cells[NeighborCellId].TerrainType == ETerraGameplayTerrainType::Mountain)
        {
            OutCard.ForwardBlockedByMountainCellIds.AddUnique(NeighborCellId);
        }
    }

    TArray<int32> Distances;
    Distances.Init(INDEX_NONE, Cells.Num());
    TArray<int32> Queue;
    Queue.Add(Action.ToCellId);
    Distances[Action.ToCellId] = 0;
    for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
    {
        const int32 CurrentCellId = Queue[QueueIndex];
        const int32 Distance = Distances[CurrentCellId];
        if (Distance >= 2)
        {
            continue;
        }

        for (int32 I = 0; I < 6; ++I)
        {
            const int32 NeighborCellId = Cells[CurrentCellId].NeighborCellIds[I];
            if (!IsValidCellId_(NeighborCellId) || Distances[NeighborCellId] != INDEX_NONE)
            {
                continue;
            }
            Distances[NeighborCellId] = Distance + 1;
            Queue.Add(NeighborCellId);

            const int32 PieceIdAtCell = HypotheticalBoard.IsValidIndex(NeighborCellId) ? HypotheticalBoard[NeighborCellId] : INDEX_NONE;
            const FTerraGameplayPieceState* PieceAtCell = GetPiece_(PieceIdAtCell);
            const bool bFriendly = PieceAtCell && PieceAtCell->bAlive && PieceAtCell->OwnerFactionId == CurrentFactionId;
            const bool bEnemy = PieceAtCell && PieceAtCell->bAlive && PieceAtCell->OwnerFactionId != CurrentFactionId;
            const ETerraGameplayTerrainType TerrainType = Cells[NeighborCellId].TerrainType;

            if (Distance == 0)
            {
                OutCard.RingOneCellCount++;
                OutCard.RingOneForestCellCount += TerrainType == ETerraGameplayTerrainType::Forest ? 1 : 0;
                OutCard.RingOneMountainCellCount += TerrainType == ETerraGameplayTerrainType::Mountain ? 1 : 0;
                OutCard.RingOneFriendlyPieceCount += bFriendly ? 1 : 0;
                OutCard.RingOneEnemyPieceCount += bEnemy ? 1 : 0;
            }
            else
            {
                OutCard.RingTwoCellCount++;
                OutCard.RingTwoForestCellCount += TerrainType == ETerraGameplayTerrainType::Forest ? 1 : 0;
                OutCard.RingTwoMountainCellCount += TerrainType == ETerraGameplayTerrainType::Mountain ? 1 : 0;
                OutCard.RingTwoFriendlyPieceCount += bFriendly ? 1 : 0;
                OutCard.RingTwoEnemyPieceCount += bEnemy ? 1 : 0;
            }
        }
    }

    FTerraGameplayPieceState PieceAfterMove = *MovingPiece;
    PieceAfterMove.CellId = Action.ToCellId;
    TSet<int32> BeforeEndpoints;
    int32 UnusedBeforeOrdinaryCount = 0;
    int32 UnusedBeforeJumpCount = 0;
    CollectReachableEndpointsForHypotheticalBoard_(*MovingPiece, CurrentFactionId, CellToPieceId, BeforeEndpoints, UnusedBeforeOrdinaryCount, UnusedBeforeJumpCount);
    OutCard.ReachableEndpointCountBeforeMove = BeforeEndpoints.Num();

    TSet<int32> AfterEndpoints;
    CollectReachableEndpointsForHypotheticalBoard_(PieceAfterMove, CurrentFactionId, HypotheticalBoard, AfterEndpoints, OutCard.OrdinaryTargetCountAfterMove, OutCard.JumpTargetCountAfterMove);
    OutCard.ReachableEndpointCountAfterMove = AfterEndpoints.Num();

    for (const int32 ForwardCellId : OutCard.ForwardNeighborCellIds)
    {
        const bool bEmpty = HypotheticalBoard.IsValidIndex(ForwardCellId) && HypotheticalBoard[ForwardCellId] == INDEX_NONE;
        if (bEmpty && CanEnterTerrain_(PieceAfterMove, ForwardCellId))
        {
            OutCard.ForwardEnterableCellCount++;
        }
    }

    for (const FTerraGameplayPieceState& FriendlyPiece : Pieces)
    {
        if (!FriendlyPiece.bAlive || FriendlyPiece.OwnerFactionId != CurrentFactionId || FriendlyPiece.PieceId == MovingPiece->PieceId)
        {
            continue;
        }

        TSet<int32> WithAnchorTargets;
        CollectJumpTargetsForHypotheticalBoard_(FriendlyPiece, CurrentFactionId, FriendlyPiece.CellId, INDEX_NONE, HypotheticalBoard, WithAnchorTargets);
        if (WithAnchorTargets.Num() == 0)
        {
            continue;
        }

        TArray<int32> WithoutMovedPieceBoard = HypotheticalBoard;
        if (WithoutMovedPieceBoard.IsValidIndex(Action.ToCellId))
        {
            WithoutMovedPieceBoard[Action.ToCellId] = INDEX_NONE;
        }
        TSet<int32> WithoutAnchorTargets;
        CollectJumpTargetsForHypotheticalBoard_(FriendlyPiece, CurrentFactionId, FriendlyPiece.CellId, INDEX_NONE, WithoutMovedPieceBoard, WithoutAnchorTargets);
        for (const int32 TargetCellId : WithAnchorTargets)
        {
            if (!WithoutAnchorTargets.Contains(TargetCellId))
            {
                OutCard.MovedPieceJumpAnchorFriendlyPieceIds.AddUnique(FriendlyPiece.PieceId);
                break;
            }
        }
    }

    const int32 ApproachToEnemy = OutCard.NearestEnemyDistanceFrom != INDEX_NONE && OutCard.NearestEnemyDistanceTo != INDEX_NONE
        ? OutCard.NearestEnemyDistanceFrom - OutCard.NearestEnemyDistanceTo
        : 0;
    if (ApproachToEnemy > 0 && !OutCard.bDestinationThreatened && OutCard.ReachableEndpointCountAfterMove >= OutCard.ReachableEndpointCountBeforeMove)
    {
        OutCard.SummaryTags.Add(TEXT("aggressive_safe_probe"));
    }
    if (MovingPiece->PieceType == ETerraGameplayPieceType::Cavalry && OutCard.ForwardBlockedByMountainCellIds.Num() > 0)
    {
        OutCard.SummaryTags.Add(TEXT("cavalry_forward_mountain_blocked"));
    }
    if (MovingPiece->PieceType == ETerraGameplayPieceType::Cavalry && OutCard.ReachableEndpointCountAfterMove < OutCard.ReachableEndpointCountBeforeMove)
    {
        OutCard.SummaryTags.Add(TEXT("cavalry_mobility_drop"));
    }
    if (MovingPiece->PieceType == ETerraGameplayPieceType::Archer
        && OutCard.ToTerrainType == ETerraGameplayTerrainType::Forest
        && !OutCard.bDestinationThreatened)
    {
        OutCard.SummaryTags.Add(TEXT("archer_forest_outpost"));
    }
    if (OutCard.MovedPieceJumpAnchorFriendlyPieceIds.Num() > 0)
    {
        OutCard.SummaryTags.Add(TEXT("jump_chain_anchor"));
    }
    if (ApproachToEnemy > 0 && OutCard.SupportDelta > 0)
    {
        OutCard.SummaryTags.Add(TEXT("supported_advance"));
    }
    if (ApproachToEnemy > 0 && OutCard.SupportDelta < 0 && (OutCard.bDestinationThreatened || OutCard.bEnemyResponseThreatensMovedPiece))
    {
        OutCard.SummaryTags.Add(TEXT("overextended_without_support"));
    }

    OutCard.ApproachCellIds.Sort();
    OutCard.ForwardNeighborCellIds.Sort();
    OutCard.ForwardFriendlyPieceIds.Sort();
    OutCard.ForwardEnemyPieceIds.Sort();
    OutCard.ForwardBlockedByMountainCellIds.Sort();
    OutCard.MovedPieceJumpAnchorFriendlyPieceIds.Sort();
    OutCard.EnemyArcherLineThreateningPieceIds.Sort();
    return true;
}

bool FTerraGameplayContainer::CollectCurrentFactionLegalActions(TArray<FLegalActionQuery>& OutActions) const
{
    OutActions.Reset();
    if (!bInitialized || bMatchEnded || CurrentFactionId == INDEX_NONE || InteractionPhase != ETerraGameplayInteractionPhase::Idle)
    {
        return false;
    }

    for (const FTerraGameplayPieceState& Piece : Pieces)
    {
        if (!IsPieceSelectable_(Piece))
        {
            continue;
        }

        TSet<int32> OrdinaryTargets;
        CollectOrdinaryMoveTargets_(Piece, OrdinaryTargets);
        for (const int32 TargetCellId : OrdinaryTargets)
        {
            FLegalActionQuery Action;
            Action.PieceId = Piece.PieceId;
            Action.FromCellId = Piece.CellId;
            Action.ToCellId = TargetCellId;
            Action.bIsJump = false;

            TMap<int32, FTerraGameplayCaptureEntry> CaptureEntriesByCellId;
            CollectCaptureEntriesAfterHypotheticalMove_(Piece, TargetCellId, CaptureEntriesByCellId);
            CaptureEntriesByCellId.GenerateValueArray(Action.CaptureEntries);
            Action.CaptureEntries.Sort([](const FTerraGameplayCaptureEntry& A, const FTerraGameplayCaptureEntry& B)
            {
                return A.CapturedPieceId < B.CapturedPieceId;
            });

            OutActions.Add(Action);
        }

        TSet<int32> JumpTargets;
        CollectJumpTargets_(Piece, JumpTargets);
        for (const int32 TargetCellId : JumpTargets)
        {
            FLegalActionQuery Action;
            Action.PieceId = Piece.PieceId;
            Action.FromCellId = Piece.CellId;
            Action.ToCellId = TargetCellId;
            Action.bIsJump = true;

            TMap<int32, FTerraGameplayCaptureEntry> CaptureEntriesByCellId;
            CollectCaptureEntriesAfterHypotheticalMove_(Piece, TargetCellId, CaptureEntriesByCellId);
            CaptureEntriesByCellId.GenerateValueArray(Action.CaptureEntries);
            Action.CaptureEntries.Sort([](const FTerraGameplayCaptureEntry& A, const FTerraGameplayCaptureEntry& B)
            {
                return A.CapturedPieceId < B.CapturedPieceId;
            });

            OutActions.Add(Action);
        }
    }

    OutActions.Sort([](const FLegalActionQuery& A, const FLegalActionQuery& B)
    {
        if (A.PieceId != B.PieceId)
        {
            return A.PieceId < B.PieceId;
        }
        if (A.ToCellId != B.ToCellId)
        {
            return A.ToCellId < B.ToCellId;
        }
        return static_cast<int32>(A.bIsJump) < static_cast<int32>(B.bIsJump);
    });

    return OutActions.Num() > 0;
}

bool FTerraGameplayContainer::CollectSelectedPieceLegalActions(TArray<FLegalActionQuery>& OutActions) const
{
    OutActions.Reset();
    if (!bInitialized || bMatchEnded || CurrentFactionId == INDEX_NONE)
    {
        return false;
    }

    const FTerraGameplayPieceState* Piece = GetPiece_(SelectedPieceId);
    if (!Piece || !Piece->bAlive || !IsCurrentFactionPiece_(*Piece))
    {
        return false;
    }

    auto AppendActionForTarget = [this, &OutActions, Piece](int32 TargetCellId, bool bIsJump)
    {
        FLegalActionQuery& Action = OutActions.AddDefaulted_GetRef();
        Action.PieceId = Piece->PieceId;
        Action.FromCellId = Piece->CellId;
        Action.ToCellId = TargetCellId;
        Action.bIsJump = bIsJump;

        TMap<int32, FTerraGameplayCaptureEntry> CaptureEntriesByCellId;
        CollectCaptureEntriesAfterHypotheticalMove_(*Piece, TargetCellId, CaptureEntriesByCellId);
        CaptureEntriesByCellId.GenerateValueArray(Action.CaptureEntries);
        Action.CaptureEntries.Sort([](const FTerraGameplayCaptureEntry& A, const FTerraGameplayCaptureEntry& B)
        {
            return A.CapturedPieceId < B.CapturedPieceId;
        });
    };

    for (const int32 TargetCellId : OrdinaryMoveTargetCellIds)
    {
        AppendActionForTarget(TargetCellId, false);
    }
    for (const int32 TargetCellId : JumpTargetCellIds)
    {
        AppendActionForTarget(TargetCellId, true);
    }

    OutActions.Sort([](const FLegalActionQuery& A, const FLegalActionQuery& B)
    {
        if (A.PieceId != B.PieceId)
        {
            return A.PieceId < B.PieceId;
        }
        if (A.FromCellId != B.FromCellId)
        {
            return A.FromCellId < B.FromCellId;
        }
        if (A.ToCellId != B.ToCellId)
        {
            return A.ToCellId < B.ToCellId;
        }
        return static_cast<int32>(A.bIsJump) < static_cast<int32>(B.bIsJump);
    });

    return OutActions.Num() > 0;
}

bool FTerraGameplayContainer::GetSelectedPieceLegalAction(int32 ToCellId, FLegalActionQuery& OutAction) const
{
    OutAction = FLegalActionQuery();

    TArray<FLegalActionQuery> Actions;
    if (!CollectSelectedPieceLegalActions(Actions))
    {
        return false;
    }

    for (const FLegalActionQuery& Action : Actions)
    {
        if (Action.ToCellId == ToCellId)
        {
            OutAction = Action;
            return true;
        }
    }

    return false;
}

bool FTerraGameplayContainer::IsCurrentFactionLegalAction(int32 PieceId, int32 ToCellId, FLegalActionQuery& OutAction) const
{
    OutAction = FLegalActionQuery();

    TArray<FLegalActionQuery> LegalActions;
    CollectCurrentFactionLegalActions(LegalActions);
    for (const FLegalActionQuery& Action : LegalActions)
    {
        if (Action.PieceId == PieceId && Action.ToCellId == ToCellId)
        {
            OutAction = Action;
            return true;
        }
    }

    return false;
}

bool FTerraGameplayContainer::EvaluateCurrentFactionActionRisk(int32 PieceId, int32 ToCellId, FActionRiskQuery& OutRisk) const
{
    OutRisk = FActionRiskQuery();

    FLegalActionQuery LegalAction;
    const bool bHasLegalAction = InteractionPhase == ETerraGameplayInteractionPhase::Idle
        ? IsCurrentFactionLegalAction(PieceId, ToCellId, LegalAction)
        : (GetSelectedPieceId() == PieceId && GetSelectedPieceLegalAction(ToCellId, LegalAction));
    if (!bHasLegalAction)
    {
        return false;
    }

    OutRisk.bValidAction = true;

    const FTerraGameplayPieceState* MovingPiece = GetPiece_(PieceId);
    if (!MovingPiece)
    {
        return true;
    }

    FTerraGameplayPieceState HypotheticalPiece = *MovingPiece;
    HypotheticalPiece.CellId = ToCellId;

    TSet<int32> CapturedPieceIds;
    for (const FTerraGameplayCaptureEntry& CaptureEntry : LegalAction.CaptureEntries)
    {
        CapturedPieceIds.Add(CaptureEntry.CapturedPieceId);
    }

    for (const FTerraGameplayFactionState& Faction : Factions)
    {
        if (!Faction.bAlive || Faction.FactionId == CurrentFactionId)
        {
            continue;
        }

        for (const FTerraGameplayPieceState& EnemyPiece : Pieces)
        {
            if (!EnemyPiece.bAlive
                || EnemyPiece.OwnerFactionId != Faction.FactionId
                || CapturedPieceIds.Contains(EnemyPiece.PieceId))
            {
                continue;
            }

            TSet<int32> OrdinaryTargets;
            CollectOrdinaryMoveTargetsForFaction_(EnemyPiece, Faction.FactionId, OrdinaryTargets);
            TSet<int32> JumpTargets;
            CollectJumpTargetsForFaction_(EnemyPiece, Faction.FactionId, INDEX_NONE, JumpTargets);

            if (!OrdinaryTargets.Contains(ToCellId) && !JumpTargets.Contains(ToCellId))
            {
                TMap<int32, FTerraGameplayCaptureEntry> CaptureEntriesByCellId;
                CollectCaptureEntriesAfterHypotheticalMoveForFaction_(EnemyPiece, HypotheticalPiece.CellId, Faction.FactionId, CaptureEntriesByCellId);
                bool bCanCaptureDestination = false;
                for (const TPair<int32, FTerraGameplayCaptureEntry>& Pair : CaptureEntriesByCellId)
                {
                    if (Pair.Value.CapturedPieceId == PieceId || Pair.Value.CapturedCellId == ToCellId)
                    {
                        bCanCaptureDestination = true;
                        break;
                    }
                }
                if (!bCanCaptureDestination)
                {
                    continue;
                }
            }

            OutRisk.bDestinationThreatened = true;
            OutRisk.ThreateningPieceIds.AddUnique(EnemyPiece.PieceId);
            OutRisk.ThreateningFactionIds.AddUnique(EnemyPiece.OwnerFactionId);
        }
    }

    OutRisk.ThreateningPieceIds.Sort();
    OutRisk.ThreateningFactionIds.Sort();
    OutRisk.ThreatCount = OutRisk.ThreateningPieceIds.Num();
    return true;
}

bool FTerraGameplayContainer::TryExecuteValidatedAction(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FValidatedActionExecutionResult& OutResult)
{
    OutResult = FValidatedActionExecutionResult();
    OutResult.TurnIndexBefore = TurnIndex;
    OutResult.FactionIdBefore = CurrentFactionId;
    OutResult.TurnIndexAfter = TurnIndex;
    OutResult.FactionIdAfter = CurrentFactionId;
    OutResult.PieceId = PieceId;
    OutResult.ToCellId = ToCellId;

    auto Reject = [this, &OutResult](const TCHAR* Reason) -> bool
    {
        OutResult.bAccepted = false;
        OutResult.bExecuted = false;
        OutResult.RejectReason = Reason;
        OutResult.TurnIndexAfter = TurnIndex;
        OutResult.FactionIdAfter = CurrentFactionId;
        return false;
    };

    if (!bInitialized)
    {
        return Reject(TEXT("gameplay_unavailable"));
    }
    if (bMatchEnded)
    {
        return Reject(TEXT("match_ended"));
    }
    if (InteractionPhase != ETerraGameplayInteractionPhase::Idle)
    {
        return Reject(TEXT("not_idle"));
    }
    if (TurnIndex != ExpectedTurnIndex || CurrentFactionId != ExpectedFactionId)
    {
        return Reject(TEXT("snapshot_mismatch"));
    }

    FLegalActionQuery LegalAction;
    if (!IsCurrentFactionLegalAction(PieceId, ToCellId, LegalAction))
    {
        return Reject(TEXT("not_current_faction_action"));
    }

    OutResult.bAccepted = true;
    OutResult.PieceId = LegalAction.PieceId;
    OutResult.FromCellId = LegalAction.FromCellId;
    OutResult.ToCellId = LegalAction.ToCellId;
    OutResult.bWasJump = LegalAction.bIsJump;
    OutResult.CaptureEntries = LegalAction.CaptureEntries;

    TArray<int32> DirtyCellIds;
    if (!HandleCellClick(LegalAction.FromCellId, DirtyCellIds))
    {
        OutResult.DirtyCellIds.Append(DirtyCellIds);
        return Reject(TEXT("select_failed"));
    }
    OutResult.DirtyCellIds.Append(DirtyCellIds);

    DirtyCellIds.Reset();
    if (!HandleCellClick(LegalAction.ToCellId, DirtyCellIds))
    {
        OutResult.DirtyCellIds.Append(DirtyCellIds);
        TArray<int32> UndoDirtyCellIds;
        UndoCurrentInteraction(UndoDirtyCellIds);
        OutResult.DirtyCellIds.Append(UndoDirtyCellIds);
        return Reject(TEXT("move_failed"));
    }
    OutResult.DirtyCellIds.Append(DirtyCellIds);

    DirtyCellIds.Reset();
    if (!HandleCellClick(LegalAction.ToCellId, DirtyCellIds))
    {
        OutResult.DirtyCellIds.Append(DirtyCellIds);
        TArray<int32> UndoDirtyCellIds;
        UndoCurrentInteraction(UndoDirtyCellIds);
        OutResult.DirtyCellIds.Append(UndoDirtyCellIds);
        return Reject(TEXT("end_turn_failed"));
    }
    OutResult.DirtyCellIds.Append(DirtyCellIds);

    OutResult.bExecuted = true;
    OutResult.RejectReason.Reset();
    OutResult.TurnIndexAfter = TurnIndex;
    OutResult.FactionIdAfter = CurrentFactionId;
    TSet<int32> UniqueDirtyCellIds;
    for (const int32 DirtyCellId : OutResult.DirtyCellIds)
    {
        if (DirtyCellId != INDEX_NONE)
        {
            UniqueDirtyCellIds.Add(DirtyCellId);
        }
    }
    OutResult.DirtyCellIds = UniqueDirtyCellIds.Array();
    OutResult.DirtyCellIds.Sort();
    return true;
}

bool FTerraGameplayContainer::UndoCurrentInteraction(TArray<int32>& OutDirtyCellIds)
{
    OutDirtyCellIds.Reset();

    if (InteractionUndoSnapshots.Num() <= 0 || InteractionPhase == ETerraGameplayInteractionPhase::Idle)
    {
        return false;
    }

    const FInteractionUndoSnapshot UndoSnapshot = InteractionUndoSnapshots.Pop(EAllowShrinking::No);

    TSet<int32> DirtyCellSet;
    for (const FTerraGameplayPieceState& Piece : Pieces)
    {
        if (Piece.bAlive && IsValidCellId_(Piece.CellId))
        {
            DirtyCellSet.Add(Piece.CellId);
        }
    }
    for (const FTerraGameplayPieceState& Piece : UndoSnapshot.Pieces)
    {
        if (Piece.bAlive && IsValidCellId_(Piece.CellId))
        {
            DirtyCellSet.Add(Piece.CellId);
        }
    }
    for (const TPair<int32, FTerraGameplayCellHighlight>& Pair : GameplayHighlights)
    {
        DirtyCellSet.Add(Pair.Key);
    }
    for (const TPair<int32, FTerraGameplayCellHighlight>& Pair : UndoSnapshot.GameplayHighlights)
    {
        DirtyCellSet.Add(Pair.Key);
    }

    Pieces = UndoSnapshot.Pieces;
    CellToPieceId = UndoSnapshot.CellToPieceId;
    GameplayHighlights = UndoSnapshot.GameplayHighlights;
    OrdinaryMoveTargetCellIds = UndoSnapshot.OrdinaryMoveTargetCellIds;
    JumpTargetCellIds = UndoSnapshot.JumpTargetCellIds;
    ActionTargetCellIdToCaptureCellIds = UndoSnapshot.ActionTargetCellIdToCaptureCellIds;
    PendingCaptureEntriesByPieceId = UndoSnapshot.PendingCaptureEntriesByPieceId;
    PendingCapturePieceIds = UndoSnapshot.PendingCapturePieceIds;
    PendingCaptureCellIds = UndoSnapshot.PendingCaptureCellIds;
    CurrentActionPathCellIds = UndoSnapshot.CurrentActionPathCellIds;
    SelectedPieceId = UndoSnapshot.SelectedPieceId;
    SelectedPieceStartCellId = UndoSnapshot.SelectedPieceStartCellId;
    LastJumpStartCellId = UndoSnapshot.LastJumpStartCellId;
    InteractionPhase = UndoSnapshot.InteractionPhase;

    for (const int32 DirtyCellId : DirtyCellSet)
    {
        AddDirtyCell_(DirtyCellId, OutDirtyCellIds);
    }

    UE_LOG(LogTerraGameplay, Log,
        TEXT("[Gameplay][G9] UndoCurrentInteraction Restored. CurrentFaction=%d TurnIndex=%d RemainingUndoDepth=%d"),
        CurrentFactionId,
        TurnIndex,
        InteractionUndoSnapshots.Num());
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
    PendingCaptureEntriesByPieceId.Reset();
    PendingCapturePieceIds.Reset();
    PendingCaptureCellIds.Reset();
    CellToPieceId.Init(INDEX_NONE, Cells.Num());
    CurrentActionPathCellIds.Reset();
    ActionLogFilePath.Reset();
    ClearUndoSnapshots_();

    CurrentFactionId = INDEX_NONE;
    TurnIndex = 0;
    WinningFactionId = INDEX_NONE;
    SelectedPieceId = INDEX_NONE;
    SelectedPieceStartCellId = INDEX_NONE;
    LastJumpStartCellId = INDEX_NONE;
    InteractionPhase = ETerraGameplayInteractionPhase::Idle;
    bMatchEnded = false;
    bInitialized = false;
}

void FTerraGameplayContainer::InitializeActionLogFilePath_()
{
    const FString LogDirectory = FPaths::ProjectLogDir();
    IFileManager::Get().MakeDirectory(*LogDirectory, true);

    const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    ActionLogFilePath = FPaths::Combine(LogDirectory, FString::Printf(TEXT("TerraGameplayActionLog_%s.jsonl"), *Timestamp));
}

void FTerraGameplayContainer::PushUndoSnapshot_()
{
    FInteractionUndoSnapshot& UndoSnapshot = InteractionUndoSnapshots.AddDefaulted_GetRef();
    UndoSnapshot.Pieces = Pieces;
    UndoSnapshot.CellToPieceId = CellToPieceId;
    UndoSnapshot.GameplayHighlights = GameplayHighlights;
    UndoSnapshot.OrdinaryMoveTargetCellIds = OrdinaryMoveTargetCellIds;
    UndoSnapshot.JumpTargetCellIds = JumpTargetCellIds;
    UndoSnapshot.ActionTargetCellIdToCaptureCellIds = ActionTargetCellIdToCaptureCellIds;
    UndoSnapshot.PendingCaptureEntriesByPieceId = PendingCaptureEntriesByPieceId;
    UndoSnapshot.PendingCapturePieceIds = PendingCapturePieceIds;
    UndoSnapshot.PendingCaptureCellIds = PendingCaptureCellIds;
    UndoSnapshot.CurrentActionPathCellIds = CurrentActionPathCellIds;
    UndoSnapshot.SelectedPieceId = SelectedPieceId;
    UndoSnapshot.SelectedPieceStartCellId = SelectedPieceStartCellId;
    UndoSnapshot.LastJumpStartCellId = LastJumpStartCellId;
    UndoSnapshot.InteractionPhase = InteractionPhase;
}

void FTerraGameplayContainer::ClearUndoSnapshots_()
{
    InteractionUndoSnapshots.Reset();
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

        int32 CommanderPieceId = INDEX_NONE;
        AddPieceIfFree_(FactionIndex, BaseCellId, ETerraGameplayPieceType::Commander, CommanderPieceId);
        Faction.CommanderPieceId = CommanderPieceId;

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
    Piece.bCanMove = PieceType != ETerraGameplayPieceType::Commander;

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
    return IsPieceSelectableForFaction_(Piece, CurrentFactionId);
}

bool FTerraGameplayContainer::IsPieceSelectableForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId) const
{
    return Piece.bAlive
        && Piece.OwnerFactionId == ActingFactionId
        && Piece.bCanMove
        && Piece.PieceType != ETerraGameplayPieceType::Commander;
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
    return CanOrdinaryMoveForFaction_(Piece, TargetCellId, CurrentFactionId);
}

bool FTerraGameplayContainer::CanOrdinaryMoveForFaction_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, int32 ActingFactionId) const
{
    return IsPieceSelectableForFaction_(Piece, ActingFactionId)
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
    CollectOrdinaryMoveTargetsForFaction_(Piece, CurrentFactionId, OutTargetCellIds);
}

void FTerraGameplayContainer::CollectOrdinaryMoveTargetsForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, TSet<int32>& OutTargetCellIds) const
{
    OutTargetCellIds.Reset();
    if (!IsPieceSelectableForFaction_(Piece, ActingFactionId))
    {
        return;
    }

    for (int32 I = 0; I < 6; ++I)
    {
        const int32 NeighborCellId = Cells[Piece.CellId].NeighborCellIds[I];
        if (CanOrdinaryMoveForFaction_(Piece, NeighborCellId, ActingFactionId))
        {
            OutTargetCellIds.Add(NeighborCellId);
        }
    }
}

void FTerraGameplayContainer::CollectJumpTargets_(const FTerraGameplayPieceState& Piece, TSet<int32>& OutTargetCellIds) const
{
    CollectJumpTargetsForFaction_(Piece, CurrentFactionId, LastJumpStartCellId, OutTargetCellIds);
}

void FTerraGameplayContainer::CollectJumpTargetsForFaction_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, int32 BlockedReturnCellId, TSet<int32>& OutTargetCellIds) const
{
    OutTargetCellIds.Reset();
    if (!IsPieceSelectableForFaction_(Piece, ActingFactionId))
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
            if (FirstLandingCellId == BlockedReturnCellId
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
                if (SecondLandingCellId != BlockedReturnCellId
                    && IsCellEmpty_(SecondLandingCellId)
                    && CanEnterTerrain_(Piece, SecondLandingCellId))
                {
                    OutTargetCellIds.Add(SecondLandingCellId);
                }
            }
        }
    }
}

void FTerraGameplayContainer::CollectCaptureEntriesAfterHypotheticalMove_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, TMap<int32, FTerraGameplayCaptureEntry>& OutCaptureEntriesByCellId) const
{
    CollectCaptureEntriesAfterHypotheticalMoveForFaction_(Piece, TargetCellId, CurrentFactionId, OutCaptureEntriesByCellId);
}

void FTerraGameplayContainer::CollectCaptureEntriesAfterHypotheticalMoveForFaction_(const FTerraGameplayPieceState& Piece, int32 TargetCellId, int32 ActingFactionId, TMap<int32, FTerraGameplayCaptureEntry>& OutCaptureEntriesByCellId) const
{
    OutCaptureEntriesByCellId.Reset();
    if (!IsPieceSelectableForFaction_(Piece, ActingFactionId) || !IsValidCellId_(TargetCellId))
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

    auto IsHypotheticalFriendlyAtCell = [ActingFactionId, &GetHypotheticalPieceAtCell](int32 CellId) -> bool
    {
        const FTerraGameplayPieceState* CandidatePiece = GetHypotheticalPieceAtCell(CellId);
        return CandidatePiece && CandidatePiece->bAlive && CandidatePiece->OwnerFactionId == ActingFactionId;
    };

    auto TryAddHypotheticalEnemyAtCell = [this, ActingFactionId, &GetHypotheticalPieceAtCell, &OutCaptureEntriesByCellId](int32 CellId, int32 AttackerPieceId, int32 VanguardPieceId)
    {
        const FTerraGameplayPieceState* CandidatePiece = GetHypotheticalPieceAtCell(CellId);
        if (CandidatePiece && CandidatePiece->bAlive && CandidatePiece->OwnerFactionId != ActingFactionId && !OutCaptureEntriesByCellId.Contains(CellId))
        {
            FTerraGameplayCaptureEntry CaptureEntry;
            CaptureEntry.CapturedPieceId = CandidatePiece->PieceId;
            CaptureEntry.CapturedCellId = CellId;
            CaptureEntry.AttackerPieceId = AttackerPieceId;
            CaptureEntry.VanguardPieceId = VanguardPieceId;
            OutCaptureEntriesByCellId.Add(CellId, CaptureEntry);
        }
    };

    auto ScanArcherRemoteCapture = [this, ActingFactionId, &GetHypotheticalPieceAtCell, &TryAddHypotheticalEnemyAtCell](int32 ArcherCellId, int32 FrontCellId)
    {
        const FTerraGameplayPieceState* ArcherPiece = GetHypotheticalPieceAtCell(ArcherCellId);
        const FTerraGameplayPieceState* FrontPiece = GetHypotheticalPieceAtCell(FrontCellId);
        if (!ArcherPiece || !ArcherPiece->bAlive
            || ArcherPiece->OwnerFactionId != ActingFactionId
            || ArcherPiece->PieceType != ETerraGameplayPieceType::Archer
            || !FrontPiece || !FrontPiece->bAlive
            || FrontPiece->OwnerFactionId != ActingFactionId)
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
            else if (CandidatePiece->OwnerFactionId == ActingFactionId)
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
                TryAddHypotheticalEnemyAtCell(Cursor.CurCellId, ArcherPiece->PieceId, FrontPiece->PieceId);
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

        const FTerraGameplayPieceState* FirstPiece = GetHypotheticalPieceAtCell(FirstCellId);
        if (!FirstPiece)
        {
            continue;
        }

        TArray<int32> EnemyCandidates;
        if (StepForwardBranches_(TargetCellId, FirstCellId, EnemyCandidates))
        {
            for (const int32 EnemyCellId : EnemyCandidates)
            {
                TryAddHypotheticalEnemyAtCell(EnemyCellId, Piece.PieceId, FirstPiece->PieceId);
            }
        }

        TArray<int32> EnemyCandidatesWithActionPieceAsFront;
        if (StepForwardBranches_(FirstCellId, TargetCellId, EnemyCandidatesWithActionPieceAsFront))
        {
            for (const int32 EnemyCellId : EnemyCandidatesWithActionPieceAsFront)
            {
                TryAddHypotheticalEnemyAtCell(EnemyCellId, FirstPiece->PieceId, Piece.PieceId);
            }
        }

        ScanArcherRemoteCapture(TargetCellId, FirstCellId);
        ScanArcherRemoteCapture(FirstCellId, TargetCellId);
    }
}

void FTerraGameplayContainer::BuildHypotheticalBoardForAction_(const FLegalActionQuery& Action, TArray<int32>& OutCellToPieceId) const
{
    OutCellToPieceId = CellToPieceId;
    if (!OutCellToPieceId.IsValidIndex(Action.FromCellId) || !OutCellToPieceId.IsValidIndex(Action.ToCellId))
    {
        return;
    }

    OutCellToPieceId[Action.FromCellId] = INDEX_NONE;
    for (const FTerraGameplayCaptureEntry& CaptureEntry : Action.CaptureEntries)
    {
        if (OutCellToPieceId.IsValidIndex(CaptureEntry.CapturedCellId))
        {
            OutCellToPieceId[CaptureEntry.CapturedCellId] = INDEX_NONE;
        }
    }
    OutCellToPieceId[Action.ToCellId] = Action.PieceId;
}

void FTerraGameplayContainer::CollectAdjacentPieceIdsForBoard_(int32 CellId, int32 PerspectiveFactionId, const TArray<int32>& HypotheticalCellToPieceId, TArray<int32>& OutFriendlyPieceIds, TArray<int32>& OutEnemyPieceIds) const
{
    OutFriendlyPieceIds.Reset();
    OutEnemyPieceIds.Reset();
    if (!IsValidCellId_(CellId))
    {
        return;
    }

    for (int32 I = 0; I < 6; ++I)
    {
        const int32 NeighborCellId = Cells[CellId].NeighborCellIds[I];
        if (!IsValidCellId_(NeighborCellId) || !HypotheticalCellToPieceId.IsValidIndex(NeighborCellId))
        {
            continue;
        }
        const FTerraGameplayPieceState* NeighborPiece = GetPiece_(HypotheticalCellToPieceId[NeighborCellId]);
        if (!NeighborPiece || !NeighborPiece->bAlive)
        {
            continue;
        }
        if (NeighborPiece->OwnerFactionId == PerspectiveFactionId)
        {
            OutFriendlyPieceIds.AddUnique(NeighborPiece->PieceId);
        }
        else
        {
            OutEnemyPieceIds.AddUnique(NeighborPiece->PieceId);
        }
    }

    OutFriendlyPieceIds.Sort();
    OutEnemyPieceIds.Sort();
}

bool FTerraGameplayContainer::FindNearestEnemyDistanceForBoard_(int32 CellId, int32 PerspectiveFactionId, const TArray<int32>& HypotheticalCellToPieceId, int32& OutDistance) const
{
    OutDistance = INDEX_NONE;
    if (!IsValidCellId_(CellId) || !HypotheticalCellToPieceId.IsValidIndex(CellId))
    {
        return false;
    }

    TArray<int32> Distances;
    Distances.Init(INDEX_NONE, Cells.Num());
    TArray<int32> Queue;
    Queue.Add(CellId);
    Distances[CellId] = 0;

    for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
    {
        const int32 CurrentCellId = Queue[QueueIndex];
        const int32 PieceIdAtCell = HypotheticalCellToPieceId.IsValidIndex(CurrentCellId) ? HypotheticalCellToPieceId[CurrentCellId] : INDEX_NONE;
        const FTerraGameplayPieceState* PieceAtCell = GetPiece_(PieceIdAtCell);
        if (PieceAtCell && PieceAtCell->bAlive && PieceAtCell->OwnerFactionId != PerspectiveFactionId)
        {
            OutDistance = Distances[CurrentCellId];
            return true;
        }

        for (int32 I = 0; I < 6; ++I)
        {
            const int32 NeighborCellId = Cells[CurrentCellId].NeighborCellIds[I];
            if (!IsValidCellId_(NeighborCellId) || Distances[NeighborCellId] != INDEX_NONE)
            {
                continue;
            }
            Distances[NeighborCellId] = Distances[CurrentCellId] + 1;
            Queue.Add(NeighborCellId);
        }
    }

    return false;
}

void FTerraGameplayContainer::CollectOrdinaryMoveTargetsForHypotheticalBoard_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, const TArray<int32>& HypotheticalCellToPieceId, TSet<int32>& OutTargetCellIds) const
{
    OutTargetCellIds.Reset();
    if (!IsPieceSelectableForFaction_(Piece, ActingFactionId) || !IsValidCellId_(Piece.CellId))
    {
        return;
    }

    for (int32 I = 0; I < 6; ++I)
    {
        const int32 NeighborCellId = Cells[Piece.CellId].NeighborCellIds[I];
        if (!IsValidCellId_(NeighborCellId)
            || !HypotheticalCellToPieceId.IsValidIndex(NeighborCellId)
            || HypotheticalCellToPieceId[NeighborCellId] != INDEX_NONE
            || !CanEnterTerrain_(Piece, NeighborCellId))
        {
            continue;
        }
        OutTargetCellIds.Add(NeighborCellId);
    }
}

void FTerraGameplayContainer::CollectReachableEndpointsForHypotheticalBoard_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, const TArray<int32>& HypotheticalCellToPieceId, TSet<int32>& OutEndpointCellIds, int32& OutOrdinaryTargetCount, int32& OutJumpTargetCount) const
{
    OutEndpointCellIds.Reset();
    OutOrdinaryTargetCount = 0;
    OutJumpTargetCount = 0;
    if (!IsPieceSelectableForFaction_(Piece, ActingFactionId))
    {
        return;
    }

    TSet<int32> OrdinaryTargets;
    CollectOrdinaryMoveTargetsForHypotheticalBoard_(Piece, ActingFactionId, HypotheticalCellToPieceId, OrdinaryTargets);
    OutOrdinaryTargetCount = OrdinaryTargets.Num();
    OutEndpointCellIds.Append(OrdinaryTargets);

    TSet<int32> InitialJumpTargets;
    CollectJumpTargetsForHypotheticalBoard_(Piece, ActingFactionId, Piece.CellId, INDEX_NONE, HypotheticalCellToPieceId, InitialJumpTargets);
    OutJumpTargetCount = InitialJumpTargets.Num();

    TFunction<void(int32, int32, const TArray<int32>&, TSet<int32>&)> ExploreJumpChains;
    ExploreJumpChains = [this, &Piece, ActingFactionId, &OutEndpointCellIds, &ExploreJumpChains](int32 CurrentCellId, int32 BlockedReturnCellId, const TArray<int32>& Board, TSet<int32>& VisitedLandingCells)
    {
        FTerraGameplayPieceState CurrentPiece = Piece;
        CurrentPiece.CellId = CurrentCellId;
        TSet<int32> JumpTargets;
        CollectJumpTargetsForHypotheticalBoard_(CurrentPiece, ActingFactionId, CurrentCellId, BlockedReturnCellId, Board, JumpTargets);
        for (const int32 TargetCellId : JumpTargets)
        {
            if (VisitedLandingCells.Contains(TargetCellId))
            {
                continue;
            }
            OutEndpointCellIds.Add(TargetCellId);
            TArray<int32> NextBoard = Board;
            if (NextBoard.IsValidIndex(CurrentCellId))
            {
                NextBoard[CurrentCellId] = INDEX_NONE;
            }
            if (NextBoard.IsValidIndex(TargetCellId))
            {
                NextBoard[TargetCellId] = Piece.PieceId;
            }
            VisitedLandingCells.Add(TargetCellId);
            ExploreJumpChains(TargetCellId, CurrentCellId, NextBoard, VisitedLandingCells);
            VisitedLandingCells.Remove(TargetCellId);
        }
    };

    TSet<int32> VisitedLandingCells;
    VisitedLandingCells.Add(Piece.CellId);
    ExploreJumpChains(Piece.CellId, INDEX_NONE, HypotheticalCellToPieceId, VisitedLandingCells);
}

void FTerraGameplayContainer::CollectJumpTargetsForHypotheticalBoard_(const FTerraGameplayPieceState& Piece, int32 ActingFactionId, int32 CurrentCellId, int32 BlockedReturnCellId, const TArray<int32>& HypotheticalCellToPieceId, TSet<int32>& OutTargetCellIds) const
{
    OutTargetCellIds.Reset();
    if (!IsPieceSelectableForFaction_(Piece, ActingFactionId) || !IsValidCellId_(CurrentCellId))
    {
        return;
    }

    auto IsHypotheticalEmpty = [this, &HypotheticalCellToPieceId](int32 CellId) -> bool
    {
        return IsValidCellId_(CellId) && HypotheticalCellToPieceId.IsValidIndex(CellId) && HypotheticalCellToPieceId[CellId] == INDEX_NONE;
    };

    auto HasHypotheticalPiece = [this, &HypotheticalCellToPieceId](int32 CellId) -> bool
    {
        return IsValidCellId_(CellId) && HypotheticalCellToPieceId.IsValidIndex(CellId) && HypotheticalCellToPieceId[CellId] != INDEX_NONE;
    };

    for (int32 I = 0; I < 6; ++I)
    {
        const int32 MiddleCellId = Cells[CurrentCellId].NeighborCellIds[I];
        if (!IsValidCellId_(MiddleCellId) || !HasHypotheticalPiece(MiddleCellId))
        {
            continue;
        }

        if (Piece.PieceType == ETerraGameplayPieceType::Cavalry
            && Cells[MiddleCellId].TerrainType == ETerraGameplayTerrainType::Mountain)
        {
            continue;
        }

        TArray<int32> FirstLandingCandidates;
        if (!StepForwardBranches_(CurrentCellId, MiddleCellId, FirstLandingCandidates))
        {
            continue;
        }

        for (const int32 FirstLandingCellId : FirstLandingCandidates)
        {
            if (FirstLandingCellId == BlockedReturnCellId
                || !IsHypotheticalEmpty(FirstLandingCellId)
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
                if (SecondLandingCellId != BlockedReturnCellId
                    && IsHypotheticalEmpty(SecondLandingCellId)
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

    TMap<int32, FTerraGameplayCaptureEntry> CaptureEntriesByCellId;
    CollectCaptureEntriesAfterHypotheticalMove_(Piece, TargetCellId, CaptureEntriesByCellId);
    for (const TPair<int32, FTerraGameplayCaptureEntry>& Pair : CaptureEntriesByCellId)
    {
        OutCaptureCellIds.Add(Pair.Key);
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
    PendingCaptureEntriesByPieceId.Reset();
    PendingCapturePieceIds.Reset();
    PendingCaptureCellIds.Reset();

    const FTerraGameplayPieceState* Piece = GetPiece_(SelectedPieceId);
    if (!Piece || !Piece->bAlive)
    {
        return;
    }

    TMap<int32, FTerraGameplayCaptureEntry> CaptureEntriesByCellId;
    CollectCaptureEntriesAfterHypotheticalMove_(*Piece, ActionTargetCellId, CaptureEntriesByCellId);
    for (const TPair<int32, FTerraGameplayCaptureEntry>& Pair : CaptureEntriesByCellId)
    {
        const FTerraGameplayCaptureEntry& CaptureEntry = Pair.Value;
        if (CaptureEntry.CapturedPieceId != INDEX_NONE && CaptureEntry.CapturedCellId != INDEX_NONE)
        {
            PendingCaptureEntriesByPieceId.Add(CaptureEntry.CapturedPieceId, CaptureEntry);
            PendingCapturePieceIds.Add(CaptureEntry.CapturedPieceId);
            PendingCaptureCellIds.Add(CaptureEntry.CapturedCellId);
            SetSingleHighlight_(CaptureEntry.CapturedCellId, GCapturePreviewCellColor, 1.0f, OutDirtyCellIds);
        }
    }
}

void FTerraGameplayContainer::RebuildPendingCapturesForSelectedPieceCell_(TArray<int32>& OutDirtyCellIds)
{
    PendingCaptureEntriesByPieceId.Reset();
    PendingCapturePieceIds.Reset();
    PendingCaptureCellIds.Reset();

    const FTerraGameplayPieceState* Piece = GetPiece_(SelectedPieceId);
    if (!Piece || !Piece->bAlive || !IsValidCellId_(Piece->CellId))
    {
        return;
    }

    TMap<int32, FTerraGameplayCaptureEntry> CaptureEntriesByCellId;
    CollectCaptureEntriesAfterHypotheticalMove_(*Piece, Piece->CellId, CaptureEntriesByCellId);
    for (const TPair<int32, FTerraGameplayCaptureEntry>& Pair : CaptureEntriesByCellId)
    {
        const FTerraGameplayCaptureEntry& CaptureEntry = Pair.Value;
        if (CaptureEntry.CapturedPieceId != INDEX_NONE && CaptureEntry.CapturedCellId != INDEX_NONE)
        {
            PendingCaptureEntriesByPieceId.Add(CaptureEntry.CapturedPieceId, CaptureEntry);
            PendingCapturePieceIds.Add(CaptureEntry.CapturedPieceId);
            PendingCaptureCellIds.Add(CaptureEntry.CapturedCellId);
            SetSingleHighlight_(CaptureEntry.CapturedCellId, GCapturePreviewCellColor, 1.0f, OutDirtyCellIds);
        }
    }
}

void FTerraGameplayContainer::ResolvePendingCaptures_(TArray<int32>& OutDirtyCellIds)
{
    TSet<int32> DefeatedFactionIds;

    for (const int32 CapturePieceId : PendingCapturePieceIds)
    {
        FTerraGameplayPieceState* CapturePiece = GetMutablePiece_(CapturePieceId);
        if (!CapturePiece || !CapturePiece->bAlive)
        {
            continue;
        }

        if (CapturePiece->PieceType == ETerraGameplayPieceType::Commander)
        {
            DefeatedFactionIds.Add(CapturePiece->OwnerFactionId);
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

    PendingCaptureEntriesByPieceId.Reset();
    PendingCapturePieceIds.Reset();
    PendingCaptureCellIds.Reset();

    for (const int32 FactionId : DefeatedFactionIds)
    {
        EliminateFaction_(FactionId, OutDirtyCellIds);
    }

    EvaluateWinStateAfterCaptures_();
}

void FTerraGameplayContainer::EliminateFaction_(int32 FactionId, TArray<int32>& OutDirtyCellIds)
{
    if (!Factions.IsValidIndex(FactionId) || !Factions[FactionId].bAlive)
    {
        return;
    }

    FTerraGameplayFactionState& Faction = Factions[FactionId];
    Faction.bAlive = false;

    for (FTerraGameplayPieceState& Piece : Pieces)
    {
        if (!Piece.bAlive || Piece.OwnerFactionId != FactionId)
        {
            continue;
        }

        const int32 PieceCellId = Piece.CellId;
        if (IsValidCellId_(PieceCellId) && GetPieceIdAtCell_(PieceCellId) == Piece.PieceId)
        {
            CellToPieceId[PieceCellId] = INDEX_NONE;
        }

        Piece.CellId = INDEX_NONE;
        Piece.bAlive = false;
        AddDirtyCell_(PieceCellId, OutDirtyCellIds);
    }

    UE_LOG(LogTerraGameplay, Log,
        TEXT("[Gameplay][G6] FactionEliminated Faction=%d"),
        FactionId);
}

void FTerraGameplayContainer::EvaluateWinStateAfterCaptures_()
{
    WinningFactionId = INDEX_NONE;
    bMatchEnded = false;

    int32 AliveFactionCount = 0;
    int32 LastAliveFactionId = INDEX_NONE;
    for (const FTerraGameplayFactionState& Faction : Factions)
    {
        if (!Faction.bAlive)
        {
            continue;
        }

        ++AliveFactionCount;
        LastAliveFactionId = Faction.FactionId;
    }

    if (AliveFactionCount == 1)
    {
        bMatchEnded = true;
        WinningFactionId = LastAliveFactionId;
        CurrentFactionId = WinningFactionId;

        UE_LOG(LogTerraGameplay, Log,
            TEXT("[Gameplay][G6] MatchEnded WinnerFaction=%d TurnIndex=%d"),
            WinningFactionId,
            TurnIndex);
    }
    else if (AliveFactionCount <= 0)
    {
        bMatchEnded = true;
        CurrentFactionId = INDEX_NONE;

        UE_LOG(LogTerraGameplay, Warning,
            TEXT("[Gameplay][G6] MatchEndedWithoutWinner TurnIndex=%d"),
            TurnIndex);
    }
}

void FTerraGameplayContainer::FinalizeTurnAfterResolution_()
{
    if (bMatchEnded)
    {
        return;
    }

    AdvanceTurn_();
}

TArray<FTerraGameplayCaptureEntry> FTerraGameplayContainer::GetSortedPendingCaptureEntries_() const
{
    TArray<FTerraGameplayCaptureEntry> CaptureEntries;
    PendingCaptureEntriesByPieceId.GenerateValueArray(CaptureEntries);
    CaptureEntries.Sort([](const FTerraGameplayCaptureEntry& Left, const FTerraGameplayCaptureEntry& Right)
    {
        return Left.CapturedPieceId < Right.CapturedPieceId;
    });
    return CaptureEntries;
}

FString FTerraGameplayContainer::BuildActionLogJson_(int32 ActionTurnIndex, int32 PlayerId, int32 PieceId, const TArray<int32>& PathCellIds, const TArray<FTerraGameplayCaptureEntry>& CaptureEntries) const
{
    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
    RootObject->SetNumberField(TEXT("turnIndex"), ActionTurnIndex);
    RootObject->SetNumberField(TEXT("playerId"), PlayerId);
    RootObject->SetNumberField(TEXT("pieceId"), PieceId);

    TArray<TSharedPtr<FJsonValue>> PathCellJsonValues;
    PathCellJsonValues.Reserve(PathCellIds.Num());
    for (const int32 PathCellId : PathCellIds)
    {
        PathCellJsonValues.Add(MakeShared<FJsonValueNumber>(PathCellId));
    }
    RootObject->SetArrayField(TEXT("pathCellIds"), PathCellJsonValues);

    TArray<TSharedPtr<FJsonValue>> CapturedPieceJsonValues;
    TArray<TSharedPtr<FJsonValue>> AttackerPieceJsonValues;
    TArray<TSharedPtr<FJsonValue>> VanguardPieceJsonValues;
    CapturedPieceJsonValues.Reserve(CaptureEntries.Num());
    AttackerPieceJsonValues.Reserve(CaptureEntries.Num());
    VanguardPieceJsonValues.Reserve(CaptureEntries.Num());
    for (const FTerraGameplayCaptureEntry& CaptureEntry : CaptureEntries)
    {
        CapturedPieceJsonValues.Add(MakeShared<FJsonValueNumber>(CaptureEntry.CapturedPieceId));
        AttackerPieceJsonValues.Add(MakeShared<FJsonValueNumber>(CaptureEntry.AttackerPieceId));
        VanguardPieceJsonValues.Add(MakeShared<FJsonValueNumber>(CaptureEntry.VanguardPieceId));
    }
    RootObject->SetArrayField(TEXT("capturedPieceIds"), CapturedPieceJsonValues);
    RootObject->SetArrayField(TEXT("attackerPieceIds"), AttackerPieceJsonValues);
    RootObject->SetArrayField(TEXT("vanguardPieceIds"), VanguardPieceJsonValues);

    FString OutputJson;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputJson);
    FJsonSerializer::Serialize(RootObject, Writer);
    return OutputJson;
}

void FTerraGameplayContainer::AppendActionLogToFile_(const FString& ActionLogJson) const
{
    const FString FileLine = ActionLogJson + LINE_TERMINATOR;
    if (!ActionLogFilePath.IsEmpty())
    {
        FFileHelper::SaveStringToFile(FileLine, *ActionLogFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
    }
}

void FTerraGameplayContainer::EmitActionLog_(int32 ActionTurnIndex, int32 PlayerId, int32 PieceId, const TArray<int32>& PathCellIds, const TArray<FTerraGameplayCaptureEntry>& CaptureEntries) const
{
    const FString ActionLogJson = BuildActionLogJson_(ActionTurnIndex, PlayerId, PieceId, PathCellIds, CaptureEntries);
    AppendActionLogToFile_(ActionLogJson);

    UE_LOG(LogTerraGameplay, Log, TEXT("[Gameplay][G7] %s"), *ActionLogJson);
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

    if (InteractionPhase == ETerraGameplayInteractionPhase::Idle)
    {
        PushUndoSnapshot_();
    }

    ClearGameplayHighlights_(OutDirtyCellIds);

    PendingCaptureEntriesByPieceId.Reset();
    PendingCapturePieceIds.Reset();
    PendingCaptureCellIds.Reset();
    ActionTargetCellIdToCaptureCellIds.Reset();
    SelectedPieceId = PieceId;
    SelectedPieceStartCellId = Piece->CellId;
    CurrentActionPathCellIds.Reset();
    CurrentActionPathCellIds.Add(Piece->CellId);
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

    PushUndoSnapshot_();

    const int32 FromCellId = Piece->CellId;
    const bool bHasPendingCaptures = HasCapturePreviewForActionTarget_(TargetCellId);

    ClearGameplayHighlights_(OutDirtyCellIds);
    AddDirtyCell_(FromCellId, OutDirtyCellIds);
    AddDirtyCell_(TargetCellId, OutDirtyCellIds);

    CellToPieceId[FromCellId] = INDEX_NONE;
    CellToPieceId[TargetCellId] = Piece->PieceId;
    Piece->CellId = TargetCellId;
    CurrentActionPathCellIds.Add(TargetCellId);
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

    PushUndoSnapshot_();

    const int32 FromCellId = Piece->CellId;

    ClearGameplayHighlights_(OutDirtyCellIds);
    AddDirtyCell_(FromCellId, OutDirtyCellIds);
    AddDirtyCell_(TargetCellId, OutDirtyCellIds);

    CellToPieceId[FromCellId] = INDEX_NONE;
    CellToPieceId[TargetCellId] = Piece->PieceId;
    Piece->CellId = TargetCellId;
    CurrentActionPathCellIds.Add(TargetCellId);
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

    const int32 EndedFactionId = CurrentFactionId;
    const int32 ActionTurnIndex = TurnIndex;
    const int32 ActionPieceId = Piece->PieceId;
    const TArray<int32> ActionPathCellIds = CurrentActionPathCellIds;
    TArray<FTerraGameplayCaptureEntry> CaptureEntries = GetSortedPendingCaptureEntries_();

    ClearGameplayHighlights_(OutDirtyCellIds);
    if (InteractionPhase == ETerraGameplayInteractionPhase::PieceJumpingCanContinue)
    {
        RebuildPendingCapturesForSelectedPieceCell_(OutDirtyCellIds);
        CaptureEntries = GetSortedPendingCaptureEntries_();
    }
    const int32 CaptureCount = PendingCapturePieceIds.Num();
    ResolvePendingCaptures_(OutDirtyCellIds);
    EmitActionLog_(ActionTurnIndex, EndedFactionId, ActionPieceId, ActionPathCellIds, CaptureEntries);
    ClearGameplayHighlights_(OutDirtyCellIds);
    AddDirtyCell_(CellId, OutDirtyCellIds);

    SelectedPieceId = INDEX_NONE;
    SelectedPieceStartCellId = INDEX_NONE;
    LastJumpStartCellId = INDEX_NONE;
    CurrentActionPathCellIds.Reset();
    ActionTargetCellIdToCaptureCellIds.Reset();
    InteractionPhase = ETerraGameplayInteractionPhase::Idle;
    ClearUndoSnapshots_();
    FinalizeTurnAfterResolution_();

    UE_LOG(LogTerraGameplay, Log,
        TEXT("[Gameplay][G6] EndTurn. EndedFaction=%d NewFaction=%d TurnIndex=%d Captures=%d Winner=%d MatchEnded=%d KeepSameFaction=%d"),
        EndedFactionId,
        CurrentFactionId,
        TurnIndex,
        CaptureCount,
        WinningFactionId,
        bMatchEnded ? 1 : 0,
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


