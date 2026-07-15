#include "Render/PlanetGameplayComponent.h"

#include "Render/PlanetTessellatedMesh.h"
#include "Render/PlanetCameraComponent.h"
#include "Render/PlanetHISMInteractionComponent.h"
#include "Render/PlanetPiecePresentationComponent.h"
#include "Tutorial/TerraTutorialScenarioData.h"
#include "NpcMcp.h"
#include "TerraNpcMcpGameplayBridge.h"

#include "FCell.h"
#include "FSphereTopology.h"
#include "CellGeoData.h"
#include "WorldGenerator.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
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

void UPlanetGameplayComponent::BuildNpcMcpInteractionState_(FTerraNpcMcpGameplayBridge::FInteractionStateSnapshot& OutState) const
{
    OutState = FTerraNpcMcpGameplayBridge::FInteractionStateSnapshot();
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        return;
    }

    OutState.TurnIndex = GameplayContainer->GetTurnIndex();
    OutState.CurrentFactionId = GameplayContainer->GetCurrentFactionId();
    OutState.InteractionPhase = GameplayContainer->GetInteractionPhase();
    OutState.SelectedPieceId = GameplayContainer->GetSelectedPieceId();
    if (!bTurnActivationReady)
    {
        return;
    }
    if (OutState.SelectedPieceId != INDEX_NONE)
    {
        GameplayContainer->TryGetPieceCellId(OutState.SelectedPieceId, OutState.SelectedPieceCellId);
    }

    switch (OutState.InteractionPhase)
    {
    case ETerraGameplayInteractionPhase::Idle:
        OutState.bCanSelectPieceNow = true;
        break;
    case ETerraGameplayInteractionPhase::PieceSelected:
        OutState.bCanSelectPieceNow = true;
        OutState.bCanPreviewTargetNow = true;
        OutState.bCanCancelNow = true;
        break;
    case ETerraGameplayInteractionPhase::PieceMovedCanEndTurn:
        OutState.bCanConfirmNow = true;
        OutState.bCanCancelNow = true;
        break;
    case ETerraGameplayInteractionPhase::PieceJumpingCanContinue:
        OutState.bCanPreviewTargetNow = true;
        OutState.bCanConfirmNow = true;
        OutState.bCanCancelNow = true;
        break;
    default:
        break;
    }

    if (OutState.InteractionPhase == ETerraGameplayInteractionPhase::PieceJumpingCanContinue)
    {
        TArray<FTerraGameplayContainer::FLegalActionQuery> Actions;
        if (GameplayContainer->CollectSelectedPieceLegalActions(Actions))
        {
            for (const FTerraGameplayContainer::FLegalActionQuery& Action : Actions)
            {
                if (Action.PieceId == OutState.SelectedPieceId && Action.bIsJump)
                {
                    OutState.ContinueJumpTargetCellIds.AddUnique(Action.ToCellId);
                }
            }
        }
    }
}

void UPlanetGameplayComponent::FillNpcMcpReviewResult_(FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult, bool bOk, const FString& Error) const
{
    OutResult = FTerraNpcMcpGameplayBridge::FUiReviewResult();
    OutResult.bOk = bOk;
    OutResult.Error = Error;
    BuildNpcMcpInteractionState_(OutResult.InteractionState);
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
    if (UWorld* World = Host->GetWorld())
    {
        World->GetTimerManager().ClearTimer(TurnActivationTimerHandle);
    }
    bTurnActivationReady = true;
    PendingTurnActivationTurnIndex = INDEX_NONE;
    PendingTurnActivationFactionId = INDEX_NONE;

    if (!Host->CellTopology.IsValid() || !Host->Generator.IsValid())
    {
        if (GameplayContainer.IsValid())
        {
            FTerraNpcMcpGameplayBridge::UnregisterExecuteValidatedActionDelegate();
            FTerraNpcMcpGameplayBridge::UnregisterUiReviewDelegates();
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
            FTerraNpcMcpGameplayBridge::UnregisterUiReviewDelegates();
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
        FTerraNpcMcpGameplayBridge::UnregisterUiReviewDelegates();
        FTerraNpcMcpGameplayBridge::UnregisterGameplayContainer(GameplayContainer.Get());
    }

    GameplayContainer = MakeUnique<FTerraGameplayContainer>();
    FTerraGameplayNeutralSpawnConfig NeutralSpawnConfig;
    NeutralSpawnConfig.CommanderCount = FMath::Max(G10NeutralCommanderCount, 0);
    NeutralSpawnConfig.InfantryCount = FMath::Max(G10NeutralInfantryCount, 0);
    NeutralSpawnConfig.CavalryCount = FMath::Max(G10NeutralCavalryCount, 0);
    NeutralSpawnConfig.ArcherCount = FMath::Max(G10NeutralArcherCount, 0);
    NeutralSpawnConfig.SpawnSeed = G10NeutralSpawnSeed;
    GameplayContainer->Initialize(GameplayCells, NeutralSpawnConfig, T0TechnologyProgressionConfig);
    GameplayContainer->SetDebugKeepSameFactionOnEndTurn(bG3DebugKeepSameFactionOnEndTurn);
    FTerraNpcMcpGameplayBridge::RegisterGameplayContainer(GameplayContainer.Get());
    FTerraNpcMcpGameplayBridge::RegisterExecuteValidatedActionDelegate(
        [this](int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult)
        {
            return TryExecuteNpcMcpValidatedAction(ExpectedTurnIndex, ExpectedFactionId, PieceId, ToCellId, OutResult);
        });
    FTerraNpcMcpGameplayBridge::RegisterUiBeginTurnReviewDelegate(
        [this](FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult)
        {
            return TryNpcMcpUiBeginTurnReview(OutResult);
        });
    FTerraNpcMcpGameplayBridge::RegisterUiSelectPieceDelegate(
        [this](int32 PieceId, FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult)
        {
            return TryNpcMcpUiSelectPiece(PieceId, OutResult);
        });
    FTerraNpcMcpGameplayBridge::RegisterUiPreviewMoveDelegate(
        [this](int32 PieceId, int32 ToCellId, FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult)
        {
            return TryNpcMcpUiPreviewMove(PieceId, ToCellId, OutResult);
        });
    FTerraNpcMcpGameplayBridge::RegisterUiCancelSelectionDelegate(
        [this](FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult)
        {
            return TryNpcMcpUiCancelSelection(OutResult);
        });
    FTerraNpcMcpGameplayBridge::RegisterUiConfirmActionDelegate(
        [this](FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult, FTerraGameplayContainer::FValidatedActionExecutionResult& OutExecutionResult)
        {
            return TryNpcMcpUiConfirmAction(OutResult, OutExecutionResult);
        });
    FNpcMcpModule::RefreshInteractiveToolAvailability();

    G2_5LastHighlightedFactionId = GameplayContainer->GetCurrentFactionId();
    if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent())
    {
        Camera->ResetCameraState();
    }
    RefreshCurrentFactionPieceHighlights();
    Host->SyncP1PiecePresentation_();
}

bool UPlanetGameplayComponent::InitializeTutorialScenario(const UTerraTutorialScenarioData& Scenario, FString& OutError)
{
    OutError.Reset();
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host || !Host->CellTopology.IsValid() || !Host->Generator.IsValid())
    {
        OutError = TEXT("tutorial_host_not_ready");
        return false;
    }

    const int32 NumCells = Host->CellTopology->Cells.Num();
    TArray<ETerraSimpleTerrainType> TerrainField;
    TerrainField.Init(ETerraSimpleTerrainType::Plain, NumCells);
    for (const FTerraTutorialTerrainPlacement& Override : Scenario.TerrainOverrides)
    {
        if (!TerrainField.IsValidIndex(Override.CellId))
        {
            OutError = FString::Printf(TEXT("invalid_terrain_cell:%d"), Override.CellId);
            return false;
        }
        TerrainField[Override.CellId] = Override.Terrain == ETerraGameplayTerrainType::Forest
            ? ETerraSimpleTerrainType::Forest
            : Override.Terrain == ETerraGameplayTerrainType::Mountain ? ETerraSimpleTerrainType::Mountain : ETerraSimpleTerrainType::Plain;
    }

    TArray<FTerraGameplayCellState> GameplayCells;
    GameplayCells.SetNum(NumCells);
    for (int32 CellId = 0; CellId < NumCells; ++CellId)
    {
        const FCell& SourceCell = Host->CellTopology->Cells[CellId];
        FTerraGameplayCellState& Cell = GameplayCells[CellId];
        Cell.CellId = CellId;
        Cell.bIsPentagon = SourceCell.bIsPentagon;
        Cell.NeighborCellIds = SourceCell.NeighborCellIds;
        Cell.TerrainType = TerrainField[CellId] == ETerraSimpleTerrainType::Forest
            ? ETerraGameplayTerrainType::Forest
            : TerrainField[CellId] == ETerraSimpleTerrainType::Mountain ? ETerraGameplayTerrainType::Mountain : ETerraGameplayTerrainType::Plain;
    }

    TArray<FTerraGameplayPieceState> Pieces;
    Pieces.SetNum(Scenario.InitialPieces.Num());
    for (const FTerraTutorialPiecePlacement& Placement : Scenario.InitialPieces)
    {
        if (!Pieces.IsValidIndex(Placement.PieceId) || Placement.PieceId == INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("piece_ids_must_be_contiguous_from_zero:%d"), Placement.PieceId);
            return false;
        }
        FTerraGameplayPieceState& Piece = Pieces[Placement.PieceId];
        if (Piece.PieceId != INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("duplicate_piece_id:%d"), Placement.PieceId);
            return false;
        }
        if (!GameplayCells.IsValidIndex(Placement.CellId)
            || (Placement.PieceType == ETerraGameplayPieceType::Cavalry && GameplayCells[Placement.CellId].TerrainType == ETerraGameplayTerrainType::Mountain))
        {
            OutError = FString::Printf(TEXT("invalid_piece_cell_or_cavalry_mountain:%d"), Placement.CellId);
            return false;
        }
        Piece.PieceId = Placement.PieceId;
        Piece.OwnerFactionId = Placement.OwnerFactionId;
        Piece.bIsNeutral = Placement.bIsNeutral;
        Piece.CellId = Placement.CellId;
        Piece.PieceType = Placement.PieceType;
        Piece.bAlive = true;
        Piece.bCanMove = Placement.bCanMove && !Placement.bIsNeutral;
    }

    TArray<FTerraGameplayEquipmentDropState> EquipmentDrops;
    for (const FTerraTutorialEquipmentDropPlacement& Placement : Scenario.InitialEquipmentDrops)
    {
        if (!GameplayCells.IsValidIndex(Placement.CellId))
        {
            OutError = FString::Printf(TEXT("invalid_equipment_cell:%d"), Placement.CellId);
            return false;
        }
        FTerraGameplayEquipmentDropState& Drop = EquipmentDrops.AddDefaulted_GetRef();
        Drop.CellId = Placement.CellId;
        Drop.bHasBow = Placement.bHasBow;
        Drop.bHasHorse = Placement.bHasHorse;
    }

    RebuildGameplay();
    if (!GameplayContainer->InitializeFixedScenario(
        GameplayCells,
        Pieces,
        EquipmentDrops,
        Scenario.InitialTurnFactionId,
        Scenario.InitialTurnIndex,
        OutError,
        T0TechnologyProgressionConfig))
    {
        return false;
    }

    Host->Generator->OverrideSimpleTerrainField(TerrainField);
    Host->RebuildHISMTileInstances_();
    G2_5LastHighlightedFactionId = GameplayContainer->GetCurrentFactionId();
    RefreshCurrentFactionPieceHighlights();
    Host->SyncP1PiecePresentation_();
    if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent())
    {
        Camera->C3InitialDistanceToFocusCM = Scenario.InitialCamera.DistanceToFocusCM;
        Camera->ResetCameraState();
        if (Scenario.InitialCamera.FocusCellId != INDEX_NONE)
        {
            Camera->FocusCameraOnCell(Scenario.InitialCamera.FocusCellId, true);
        }
    }
    TutorialNpcActionSequence = Scenario.NpcActionSequence;
    TutorialNpcActionSequence.Sort([](const FTerraTutorialNpcAction& A, const FTerraTutorialNpcAction& B) { return A.TurnIndex < B.TurnIndex; });
    bTutorialNpcScriptFailed = false;
    return true;
}

void UPlanetGameplayComponent::TickTutorialNpcScript()
{
    if (bTutorialNpcScriptFailed || TutorialNpcActionSequence.IsEmpty() || !GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        return;
    }

    const int32 CurrentTurnIndex = GameplayContainer->GetTurnIndex();
    const int32 ActionIndex = TutorialNpcActionSequence.IndexOfByPredicate([CurrentTurnIndex](const FTerraTutorialNpcAction& Action)
    {
        return Action.TurnIndex == CurrentTurnIndex;
    });
    if (ActionIndex == INDEX_NONE)
    {
        return;
    }

    const FTerraTutorialNpcAction Action = TutorialNpcActionSequence[ActionIndex];
    FTerraGameplayContainer::FValidatedActionExecutionResult Result;
    const bool bExecuted = TryExecuteNpcMcpValidatedAction(CurrentTurnIndex, GameplayContainer->GetCurrentFactionId(), Action.PieceId, Action.TargetCellId, Result);
    TutorialNpcActionSequence.RemoveAt(ActionIndex);
    if (!bExecuted)
    {
        bTutorialNpcScriptFailed = true;
        UE_LOG(LogPlanetGameplayComponent, Error, TEXT("[Gameplay][Tutorial] NPC script action rejected. Turn=%d Piece=%d Target=%d Reason=%s"),
            Action.TurnIndex, Action.PieceId, Action.TargetCellId, *Result.RejectReason);
    }
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
        if (Host->GetPlanetHISMInteractionComponent())
        {
            Host->GetPlanetHISMInteractionComponent()->WriteHISMHighlightForCell(CellId);
        }
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
        if (Host->GetPlanetHISMInteractionComponent())
        {
            Host->GetPlanetHISMInteractionComponent()->WriteHISMHighlightForCell(CellId);
        }
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
        if (Host->GetPlanetHISMInteractionComponent())
        {
            Host->GetPlanetHISMInteractionComponent()->WriteHISMHighlightForCell(CellId);
        }
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
            || PrevPiece.OwnerFactionId != NewPiece.OwnerFactionId
            // A G12 pickup changes the actor's visual composition. Do not animate that
            // actor back as a different piece type; the restored snapshot must replace it.
            || PrevPiece.PieceType != NewPiece.PieceType)
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
    if (Host->GetPlanetHISMInteractionComponent())
    {
        Host->GetPlanetHISMInteractionComponent()->RefreshCapturePreviewCellsForActionTarget(
            Host->GetPlanetHISMInteractionComponent()->GetLastHISMPickedCellId());
    }
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

void UPlanetGameplayComponent::BeginTurnActivationGate_(int32 ExpectedTurnIndex, int32 ExpectedFactionId, float DelaySeconds)
{
    APlanetTessellatedMesh* Host = GetHost();
    UWorld* World = Host ? Host->GetWorld() : nullptr;
    if (!World || !World->IsGameWorld())
    {
        OpenTurnActivationGate_(ExpectedTurnIndex, ExpectedFactionId);
        return;
    }

    bTurnActivationReady = false;
    PendingTurnActivationTurnIndex = ExpectedTurnIndex;
    PendingTurnActivationFactionId = ExpectedFactionId;
    World->GetTimerManager().ClearTimer(TurnActivationTimerHandle);
    World->GetTimerManager().SetTimer(
        TurnActivationTimerHandle,
        FTimerDelegate::CreateUObject(this, &UPlanetGameplayComponent::OpenTurnActivationGate_, ExpectedTurnIndex, ExpectedFactionId),
        FMath::Max(DelaySeconds, 0.001f),
        false);

    UE_LOG(LogPlanetGameplayComponent, Log,
        TEXT("[Gameplay][TurnActivation] Closed. Faction=%d Turn=%d Delay=%.3f"),
        ExpectedFactionId,
        ExpectedTurnIndex,
        DelaySeconds);
}

void UPlanetGameplayComponent::OpenTurnActivationGate_(int32 ExpectedTurnIndex, int32 ExpectedFactionId)
{
    if (!GameplayContainer.IsValid()
        || !GameplayContainer->IsInitialized()
        || GameplayContainer->GetTurnIndex() != ExpectedTurnIndex
        || GameplayContainer->GetCurrentFactionId() != ExpectedFactionId)
    {
        UE_LOG(LogPlanetGameplayComponent, Verbose,
            TEXT("[Gameplay][TurnActivation] Skip stale activation. ExpectedFaction=%d ExpectedTurn=%d"),
            ExpectedFactionId,
            ExpectedTurnIndex);
        return;
    }

    bTurnActivationReady = true;
    PendingTurnActivationTurnIndex = INDEX_NONE;
    PendingTurnActivationFactionId = INDEX_NONE;
    if (APlanetTessellatedMesh* Host = GetHost())
    {
        if (UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent())
        {
            Camera->ExecuteC6_5DelayedTurnStartFocus(ExpectedTurnIndex, ExpectedFactionId);
        }
    }

    UE_LOG(LogPlanetGameplayComponent, Log,
        TEXT("[Gameplay][TurnActivation] Opened. Faction=%d Turn=%d"),
        ExpectedFactionId,
        ExpectedTurnIndex);
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

    if (!bTurnActivationReady)
    {
        UE_LOG(LogPlanetGameplayComponent, Verbose,
            TEXT("[Gameplay][TurnActivation] %s ignored: previous action presentation is still active. Cell=%d"),
            SourceLabel ? SourceLabel : TEXT("CellClick"),
            CellId);
        return false;
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
    if (Host->GetPlanetHISMInteractionComponent())
    {
        Host->GetPlanetHISMInteractionComponent()->RefreshCapturePreviewCellsForActionTarget(
            Host->GetPlanetHISMInteractionComponent()->GetLastHISMPickedCellId());
    }

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

    if (bTurnChanged)
    {
        UPlanetCameraComponent* Camera = Host->GetPlanetCameraComponent();
        const float DelaySeconds = Camera && Camera->bEnableC6_5DelayTurnStartFocusUntilActionPresentationEnds
            ? Camera->GetC6_5ActionPresentationDelaySeconds(P2MoveEvents, P3CaptureEvents)
                + FMath::Max(Camera->C6_5TurnStartFocusDelayPaddingSeconds, 0.0f)
            : 0.0f;
        if (DelaySeconds > KINDA_SMALL_NUMBER)
        {
            BeginTurnActivationGate_(NewTurnIndex, NewFactionId, DelaySeconds);
        }
        else
        {
            OpenTurnActivationGate_(NewTurnIndex, NewFactionId);
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
    return TryExecuteNpcValidatedAction(ExpectedTurnIndex, ExpectedFactionId, PieceId, ToCellId, OutResult);
}

bool UPlanetGameplayComponent::TryExecuteNpcValidatedAction(
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
    if (!bTurnActivationReady)
    {
        return Reject(TEXT("turn_activation_pending"));
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

bool UPlanetGameplayComponent::TryNpcMcpUiBeginTurnReview(FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult)
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("gameplay_unavailable"));
        return false;
    }
    if (GameplayContainer->IsMatchEnded())
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("match_ended"));
        return false;
    }
    if (GameplayContainer->GetInteractionPhase() != ETerraGameplayInteractionPhase::Idle)
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("not_idle"));
        return false;
    }

    FillNpcMcpReviewResult_(OutResult, true, FString());
    return true;
}

bool UPlanetGameplayComponent::TryNpcMcpUiSelectPiece(int32 PieceId, FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host || !GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("gameplay_unavailable"));
        return false;
    }

    const ETerraGameplayInteractionPhase Phase = GameplayContainer->GetInteractionPhase();
    if (Phase != ETerraGameplayInteractionPhase::Idle && Phase != ETerraGameplayInteractionPhase::PieceSelected)
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("invalid_phase_for_select"));
        return false;
    }
    if (Phase == ETerraGameplayInteractionPhase::PieceSelected && GameplayContainer->GetSelectedPieceId() == PieceId)
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("piece_already_selected"));
        return false;
    }

    TArray<int32> SelectablePieceIds;
    GameplayContainer->CollectCurrentFactionSelectablePieceIds(SelectablePieceIds);
    if (!SelectablePieceIds.Contains(PieceId))
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("piece_not_current_faction_or_not_selectable"));
        return false;
    }

    int32 CellId = INDEX_NONE;
    if (!GameplayContainer->TryGetPieceCellId(PieceId, CellId))
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("piece_cell_unavailable"));
        return false;
    }

    Host->HISMTileRenderer.SetLastClickedCellId(CellId);
    if (!HandleGameplayCellClick(CellId, TEXT("NPC MCP UI Select"), INDEX_NONE, TEXT("NpcMcp")))
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("select_click_failed"));
        return false;
    }
    if (GameplayContainer->GetSelectedPieceId() != PieceId || GameplayContainer->GetInteractionPhase() != ETerraGameplayInteractionPhase::PieceSelected)
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("select_state_mismatch"));
        return false;
    }

    FillNpcMcpReviewResult_(OutResult, true, FString());
    return true;
}

bool UPlanetGameplayComponent::TryNpcMcpUiPreviewMove(int32 PieceId, int32 ToCellId, FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult)
{
    APlanetTessellatedMesh* Host = GetHost();
    if (!Host || !GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("gameplay_unavailable"));
        return false;
    }

    const ETerraGameplayInteractionPhase Phase = GameplayContainer->GetInteractionPhase();
    if (Phase != ETerraGameplayInteractionPhase::PieceSelected
        && Phase != ETerraGameplayInteractionPhase::PieceJumpingCanContinue)
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("invalid_phase_for_preview"));
        return false;
    }
    if (GameplayContainer->GetSelectedPieceId() != PieceId)
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("piece_mismatch"));
        return false;
    }

    FTerraGameplayContainer::FLegalActionQuery LegalAction;
    if (!GameplayContainer->GetSelectedPieceLegalAction(ToCellId, LegalAction))
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("not_current_selected_piece_action"));
        return false;
    }

    Host->HISMTileRenderer.SetLastClickedCellId(ToCellId);
    if (!HandleGameplayCellClick(ToCellId, TEXT("NPC MCP UI Preview"), INDEX_NONE, TEXT("NpcMcp")))
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("preview_click_failed"));
        return false;
    }

    const ETerraGameplayInteractionPhase NewPhase = GameplayContainer->GetInteractionPhase();
    if (NewPhase != ETerraGameplayInteractionPhase::PieceMovedCanEndTurn
        && NewPhase != ETerraGameplayInteractionPhase::PieceJumpingCanContinue)
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("preview_state_mismatch"));
        return false;
    }

    FillNpcMcpReviewResult_(OutResult, true, FString());
    return true;
}

bool UPlanetGameplayComponent::TryNpcMcpUiCancelSelection(FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult)
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("gameplay_unavailable"));
        return false;
    }

    if (GameplayContainer->GetInteractionPhase() == ETerraGameplayInteractionPhase::Idle)
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("nothing_to_cancel"));
        return false;
    }

    if (!HandleHISMUndo())
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("cancel_failed"));
        return false;
    }

    FillNpcMcpReviewResult_(OutResult, true, FString());
    return true;
}

bool UPlanetGameplayComponent::TryNpcMcpUiConfirmAction(FTerraNpcMcpGameplayBridge::FUiReviewResult& OutResult, FTerraGameplayContainer::FValidatedActionExecutionResult& OutExecutionResult)
{
    APlanetTessellatedMesh* Host = GetHost();
    OutExecutionResult = FTerraGameplayContainer::FValidatedActionExecutionResult();
    if (!Host || !GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("gameplay_unavailable"));
        return false;
    }

    const ETerraGameplayInteractionPhase Phase = GameplayContainer->GetInteractionPhase();
    if (Phase != ETerraGameplayInteractionPhase::PieceMovedCanEndTurn
        && Phase != ETerraGameplayInteractionPhase::PieceJumpingCanContinue)
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("invalid_phase_for_confirm"));
        return false;
    }

    const int32 PieceId = GameplayContainer->GetSelectedPieceId();
    int32 CellId = INDEX_NONE;
    if (PieceId == INDEX_NONE || !GameplayContainer->TryGetPieceCellId(PieceId, CellId))
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("selected_piece_unavailable"));
        return false;
    }

    OutExecutionResult.TurnIndexBefore = GameplayContainer->GetTurnIndex();
    OutExecutionResult.FactionIdBefore = GameplayContainer->GetCurrentFactionId();
    OutExecutionResult.PieceId = PieceId;
    OutExecutionResult.FromCellId = CellId;
    OutExecutionResult.ToCellId = CellId;

    Host->HISMTileRenderer.SetLastClickedCellId(CellId);
    if (!HandleGameplayCellClick(CellId, TEXT("NPC MCP UI Confirm"), INDEX_NONE, TEXT("NpcMcp")))
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("confirm_click_failed"));
        OutExecutionResult.RejectReason = TEXT("confirm_click_failed");
        return false;
    }

    if (GameplayContainer->GetInteractionPhase() != ETerraGameplayInteractionPhase::Idle)
    {
        FillNpcMcpReviewResult_(OutResult, false, TEXT("execute_failed"));
        OutExecutionResult.RejectReason = TEXT("execute_failed");
        return false;
    }

    OutExecutionResult.bAccepted = true;
    OutExecutionResult.bExecuted = true;
    OutExecutionResult.TurnIndexAfter = GameplayContainer->GetTurnIndex();
    OutExecutionResult.FactionIdAfter = GameplayContainer->GetCurrentFactionId();

    FillNpcMcpReviewResult_(OutResult, true, FString());
    return true;
}
