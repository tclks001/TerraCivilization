#include "Render/PlanetGameplayComponent.h"

#include "Render/PlanetTessellatedMesh.h"
#include "Render/PlanetCameraComponent.h"
#include "Render/PlanetPiecePresentationComponent.h"
#include "TerraNpcMcpGameplayBridge.h"

#include "FCell.h"
#include "FSphereTopology.h"
#include "CellGeoData.h"
#include "WorldGenerator.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"

UPlanetGameplayComponent::UPlanetGameplayComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

APlanetTessellatedMesh* UPlanetGameplayComponent::GetHost() const
{
    return Cast<APlanetTessellatedMesh>(GetOwner());
}

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogPlanetGameplayComponent, Log, All);
}

void UPlanetGameplayComponent::RebuildGameplay()
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return;
    }

    if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent())
    {
        Camera->ClearDelayedTurnStartFocusTimer();
    }

    if (!Host->CellTopology.IsValid() || !Host->Generator.IsValid())
    {
        if (GameplayContainer.IsValid())
        {
            FTerraNpcMcpGameplayBridge::UnregisterExecuteValidatedActionDelegate();
            FTerraNpcMcpGameplayBridge::UnregisterGameplayContainer(GameplayContainer.Get());
        }
        GameplayContainer.Reset();
        Host->ClearP1PiecePresentation_();
        G2_5LastHighlightedFactionId = INDEX_NONE;
        if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent())
        {
            Camera->ResetCameraState();
        }
        return;
    }

    const int32 NumCells = Host->CellTopology->Cells.Num();
    const TArray<FCellGeoData>& GeoCells = Host->Generator->GetCellData();
    if (GeoCells.Num() != NumCells)
    {
        UE_LOG(LogPlanetGameplayComponent, Warning,
            TEXT("[PlanetGameplay][G2] Skip Gameplay rebuild: WorldGen cell count mismatch. Got=%d Expected=%d"),
            GeoCells.Num(),
            NumCells);
        if (GameplayContainer.IsValid())
        {
            FTerraNpcMcpGameplayBridge::UnregisterExecuteValidatedActionDelegate();
            FTerraNpcMcpGameplayBridge::UnregisterGameplayContainer(GameplayContainer.Get());
        }
        GameplayContainer.Reset();
        Host->ClearP1PiecePresentation_();
        G2_5LastHighlightedFactionId = INDEX_NONE;
        if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent())
        {
            Camera->ResetCameraState();
        }
        return;
    }

    TArray<FTerraGameplayCellState> GameplayCells;
    GameplayCells.SetNum(NumCells);

    for (int32 CellId = 0; CellId < NumCells; ++CellId)
    {
        const FCell& SourceCell = Host->CellTopology->Cells[CellId];
        FTerraGameplayCellState& TargetCell = GameplayCells[CellId];
        TargetCell.CellId = CellId;
        TargetCell.bIsPentagon = SourceCell.bIsPentagon;
        TargetCell.NeighborCellIds = SourceCell.NeighborCellIds;

        switch (GeoCells[CellId].SimpleTerrainType)
        {
        case ETerraSimpleTerrainType::Forest:
            TargetCell.TerrainType = ETerraGameplayTerrainType::Forest;
            break;
        case ETerraSimpleTerrainType::Mountain:
            TargetCell.TerrainType = ETerraGameplayTerrainType::Mountain;
            break;
        case ETerraSimpleTerrainType::Plain:
        default:
            TargetCell.TerrainType = ETerraGameplayTerrainType::Plain;
            break;
        }
    }

    if (GameplayContainer.IsValid())
    {
        FTerraNpcMcpGameplayBridge::UnregisterExecuteValidatedActionDelegate();
        FTerraNpcMcpGameplayBridge::UnregisterGameplayContainer(GameplayContainer.Get());
    }

    GameplayContainer = MakeUnique<FTerraGameplayContainer>();
    GameplayContainer->Initialize(GameplayCells);
    GameplayContainer->SetDebugKeepSameFactionOnEndTurn(bG3DebugKeepSameFactionOnEndTurn);
    FTerraNpcMcpGameplayBridge::RegisterGameplayContainer(GameplayContainer.Get());
    FTerraNpcMcpGameplayBridge::RegisterExecuteValidatedActionDelegate(
        [this](int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult)
        {
            return TryExecuteNpcMcpValidatedAction(ExpectedTurnIndex, ExpectedFactionId, PieceId, ToCellId, OutResult);
        });

    G2_5LastHighlightedFactionId = GameplayContainer->GetCurrentFactionId();
    if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent())
    {
        Camera->ResetCameraState();
    }
    RefreshCurrentFactionPieceHighlights();
    Host->SyncP1PiecePresentation_();
}

void UPlanetGameplayComponent::RefreshGameplayHighlights(const TArray<int32>& DirtyCellIds)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host)
    {
        return;
    }

    for (const int32 CellId : DirtyCellIds)
    {
        Host->WriteHISMHighlightForCell_(CellId);
    }
}

void UPlanetGameplayComponent::RefreshFactionPieceHighlights(int32 FactionId)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host || !GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        return;
    }

    TArray<int32> PieceCellIds;
    if (!GameplayContainer->CollectFactionPieceCellIds(FactionId, PieceCellIds))
    {
        return;
    }

    for (const int32 CellId : PieceCellIds)
    {
        Host->WriteHISMHighlightForCell_(CellId);
    }
}

void UPlanetGameplayComponent::RefreshCurrentFactionPieceHighlights()
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host || !GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        return;
    }

    TArray<int32> PieceCellIds;
    if (!GameplayContainer->CollectCurrentFactionPieceCellIds(PieceCellIds))
    {
        return;
    }

    for (const int32 CellId : PieceCellIds)
    {
        Host->WriteHISMHighlightForCell_(CellId);
    }
}

void UPlanetGameplayComponent::RebuildG1DebugPieces()
{
    APlanetTessellatedMesh* Host = GetHost();
    G1DebugPieces.Reset();

    if (!Host || !bEnableG1DebugPieces || !Host->CellTopology.IsValid())
    {
        return;
    }

    if (GameplayContainer.IsValid() && GameplayContainer->IsInitialized())
    {
        for (const FTerraGameplayPieceState& Piece : GameplayContainer->GetPieces())
        {
            if (!Piece.bAlive || !Host->CellTopology->Cells.IsValidIndex(Piece.CellId))
            {
                continue;
            }

            FTerraG1DebugPiece& DebugPiece = G1DebugPieces.AddDefaulted_GetRef();
            DebugPiece.FactionId = Piece.OwnerFactionId;
            DebugPiece.CellId = Piece.CellId;

            switch (Piece.PieceType)
            {
            case ETerraGameplayPieceType::Commander:
                DebugPiece.PieceType = ETerraG1DebugPieceType::Base;
                break;
            case ETerraGameplayPieceType::Cavalry:
                DebugPiece.PieceType = ETerraG1DebugPieceType::Cavalry;
                break;
            case ETerraGameplayPieceType::Archer:
                DebugPiece.PieceType = ETerraG1DebugPieceType::Archer;
                break;
            case ETerraGameplayPieceType::Infantry:
            default:
                DebugPiece.PieceType = ETerraG1DebugPieceType::Infantry;
                break;
            }
        }
        return;
    }

    const int32 NumCells = Host->CellTopology->Cells.Num();
    TSet<int32> OccupiedCells;

    auto IsValidCellId = [NumCells](int32 CellId)
    {
        return CellId >= 0 && CellId < NumCells;
    };

    auto AddPiece = [this, &OccupiedCells, &IsValidCellId](int32 FactionId, int32 CellId, ETerraG1DebugPieceType PieceType)
    {
        if (!IsValidCellId(CellId))
        {
            UE_LOG(LogPlanetGameplayComponent, Warning,
                TEXT("[PlanetGameplay][G1] Skip invalid debug piece cell. Faction=%d Cell=%d Type=%d"),
                FactionId,
                CellId,
                static_cast<int32>(PieceType));
            return false;
        }

        if (OccupiedCells.Contains(CellId))
        {
            UE_LOG(LogPlanetGameplayComponent, Warning,
                TEXT("[PlanetGameplay][G1] Skip occupied debug piece cell. Faction=%d Cell=%d Type=%d"),
                FactionId,
                CellId,
                static_cast<int32>(PieceType));
            return false;
        }

        OccupiedCells.Add(CellId);

        FTerraG1DebugPiece& Piece = G1DebugPieces.AddDefaulted_GetRef();
        Piece.FactionId = FactionId;
        Piece.CellId = CellId;
        Piece.PieceType = PieceType;
        return true;
    };

    TArray<int32> PentagonCellIds;
    for (const FCell& Cell : Host->CellTopology->Cells)
    {
        if (Cell.bIsPentagon)
        {
            PentagonCellIds.Add(Cell.CellId);
        }
    }

    PentagonCellIds.Sort();

    if (PentagonCellIds.Num() != 12)
    {
        UE_LOG(LogPlanetGameplayComponent, Warning,
            TEXT("[PlanetGameplay][G1] Expected 12 pentagon bases, got %d."),
            PentagonCellIds.Num());
    }

    int32 BaseCount = 0;
    int32 InfantryCount = 0;
    int32 CavalryCount = 0;
    int32 ArcherCount = 0;

    for (int32 FactionId = 0; FactionId < PentagonCellIds.Num(); ++FactionId)
    {
        const int32 BaseCellId = PentagonCellIds[FactionId];
        if (!IsValidCellId(BaseCellId))
        {
            continue;
        }

        const FCell& BaseCell = Host->CellTopology->Cells[BaseCellId];
        if (AddPiece(FactionId, BaseCellId, ETerraG1DebugPieceType::Base))
        {
            ++BaseCount;
        }

        TArray<int32> ArcherCells;
        TArray<int32> CavalryCells;
        ArcherCells.Reserve(5);
        CavalryCells.Reserve(5);

        for (int32 NeighborSlot = 0; NeighborSlot < 5; ++NeighborSlot)
        {
            const int32 ArcherCellId = BaseCell.NeighborCellIds[NeighborSlot];
            if (!IsValidCellId(ArcherCellId))
            {
                UE_LOG(LogPlanetGameplayComponent, Warning,
                    TEXT("[PlanetGameplay][G1] Invalid archer neighbor. Faction=%d Base=%d Slot=%d Cell=%d"),
                    FactionId,
                    BaseCellId,
                    NeighborSlot,
                    ArcherCellId);
                continue;
            }

            ArcherCells.Add(ArcherCellId);
            if (AddPiece(FactionId, ArcherCellId, ETerraG1DebugPieceType::Archer))
            {
                ++ArcherCount;
            }

            const FCell& ArcherCell = Host->CellTopology->Cells[ArcherCellId];
            int32 BackToBaseIndex = INDEX_NONE;
            int32 NeighborCount = 0;
            for (int32 I = 0; I < 6; ++I)
            {
                const int32 NeighborCellId = ArcherCell.NeighborCellIds[I];
                if (IsValidCellId(NeighborCellId))
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
                UE_LOG(LogPlanetGameplayComponent, Warning,
                    TEXT("[PlanetGameplay][G1] Cannot resolve cavalry opposite cell. Faction=%d Base=%d Archer=%d NeighborCount=%d BackIndex=%d"),
                    FactionId,
                    BaseCellId,
                    ArcherCellId,
                    NeighborCount,
                    BackToBaseIndex);
                CavalryCells.Add(INDEX_NONE);
                continue;
            }

            const int32 CavalryCellId = ArcherCell.NeighborCellIds[(BackToBaseIndex + 3) % 6];
            CavalryCells.Add(CavalryCellId);
            if (AddPiece(FactionId, CavalryCellId, ETerraG1DebugPieceType::Cavalry))
            {
                ++CavalryCount;
            }
        }

        for (int32 I = 0; I < CavalryCells.Num(); ++I)
        {
            const int32 CavalryAId = CavalryCells[I];
            const int32 CavalryBId = CavalryCells[(I + 1) % CavalryCells.Num()];
            if (!IsValidCellId(CavalryAId) || !IsValidCellId(CavalryBId))
            {
                continue;
            }

            const FCell& CavalryA = Host->CellTopology->Cells[CavalryAId];
            const FCell& CavalryB = Host->CellTopology->Cells[CavalryBId];
            const FVector MidDir = (CavalryA.UnitCenter + CavalryB.UnitCenter).GetSafeNormal();

            int32 BestInfantryCellId = INDEX_NONE;
            float BestScore = -FLT_MAX;

            for (int32 SlotA = 0; SlotA < 6; ++SlotA)
            {
                const int32 CandidateCellId = CavalryA.NeighborCellIds[SlotA];
                if (!IsValidCellId(CandidateCellId)
                    || CandidateCellId == BaseCellId
                    || ArcherCells.Contains(CandidateCellId)
                    || CavalryCells.Contains(CandidateCellId)
                    || OccupiedCells.Contains(CandidateCellId))
                {
                    continue;
                }

                bool bAlsoNeighborOfB = false;
                for (int32 SlotB = 0; SlotB < 6; ++SlotB)
                {
                    if (CavalryB.NeighborCellIds[SlotB] == CandidateCellId)
                    {
                        bAlsoNeighborOfB = true;
                        break;
                    }
                }

                if (!bAlsoNeighborOfB)
                {
                    continue;
                }

                const float Score = static_cast<float>(FVector::DotProduct(Host->CellTopology->Cells[CandidateCellId].UnitCenter, MidDir));
                if (Score > BestScore)
                {
                    BestScore = Score;
                    BestInfantryCellId = CandidateCellId;
                }
            }

            if (BestInfantryCellId == INDEX_NONE)
            {
                UE_LOG(LogPlanetGameplayComponent, Warning,
                    TEXT("[PlanetGameplay][G1] Cannot resolve infantry between cavalry cells. Faction=%d Base=%d CavalryA=%d CavalryB=%d"),
                    FactionId,
                    BaseCellId,
                    CavalryAId,
                    CavalryBId);
                continue;
            }

            if (AddPiece(FactionId, BestInfantryCellId, ETerraG1DebugPieceType::Infantry))
            {
                ++InfantryCount;
            }
        }
    }

    UE_LOG(LogPlanetGameplayComponent, Log,
        TEXT("[PlanetGameplay][G1] Rebuilt debug pieces: Factions=%d Bases=%d Infantry=%d Cavalry=%d Archer=%d Total=%d"),
        PentagonCellIds.Num(),
        BaseCount,
        InfantryCount,
        CavalryCount,
        ArcherCount,
        G1DebugPieces.Num());
}

void UPlanetGameplayComponent::DrawG1DebugPieces() const
{
    const APlanetTessellatedMesh* Host = GetHost();
    UWorld* World = Host ? Host->GetWorld() : nullptr;
    if (Host
        && Host->GetPlanetPiecePresentationComponent()
        && Host->GetPlanetPiecePresentationComponent()->bHideG1DebugPiecesWhenP1IsActive
        && Host->GetPlanetPiecePresentationComponent()->bEnableP1PiecePresentation
        && World
        && World->IsGameWorld())
    {
        return;
    }

    if (!Host || !bEnableG1DebugPieces || G1DebugPieces.Num() == 0 || !Host->CellTopology.IsValid() || !World)
    {
        return;
    }

    const float DrawRadius = FMath::Max(10.0f, G1DebugPieceRadiusCM);
    const float DrawDistance = Host->GlobeRadiusCM + G1DebugPieceHeightOffsetCM;
    const FTransform ActorTransform = Host->GetActorTransform();

    for (const FTerraG1DebugPiece& Piece : G1DebugPieces)
    {
        if (!Host->CellTopology->Cells.IsValidIndex(Piece.CellId))
        {
            continue;
        }

        FColor DrawColor = FColor::White;
        switch (Piece.PieceType)
        {
        case ETerraG1DebugPieceType::Base:
            DrawColor = FColor::Red;
            break;
        case ETerraG1DebugPieceType::Infantry:
            DrawColor = FColor::Green;
            break;
        case ETerraG1DebugPieceType::Cavalry:
            DrawColor = FColor::Blue;
            break;
        case ETerraG1DebugPieceType::Archer:
            DrawColor = FColor::Yellow;
            break;
        default:
            break;
        }

        const FVector WorldPos = ActorTransform.TransformPosition(Host->CellTopology->Cells[Piece.CellId].UnitCenter * DrawDistance);
        DrawDebugSphere(World, WorldPos, DrawRadius, 12, DrawColor, false, -1.0f, 0, 2.0f);
    }
}

bool UPlanetGameplayComponent::HandleC5NavigateCurrentFactionPiece(bool bReverse)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host || !GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        UE_LOG(LogPlanetGameplayComponent, Warning,
            TEXT("[PlanetGameplay][C5] Tab navigation ignored: GameplayContainer is not ready."));
        return false;
    }

    const ETerraGameplayInteractionPhase Phase = GameplayContainer->GetInteractionPhase();
    if (Phase != ETerraGameplayInteractionPhase::Idle
        && Phase != ETerraGameplayInteractionPhase::PieceSelected)
    {
        UE_LOG(LogPlanetGameplayComponent, Log,
            TEXT("[PlanetGameplay][C5] Tab navigation ignored in Phase=%d."),
            static_cast<int32>(Phase));
        return false;
    }

    TArray<int32> SelectablePieceIds;
    if (!GameplayContainer->CollectCurrentFactionSelectablePieceIds(SelectablePieceIds))
    {
        UE_LOG(LogPlanetGameplayComponent, Log,
            TEXT("[PlanetGameplay][C5] Tab navigation ignored: no selectable pieces. CurrentFaction=%d Turn=%d"),
            GameplayContainer->GetCurrentFactionId(),
            GameplayContainer->GetTurnIndex());
        return false;
    }

    const int32 CurrentSelectedPieceId = GameplayContainer->GetSelectedPieceId();
    int32 CurrentIndex = INDEX_NONE;
    if (CurrentSelectedPieceId != INDEX_NONE)
    {
        CurrentIndex = SelectablePieceIds.IndexOfByKey(CurrentSelectedPieceId);
    }

    int32 TargetIndex = INDEX_NONE;
    if (CurrentIndex == INDEX_NONE)
    {
        TargetIndex = bReverse ? SelectablePieceIds.Num() - 1 : 0;
    }
    else
    {
        const int32 Direction = bReverse ? -1 : 1;
        TargetIndex = (CurrentIndex + Direction + SelectablePieceIds.Num()) % SelectablePieceIds.Num();
    }

    if (!SelectablePieceIds.IsValidIndex(TargetIndex))
    {
        return false;
    }

    const int32 TargetPieceId = SelectablePieceIds[TargetIndex];
    int32 TargetCellId = INDEX_NONE;
    if (!GameplayContainer->TryGetPieceCellId(TargetPieceId, TargetCellId))
    {
        return false;
    }

    Host->HISMTileRenderer.SetLastClickedCellId(TargetCellId);
    const bool bHandled = HandleGameplayCellClick(
        TargetCellId,
        bReverse ? TEXT("C5 Shift+Tab") : TEXT("C5 Tab"),
        INDEX_NONE,
        TEXT("Keyboard"));

    UE_LOG(LogPlanetGameplayComponent, Log,
        TEXT("[PlanetGameplay][C5] Navigate Reverse=%d TargetPiece=%d TargetCell=%d Handled=%d Index=%d/%d"),
        bReverse ? 1 : 0,
        TargetPieceId,
        TargetCellId,
        bHandled ? 1 : 0,
        TargetIndex,
        SelectablePieceIds.Num());
    return bHandled;
}

bool UPlanetGameplayComponent::HandleHISMUndo()
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host || !GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        return false;
    }

    const ETerraGameplayInteractionPhase PrevPhase = GameplayContainer->GetInteractionPhase();
    const TArray<FTerraGameplayPieceState> PrevPieces = GameplayContainer->GetPieces();

    TArray<int32> DirtyCellIds;
    const bool bUndone = GameplayContainer->UndoCurrentInteraction(DirtyCellIds);
    if (!bUndone)
    {
        return false;
    }

    const TArray<FTerraGameplayPieceState>& NewPieces = GameplayContainer->GetPieces();
    TArray<FTerraPiecePresentationMoveEvent> UndoMoveEvents;
    for (const FTerraGameplayPieceState& NewPiece : NewPieces)
    {
        if (!NewPiece.bAlive || !PrevPieces.IsValidIndex(NewPiece.PieceId))
        {
            continue;
        }

        const FTerraGameplayPieceState& PrevPiece = PrevPieces[NewPiece.PieceId];
        if (!PrevPiece.bAlive
            || PrevPiece.CellId == NewPiece.CellId
            || PrevPiece.OwnerFactionId != NewPiece.OwnerFactionId)
        {
            continue;
        }

        ETerraPiecePresentationMoveType MoveType = ETerraPiecePresentationMoveType::Move;
        if (PrevPhase == ETerraGameplayInteractionPhase::PieceJumpingCanContinue)
        {
            MoveType = ETerraPiecePresentationMoveType::Jump;
        }

        FTransform FromWorldTransform = FTransform::Identity;
        FTransform ToWorldTransform = FTransform::Identity;
        if (Host->BuildP1PieceWorldTransform_(PrevPiece.CellId, NewPiece.PieceType, FromWorldTransform)
            && Host->BuildP1PieceWorldTransform_(NewPiece.CellId, NewPiece.PieceType, ToWorldTransform))
        {
            FTerraPiecePresentationMoveEvent& MoveEvent = UndoMoveEvents.AddDefaulted_GetRef();
            MoveEvent.PieceId = NewPiece.PieceId;
            MoveEvent.FromCellId = PrevPiece.CellId;
            MoveEvent.ToCellId = NewPiece.CellId;
            MoveEvent.MoveType = MoveType;
            MoveEvent.FromWorldTransform = FromWorldTransform;
            MoveEvent.ToWorldTransform = ToWorldTransform;
        }
    }

    RefreshGameplayHighlights(DirtyCellIds);
    RefreshCurrentFactionPieceHighlights();
    Host->RefreshG4CapturePreviewCellsForActionTarget_(Host->HISMTileRenderer.GetCurrentHoverCellId());
    RebuildG1DebugPieces();
    if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent())
    {
        Camera->RequestC6ActionCameraTrackingForMoveEvents(UndoMoveEvents);
    }
    Host->SyncP1PiecePresentation_(UndoMoveEvents);

    UE_LOG(LogPlanetGameplayComponent, Log,
        TEXT("[PlanetGameplay][G9] HISM Undo -> CurrentFaction=%d Turn=%d Phase=%d UndoMoveEvents=%d"),
        GameplayContainer->GetCurrentFactionId(),
        GameplayContainer->GetTurnIndex(),
        static_cast<int32>(GameplayContainer->GetInteractionPhase()),
        UndoMoveEvents.Num());
    return true;
}

bool UPlanetGameplayComponent::HandleGameplayCellClick(int32 CellId, const TCHAR* SourceLabel, int32 InstanceIndex, const FString& ComponentName)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host || !GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        UE_LOG(LogPlanetGameplayComponent, Warning,
            TEXT("[PlanetGameplay][G2] %s ignored: GameplayContainer is not ready. Cell=%d Instance=%d Component=%s"),
            SourceLabel ? SourceLabel : TEXT("CellClick"),
            CellId,
            InstanceIndex,
            *ComponentName);
        return true;
    }

    GameplayContainer->SetDebugKeepSameFactionOnEndTurn(bG3DebugKeepSameFactionOnEndTurn);

    const int32 PrevTurnIndex = GameplayContainer->GetTurnIndex();
    const int32 PrevFactionId = GameplayContainer->GetCurrentFactionId();
    const int32 PrevSelectedPieceId = GameplayContainer->GetSelectedPieceId();
    const ETerraGameplayInteractionPhase PrevPhase = GameplayContainer->GetInteractionPhase();
    const TArray<FTerraGameplayPieceState> PrevPieces = GameplayContainer->GetPieces();
    int32 PrevSelectedPieceCellId = INDEX_NONE;
    if (PrevSelectedPieceId != INDEX_NONE)
    {
        GameplayContainer->TryGetPieceCellId(PrevSelectedPieceId, PrevSelectedPieceCellId);
    }

    TArray<FTerraGameplayCaptureEntry> PendingCaptureEntriesBeforeClick;
    const bool bCouldConfirmTurnBeforeClick =
        PrevSelectedPieceId != INDEX_NONE
        && PrevSelectedPieceCellId == CellId
        && (PrevPhase == ETerraGameplayInteractionPhase::PieceMovedCanEndTurn
            || PrevPhase == ETerraGameplayInteractionPhase::PieceJumpingCanContinue);
    if (bCouldConfirmTurnBeforeClick)
    {
        GameplayContainer->CollectPendingCaptureEntries(PendingCaptureEntriesBeforeClick);
    }

    TArray<int32> DirtyCellIds;
    const bool bGameplayHandled = GameplayContainer->HandleCellClick(CellId, DirtyCellIds);
    RefreshGameplayHighlights(DirtyCellIds);
    Host->RefreshG4CapturePreviewCellsForActionTarget_(Host->HISMTileRenderer.GetCurrentHoverCellId());

    const int32 NewFactionId = GameplayContainer->GetCurrentFactionId();
    const int32 NewTurnIndex = GameplayContainer->GetTurnIndex();
    const int32 NewSelectedPieceId = GameplayContainer->GetSelectedPieceId();
    const ETerraGameplayInteractionPhase NewPhase = GameplayContainer->GetInteractionPhase();
    int32 NewSelectedPieceCellId = INDEX_NONE;
    if (NewSelectedPieceId != INDEX_NONE)
    {
        GameplayContainer->TryGetPieceCellId(NewSelectedPieceId, NewSelectedPieceCellId);
    }

    TArray<FTerraPiecePresentationCaptureEvent> P3CaptureEvents;
    if (bGameplayHandled
        && bCouldConfirmTurnBeforeClick
        && PendingCaptureEntriesBeforeClick.Num() > 0
        && NewPhase == ETerraGameplayInteractionPhase::Idle)
    {
        Host->BuildP3CaptureEventsFromPendingEntries_(PendingCaptureEntriesBeforeClick, PrevPieces, P3CaptureEvents);
    }

    TArray<FTerraPiecePresentationMoveEvent> P2MoveEvents;
    if (bGameplayHandled
        && PrevSelectedPieceId != INDEX_NONE
        && PrevSelectedPieceId == NewSelectedPieceId
        && PrevSelectedPieceCellId != INDEX_NONE
        && NewSelectedPieceCellId != INDEX_NONE
        && PrevSelectedPieceCellId != NewSelectedPieceCellId)
    {
        ETerraPiecePresentationMoveType MoveType = ETerraPiecePresentationMoveType::None;
        if (NewPhase == ETerraGameplayInteractionPhase::PieceJumpingCanContinue)
        {
            MoveType = ETerraPiecePresentationMoveType::Jump;
        }
        else if (NewPhase == ETerraGameplayInteractionPhase::PieceMovedCanEndTurn)
        {
            MoveType = ETerraPiecePresentationMoveType::Move;
        }

        if (MoveType != ETerraPiecePresentationMoveType::None)
        {
            const ETerraGameplayPieceType MovingPieceType = PrevPieces.IsValidIndex(PrevSelectedPieceId)
                ? PrevPieces[PrevSelectedPieceId].PieceType
                : ETerraGameplayPieceType::Infantry;
            FTransform FromWorldTransform = FTransform::Identity;
            FTransform ToWorldTransform = FTransform::Identity;
            if (Host->BuildP1PieceWorldTransform_(PrevSelectedPieceCellId, MovingPieceType, FromWorldTransform)
                && Host->BuildP1PieceWorldTransform_(NewSelectedPieceCellId, MovingPieceType, ToWorldTransform))
            {
                FTerraPiecePresentationMoveEvent& MoveEvent = P2MoveEvents.AddDefaulted_GetRef();
                MoveEvent.PieceId = NewSelectedPieceId;
                MoveEvent.FromCellId = PrevSelectedPieceCellId;
                MoveEvent.ToCellId = NewSelectedPieceCellId;
                MoveEvent.MoveType = MoveType;
                MoveEvent.FromWorldTransform = FromWorldTransform;
                MoveEvent.ToWorldTransform = ToWorldTransform;
            }
        }
    }

    const bool bTurnChanged = NewTurnIndex != PrevTurnIndex;
    const bool bFactionChanged = NewFactionId != PrevFactionId;

    if (bTurnChanged)
    {
        UE_LOG(LogPlanetGameplayComponent, Log,
            TEXT("[PlanetGameplay][C6.5] Turn changed after click. PrevTurn=%d NewTurn=%d PrevFaction=%d NewFaction=%d FactionChanged=%d PrevPhase=%d NewPhase=%d bGameplayHandled=%d PendingCapturesBeforeClick=%d P2MoveEvents=%d P3CaptureEvents=%d"),
            PrevTurnIndex,
            NewTurnIndex,
            PrevFactionId,
            NewFactionId,
            bFactionChanged ? 1 : 0,
            static_cast<int32>(PrevPhase),
            static_cast<int32>(NewPhase),
            bGameplayHandled ? 1 : 0,
            PendingCaptureEntriesBeforeClick.Num(),
            P2MoveEvents.Num(),
            P3CaptureEvents.Num());
        if (bFactionChanged)
        {
            RefreshFactionPieceHighlights(PrevFactionId);
        }
        RefreshFactionPieceHighlights(NewFactionId);
        G2_5LastHighlightedFactionId = NewFactionId;
        if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent())
        {
            Camera->SetLastFocusedTurnIndex(NewTurnIndex);
        }
    }
    else if (NewPhase != PrevPhase || bGameplayHandled)
    {
        RefreshFactionPieceHighlights(NewFactionId);
    }

    if (bGameplayHandled
        && NewSelectedPieceId != INDEX_NONE
        && NewSelectedPieceId != PrevSelectedPieceId
        && NewPhase == ETerraGameplayInteractionPhase::PieceSelected)
    {
        int32 SelectedPieceCellId = INDEX_NONE;
        if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent();
            Camera && GameplayContainer->TryGetPieceCellId(NewSelectedPieceId, SelectedPieceCellId))
        {
            Camera->FocusCameraOnSelectedCellSmart(SelectedPieceCellId);
        }
    }

    RebuildG1DebugPieces();
    if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent())
    {
        Camera->RequestC6ActionCameraTrackingForMoveEvents(P2MoveEvents);
    }
    Host->SyncP1PiecePresentation_(P2MoveEvents, P3CaptureEvents);

    if (bTurnChanged
        && (!Host->GetPlanetCameraComponent() || !Host->GetPlanetCameraComponent()->TryRequestC6_5DelayedTurnStartFocus(
            NewTurnIndex,
            NewFactionId,
            P2MoveEvents,
            P3CaptureEvents)))
    {
        UE_LOG(LogPlanetGameplayComponent, Log,
            TEXT("[PlanetGameplay][C6.5] Falling back to immediate turn focus. Faction=%d Turn=%d FactionChanged=%d P2MoveEvents=%d P3CaptureEvents=%d"),
            NewFactionId,
            NewTurnIndex,
            bFactionChanged ? 1 : 0,
            P2MoveEvents.Num(),
            P3CaptureEvents.Num());
        if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent())
        {
            Camera->FocusCameraOnCurrentFactionBase();
        }
    }

    UE_LOG(LogPlanetGameplayComponent, Log,
        TEXT("[PlanetGameplay][G2] %s -> Gameplay Cell=%d Instance=%d Component=%s Handled=%d CurrentFaction=%d Turn=%d Phase=%d P2MoveEvents=%d P3CaptureEvents=%d"),
        SourceLabel ? SourceLabel : TEXT("CellClick"),
        CellId,
        InstanceIndex,
        *ComponentName,
        bGameplayHandled ? 1 : 0,
        GameplayContainer->GetCurrentFactionId(),
        GameplayContainer->GetTurnIndex(),
        static_cast<int32>(GameplayContainer->GetInteractionPhase()),
        P2MoveEvents.Num(),
        P3CaptureEvents.Num());

    return true;
}

bool UPlanetGameplayComponent::TryExecuteNpcMcpValidatedAction(
    int32 ExpectedTurnIndex,
    int32 ExpectedFactionId,
    int32 PieceId,
    int32 ToCellId,
    FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult)
{
    APlanetTessellatedMesh* Host = GetHost();

    OutResult = FTerraGameplayContainer::FValidatedActionExecutionResult();
    OutResult.TurnIndexBefore = GameplayContainer.IsValid() ? GameplayContainer->GetTurnIndex() : INDEX_NONE;
    OutResult.TurnIndexAfter = OutResult.TurnIndexBefore;
    OutResult.FactionIdBefore = GameplayContainer.IsValid() ? GameplayContainer->GetCurrentFactionId() : INDEX_NONE;
    OutResult.FactionIdAfter = OutResult.FactionIdBefore;
    OutResult.PieceId = PieceId;
    OutResult.ToCellId = ToCellId;

    auto Reject = [this, &OutResult](const TCHAR* Reason) -> bool
    {
        OutResult.bAccepted = false;
        OutResult.bExecuted = false;
        OutResult.RejectReason = Reason;
        OutResult.TurnIndexAfter = GameplayContainer.IsValid() ? GameplayContainer->GetTurnIndex() : INDEX_NONE;
        OutResult.FactionIdAfter = GameplayContainer.IsValid() ? GameplayContainer->GetCurrentFactionId() : INDEX_NONE;
        return false;
    };

    if (!Host || !GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        return Reject(TEXT("gameplay_unavailable"));
    }
    if (GameplayContainer->IsMatchEnded())
    {
        return Reject(TEXT("match_ended"));
    }
    if (GameplayContainer->GetInteractionPhase() != ETerraGameplayInteractionPhase::Idle)
    {
        return Reject(TEXT("not_idle"));
    }
    if (GameplayContainer->GetTurnIndex() != ExpectedTurnIndex || GameplayContainer->GetCurrentFactionId() != ExpectedFactionId)
    {
        return Reject(TEXT("snapshot_mismatch"));
    }

    FTerraGameplayContainer::FLegalActionQuery LegalAction;
    if (!GameplayContainer->IsCurrentFactionLegalAction(PieceId, ToCellId, LegalAction))
    {
        return Reject(TEXT("not_current_faction_action"));
    }

    OutResult.bAccepted = true;
    OutResult.PieceId = LegalAction.PieceId;
    OutResult.FromCellId = LegalAction.FromCellId;
    OutResult.ToCellId = LegalAction.ToCellId;
    OutResult.bWasJump = LegalAction.bIsJump;
    OutResult.CaptureEntries = LegalAction.CaptureEntries;

    if (!HandleGameplayCellClick(LegalAction.FromCellId, TEXT("NPC MCP Select"), INDEX_NONE, TEXT("NpcMcp")))
    {
        return Reject(TEXT("select_failed"));
    }
    if (GameplayContainer->GetSelectedPieceId() != LegalAction.PieceId)
    {
        return Reject(TEXT("select_failed"));
    }

    if (!HandleGameplayCellClick(LegalAction.ToCellId, TEXT("NPC MCP Move"), INDEX_NONE, TEXT("NpcMcp")))
    {
        TArray<int32> UndoDirtyCellIds;
        GameplayContainer->UndoCurrentInteraction(UndoDirtyCellIds);
        RefreshGameplayHighlights(UndoDirtyCellIds);
        Host->SyncP1PiecePresentation_();
        return Reject(TEXT("move_failed"));
    }

    const ETerraGameplayInteractionPhase PostMovePhase = GameplayContainer->GetInteractionPhase();
    if (PostMovePhase != ETerraGameplayInteractionPhase::PieceMovedCanEndTurn
        && PostMovePhase != ETerraGameplayInteractionPhase::PieceJumpingCanContinue)
    {
        TArray<int32> UndoDirtyCellIds;
        GameplayContainer->UndoCurrentInteraction(UndoDirtyCellIds);
        RefreshGameplayHighlights(UndoDirtyCellIds);
        Host->SyncP1PiecePresentation_();
        return Reject(TEXT("move_failed"));
    }

    if (!HandleGameplayCellClick(LegalAction.ToCellId, TEXT("NPC MCP EndTurn"), INDEX_NONE, TEXT("NpcMcp")))
    {
        TArray<int32> UndoDirtyCellIds;
        GameplayContainer->UndoCurrentInteraction(UndoDirtyCellIds);
        RefreshGameplayHighlights(UndoDirtyCellIds);
        Host->SyncP1PiecePresentation_();
        return Reject(TEXT("end_turn_failed"));
    }

    if (GameplayContainer->GetInteractionPhase() != ETerraGameplayInteractionPhase::Idle)
    {
        return Reject(TEXT("end_turn_failed"));
    }

    OutResult.bExecuted = true;
    OutResult.RejectReason.Reset();
    OutResult.TurnIndexAfter = GameplayContainer->GetTurnIndex();
    OutResult.FactionIdAfter = GameplayContainer->GetCurrentFactionId();
    return true;
}
