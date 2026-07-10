// Fill out your copyright notice in the Description page of Project Settings.

#include "Render/PlanetTessellatedMesh.h"
#include "Render/PlanetCameraComponent.h"
#include "Render/PlanetPiecePresentationComponent.h"
#include "TerraGameplayContainer.h"
#include "TerraNpcMcpGameplayBridge.h"

#include "FSphereTopology.h"
#include "FCell.h"

// T4：WorldGen 接入——仅在 cpp 侧 include（头文件仅使用前向声明）
#include "WorldGenerator.h"
#include "CellGeoData.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"

#include "Components/SceneComponent.h"
#include "Logging/LogMacros.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Interaction/PlanetInteractionController.h"
#include "Kismet/GameplayStatics.h"
DEFINE_LOG_CATEGORY_STATIC(LogPlanetTess, Log, All);

// ===================================================================
//  Construction / Destruction
// ===================================================================

APlanetTessellatedMesh::APlanetTessellatedMesh()
{
    // 与 APlanetTopologyDebugMesh 一致：在编辑器中拖动属性即时刷新；Tick 仅在 HISM hover 防抖启用时打开。
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
#if WITH_EDITORONLY_DATA
    bRunConstructionScriptOnDrag = true;
#endif

    USceneComponent* RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    SetRootComponent(RootScene);

    PlainTileHISMComp = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PlainTileHISMComp"));
    PlainTileHISMComp->SetupAttachment(RootScene);
    PlainTileHISMComp->SetCanEverAffectNavigation(false);
    PlainTileHISMComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PlainTileHISMComp->SetCollisionObjectType(ECC_WorldStatic);
    PlainTileHISMComp->SetCollisionResponseToAllChannels(ECR_Block);

    ForestTileHISMComp = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ForestTileHISMComp"));
    ForestTileHISMComp->SetupAttachment(RootScene);
    ForestTileHISMComp->SetCanEverAffectNavigation(false);
    ForestTileHISMComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ForestTileHISMComp->SetCollisionObjectType(ECC_WorldStatic);
    ForestTileHISMComp->SetCollisionResponseToAllChannels(ECR_Block);

    MountainTileHISMComp = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MountainTileHISMComp"));
    MountainTileHISMComp->SetupAttachment(RootScene);
    MountainTileHISMComp->SetCanEverAffectNavigation(false);
    MountainTileHISMComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MountainTileHISMComp->SetCollisionObjectType(ECC_WorldStatic);
    MountainTileHISMComp->SetCollisionResponseToAllChannels(ECR_Block);

    PlanetCameraComponent = CreateDefaultSubobject<UPlanetCameraComponent>(TEXT("PlanetCameraComponent"));
    PlanetPiecePresentationComponent = CreateDefaultSubobject<UPlanetPiecePresentationComponent>(TEXT("PlanetPiecePresentationComponent"));

    HISMTileRenderer.Initialize(PlainTileHISMComp, ForestTileHISMComp, MountainTileHISMComp);
    HISMTileRenderer.PrepareHighlightComponents(BuildHISMHighlightConfig_());

#if WITH_EDITOR
    // ===== D15：PIE 退出后材质恢复钩子（AgentWorkflow §3.11）=====
    //
    // 订阅 FWorldDelegates::OnPostWorldCleanup。该委托在任何 UWorld 被 Cleanup 后触发，
    // PIE 退出也走这条路径。重复订阅 / dangling 风险由 ~APlanetTessellatedMesh() 中
    // Remove 防御。仅 Editor 构建路径下有效——Standalone 只有一个 Game World，不会
    // 发生跨 World duplicate。与 APlanetTopologyDebugMesh 完全对齐。
    PostWorldCleanupHandle = FWorldDelegates::OnPostWorldCleanup.AddUObject(
        this, &APlanetTessellatedMesh::OnPostWorldCleanup_);
#endif
}

APlanetTessellatedMesh::~APlanetTessellatedMesh()
{
    if (GameplayContainer.IsValid())
    {
        FTerraNpcMcpGameplayBridge::UnregisterExecuteValidatedActionDelegate();
        FTerraNpcMcpGameplayBridge::UnregisterGameplayContainer(GameplayContainer.Get());
    }

    // PIE 钩子在析构中取消订阅。AddUObject 路径会在 UObject 销毁时自动撤销，
    // 但显式 Remove 避免多次触发 / dangling 风险。详见 AgentWorkflow §3.11。
#if WITH_EDITOR
    if (PostWorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnPostWorldCleanup.Remove(PostWorldCleanupHandle);
        PostWorldCleanupHandle.Reset();
    }
#endif
}

// FVTableHelper 构造函数不能 = default（UE 编码规范，与基类 AActor 实现保持一致）。
APlanetTessellatedMesh::APlanetTessellatedMesh(FVTableHelper& Helper)
    : Super(Helper)
{
}

void APlanetTessellatedMesh::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildAll_();
}

void APlanetTessellatedMesh::BeginPlay()
{
    Super::BeginPlay();

    const UWorld* World = GetWorld();
    if (World && World->IsGameWorld()
        && (!CellTopology.IsValid()
            || !Generator.IsValid()
            || !GameplayContainer.IsValid()
            || !GameplayContainer->IsInitialized()))
    {
        RebuildAll_();
    }
}

void APlanetTessellatedMesh::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    HISMTileRenderer.TickHoverFade(
        DeltaSeconds,
        HISMHoverFadeDuration,
        BuildHISMHighlightConfig_(),
        GameplayContainer.Get());

    if (PlanetCameraComponent)
    {
        PlanetCameraComponent->TickCamera(DeltaSeconds);
    }

    DrawG1DebugPieces_();
}

void APlanetTessellatedMesh::Rebuild()
{
    RebuildAll_();
}

#if WITH_EDITOR
bool APlanetTessellatedMesh::ShouldTickIfViewportsOnly() const
{
    return bEnableG1DebugPieces;
}
#endif

void APlanetTessellatedMesh::RebuildAll_()
{
    RebuildTopologies_();

    Generator.Reset();
    if (CellTopology.IsValid())
    {
        Generator = MakeUnique<FWorldGenerator>(CellTopology.Get(), WorldGenSettings);
        Generator->Generate();
    }

    RebuildHISMTileInstances_();
    ApplyRenderModeVisibility_();
    RebuildGameplay_();
    RebuildG1DebugPieces_();
    LogTopologyStats_();
}

void APlanetTessellatedMesh::RebuildTopologies_()
{
    const int32 CellSub = FMath::Clamp(CellSubdivisionLevel, 1, 5);
    CellTopology = MakeUnique<FSphereTopology>(CellSub);
    CellTopology->Build();
}

void APlanetTessellatedMesh::LogTopologyStats_() const
{
    const int32 CellCount = CellTopology ? CellTopology->Cells.Num() : 0;

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess] CellTopo: %d cells (CellSub=%d)"),
        CellCount,
        CellTopology ? CellTopology->SubdivisionLevel : -1);
}

FPlanetHISMTileRenderConfig APlanetTessellatedMesh::BuildHISMTileRenderConfig_() const
{
    FPlanetHISMTileRenderConfig Config;
    Config.bEnableRendering = bEnableHISMTileRendering;
    Config.bEnableCollision = bEnableHISMTileCollision;
    Config.bEnableInstanceHighlight = bEnableHISMInstanceHighlight;
    Config.PlainTileStaticMesh = PlainTileStaticMesh;
    Config.ForestTileStaticMesh = ForestTileStaticMesh;
    Config.MountainTileStaticMesh = MountainTileStaticMesh;
    Config.GlobeRadiusCM = GlobeRadiusCM;
    Config.SourceRadiusCM = HISMTileSourceRadiusCM;
    Config.RadiusOffsetCM = HISMTileRadiusOffsetCM;
    Config.AdditionalUniformScale = HISMTileAdditionalUniformScale;
    return Config;
}

FPlanetHISMHighlightConfig APlanetTessellatedMesh::BuildHISMHighlightConfig_() const
{
    FPlanetHISMHighlightConfig Config;
    Config.bEnableInstanceHighlight = bEnableHISMInstanceHighlight;
    Config.HighlightStrength = HighlightStrength;
    Config.HighlightInnerRadius = HISMHighlightInnerRadius;
    Config.HighlightOuterRadius = HISMHighlightOuterRadius;
    Config.HoverColor = HighlightHoverColor;
    Config.CurrentFactionPieceColor = G2_5CurrentFactionPieceColor;
    Config.CurrentFactionPieceHoverColor = G2_5CurrentFactionPieceHoverColor;
    Config.ActionTargetHoverColor = G3ActionTargetHoverColor;
    Config.CaptureTargetHoverColor = G4CaptureTargetHoverColor;
    return Config;
}

void APlanetTessellatedMesh::RebuildHISMTileInstances_()
{
    if (!CellTopology.IsValid() || !Generator.IsValid())
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess] Skip HISM spherical tiles: CellTopology or WorldGen is not ready."));
        return;
    }

    HISMTileRenderer.PrepareHighlightComponents(BuildHISMHighlightConfig_());
    HISMTileRenderer.RebuildInstances(
        *CellTopology,
        Generator->GetCellData(),
        BuildHISMTileRenderConfig_());
}

void APlanetTessellatedMesh::WriteHISMHighlightForCell_(int32 CellId, bool bMarkRenderStateDirty)
{
    HISMTileRenderer.WriteHighlightForCell(
        CellId,
        BuildHISMHighlightConfig_(),
        GameplayContainer.Get(),
        bMarkRenderStateDirty);
}

void APlanetTessellatedMesh::RefreshG4CapturePreviewCellsForActionTarget_(int32 ActionTargetCellId, bool bMarkLastRenderStateDirty)
{
    if (!GameplayContainer.IsValid() || ActionTargetCellId == INDEX_NONE)
    {
        return;
    }

    TArray<int32> CaptureCellIds;
    if (!GameplayContainer->CollectCapturePreviewCellIdsForActionTarget(ActionTargetCellId, CaptureCellIds))
    {
        return;
    }

    for (int32 I = 0; I < CaptureCellIds.Num(); ++I)
    {
        WriteHISMHighlightForCell_(CaptureCellIds[I], bMarkLastRenderStateDirty && I == CaptureCellIds.Num() - 1);
    }
}

void APlanetTessellatedMesh::UpdateHISMHoverCell_(int32 NewCellId)
{
    HISMTileRenderer.UpdateHoverCell(
        NewCellId,
        HISMHoverFadeDuration,
        BuildHISMHighlightConfig_(),
        GameplayContainer.Get());
}

void APlanetTessellatedMesh::RebuildGameplay_()
{
    if (UWorld* World = GetWorld())
    {
        if (PlanetCameraComponent)
        {
            PlanetCameraComponent->ClearDelayedTurnStartFocusTimer();
        }
    }

    if (!CellTopology.IsValid() || !Generator.IsValid())
    {
        if (GameplayContainer.IsValid())
        {
            FTerraNpcMcpGameplayBridge::UnregisterExecuteValidatedActionDelegate();
            FTerraNpcMcpGameplayBridge::UnregisterGameplayContainer(GameplayContainer.Get());
        }
        GameplayContainer.Reset();
        ClearP1PiecePresentation_();
        G2_5LastHighlightedFactionId = INDEX_NONE;
        if (PlanetCameraComponent)
        {
            PlanetCameraComponent->ResetCameraState();
        }
        return;
    }

    const int32 NumCells = CellTopology->Cells.Num();
    const TArray<FCellGeoData>& GeoCells = Generator->GetCellData();
    if (GeoCells.Num() != NumCells)
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess][G2] Skip Gameplay rebuild: WorldGen cell count mismatch. Got=%d Expected=%d"),
            GeoCells.Num(),
            NumCells);
        if (GameplayContainer.IsValid())
        {
            FTerraNpcMcpGameplayBridge::UnregisterExecuteValidatedActionDelegate();
            FTerraNpcMcpGameplayBridge::UnregisterGameplayContainer(GameplayContainer.Get());
        }
        GameplayContainer.Reset();
        ClearP1PiecePresentation_();
        G2_5LastHighlightedFactionId = INDEX_NONE;
        if (PlanetCameraComponent)
        {
            PlanetCameraComponent->ResetCameraState();
        }
        return;
    }

    TArray<FTerraGameplayCellState> GameplayCells;
    GameplayCells.SetNum(NumCells);

    for (int32 CellId = 0; CellId < NumCells; ++CellId)
    {
        const FCell& SourceCell = CellTopology->Cells[CellId];
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
            return TryExecuteNpcMcpValidatedAction_(ExpectedTurnIndex, ExpectedFactionId, PieceId, ToCellId, OutResult);
        });

    G2_5LastHighlightedFactionId = GameplayContainer->GetCurrentFactionId();
    if (PlanetCameraComponent)
        {
            PlanetCameraComponent->ResetCameraState();
        }
    RefreshCurrentFactionPieceHighlights_();
    SyncP1PiecePresentation_();
}

void APlanetTessellatedMesh::RefreshGameplayHighlights_(const TArray<int32>& DirtyCellIds)
{
    for (const int32 CellId : DirtyCellIds)
    {
        WriteHISMHighlightForCell_(CellId);
    }
}

void APlanetTessellatedMesh::RefreshFactionPieceHighlights_(int32 FactionId)
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
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
        WriteHISMHighlightForCell_(CellId);
    }
}

void APlanetTessellatedMesh::RefreshCurrentFactionPieceHighlights_()
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
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
        WriteHISMHighlightForCell_(CellId);
    }
}

bool APlanetTessellatedMesh::GetCellSurfaceWorldPosition_(int32 CellId, float RadiusOffsetCM, FVector& OutWorldPosition) const
{
    if (!CellTopology.IsValid() || !CellTopology->Cells.IsValidIndex(CellId))
    {
        return false;
    }

    const float Radius = GlobeRadiusCM + RadiusOffsetCM;
    const FVector LocalPosition = CellTopology->Cells[CellId].UnitCenter * Radius;
    OutWorldPosition = GetActorTransform().TransformPosition(LocalPosition);
    return true;
}

void APlanetTessellatedMesh::SyncP1PiecePresentation_(
    const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
    const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents)
{
    if (PlanetPiecePresentationComponent)
    {
        PlanetPiecePresentationComponent->SyncPresentation(MoveEvents, CaptureEvents);
    }
}

void APlanetTessellatedMesh::ClearP1PiecePresentation_()
{
    if (PlanetPiecePresentationComponent)
    {
        PlanetPiecePresentationComponent->ClearPresentation();
    }
}

bool APlanetTessellatedMesh::BuildP1PieceWorldTransform_(int32 CellId, FTransform& OutWorldTransform) const
{
    return PlanetPiecePresentationComponent
        ? PlanetPiecePresentationComponent->BuildPieceWorldTransform(CellId, OutWorldTransform)
        : false;
}

bool APlanetTessellatedMesh::BuildP1PieceWorldTransform_(int32 CellId, ETerraGameplayPieceType PieceType, FTransform& OutWorldTransform) const
{
    return PlanetPiecePresentationComponent
        ? PlanetPiecePresentationComponent->BuildPieceWorldTransform(CellId, PieceType, OutWorldTransform)
        : false;
}

bool APlanetTessellatedMesh::TryResolveP2_5PieceHeightFromHISM_(
    int32 CellId,
    const FVector& TraceWorldUp,
    const FVector& PlacementWorldUp,
    float TraceAngularOffsetDeg,
    FVector& OutWorldPosition) const
{
    return PlanetPiecePresentationComponent
        ? PlanetPiecePresentationComponent->TryResolvePieceHeightFromHISM(
            CellId,
            TraceWorldUp,
            PlacementWorldUp,
            TraceAngularOffsetDeg,
            OutWorldPosition)
        : false;
}

bool APlanetTessellatedMesh::BuildP1PieceWorldTransformForPiece_(
    const FTerraGameplayPieceState& Piece,
    const TArray<FTerraGameplayPieceState>& Pieces,
    FTransform& OutWorldTransform) const
{
    return PlanetPiecePresentationComponent
        ? PlanetPiecePresentationComponent->BuildPieceWorldTransformForPiece(Piece, Pieces, OutWorldTransform)
        : false;
}

void APlanetTessellatedMesh::BuildP3CaptureEventsFromPendingEntries_(
    const TArray<FTerraGameplayCaptureEntry>& CaptureEntries,
    const TArray<FTerraGameplayPieceState>& PiecesBeforeResolution,
    TArray<FTerraPiecePresentationCaptureEvent>& OutCaptureEvents) const
{
    if (PlanetPiecePresentationComponent)
    {
        PlanetPiecePresentationComponent->BuildCaptureEventsFromPendingEntries(
            CaptureEntries,
            PiecesBeforeResolution,
            OutCaptureEvents);
    }
    else
    {
        OutCaptureEvents.Reset();
    }
}

FTerraPieceVisualConfig APlanetTessellatedMesh::BuildP1PieceVisualConfig_() const
{
    return PlanetPiecePresentationComponent
        ? PlanetPiecePresentationComponent->BuildVisualConfig()
        : FTerraPieceVisualConfig();
}

FVector APlanetTessellatedMesh::GetPlanetCenterWorld_() const
{
    return GetActorTransform().TransformPosition(FVector::ZeroVector);
}

bool APlanetTessellatedMesh::SyncOrbitCameraStateFromWorldPosition(const FVector& CameraWorldPosition, float& InOutLongitudeDeg, float& InOutLatitudeDeg, float& InOutHeightOffsetCM) const
{
    return PlanetCameraComponent
        ? PlanetCameraComponent->SyncOrbitCameraStateFromWorldPosition(CameraWorldPosition, InOutLongitudeDeg, InOutLatitudeDeg, InOutHeightOffsetCM)
        : false;
}

bool APlanetTessellatedMesh::ApplyOrbitCameraState(float LongitudeDeg, float LatitudeDeg, float HeightOffsetCM)
{
    return PlanetCameraComponent
        ? PlanetCameraComponent->ApplyOrbitCameraState(LongitudeDeg, LatitudeDeg, HeightOffsetCM)
        : false;
}

bool APlanetTessellatedMesh::SyncFocusCameraStateFromView(
    const FVector& CameraWorldPosition,
    const FRotator& CameraWorldRotation,
    FVector& OutFocusUnitDir,
    float& OutDistanceToFocusCM,
    float& OutTiltDeg,
    float& OutYawAroundFocusDeg) const
{
    return PlanetCameraComponent
        ? PlanetCameraComponent->SyncFocusCameraStateFromView(
        CameraWorldPosition,
        CameraWorldRotation,
        OutFocusUnitDir,
        OutDistanceToFocusCM,
        OutTiltDeg,
        OutYawAroundFocusDeg)
        : false;
}

bool APlanetTessellatedMesh::ApplyFocusCameraState(
    const FVector& FocusUnitDir,
    float DistanceToFocusCM,
    float TiltDeg,
    float YawAroundFocusDeg)
{
    return PlanetCameraComponent
        ? PlanetCameraComponent->ApplyFocusCameraState(FocusUnitDir, DistanceToFocusCM, TiltDeg, YawAroundFocusDeg)
        : false;
}

bool APlanetTessellatedMesh::OffsetFocusCameraStateOnTangent(
    float RightDeltaDeg,
    float ForwardDeltaDeg,
    FVector& InOutFocusUnitDir,
    float& InOutYawAroundFocusDeg) const
{
    return PlanetCameraComponent
        ? PlanetCameraComponent->OffsetFocusCameraStateOnTangent(
        RightDeltaDeg,
        ForwardDeltaDeg,
        InOutFocusUnitDir,
        InOutYawAroundFocusDeg)
        : false;
}

void APlanetTessellatedMesh::ExecuteC6_5DelayedTurnStartFocus_(int32 ExpectedTurnIndex, int32 ExpectedFactionId)
{
    if (PlanetCameraComponent)
    {
        PlanetCameraComponent->ExecuteC6_5DelayedTurnStartFocus(ExpectedTurnIndex, ExpectedFactionId);
    }
}

bool APlanetTessellatedMesh::TryResolveHISMHitToCellId(const FHitResult& Hit, int32& OutCellId) const
{
    if (!bEnableHISMInstanceHighlight || !bEnableHISMTileRendering || !bEnableHISMTileCollision)
    {
        OutCellId = INDEX_NONE;
        return false;
    }

    return HISMTileRenderer.TryResolveHitToCellId(Hit, bEnableHISMTileCollision, OutCellId);
}

bool APlanetTessellatedMesh::HandleHISMHoverHit(const FHitResult& Hit)
{
    int32 CellId = INDEX_NONE;
    if (!TryResolveHISMHitToCellId(Hit, CellId))
    {
        return false;
    }

    UpdateHISMHoverCell_(CellId);

    if (GEngine && CellId != INDEX_NONE && CellId != HISMTileRenderer.GetLastClickedCellId())
    {
        const FString Msg = FString::Printf(TEXT("HISM Hover Cell #%d  Instance=%d  Component=%s"),
            CellId, Hit.Item, *GetNameSafe(Hit.GetComponent()));
        GEngine->AddOnScreenDebugMessage(2, 0.25f, FColor::Yellow, Msg);
    }

    return true;
}

bool APlanetTessellatedMesh::HandleHISMClickHit(const FHitResult& Hit)
{
    int32 CellId = INDEX_NONE;
    if (!TryResolveHISMHitToCellId(Hit, CellId))
    {
        return false;
    }

    HISMTileRenderer.SetLastClickedCellId(CellId);
    return HandleGameplayCellClick_(CellId, TEXT("HISM Click"), Hit.Item, GetNameSafe(Hit.GetComponent()));
}

bool APlanetTessellatedMesh::HandleC5NavigateCurrentFactionPiece(bool bReverse)
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess][C5] Tab navigation ignored: GameplayContainer is not ready."));
        return false;
    }

    const ETerraGameplayInteractionPhase Phase = GameplayContainer->GetInteractionPhase();
    if (Phase != ETerraGameplayInteractionPhase::Idle
        && Phase != ETerraGameplayInteractionPhase::PieceSelected)
    {
        UE_LOG(LogPlanetTess, Log,
            TEXT("[Tess][C5] Tab navigation ignored in Phase=%d."),
            static_cast<int32>(Phase));
        return false;
    }

    TArray<int32> SelectablePieceIds;
    if (!GameplayContainer->CollectCurrentFactionSelectablePieceIds(SelectablePieceIds))
    {
        UE_LOG(LogPlanetTess, Log,
            TEXT("[Tess][C5] Tab navigation ignored: no selectable pieces. CurrentFaction=%d Turn=%d"),
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

    HISMTileRenderer.SetLastClickedCellId(TargetCellId);
    const bool bHandled = HandleGameplayCellClick_(
        TargetCellId,
        bReverse ? TEXT("C5 Shift+Tab") : TEXT("C5 Tab"),
        INDEX_NONE,
        TEXT("Keyboard"));

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess][C5] Navigate Reverse=%d TargetPiece=%d TargetCell=%d Handled=%d Index=%d/%d"),
        bReverse ? 1 : 0,
        TargetPieceId,
        TargetCellId,
        bHandled ? 1 : 0,
        TargetIndex,
        SelectablePieceIds.Num());
    return bHandled;
}

bool APlanetTessellatedMesh::HandleGameplayCellClick_(int32 CellId, const TCHAR* SourceLabel, int32 InstanceIndex, const FString& ComponentName)
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess][G2] %s ignored: GameplayContainer is not ready. Cell=%d Instance=%d Component=%s"),
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
    RefreshGameplayHighlights_(DirtyCellIds);
    RefreshG4CapturePreviewCellsForActionTarget_(HISMTileRenderer.GetCurrentHoverCellId());

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
        BuildP3CaptureEventsFromPendingEntries_(PendingCaptureEntriesBeforeClick, PrevPieces, P3CaptureEvents);
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
            if (BuildP1PieceWorldTransform_(PrevSelectedPieceCellId, MovingPieceType, FromWorldTransform)
                && BuildP1PieceWorldTransform_(NewSelectedPieceCellId, MovingPieceType, ToWorldTransform))
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
        UE_LOG(LogPlanetTess, Log,
            TEXT("[Tess][C6.5] Turn changed after click. PrevTurn=%d NewTurn=%d PrevFaction=%d NewFaction=%d FactionChanged=%d PrevPhase=%d NewPhase=%d bGameplayHandled=%d PendingCapturesBeforeClick=%d P2MoveEvents=%d P3CaptureEvents=%d"),
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
            RefreshFactionPieceHighlights_(PrevFactionId);
        }
        RefreshFactionPieceHighlights_(NewFactionId);
        G2_5LastHighlightedFactionId = NewFactionId;
        if (PlanetCameraComponent)
        {
            PlanetCameraComponent->SetLastFocusedTurnIndex(NewTurnIndex);
        }
    }
    else if (NewPhase != PrevPhase || bGameplayHandled)
    {
        RefreshFactionPieceHighlights_(NewFactionId);
    }

    if (bGameplayHandled
        && NewSelectedPieceId != INDEX_NONE
        && NewSelectedPieceId != PrevSelectedPieceId
        && NewPhase == ETerraGameplayInteractionPhase::PieceSelected)
    {
        int32 SelectedPieceCellId = INDEX_NONE;
        if (PlanetCameraComponent && GameplayContainer->TryGetPieceCellId(NewSelectedPieceId, SelectedPieceCellId))
        {
            PlanetCameraComponent->FocusCameraOnSelectedCellSmart(SelectedPieceCellId);
        }
    }

    RebuildG1DebugPieces_();
    if (PlanetCameraComponent)
    {
        PlanetCameraComponent->RequestC6ActionCameraTrackingForMoveEvents(P2MoveEvents);
    }
    SyncP1PiecePresentation_(P2MoveEvents, P3CaptureEvents);

    if (bTurnChanged
        && (!PlanetCameraComponent || !PlanetCameraComponent->TryRequestC6_5DelayedTurnStartFocus(
            NewTurnIndex,
            NewFactionId,
            P2MoveEvents,
            P3CaptureEvents)))
    {
        UE_LOG(LogPlanetTess, Log,
            TEXT("[Tess][C6.5] Falling back to immediate turn focus. Faction=%d Turn=%d FactionChanged=%d P2MoveEvents=%d P3CaptureEvents=%d"),
            NewFactionId,
            NewTurnIndex,
            bFactionChanged ? 1 : 0,
            P2MoveEvents.Num(),
            P3CaptureEvents.Num());
        if (PlanetCameraComponent)
        {
            PlanetCameraComponent->FocusCameraOnCurrentFactionBase();
        }
    }

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess][G2] %s -> Gameplay Cell=%d Instance=%d Component=%s Handled=%d CurrentFaction=%d Turn=%d Phase=%d P2MoveEvents=%d P3CaptureEvents=%d"),
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

bool APlanetTessellatedMesh::TryExecuteNpcMcpValidatedAction_(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult)
{
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

    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
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

    if (!HandleGameplayCellClick_(LegalAction.FromCellId, TEXT("NPC MCP Select"), INDEX_NONE, TEXT("NpcMcp")))
    {
        return Reject(TEXT("select_failed"));
    }
    if (GameplayContainer->GetSelectedPieceId() != LegalAction.PieceId)
    {
        return Reject(TEXT("select_failed"));
    }

    if (!HandleGameplayCellClick_(LegalAction.ToCellId, TEXT("NPC MCP Move"), INDEX_NONE, TEXT("NpcMcp")))
    {
        TArray<int32> UndoDirtyCellIds;
        GameplayContainer->UndoCurrentInteraction(UndoDirtyCellIds);
        RefreshGameplayHighlights_(UndoDirtyCellIds);
        SyncP1PiecePresentation_();
        return Reject(TEXT("move_failed"));
    }

    const ETerraGameplayInteractionPhase PostMovePhase = GameplayContainer->GetInteractionPhase();
    if (PostMovePhase != ETerraGameplayInteractionPhase::PieceMovedCanEndTurn
        && PostMovePhase != ETerraGameplayInteractionPhase::PieceJumpingCanContinue)
    {
        TArray<int32> UndoDirtyCellIds;
        GameplayContainer->UndoCurrentInteraction(UndoDirtyCellIds);
        RefreshGameplayHighlights_(UndoDirtyCellIds);
        SyncP1PiecePresentation_();
        return Reject(TEXT("move_failed"));
    }

    if (!HandleGameplayCellClick_(LegalAction.ToCellId, TEXT("NPC MCP EndTurn"), INDEX_NONE, TEXT("NpcMcp")))
    {
        TArray<int32> UndoDirtyCellIds;
        GameplayContainer->UndoCurrentInteraction(UndoDirtyCellIds);
        RefreshGameplayHighlights_(UndoDirtyCellIds);
        SyncP1PiecePresentation_();
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

bool APlanetTessellatedMesh::HandleHISMUndo()
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
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
        if (BuildP1PieceWorldTransform_(PrevPiece.CellId, NewPiece.PieceType, FromWorldTransform)
            && BuildP1PieceWorldTransform_(NewPiece.CellId, NewPiece.PieceType, ToWorldTransform))
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

    RefreshGameplayHighlights_(DirtyCellIds);
    RefreshCurrentFactionPieceHighlights_();
    RefreshG4CapturePreviewCellsForActionTarget_(HISMTileRenderer.GetCurrentHoverCellId());
    RebuildG1DebugPieces_();
    if (PlanetCameraComponent)
    {
        PlanetCameraComponent->RequestC6ActionCameraTrackingForMoveEvents(UndoMoveEvents);
    }
    SyncP1PiecePresentation_(UndoMoveEvents);

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess][G9] HISM Undo -> CurrentFaction=%d Turn=%d Phase=%d UndoMoveEvents=%d"),
        GameplayContainer->GetCurrentFactionId(),
        GameplayContainer->GetTurnIndex(),
        static_cast<int32>(GameplayContainer->GetInteractionPhase()),
        UndoMoveEvents.Num());
    return true;
}

void APlanetTessellatedMesh::ClearHISMHover()
{
    UpdateHISMHoverCell_(INDEX_NONE);
}

void APlanetTessellatedMesh::ClearAllHISMHighlights()
{
    HISMTileRenderer.ClearAllHighlights(BuildHISMHighlightConfig_(), GameplayContainer.Get());
}

void APlanetTessellatedMesh::RebuildG1DebugPieces_()
{
    G1DebugPieces.Reset();

    if (!bEnableG1DebugPieces || !CellTopology.IsValid())
    {
        return;
    }

    if (GameplayContainer.IsValid() && GameplayContainer->IsInitialized())
    {
        for (const FTerraGameplayPieceState& Piece : GameplayContainer->GetPieces())
        {
            if (!Piece.bAlive || !CellTopology->Cells.IsValidIndex(Piece.CellId))
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

    const int32 NumCells = CellTopology->Cells.Num();
    TSet<int32> OccupiedCells;

    auto IsValidCellId = [NumCells](int32 CellId)
    {
        return CellId >= 0 && CellId < NumCells;
    };

    auto AddPiece = [this, &OccupiedCells, &IsValidCellId](int32 FactionId, int32 CellId, ETerraG1DebugPieceType PieceType)
    {
        if (!IsValidCellId(CellId))
        {
            UE_LOG(LogPlanetTess, Warning,
                TEXT("[Tess][G1] Skip invalid debug piece cell. Faction=%d Cell=%d Type=%d"),
                FactionId,
                CellId,
                static_cast<int32>(PieceType));
            return false;
        }

        if (OccupiedCells.Contains(CellId))
        {
            UE_LOG(LogPlanetTess, Warning,
                TEXT("[Tess][G1] Skip occupied debug piece cell. Faction=%d Cell=%d Type=%d"),
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
    for (const FCell& Cell : CellTopology->Cells)
    {
        if (Cell.bIsPentagon)
        {
            PentagonCellIds.Add(Cell.CellId);
        }
    }

    PentagonCellIds.Sort();

    if (PentagonCellIds.Num() != 12)
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess][G1] Expected 12 pentagon bases, got %d."),
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

        const FCell& BaseCell = CellTopology->Cells[BaseCellId];
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
                UE_LOG(LogPlanetTess, Warning,
                    TEXT("[Tess][G1] Invalid archer neighbor. Faction=%d Base=%d Slot=%d Cell=%d"),
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

            const FCell& ArcherCell = CellTopology->Cells[ArcherCellId];
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
                UE_LOG(LogPlanetTess, Warning,
                    TEXT("[Tess][G1] Cannot resolve cavalry opposite cell. Faction=%d Base=%d Archer=%d NeighborCount=%d BackIndex=%d"),
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

            const FCell& CavalryA = CellTopology->Cells[CavalryAId];
            const FCell& CavalryB = CellTopology->Cells[CavalryBId];
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

                const float Score = static_cast<float>(FVector::DotProduct(CellTopology->Cells[CandidateCellId].UnitCenter, MidDir));
                if (Score > BestScore)
                {
                    BestScore = Score;
                    BestInfantryCellId = CandidateCellId;
                }
            }

            if (BestInfantryCellId == INDEX_NONE)
            {
                UE_LOG(LogPlanetTess, Warning,
                    TEXT("[Tess][G1] Cannot resolve infantry between cavalry cells. Faction=%d Base=%d CavalryA=%d CavalryB=%d"),
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

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess][G1] Rebuilt debug pieces: Factions=%d Bases=%d Infantry=%d Cavalry=%d Archer=%d Total=%d"),
        PentagonCellIds.Num(),
        BaseCount,
        InfantryCount,
        CavalryCount,
        ArcherCount,
        G1DebugPieces.Num());
}

void APlanetTessellatedMesh::DrawG1DebugPieces_() const
{
    UWorld* World = GetWorld();
    if (PlanetPiecePresentationComponent
        && PlanetPiecePresentationComponent->bHideG1DebugPiecesWhenP1IsActive
        && PlanetPiecePresentationComponent->bEnableP1PiecePresentation
        && World
        && World->IsGameWorld())
    {
        return;
    }

    if (!bEnableG1DebugPieces || G1DebugPieces.Num() == 0 || !CellTopology.IsValid())
    {
        return;
    }

    if (!World)
    {
        return;
    }

    const float DrawRadius = FMath::Max(10.0f, G1DebugPieceRadiusCM);
    const float DrawDistance = GlobeRadiusCM + G1DebugPieceHeightOffsetCM;
    const FTransform ActorTransform = GetActorTransform();

    for (const FTerraG1DebugPiece& Piece : G1DebugPieces)
    {
        if (!CellTopology->Cells.IsValidIndex(Piece.CellId))
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
            DrawColor = FColor::Yellow;
            break;
        case ETerraG1DebugPieceType::Cavalry:
            DrawColor = FColor::Blue;
            break;
        case ETerraG1DebugPieceType::Archer:
            DrawColor = FColor::Green;
            break;
        default:
            break;
        }

        const FVector LocalPosition = CellTopology->Cells[Piece.CellId].UnitCenter * DrawDistance;
        const FVector WorldPosition = ActorTransform.TransformPosition(LocalPosition);
        DrawDebugSphere(World, WorldPosition, DrawRadius, 12, DrawColor, false, 0.05f, 0, 3.0f);
    }
}

void APlanetTessellatedMesh::ApplyRenderModeVisibility_()
{
    HISMTileRenderer.ApplyVisibility(bEnableHISMTileRendering, bEnableHISMTileCollision);

    const bool bNeedTick = (bEnableHISMInstanceHighlight && bEnableHISMTileRendering) || bEnableG1DebugPieces;
    PrimaryActorTick.SetTickFunctionEnable(bNeedTick);
}

// ===================================================================
//  T3 §6.2 / D15：OnPostWorldCleanup_ —— PIE 退出后材质恢复
// ===================================================================
void APlanetTessellatedMesh::OnPostWorldCleanup_(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
#if WITH_EDITOR
    // 防御 1：本 Actor 即将销毁（GetWorld 不可信）—— 跳过
    if (!IsValid(this) || HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
    {
        return;
    }

    // 防御 2：被 cleanup 的就是本 Actor 所在 World —— 即将销毁，跳过
    UWorld* MyWorld = GetWorld();
    if (!MyWorld || MyWorld == World)
    {
        return;
    }

    // 防御 3：仅在"游戏会话真的结束"（bSessionEnded=true，PIE/Standalone 退出）时恢复
    if (!bSessionEnded)
    {
        return;
    }

    // 防御 4：仅当本 Actor 在 Editor World 中时才需要恢复
    if (MyWorld->WorldType != EWorldType::Editor && MyWorld->WorldType != EWorldType::EditorPreview)
    {
        return;
    }

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess] PIE world cleaned up; rebuilding to restore Editor MID + materials. "
             "(MyWorld=%s, CleanedWorld=%s)"),
        *MyWorld->GetName(), World ? *World->GetName() : TEXT("(null)"));

    Rebuild();
#endif
}

void APlanetTessellatedMesh::SetHighlightLUT(UTexture2D* InLUT)
{
    UE_LOG(LogPlanetTess, Verbose,
        TEXT("[Tess] Deprecated SetHighlightLUT ignored (%s). HISM highlight uses PerInstanceCustomData."),
        *GetNameSafe(InLUT));
}
