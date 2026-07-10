// Fill out your copyright notice in the Description page of Project Settings.

#include "Render/PlanetTessellatedMesh.h"
#include "TerraGameplayContainer.h"
#include "TerraNpcMcpGameplayBridge.h"
#include "TerraPiecePresentationManager.h"

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
#include "Animation/AnimationAsset.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Interaction/PlanetInteractionController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlanetTess, Log, All);

namespace
{
    FTerraPiecePaletteTile MakeP7Tile(int32 Row, int32 Col)
    {
        FTerraPiecePaletteTile Tile;
        Tile.Row = Row;
        Tile.Col = Col;
        return Tile;
    }

    FTerraPiecePaletteMask MakeP7Mask(ETerraGameplayPieceType PieceType, int32 PrimaryRow, int32 PrimaryCol, int32 SecondaryRow, int32 SecondaryCol)
    {
        FTerraPiecePaletteMask Mask;
        Mask.PieceType = PieceType;
        Mask.PrimaryTiles.Add(MakeP7Tile(PrimaryRow, PrimaryCol));
        Mask.SecondaryTiles.Add(MakeP7Tile(SecondaryRow, SecondaryCol));
        Mask.MinSaturationToReplace = 0.05f;
        return Mask;
    }

    FTerraPieceFactionPalette MakeP7Palette(const FLinearColor& Primary, const FLinearColor& Secondary)
    {
        FTerraPieceFactionPalette Palette;
        Palette.PrimaryColor = Primary;
        Palette.SecondaryColor = Secondary;
        Palette.ColorStrength = 1.0f;
        return Palette;
    }

    void BuildDefaultP7FactionPalettes(TArray<FTerraPieceFactionPalette>& OutPalettes)
    {
        OutPalettes.Reset();
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.85f, 0.08f, 0.06f, 1.0f), FLinearColor(1.00f, 0.72f, 0.18f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.08f, 0.30f, 0.90f, 1.0f), FLinearColor(0.92f, 0.95f, 1.00f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.08f, 0.58f, 0.20f, 1.0f), FLinearColor(0.68f, 0.50f, 0.32f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.48f, 0.18f, 0.78f, 1.0f), FLinearColor(0.72f, 0.72f, 0.78f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.95f, 0.38f, 0.08f, 1.0f), FLinearColor(0.06f, 0.06f, 0.06f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.05f, 0.78f, 0.92f, 1.0f), FLinearColor(0.02f, 0.10f, 0.32f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.95f, 0.82f, 0.10f, 1.0f), FLinearColor(0.38f, 0.22f, 0.10f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.92f, 0.12f, 0.58f, 1.0f), FLinearColor(0.18f, 0.18f, 0.20f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.02f, 0.55f, 0.50f, 1.0f), FLinearColor(0.74f, 0.42f, 0.20f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.88f, 0.88f, 0.82f, 1.0f), FLinearColor(0.72f, 0.04f, 0.04f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.03f, 0.03f, 0.035f, 1.0f), FLinearColor(0.02f, 0.62f, 0.34f, 1.0f)));
        OutPalettes.Add(MakeP7Palette(FLinearColor(0.55f, 0.85f, 0.08f, 1.0f), FLinearColor(0.46f, 0.16f, 0.72f, 1.0f)));
    }

    void BuildDefaultP7PaletteMasks(TArray<FTerraPiecePaletteMask>& OutMasks)
    {
        OutMasks.Reset();
        OutMasks.Add(MakeP7Mask(ETerraGameplayPieceType::Commander, 1, 2, 1, 1));
        OutMasks.Add(MakeP7Mask(ETerraGameplayPieceType::Infantry, 1, 0, 1, 1));
        OutMasks.Add(MakeP7Mask(ETerraGameplayPieceType::Cavalry, 1, 0, 1, 1));
        OutMasks.Add(MakeP7Mask(ETerraGameplayPieceType::Archer, 1, 0, 1, 6));
    }

}

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

    PiecePresentationManager = CreateDefaultSubobject<UTerraPiecePresentationManager>(TEXT("PiecePresentationManager"));

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
APlanetTessellatedMesh::APlanetTessellatedMesh(FVTableHelper& Helper) : Super(Helper) {}

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

    if (bEnableG2_5CameraAssist
        && GetWorld()
        && GetWorld()->IsGameWorld()
        && GameplayContainer.IsValid()
        && GameplayContainer->IsInitialized()
        && G2_5LastCameraFocusedTurnIndex != GameplayContainer->GetTurnIndex())
    {
        if (!GetWorld()->GetTimerManager().IsTimerActive(C6_5DelayedTurnStartFocusTimerHandle))
        {
            UE_LOG(LogPlanetTess, Log,
                TEXT("[Tess][C6.5] Tick fallback turn focus is not delayed. LastFocusedTurn=%d CurrentTurn=%d CurrentFaction=%d"),
                G2_5LastCameraFocusedTurnIndex,
                GameplayContainer->GetTurnIndex(),
                GameplayContainer->GetCurrentFactionId());
            FocusCameraOnCurrentFactionBase_();
            G2_5LastCameraFocusedTurnIndex = GameplayContainer->GetTurnIndex();
        }
        else
        {
            UE_LOG(LogPlanetTess, Verbose,
                TEXT("[Tess][C6.5] Tick fallback turn focus suppressed by active delay timer. LastFocusedTurn=%d CurrentTurn=%d CurrentFaction=%d"),
                G2_5LastCameraFocusedTurnIndex,
                GameplayContainer->GetTurnIndex(),
                GameplayContainer->GetCurrentFactionId());
        }
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
        World->GetTimerManager().ClearTimer(C6_5DelayedTurnStartFocusTimerHandle);
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
        G2_5LastCameraFocusedTurnIndex = INDEX_NONE;
        bC2GameStartCameraApplied = false;
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
        G2_5LastCameraFocusedTurnIndex = INDEX_NONE;
        bC2GameStartCameraApplied = false;
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
    G2_5LastCameraFocusedTurnIndex = INDEX_NONE;
    bC2GameStartCameraApplied = false;
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

void APlanetTessellatedMesh::FocusCameraOnCell_(int32 CellId, bool bMoveCamera)
{
    if (!bEnableG2_5CameraAssist)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
    if (!PlayerController)
    {
        return;
    }

    FVector TargetWorldPosition;
    if (!GetCellSurfaceWorldPosition_(CellId, 0.0f, TargetWorldPosition))
    {
        return;
    }

    AActor* ViewTarget = PlayerController->GetViewTarget();
    FVector CameraWorldPosition = FVector::ZeroVector;
    if (bMoveCamera)
    {
        if (!GetCellSurfaceWorldPosition_(CellId, FMath::Max(0.0f, G2_5TurnStartCameraHeightCM), CameraWorldPosition))
        {
            return;
        }
    }
    else if (ViewTarget)
    {
        CameraWorldPosition = ViewTarget->GetActorLocation();
    }
    else if (PlayerController->PlayerCameraManager)
    {
        CameraWorldPosition = PlayerController->PlayerCameraManager->GetCameraLocation();
    }
    else
    {
        return;
    }

    const FVector LookDirection = TargetWorldPosition - CameraWorldPosition;
    if (LookDirection.IsNearlyZero())
    {
        return;
    }

    const FRotator LookRotation = LookDirection.Rotation();
    if (ViewTarget)
    {
        if (bMoveCamera)
        {
            ViewTarget->SetActorLocation(CameraWorldPosition);
        }
        ViewTarget->SetActorRotation(LookRotation);
    }
    PlayerController->SetControlRotation(LookRotation);

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess][G2.5] Focus camera on Cell=%d Move=%d Camera=(%.1f, %.1f, %.1f) Target=(%.1f, %.1f, %.1f)"),
        CellId,
        bMoveCamera ? 1 : 0,
        CameraWorldPosition.X,
        CameraWorldPosition.Y,
        CameraWorldPosition.Z,
        TargetWorldPosition.X,
        TargetWorldPosition.Y,
        TargetWorldPosition.Z);
}

bool APlanetTessellatedMesh::IsCellInC4ComfortView_(int32 CellId) const
{
    if (!CellTopology.IsValid() || !CellTopology->Cells.IsValidIndex(CellId))
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return false;
    }
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
    if (!PlayerController)
    {
        return false;
    }

    FVector CameraWorldPosition = FVector::ZeroVector;
    FRotator CameraWorldRotation = FRotator::ZeroRotator;
    if (PlayerController->PlayerCameraManager)
    {
        CameraWorldPosition = PlayerController->PlayerCameraManager->GetCameraLocation();
        CameraWorldRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
    }
    else if (AActor* ViewTarget = PlayerController->GetViewTarget())
    {
        CameraWorldPosition = ViewTarget->GetActorLocation();
        CameraWorldRotation = ViewTarget->GetActorRotation();
    }
    else
    {
        return false;
    }

    FVector CurrentFocusUnitDir = FVector::ZeroVector;
    float CurrentDistanceToFocusCM = 0.0f;
    float CurrentTiltDeg = 0.0f;
    float CurrentYawAroundFocusDeg = 0.0f;
    if (!SyncFocusCameraStateFromView(
        CameraWorldPosition,
        CameraWorldRotation,
        CurrentFocusUnitDir,
        CurrentDistanceToFocusCM,
        CurrentTiltDeg,
        CurrentYawAroundFocusDeg))
    {
        return false;
    }

    const FVector TargetFocusUnitDir = CellTopology->Cells[CellId].UnitCenter.GetSafeNormal();
    CurrentFocusUnitDir = CurrentFocusUnitDir.GetSafeNormal();
    if (TargetFocusUnitDir.IsNearlyZero() || CurrentFocusUnitDir.IsNearlyZero())
    {
        return false;
    }

    const float Dot = FMath::Clamp(static_cast<float>(FVector::DotProduct(CurrentFocusUnitDir, TargetFocusUnitDir)), -1.0f, 1.0f);
    const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));
    const float ComfortAngleDeg = FMath::Clamp(C4ComfortFocusAngleDeg, 0.0f, 180.0f);
    return AngleDeg <= ComfortAngleDeg;
}

bool APlanetTessellatedMesh::RequestC4SelectionFocus_(int32 CellId) const
{
    if (!CellTopology.IsValid() || !CellTopology->Cells.IsValidIndex(CellId))
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return false;
    }

    APlanetInteractionController* InteractionController = Cast<APlanetInteractionController>(
        UGameplayStatics::GetPlayerController(World, 0));
    if (!InteractionController)
    {
        return false;
    }

    const FVector TargetFocusUnitDir = CellTopology->Cells[CellId].UnitCenter.GetSafeNormal();
    return InteractionController->RequestC4FocusOnUnitDir(
        TargetFocusUnitDir,
        FMath::Max(0.0f, C4SelectedPieceFocusBlendSeconds));
}

float APlanetTessellatedMesh::GetC6ActionCameraBlendSeconds_(ETerraPiecePresentationMoveType MoveType) const
{
    return MoveType == ETerraPiecePresentationMoveType::Jump
        ? FMath::Max(P2JumpDurationSeconds, 0.001f)
        : FMath::Max(P2MoveDurationSeconds, 0.001f);
}

void APlanetTessellatedMesh::RequestC6ActionCameraTrackingForMoveEvents_(const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents) const
{
    if (!bEnableC6ActionCameraTracking || MoveEvents.Num() <= 0)
    {
        return;
    }

    APlanetInteractionController* InteractionController = Cast<APlanetInteractionController>(UGameplayStatics::GetPlayerController(this, 0));
    if (!InteractionController || !CellTopology.IsValid())
    {
        return;
    }

    for (const FTerraPiecePresentationMoveEvent& MoveEvent : MoveEvents)
    {
        if (!MoveEvent.IsValidMove() || !CellTopology->Cells.IsValidIndex(MoveEvent.ToCellId))
        {
            continue;
        }

        if (IsCellInC4ComfortView_(MoveEvent.ToCellId))
        {
            UE_LOG(LogPlanetTess, Verbose,
                TEXT("[Tess][C6] Move target in comfort view. Piece=%d ToCell=%d"),
                MoveEvent.PieceId,
                MoveEvent.ToCellId);
            continue;
        }

        const FVector TargetFocusUnitDir = CellTopology->Cells[MoveEvent.ToCellId].UnitCenter.GetSafeNormal();
        if (TargetFocusUnitDir.IsNearlyZero())
        {
            continue;
        }

        const float BlendSeconds = GetC6ActionCameraBlendSeconds_(MoveEvent.MoveType);
        const bool bRequested = InteractionController->RequestC6FocusOnUnitDir(TargetFocusUnitDir, BlendSeconds);
        if (bRequested)
        {
            UE_LOG(LogPlanetTess, Log,
                TEXT("[Tess][C6] Requested action camera tracking. Piece=%d FromCell=%d ToCell=%d MoveType=%d Blend=%.2f"),
                MoveEvent.PieceId,
                MoveEvent.FromCellId,
                MoveEvent.ToCellId,
                static_cast<int32>(MoveEvent.MoveType),
                BlendSeconds);
        }
    }
}

float APlanetTessellatedMesh::GetC6_5AnimationLengthSeconds_(UAnimationAsset* AnimationAsset, float FallbackSeconds) const
{
    return AnimationAsset
        ? FMath::Max(AnimationAsset->GetPlayLength(), 0.001f)
        : FMath::Max(FallbackSeconds, 0.001f);
}

float APlanetTessellatedMesh::GetC6_5AttackAnimationDurationSeconds_(
    ETerraGameplayPieceType PieceType,
    UAnimationAsset* AttackAnimation,
    float FallbackSeconds) const
{
    if (PieceType == ETerraGameplayPieceType::Commander)
    {
        return FMath::Max(P35CommanderAttackDurationSeconds, 0.001f)
            / FMath::Max(P35CommanderAttackPlayRateScale, 0.001f);
    }

    if (PieceType == ETerraGameplayPieceType::Archer)
    {
        return FMath::Max(P35ArcherAttackDurationSeconds, 0.001f)
            / FMath::Max(P35ArcherAttackPlayRateScale, 0.001f);
    }

    return P3AttackAnimationStartOffsetSeconds
        + GetC6_5AnimationLengthSeconds_(AttackAnimation, FallbackSeconds);
}

float APlanetTessellatedMesh::GetC6_5ActionPresentationDelaySeconds_(
    const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
    const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents) const
{
    float MoveSeconds = 0.0f;
    int32 ValidMoveEventCount = 0;
    for (const FTerraPiecePresentationMoveEvent& MoveEvent : MoveEvents)
    {
        if (MoveEvent.IsValidMove())
        {
            ++ValidMoveEventCount;
            MoveSeconds = FMath::Max(MoveSeconds, GetC6ActionCameraBlendSeconds_(MoveEvent.MoveType));
            UE_LOG(LogPlanetTess, Log,
                TEXT("[Tess][C6.5] Delay estimate includes move. Piece=%d FromCell=%d ToCell=%d MoveType=%d MoveSecondsNow=%.3f"),
                MoveEvent.PieceId,
                MoveEvent.FromCellId,
                MoveEvent.ToCellId,
                static_cast<int32>(MoveEvent.MoveType),
                MoveSeconds);
        }
        else
        {
            UE_LOG(LogPlanetTess, Log,
                TEXT("[Tess][C6.5] Delay estimate ignores invalid move event. Piece=%d FromCell=%d ToCell=%d MoveType=%d"),
                MoveEvent.PieceId,
                MoveEvent.FromCellId,
                MoveEvent.ToCellId,
                static_cast<int32>(MoveEvent.MoveType));
        }
    }

    float CaptureSeconds = 0.0f;
    int32 ValidCaptureEventCount = 0;
    const FTerraPieceVisualConfig VisualConfig = BuildP1PieceVisualConfig_();
    for (const FTerraPiecePresentationCaptureEvent& CaptureEvent : CaptureEvents)
    {
        if (!CaptureEvent.IsValidCapture())
        {
            UE_LOG(LogPlanetTess, Log,
                TEXT("[Tess][C6.5] Delay estimate ignores invalid capture event. Captured=%d Attacker=%d Vanguard=%d"),
                CaptureEvent.Captured.PieceId,
                CaptureEvent.Attacker.PieceId,
                CaptureEvent.Vanguard.PieceId);
            continue;
        }
        ++ValidCaptureEventCount;

        float MaxAttackToHitSeconds = FMath::Max(P3HitReactDelaySeconds, 0.0f);
        auto IncludeAttackToHit = [&VisualConfig, &MaxAttackToHitSeconds](const FTerraPiecePresentationCaptureParticipant& Participant)
        {
            if (Participant.IsValid())
            {
                MaxAttackToHitSeconds = FMath::Max(
                    MaxAttackToHitSeconds,
                    FMath::Max(VisualConfig.ResolveAttackToHitSeconds(Participant.PieceType), 0.0f));
            }
        };
        IncludeAttackToHit(CaptureEvent.Attacker);
        if (CaptureEvent.Vanguard.PieceId != CaptureEvent.Attacker.PieceId)
        {
            IncludeAttackToHit(CaptureEvent.Vanguard);
        }

        float AttackStepSeconds = 0.0f;
        auto IncludeAttackLength = [this, &VisualConfig, MaxAttackToHitSeconds, &AttackStepSeconds](const FTerraPiecePresentationCaptureParticipant& Participant)
        {
            if (!Participant.IsValid())
            {
                return;
            }

            const float AttackToHitSeconds = FMath::Max(VisualConfig.ResolveAttackToHitSeconds(Participant.PieceType), 0.0f);
            const float AttackStartDelaySeconds = FMath::Max(MaxAttackToHitSeconds - AttackToHitSeconds, 0.0f);
            UAnimationAsset* AttackAnimation = VisualConfig.ResolveAttackAnimation(Participant.PieceType);
            AttackStepSeconds = FMath::Max(
                AttackStepSeconds,
                AttackStartDelaySeconds
                    + GetC6_5AttackAnimationDurationSeconds_(Participant.PieceType, AttackAnimation, 0.5f));
        };

        IncludeAttackLength(CaptureEvent.Attacker);
        if (CaptureEvent.Vanguard.PieceId != CaptureEvent.Attacker.PieceId)
        {
            IncludeAttackLength(CaptureEvent.Vanguard);
        }

        const float HitSeconds = MaxAttackToHitSeconds
            + P3HitAnimationStartOffsetSeconds
            + GetC6_5AnimationLengthSeconds_(P3HitAnimation, 0.35f);
        const float DeathSeconds = MaxAttackToHitSeconds
            + P3DeathAfterHitDelaySeconds
            + P3DeathAnimationStartOffsetSeconds
            + GetC6_5AnimationLengthSeconds_(P3DeathAnimation, 0.75f);
        AttackStepSeconds = FMath::Max(AttackStepSeconds, HitSeconds);
        AttackStepSeconds = FMath::Max(AttackStepSeconds, DeathSeconds);

        const float FinishSeconds = AttackStepSeconds
            + FMath::Max(P3MeleeReturnSeconds, P3CapturedFadeSeconds);
        const float CaptureEventSeconds = P3FacingBlendSeconds
            + FMath::Max(P3MeleeRunInSeconds, 0.001f)
            + FinishSeconds;
        CaptureSeconds += CaptureEventSeconds;

        UE_LOG(LogPlanetTess, Log,
            TEXT("[Tess][C6.5] Delay estimate includes capture. Captured=%d Attacker=%d Vanguard=%d MaxAttackToHit=%.3f AttackStep=%.3f Hit=%.3f Death=%.3f EventSeconds=%.3f CaptureSecondsTotal=%.3f"),
            CaptureEvent.Captured.PieceId,
            CaptureEvent.Attacker.PieceId,
            CaptureEvent.Vanguard.PieceId,
            MaxAttackToHitSeconds,
            AttackStepSeconds,
            HitSeconds,
            DeathSeconds,
            CaptureEventSeconds,
            CaptureSeconds);
    }

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess][C6.5] Delay estimate summary. MoveEvents=%d ValidMoveEvents=%d CaptureEvents=%d ValidCaptureEvents=%d MoveSeconds=%.3f CaptureSeconds=%.3f Total=%.3f"),
        MoveEvents.Num(),
        ValidMoveEventCount,
        CaptureEvents.Num(),
        ValidCaptureEventCount,
        MoveSeconds,
        CaptureSeconds,
        MoveSeconds + CaptureSeconds);

    return MoveSeconds + CaptureSeconds;
}

bool APlanetTessellatedMesh::TryRequestC6_5DelayedTurnStartFocus_(
    int32 ExpectedTurnIndex,
    int32 ExpectedFactionId,
    const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
    const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents)
{
    if (!bEnableC6_5DelayTurnStartFocusUntilActionPresentationEnds)
    {
        UE_LOG(LogPlanetTess, Log,
            TEXT("[Tess][C6.5] Delay request rejected: disabled. Faction=%d Turn=%d MoveEvents=%d CaptureEvents=%d"),
            ExpectedFactionId,
            ExpectedTurnIndex,
            MoveEvents.Num(),
            CaptureEvents.Num());
        return false;
    }

    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        UE_LOG(LogPlanetTess, Log,
            TEXT("[Tess][C6.5] Delay request rejected: no game world. Faction=%d Turn=%d MoveEvents=%d CaptureEvents=%d"),
            ExpectedFactionId,
            ExpectedTurnIndex,
            MoveEvents.Num(),
            CaptureEvents.Num());
        return false;
    }

    const float DelaySeconds = GetC6_5ActionPresentationDelaySeconds_(MoveEvents, CaptureEvents);
    if (DelaySeconds <= KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogPlanetTess, Log,
            TEXT("[Tess][C6.5] Delay request rejected: estimated delay is zero. Faction=%d Turn=%d Delay=%.6f MoveEvents=%d CaptureEvents=%d"),
            ExpectedFactionId,
            ExpectedTurnIndex,
            DelaySeconds,
            MoveEvents.Num(),
            CaptureEvents.Num());
        return false;
    }

    const bool bHadExistingTimer = World->GetTimerManager().IsTimerActive(C6_5DelayedTurnStartFocusTimerHandle);
    World->GetTimerManager().ClearTimer(C6_5DelayedTurnStartFocusTimerHandle);
    const float TotalDelaySeconds = DelaySeconds + FMath::Max(C6_5TurnStartFocusDelayPaddingSeconds, 0.0f);
    const FTimerDelegate Delegate = FTimerDelegate::CreateUObject(
        this,
        &APlanetTessellatedMesh::ExecuteC6_5DelayedTurnStartFocus_,
        ExpectedTurnIndex,
        ExpectedFactionId);
    World->GetTimerManager().SetTimer(
        C6_5DelayedTurnStartFocusTimerHandle,
        Delegate,
        FMath::Max(TotalDelaySeconds, 0.001f),
        false);

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess][C6.5] Delayed turn start camera focus. Faction=%d Turn=%d Delay=%.3f RawDelay=%.3f Padding=%.3f MoveEvents=%d CaptureEvents=%d ReplacedExistingTimer=%d"),
        ExpectedFactionId,
        ExpectedTurnIndex,
        TotalDelaySeconds,
        DelaySeconds,
        FMath::Max(C6_5TurnStartFocusDelayPaddingSeconds, 0.0f),
        MoveEvents.Num(),
        CaptureEvents.Num(),
        bHadExistingTimer ? 1 : 0);
    return true;
}

void APlanetTessellatedMesh::ExecuteC6_5DelayedTurnStartFocus_(int32 ExpectedTurnIndex, int32 ExpectedFactionId)
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        UE_LOG(LogPlanetTess, Log,
            TEXT("[Tess][C6.5] Delayed turn start focus fired but gameplay is unavailable. ExpectedFaction=%d ExpectedTurn=%d"),
            ExpectedFactionId,
            ExpectedTurnIndex);
        return;
    }

    if (GameplayContainer->GetTurnIndex() != ExpectedTurnIndex
        || GameplayContainer->GetCurrentFactionId() != ExpectedFactionId)
    {
        UE_LOG(LogPlanetTess, Verbose,
            TEXT("[Tess][C6.5] Skip stale delayed turn start focus. ExpectedFaction=%d ExpectedTurn=%d CurrentFaction=%d CurrentTurn=%d"),
            ExpectedFactionId,
            ExpectedTurnIndex,
            GameplayContainer->GetCurrentFactionId(),
            GameplayContainer->GetTurnIndex());
        return;
    }

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess][C6.5] Delayed turn start focus fired. Faction=%d Turn=%d"),
        ExpectedFactionId,
        ExpectedTurnIndex);
    FocusCameraOnCurrentFactionBase_();
}

void APlanetTessellatedMesh::FocusCameraOnSelectedCellSmart_(int32 CellId)
{
    if (!bEnableG2_5CameraAssist)
    {
        return;
    }

    if (!bEnableC4SmartSelectionFocus)
    {
        FocusCameraOnCell_(CellId, false);
        return;
    }

    if (IsCellInC4ComfortView_(CellId))
    {
        UE_LOG(LogPlanetTess, Log, TEXT("[Tess][C4] Selected Cell=%d already in comfort view. Camera unchanged."), CellId);
        return;
    }

    if (!RequestC4SelectionFocus_(CellId))
    {
        FocusCameraOnCell_(CellId, false);
        return;
    }

    UE_LOG(LogPlanetTess, Log, TEXT("[Tess][C4] Requested smart selection focus. Cell=%d Blend=%.2f"),
        CellId,
        C4SelectedPieceFocusBlendSeconds);
}

void APlanetTessellatedMesh::FocusCameraOnCurrentFactionBase_()
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        return;
    }

    if (!bC2GameStartCameraApplied)
    {
        bC2GameStartCameraApplied = true;
        if (bEnableC2GameStartWarZoneCamera && FocusCameraOnCurrentFactionWarZoneHard_())
        {
            return;
        }
    }
    else if (bEnableC2_5TurnStartWarZoneFocusBlend && BlendCameraFocusToCurrentFactionWarZone_())
    {
        return;
    }

    FocusCameraOnCell_(GameplayContainer->GetCurrentFactionBaseCellId(), true);
}

bool APlanetTessellatedMesh::TryBuildCurrentFactionWarZoneDirection_(FVector& OutLocalWarZoneDir) const
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized() || !CellTopology.IsValid())
    {
        return false;
    }

    const int32 CurrentFactionId = GameplayContainer->GetCurrentFactionId();
    FVector LocalDirSum = FVector::ZeroVector;
    int32 AlivePieceCount = 0;

    for (const FTerraGameplayPieceState& Piece : GameplayContainer->GetPieces())
    {
        if (!Piece.bAlive
            || Piece.OwnerFactionId != CurrentFactionId
            || !CellTopology->Cells.IsValidIndex(Piece.CellId))
        {
            continue;
        }

        LocalDirSum += CellTopology->Cells[Piece.CellId].UnitCenter.GetSafeNormal();
        ++AlivePieceCount;
    }

    if (AlivePieceCount <= 0 || LocalDirSum.IsNearlyZero())
    {
        return false;
    }

    OutLocalWarZoneDir = LocalDirSum.GetSafeNormal();
    return !OutLocalWarZoneDir.IsNearlyZero();
}

bool APlanetTessellatedMesh::TryBuildCurrentFactionCommanderDirection_(FVector& OutLocalCommanderDir) const
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized() || !CellTopology.IsValid())
    {
        return false;
    }

    const int32 CurrentFactionId = GameplayContainer->GetCurrentFactionId();
    for (const FTerraGameplayPieceState& Piece : GameplayContainer->GetPieces())
    {
        if (Piece.bAlive
            && Piece.OwnerFactionId == CurrentFactionId
            && Piece.PieceType == ETerraGameplayPieceType::Commander
            && CellTopology->Cells.IsValidIndex(Piece.CellId))
        {
            OutLocalCommanderDir = CellTopology->Cells[Piece.CellId].UnitCenter.GetSafeNormal();
            return !OutLocalCommanderDir.IsNearlyZero();
        }
    }

    const int32 BaseCellId = GameplayContainer->GetCurrentFactionBaseCellId();
    if (CellTopology->Cells.IsValidIndex(BaseCellId))
    {
        OutLocalCommanderDir = CellTopology->Cells[BaseCellId].UnitCenter.GetSafeNormal();
        return !OutLocalCommanderDir.IsNearlyZero();
    }

    return false;
}

bool APlanetTessellatedMesh::FocusCameraOnCurrentFactionWarZoneHard_()
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return false;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
    if (!PlayerController)
    {
        return false;
    }

    FVector LocalWarZoneDir = FVector::ZeroVector;
    if (!TryBuildCurrentFactionWarZoneDirection_(LocalWarZoneDir))
    {
        return false;
    }

    FVector LocalCommanderDir = FVector::ZeroVector;
    TryBuildCurrentFactionCommanderDirection_(LocalCommanderDir);

    const FTransform ActorTransform = GetActorTransform();
    const FVector TargetWorldPosition = ActorTransform.TransformPosition(LocalWarZoneDir * GlobeRadiusCM);
    const FVector WorldUp = ActorTransform.TransformVectorNoScale(LocalWarZoneDir).GetSafeNormal();
    if (WorldUp.IsNearlyZero())
    {
        return false;
    }

    FVector LocalForwardHint = FVector::VectorPlaneProject(LocalWarZoneDir - LocalCommanderDir, LocalWarZoneDir).GetSafeNormal();
    if (LocalForwardHint.IsNearlyZero())
    {
        FVector CurrentCameraWorldPosition = FVector::ZeroVector;
        if (AActor* ViewTarget = PlayerController->GetViewTarget())
        {
            CurrentCameraWorldPosition = ViewTarget->GetActorLocation();
        }
        else if (PlayerController->PlayerCameraManager)
        {
            CurrentCameraWorldPosition = PlayerController->PlayerCameraManager->GetCameraLocation();
        }

        if (!CurrentCameraWorldPosition.IsNearlyZero())
        {
            const FVector LocalCameraDir = ActorTransform.InverseTransformPosition(CurrentCameraWorldPosition).GetSafeNormal();
            LocalForwardHint = FVector::VectorPlaneProject(LocalWarZoneDir - LocalCameraDir, LocalWarZoneDir).GetSafeNormal();
        }
    }

    if (LocalForwardHint.IsNearlyZero())
    {
        LocalForwardHint = FVector::VectorPlaneProject(FVector::ForwardVector, LocalWarZoneDir).GetSafeNormal();
    }
    if (LocalForwardHint.IsNearlyZero())
    {
        LocalForwardHint = FVector::VectorPlaneProject(FVector::RightVector, LocalWarZoneDir).GetSafeNormal();
    }
    if (LocalForwardHint.IsNearlyZero())
    {
        return false;
    }

    const FVector WorldForwardHint = ActorTransform.TransformVectorNoScale(LocalForwardHint).GetSafeNormal();
    if (WorldForwardHint.IsNearlyZero())
    {
        return false;
    }

    const float CameraDistance = FMath::Clamp(
        FMath::Max(1.0f, C3InitialDistanceToFocusCM),
        G8CameraMinHeightOffsetCM,
        FMath::Max(G8CameraMinHeightOffsetCM, G8CameraMaxHeightOffsetCM));
    const float TiltT = (C3AutoTiltMaxDistanceCM > C3AutoTiltMinDistanceCM)
        ? (CameraDistance - C3AutoTiltMinDistanceCM) / (C3AutoTiltMaxDistanceCM - C3AutoTiltMinDistanceCM)
        : 0.0f;
    const float CameraTiltDeg = FMath::Lerp(
        C3AutoTiltAtMinDistanceDeg,
        C3AutoTiltAtMaxDistanceDeg,
        FMath::Clamp(TiltT, 0.0f, 1.0f));
    const float TiltRad = FMath::DegreesToRadians(CameraTiltDeg);
    const float HorizontalDistance = CameraDistance * FMath::Cos(TiltRad);
    const float VerticalDistance = CameraDistance * FMath::Sin(TiltRad);
    const FVector CameraWorldPosition = TargetWorldPosition - WorldForwardHint * HorizontalDistance + WorldUp * VerticalDistance;

    const FVector LookDirection = TargetWorldPosition - CameraWorldPosition;
    if (LookDirection.IsNearlyZero())
    {
        return false;
    }

    const FVector CameraForward = LookDirection.GetSafeNormal();
    const FRotator LookRotation = FRotationMatrix::MakeFromXZ(CameraForward, WorldUp).Rotator();

    FVector LocalEast = FVector::CrossProduct(FVector::UpVector, LocalWarZoneDir).GetSafeNormal();
    if (LocalEast.IsNearlyZero())
    {
        LocalEast = FVector::CrossProduct(FVector::RightVector, LocalWarZoneDir).GetSafeNormal();
    }
    const FVector LocalNorth = FVector::CrossProduct(LocalWarZoneDir, LocalEast).GetSafeNormal();
    if (!LocalEast.IsNearlyZero() && !LocalNorth.IsNearlyZero())
    {
        const float C3YawAroundFocusDeg = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(
            static_cast<float>(FVector::DotProduct(LocalForwardHint, LocalEast)),
            static_cast<float>(FVector::DotProduct(LocalForwardHint, LocalNorth)))));

        if (APlanetInteractionController* InteractionController = Cast<APlanetInteractionController>(PlayerController))
        {
            InteractionController->SetC3FocusCameraState(LocalWarZoneDir, CameraDistance, C3YawAroundFocusDeg);
            InteractionController->SetControlRotation(LookRotation);
            ApplyFocusCameraState(LocalWarZoneDir, CameraDistance, CameraTiltDeg, C3YawAroundFocusDeg);

            UE_LOG(LogPlanetTess, Log,
                TEXT("[Tess][C2] Game start focus war zone camera via C3 state Faction=%d Turn=%d Camera=(%.1f, %.1f, %.1f) Target=(%.1f, %.1f, %.1f) Distance=%.1f Tilt=%.1f Yaw=%.1f"),
                GameplayContainer.IsValid() ? GameplayContainer->GetCurrentFactionId() : INDEX_NONE,
                GameplayContainer.IsValid() ? GameplayContainer->GetTurnIndex() : INDEX_NONE,
                CameraWorldPosition.X,
                CameraWorldPosition.Y,
                CameraWorldPosition.Z,
                TargetWorldPosition.X,
                TargetWorldPosition.Y,
                TargetWorldPosition.Z,
                CameraDistance,
                CameraTiltDeg,
                C3YawAroundFocusDeg);

            return true;
        }
    }

    if (AActor* ViewTarget = PlayerController->GetViewTarget())
    {
        ViewTarget->SetActorLocation(CameraWorldPosition);
        ViewTarget->SetActorRotation(LookRotation);
    }
    PlayerController->SetControlRotation(LookRotation);

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess][C2] Game start focus war zone camera fallback Faction=%d Turn=%d Camera=(%.1f, %.1f, %.1f) Target=(%.1f, %.1f, %.1f) Distance=%.1f Tilt=%.1f"),
        GameplayContainer.IsValid() ? GameplayContainer->GetCurrentFactionId() : INDEX_NONE,
        GameplayContainer.IsValid() ? GameplayContainer->GetTurnIndex() : INDEX_NONE,
        CameraWorldPosition.X,
        CameraWorldPosition.Y,
        CameraWorldPosition.Z,
        TargetWorldPosition.X,
        TargetWorldPosition.Y,
        TargetWorldPosition.Z,
        CameraDistance,
        CameraTiltDeg);

    return true;
}

bool APlanetTessellatedMesh::BlendCameraFocusToCurrentFactionWarZone_()
{
    if (!GameplayContainer.IsValid() || !GameplayContainer->IsInitialized() || !CellTopology.IsValid())
    {
        return false;
    }

    FVector LocalWarZoneDir = FVector::ZeroVector;
    if (!TryBuildCurrentFactionWarZoneDirection_(LocalWarZoneDir))
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return false;
    }

    APlanetInteractionController* InteractionController = Cast<APlanetInteractionController>(
        UGameplayStatics::GetPlayerController(World, 0));
    if (!InteractionController)
    {
        return false;
    }

    const bool bRequested = InteractionController->RequestC2_5FocusOnUnitDir(
        LocalWarZoneDir,
        FMath::Max(0.0f, C2_5TurnStartFocusBlendSeconds));
    if (bRequested)
    {
        UE_LOG(LogPlanetTess, Log,
            TEXT("[Tess][C2.5] Requested turn start war zone focus blend Faction=%d Turn=%d Blend=%.2f"),
            GameplayContainer->GetCurrentFactionId(),
            GameplayContainer->GetTurnIndex(),
            C2_5TurnStartFocusBlendSeconds);
    }
    return bRequested;
}

void APlanetTessellatedMesh::SyncP1PiecePresentation_(
    const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents,
    const TArray<FTerraPiecePresentationCaptureEvent>& CaptureEvents)
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld() || !bEnableP1PiecePresentation)
    {
        ClearP1PiecePresentation_();
        return;
    }

    if (!PiecePresentationManager || !GameplayContainer.IsValid() || !GameplayContainer->IsInitialized())
    {
        ClearP1PiecePresentation_();
        return;
    }

    const FTerraPieceVisualConfig VisualConfig = BuildP1PieceVisualConfig_();

    TArray<FTerraPiecePresentationSnapshot> Snapshots;
    const TArray<FTerraGameplayPieceState>& Pieces = GameplayContainer->GetPieces();
    Snapshots.Reserve(Pieces.Num());

    int32 MissingMeshCount = 0;
    for (const FTerraGameplayPieceState& Piece : Pieces)
    {
        if (!Piece.bAlive)
        {
            continue;
        }

        FTransform PieceWorldTransform = FTransform::Identity;
        if (!BuildP1PieceWorldTransformForPiece_(Piece, Pieces, PieceWorldTransform))
        {
            continue;
        }

        if (!VisualConfig.ResolveMesh(Piece.PieceType))
        {
            ++MissingMeshCount;
        }

        FTerraPiecePresentationSnapshot& Snapshot = Snapshots.AddDefaulted_GetRef();
        Snapshot.PieceId = Piece.PieceId;
        Snapshot.OwnerFactionId = Piece.OwnerFactionId;
        Snapshot.CellId = Piece.CellId;
        Snapshot.PieceType = Piece.PieceType;
        Snapshot.WorldTransform = PieceWorldTransform;
    }

    if (MissingMeshCount > 0)
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess][P1] Piece presentation has %d alive pieces without resolved SkeletalMesh. Check P1 mesh slots on APlanetTessellatedMesh."),
            MissingMeshCount);
    }

    PiecePresentationManager->SyncPieces(Snapshots, VisualConfig, MoveEvents, CaptureEvents);
}

void APlanetTessellatedMesh::ClearP1PiecePresentation_()
{
    if (PiecePresentationManager)
    {
        PiecePresentationManager->ClearPieces();
    }
}

bool APlanetTessellatedMesh::BuildP1PieceWorldTransform_(int32 CellId, FTransform& OutWorldTransform) const
{
    return BuildP1PieceWorldTransform_(CellId, ETerraGameplayPieceType::Infantry, OutWorldTransform);
}

bool APlanetTessellatedMesh::BuildP1PieceWorldTransform_(int32 CellId, ETerraGameplayPieceType PieceType, FTransform& OutWorldTransform) const
{
    if (!CellTopology.IsValid() || !CellTopology->Cells.IsValidIndex(CellId))
    {
        return false;
    }

    const FTransform ActorTransform = GetActorTransform();
    const FVector LocalUp = CellTopology->Cells[CellId].UnitCenter.GetSafeNormal();
    if (LocalUp.IsNearlyZero())
    {
        return false;
    }

    FVector LocalForward = FVector::VectorPlaneProject(FVector::ForwardVector, LocalUp).GetSafeNormal();
    if (LocalForward.IsNearlyZero())
    {
        LocalForward = FVector::VectorPlaneProject(FVector::RightVector, LocalUp).GetSafeNormal();
    }
    if (LocalForward.IsNearlyZero())
    {
        return false;
    }

    const FVector WorldUp = ActorTransform.TransformVectorNoScale(LocalUp).GetSafeNormal();
    const FVector WorldForward = ActorTransform.TransformVectorNoScale(LocalForward).GetSafeNormal();
    if (WorldUp.IsNearlyZero() || WorldForward.IsNearlyZero())
    {
        return false;
    }

    FVector TraceWorldUp = WorldUp;
    float TraceAngularOffsetDeg = 0.0f;
    if (PieceType == ETerraGameplayPieceType::Cavalry)
    {
        const float RequestedOffsetDeg = FMath::Clamp(P2_6CavalryHeightTraceAngularOffsetDeg, 0.0f, 15.0f);
        if (RequestedOffsetDeg > KINDA_SMALL_NUMBER)
        {
            const FVector TraceTangent = WorldForward;
            const FVector RotationAxis = FVector::CrossProduct(WorldUp, TraceTangent).GetSafeNormal();
            if (!RotationAxis.IsNearlyZero())
            {
                TraceWorldUp = WorldUp.RotateAngleAxis(RequestedOffsetDeg, RotationAxis).GetSafeNormal();
                TraceAngularOffsetDeg = RequestedOffsetDeg;
            }
        }
    }

    FVector WorldPosition = FVector::ZeroVector;
    if (!TryResolveP2_5PieceHeightFromHISM_(CellId, TraceWorldUp, WorldUp, TraceAngularOffsetDeg, WorldPosition))
    {
        const FVector LocalPosition = LocalUp * (GlobeRadiusCM + FMath::Max(0.0f, P1PieceRadiusOffsetCM));
        WorldPosition = ActorTransform.TransformPosition(LocalPosition);
        if (bDebugP2_5HISMPieceHeightTrace)
        {
            //const FVector PlanetCenterWorld = GetPlanetCenterWorld_();
            //UE_LOG(LogPlanetTess, Warning,
            //    TEXT("[Tess][P2.5/P2.6] Cell=%d PieceType=%d FallbackFixedRadius WorldRadius=%.1f GlobeRadius=%.1f PieceOffset=%.1f TraceOffsetDeg=%.2f"),
            //    CellId,
            //    static_cast<int32>(PieceType),
            //    FVector::Distance(WorldPosition, PlanetCenterWorld),
            //    GlobeRadiusCM,
            //    P1PieceRadiusOffsetCM,
            //    TraceAngularOffsetDeg);
        }
    }

    const FQuat WorldRotation = FRotationMatrix::MakeFromXZ(WorldForward, WorldUp).ToQuat();

    OutWorldTransform = FTransform(WorldRotation, WorldPosition, FVector::OneVector);
    return true;
}

bool APlanetTessellatedMesh::TryResolveP2_5PieceHeightFromHISM_(
    int32 CellId,
    const FVector& TraceWorldUp,
    const FVector& PlacementWorldUp,
    float TraceAngularOffsetDeg,
    FVector& OutWorldPosition) const
{
    UWorld* World = GetWorld();
    auto LogReject = [this, CellId, TraceAngularOffsetDeg](const TCHAR* Reason)
    {
        if (bDebugP2_5HISMPieceHeightTrace)
        {
            //UE_LOG(LogPlanetTess, Warning,
            //    TEXT("[Tess][P2.5/P2.6] Cell=%d Reject Reason=%s TraceOffsetDeg=%.2f"),
            //    CellId,
            //    Reason,
            //    TraceAngularOffsetDeg);
        }
    };

    if (!World)
    {
        LogReject(TEXT("NoWorld"));
        return false;
    }
    if (!bEnableP2_5HISMPieceHeightTrace)
    {
        LogReject(TEXT("FeatureDisabled"));
        return false;
    }
    if (!bEnableHISMTileRendering)
    {
        LogReject(TEXT("HISMTileRenderingDisabled"));
        return false;
    }
    if (!bEnableHISMTileCollision)
    {
        LogReject(TEXT("HISMTileCollisionDisabled"));
        return false;
    }
    if (!CellTopology.IsValid())
    {
        LogReject(TEXT("NoCellTopology"));
        return false;
    }
    if (!CellTopology->Cells.IsValidIndex(CellId))
    {
        LogReject(TEXT("InvalidCellId"));
        return false;
    }
    if (TraceWorldUp.IsNearlyZero())
    {
        LogReject(TEXT("ZeroTraceWorldUp"));
        return false;
    }
    if (PlacementWorldUp.IsNearlyZero())
    {
        LogReject(TEXT("ZeroPlacementWorldUp"));
        return false;
    }

    if (!PlainTileHISMComp || !ForestTileHISMComp || !MountainTileHISMComp)
    {
        LogReject(TEXT("MissingHISMComponents"));
        return false;
    }

    const FVector PlanetCenterWorld = GetPlanetCenterWorld_();
    const FVector TraceDirection = TraceWorldUp.GetSafeNormal();
    const FVector PlacementDirection = PlacementWorldUp.GetSafeNormal();
    const FVector TraceStart = PlanetCenterWorld + TraceDirection * (GlobeRadiusCM + FMath::Max(P2_5PieceHeightTraceStartOffsetCM, 0.0f));
    const FVector TraceEnd = PlanetCenterWorld - TraceDirection * FMath::Max(P2_5PieceHeightTracePastCenterOffsetCM, 0.0f);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TerraPieceHeightTrace), false);
    QueryParams.bReturnFaceIndex = false;
    QueryParams.bReturnPhysicalMaterial = false;

    TArray<FHitResult> Hits;
    if (!World->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
    {
        if (bDebugP2_5HISMPieceHeightTrace)
        {
            //UE_LOG(LogPlanetTess, Warning,
            //    TEXT("[Tess][P2.5/P2.6] Cell=%d NoTraceHit Start=(%.1f,%.1f,%.1f) End=(%.1f,%.1f,%.1f) StartRadius=%.1f EndRadius=%.1f TraceOffsetDeg=%.2f"),
            //    CellId,
            //    TraceStart.X, TraceStart.Y, TraceStart.Z,
            //    TraceEnd.X, TraceEnd.Y, TraceEnd.Z,
            //    FVector::Distance(TraceStart, PlanetCenterWorld),
            //    FVector::Distance(TraceEnd, PlanetCenterWorld),
            //    TraceAngularOffsetDeg);
        }
        return false;
    }

    const FHitResult* FirstHISMHit = nullptr;
    const FHitResult* CurrentCellHit = nullptr;
    int32 HISMHitCount = 0;

    for (int32 HitIndex = 0; HitIndex < Hits.Num(); ++HitIndex)
    {
        const FHitResult& Hit = Hits[HitIndex];
        UPrimitiveComponent* HitComp = Hit.GetComponent();
        if (HitComp != PlainTileHISMComp
            && HitComp != ForestTileHISMComp
            && HitComp != MountainTileHISMComp)
        {
            if (bDebugP2_5HISMPieceHeightTrace)
            {
                //UE_LOG(LogPlanetTess, Log,
                //    TEXT("[Tess][P2.5/P2.6] Cell=%d Hit[%d/%d] NonHISM Component=%s Item=%d Distance=%.1f ImpactRadius=%.1f TraceOffsetDeg=%.2f"),
                //    CellId,
                //    HitIndex,
                //    Hits.Num(),
                //    *GetNameSafe(HitComp),
                //    Hit.Item,
                //    Hit.Distance,
                //    FVector::Distance(Hit.ImpactPoint, PlanetCenterWorld),
                //    TraceAngularOffsetDeg);
            }
            continue;
        }

        ++HISMHitCount;
        if (!FirstHISMHit)
        {
            FirstHISMHit = &Hit;
        }

        int32 HitCellId = INDEX_NONE;
        const bool bResolvedCellId = TryResolveHISMHitToCellId(Hit, HitCellId);
        if (bDebugP2_5HISMPieceHeightTrace)
        {
            //UE_LOG(LogPlanetTess, Warning,
            //    TEXT("[Tess][P2.5/P2.6] Cell=%d Hit[%d/%d] HISM Component=%s Item=%d Resolved=%d HitCell=%d WantedCell=%d Distance=%.1f ImpactRadius=%.1f Impact=(%.1f,%.1f,%.1f) TraceOffsetDeg=%.2f"),
            //    CellId,
            //    HitIndex,
            //    Hits.Num(),
            //    *GetNameSafe(HitComp),
            //    Hit.Item,
            //    bResolvedCellId ? 1 : 0,
            //    HitCellId,
            //    CellId,
            //    Hit.Distance,
            //    FVector::Distance(Hit.ImpactPoint, PlanetCenterWorld),
            //    Hit.ImpactPoint.X,
            //    Hit.ImpactPoint.Y,
            //    Hit.ImpactPoint.Z,
            //    TraceAngularOffsetDeg);
        }

        if (bResolvedCellId && HitCellId == CellId)
        {
            CurrentCellHit = &Hit;
            break;
        }
    }

    const FHitResult* SelectedHit = CurrentCellHit ? CurrentCellHit : FirstHISMHit;
    if (!SelectedHit)
    {
        //if (bDebugP2_5HISMPieceHeightTrace)
        //{
        //    UE_LOG(LogPlanetTess, Warning,
        //        TEXT("[Tess][P2.5/P2.6] Cell=%d NoHISMHit TotalHits=%d HISMHits=%d StartRadius=%.1f EndRadius=%.1f TraceOffsetDeg=%.2f"),
        //        CellId,
        //        Hits.Num(),
        //        HISMHitCount,
        //        FVector::Distance(TraceStart, PlanetCenterWorld),
        //        FVector::Distance(TraceEnd, PlanetCenterWorld),
        //        TraceAngularOffsetDeg);
        //}
        return false;
    }

    const float ImpactRadius = FVector::Distance(SelectedHit->ImpactPoint, PlanetCenterWorld);
    const float FinalRadius = ImpactRadius + FMath::Max(P1PieceRadiusOffsetCM, 0.0f);
    OutWorldPosition = PlanetCenterWorld + PlacementDirection * FinalRadius;
    if (bDebugP2_5HISMPieceHeightTrace)
    {
        //UE_LOG(LogPlanetTess, Warning,
        //    TEXT("[Tess][P2.5/P2.6] Cell=%d UseHit Mode=%s Component=%s Item=%d ImpactRadius=%.1f FinalRadius=%.1f PieceOffset=%.1f TotalHits=%d HISMHits=%d TraceOffsetDeg=%.2f"),
        //    CellId,
        //    CurrentCellHit ? TEXT("CurrentCell") : TEXT("FirstHISMFallback"),
        //    *GetNameSafe(SelectedHit->GetComponent()),
        //    SelectedHit->Item,
        //    ImpactRadius,
        //    FinalRadius,
        //    P1PieceRadiusOffsetCM,
        //    Hits.Num(),
        //    HISMHitCount,
        //    TraceAngularOffsetDeg);
    }
    return true;
}

bool APlanetTessellatedMesh::BuildP1PieceWorldTransformForPiece_(
    const FTerraGameplayPieceState& Piece,
    const TArray<FTerraGameplayPieceState>& Pieces,
    FTransform& OutWorldTransform) const
{
    if (!BuildP1PieceWorldTransform_(Piece.CellId, Piece.PieceType, OutWorldTransform))
    {
        return false;
    }

    if (Piece.PieceType == ETerraGameplayPieceType::Commander
        || Piece.OwnerFactionId == INDEX_NONE
        || Piece.CellId == INDEX_NONE
        || !CellTopology.IsValid()
        || !CellTopology->Cells.IsValidIndex(Piece.CellId))
    {
        return true;
    }

    const FTerraGameplayPieceState* CommanderPiece = nullptr;
    for (const FTerraGameplayPieceState& CandidatePiece : Pieces)
    {
        if (CandidatePiece.bAlive
            && CandidatePiece.OwnerFactionId == Piece.OwnerFactionId
            && CandidatePiece.PieceType == ETerraGameplayPieceType::Commander)
        {
            CommanderPiece = &CandidatePiece;
            break;
        }
    }

    if (!CommanderPiece
        || CommanderPiece->CellId == INDEX_NONE
        || CommanderPiece->CellId == Piece.CellId
        || !CellTopology->Cells.IsValidIndex(CommanderPiece->CellId))
    {
        return true;
    }

    const FTransform ActorTransform = GetActorTransform();
    const FVector LocalUp = CellTopology->Cells[Piece.CellId].UnitCenter.GetSafeNormal();
    const FVector CommanderLocalDir = CellTopology->Cells[CommanderPiece->CellId].UnitCenter.GetSafeNormal();
    const FVector PieceLocalDir = CellTopology->Cells[Piece.CellId].UnitCenter.GetSafeNormal();
    FVector LocalForward = FVector::VectorPlaneProject(PieceLocalDir - CommanderLocalDir, LocalUp).GetSafeNormal();
    if (LocalForward.IsNearlyZero())
    {
        return true;
    }

    const FVector WorldUp = ActorTransform.TransformVectorNoScale(LocalUp).GetSafeNormal();
    const FVector WorldForward = ActorTransform.TransformVectorNoScale(LocalForward).GetSafeNormal();
    if (WorldUp.IsNearlyZero() || WorldForward.IsNearlyZero())
    {
        return true;
    }

    OutWorldTransform.SetRotation(FRotationMatrix::MakeFromXZ(WorldForward, WorldUp).ToQuat());
    return true;
}

void APlanetTessellatedMesh::BuildP3CaptureEventsFromPendingEntries_(
    const TArray<FTerraGameplayCaptureEntry>& CaptureEntries,
    const TArray<FTerraGameplayPieceState>& PiecesBeforeResolution,
    TArray<FTerraPiecePresentationCaptureEvent>& OutCaptureEvents) const
{
    OutCaptureEvents.Reset();
    OutCaptureEvents.Reserve(CaptureEntries.Num());

    auto BuildParticipant = [this, &PiecesBeforeResolution](int32 PieceId, FTerraPiecePresentationCaptureParticipant& OutParticipant) -> bool
    {
        if (!PiecesBeforeResolution.IsValidIndex(PieceId))
        {
            return false;
        }

        const FTerraGameplayPieceState& Piece = PiecesBeforeResolution[PieceId];
        if (Piece.PieceId == INDEX_NONE || Piece.CellId == INDEX_NONE)
        {
            return false;
        }

        FTransform WorldTransform = FTransform::Identity;
        if (!BuildP1PieceWorldTransformForPiece_(Piece, PiecesBeforeResolution, WorldTransform))
        {
            return false;
        }

        OutParticipant.PieceId = Piece.PieceId;
        OutParticipant.CellId = Piece.CellId;
        OutParticipant.PieceType = Piece.PieceType;
        OutParticipant.WorldTransform = WorldTransform;
        return true;
    };

    for (const FTerraGameplayCaptureEntry& CaptureEntry : CaptureEntries)
    {
        FTerraPiecePresentationCaptureEvent CaptureEvent;
        const bool bHasCaptured = BuildParticipant(CaptureEntry.CapturedPieceId, CaptureEvent.Captured);
        const bool bHasAttacker = BuildParticipant(CaptureEntry.AttackerPieceId, CaptureEvent.Attacker);
        const bool bHasVanguard = BuildParticipant(CaptureEntry.VanguardPieceId, CaptureEvent.Vanguard);

        if (!bHasCaptured || (!bHasAttacker && !bHasVanguard))
        {
            UE_LOG(LogPlanetTess, Warning,
                TEXT("[Tess][P3] Skip capture presentation event. Captured=%d HasCaptured=%d Attacker=%d HasAttacker=%d Vanguard=%d HasVanguard=%d"),
                CaptureEntry.CapturedPieceId,
                bHasCaptured ? 1 : 0,
                CaptureEntry.AttackerPieceId,
                bHasAttacker ? 1 : 0,
                CaptureEntry.VanguardPieceId,
                bHasVanguard ? 1 : 0);
            continue;
        }

        OutCaptureEvents.Add(CaptureEvent);
    }
}

FTerraPieceVisualConfig APlanetTessellatedMesh::BuildP1PieceVisualConfig_() const
{
    FTerraPieceVisualConfig VisualConfig;
    VisualConfig.CommanderMesh = P1CommanderMesh.Get() ? P1CommanderMesh.Get() : LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Characters/Mage1.Mage1"));
    VisualConfig.InfantryMesh = P1InfantryMesh.Get() ? P1InfantryMesh.Get() : LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Characters/Knight1.Knight1"));
    VisualConfig.CavalryMesh = P1CavalryMesh.Get() ? P1CavalryMesh.Get() : LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Characters/Knight1.Knight1"));
    VisualConfig.ArcherMesh = P1ArcherMesh.Get() ? P1ArcherMesh.Get() : LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Characters/Ranger1.Ranger1"));
    VisualConfig.UniformScale = FMath::Max(P1PieceUniformScale, 0.001f);
    VisualConfig.MeshRelativeLocation = P1MeshRelativeLocation;
    VisualConfig.MeshRelativeRotation = P1MeshRelativeRotation;
    VisualConfig.HorseMesh = P4HorseMesh.Get() ? P4HorseMesh.Get() : LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Animations/Horse/Horse.Horse"));
    VisualConfig.HorseIdleAnimation = P4HorseIdleAnimation.Get() ? P4HorseIdleAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Horse/HorseIdle.HorseIdle"));
    VisualConfig.HorseMoveAnimation = P4HorseMoveAnimation.Get() ? P4HorseMoveAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Horse/HorseWalk.HorseWalk"));
    VisualConfig.HorseJumpAnimation = P4HorseJumpAnimation.Get() ? P4HorseJumpAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Horse/HorseGallop_Jump.HorseGallop_Jump"));
    VisualConfig.HorseJumpPlayRateScale = FMath::Max(P4HorseJumpPlayRateScale, 0.001f);
    VisualConfig.HorseDeathAnimation = P4HorseDeathAnimation.Get() ? P4HorseDeathAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Horse/HorseDeath.HorseDeath"));
    VisualConfig.RiderSittingAnimation = P4RiderSittingAnimation.Get() ? P4RiderSittingAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralSitting.Rig_Medium_GeneralSitting"));
    VisualConfig.RiderAnimInstanceClass = P4RiderAnimInstanceClass;
    VisualConfig.RiderUpperBodyAttackMontage = P4RiderUpperBodyAttackMontage;
    VisualConfig.RiderUpperBodyHitMontage = P4RiderUpperBodyHitMontage;
    VisualConfig.RiderSaddleAttachName = P4SaddleAttachName.IsNone() ? FName(TEXT("Torso")) : P4SaddleAttachName;
    VisualConfig.HorseRelativeLocation = P4HorseRelativeLocation;
    VisualConfig.HorseRelativeRotation = P4HorseRelativeRotation;
    VisualConfig.HorseUniformScale = FMath::Max(P4HorseUniformScale, 0.001f);
    VisualConfig.RiderRelativeLocation = P4RiderRelativeLocation;
    VisualConfig.RiderRelativeRotation = P4RiderRelativeRotation;
    VisualConfig.RiderUniformScale = FMath::Max(P4RiderUniformScale, 0.001f);
    VisualConfig.RiderDeathRelativeLocation = P4RiderDeathRelativeLocation;
    VisualConfig.RiderDeathRelativeRotation = P4RiderDeathRelativeRotation;
    VisualConfig.ArcherBowMesh = P5ArcherBowMesh.Get() ? P5ArcherBowMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/bow.bow"));
    VisualConfig.InfantrySwordMesh = P5InfantrySwordMesh.Get() ? P5InfantrySwordMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/sword_1handed.sword_1handed"));
    VisualConfig.InfantryShieldMesh = P5InfantryShieldMesh.Get() ? P5InfantryShieldMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/shield_round.shield_round"));
    VisualConfig.CavalryAxeMesh = P5CavalryAxeMesh.Get() ? P5CavalryAxeMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/axe_1handed.axe_1handed"));
    VisualConfig.ArcherBowAttachName = P5ArcherBowAttachName;
    VisualConfig.InfantrySwordAttachName = P5InfantrySwordAttachName;
    VisualConfig.InfantryShieldAttachName = P5InfantryShieldAttachName;
    VisualConfig.CavalryAxeAttachName = P5CavalryAxeAttachName;
    VisualConfig.ArcherBowRelativeLocation = P5ArcherBowRelativeLocation;
    VisualConfig.ArcherBowRelativeRotation = P5ArcherBowRelativeRotation;
    VisualConfig.ArcherBowUniformScale = FMath::Max(P5ArcherBowUniformScale, 0.001f);
    VisualConfig.InfantrySwordRelativeLocation = P5InfantrySwordRelativeLocation;
    VisualConfig.InfantrySwordRelativeRotation = P5InfantrySwordRelativeRotation;
    VisualConfig.InfantrySwordUniformScale = FMath::Max(P5InfantrySwordUniformScale, 0.001f);
    VisualConfig.InfantryShieldRelativeLocation = P5InfantryShieldRelativeLocation;
    VisualConfig.InfantryShieldRelativeRotation = P5InfantryShieldRelativeRotation;
    VisualConfig.InfantryShieldUniformScale = FMath::Max(P5InfantryShieldUniformScale, 0.001f);
    VisualConfig.CavalryAxeRelativeLocation = P5CavalryAxeRelativeLocation;
    VisualConfig.CavalryAxeRelativeRotation = P5CavalryAxeRelativeRotation;
    VisualConfig.CavalryAxeUniformScale = FMath::Max(P5CavalryAxeUniformScale, 0.001f);
    VisualConfig.P6ArrowProjectileMesh = P6ArrowProjectileMesh.Get() ? P6ArrowProjectileMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/arrow_bow.arrow_bow"));
    UClass* DefaultP6SpellProjectileClass = P6SpellProjectileActorClass.Get();
    if (!DefaultP6SpellProjectileClass)
    {
        DefaultP6SpellProjectileClass = LoadClass<AActor>(nullptr, TEXT("/Game/FXVarietyPack/Blueprints/BP_ky_fireBall.BP_ky_fireBall_C"));
    }
    VisualConfig.P6SpellProjectileActorClass = DefaultP6SpellProjectileClass;
    VisualConfig.P6ArrowAttachName = P6ArrowAttachName;
    VisualConfig.P6SpellAttachName = P6SpellAttachName;
    VisualConfig.P6ArrowRelativeLocation = P6ArrowRelativeLocation;
    VisualConfig.P6ArrowRelativeRotation = P6ArrowRelativeRotation;
    VisualConfig.P6ArrowUniformScale = FMath::Max(P6ArrowUniformScale, 0.001f);
    VisualConfig.P6ArrowTargetRelativeLocation = P6ArrowTargetRelativeLocation;
    VisualConfig.P6SpellRelativeLocation = P6SpellRelativeLocation;
    VisualConfig.P6SpellRelativeRotation = P6SpellRelativeRotation;
    VisualConfig.P6SpellUniformScale = FMath::Max(P6SpellUniformScale, 0.001f);
    VisualConfig.P6ArrowReleaseDelaySeconds = FMath::Max(P6ArrowReleaseDelaySeconds, 0.0f);
    VisualConfig.P6SpellReleaseDelaySeconds = FMath::Max(P6SpellReleaseDelaySeconds, 0.0f);
    VisualConfig.P6ArrowFlightSeconds = FMath::Max(P6ArrowFlightSeconds, 0.001f);
    VisualConfig.P6SpellFlightSeconds = FMath::Max(P6SpellFlightSeconds, 0.001f);
    VisualConfig.P6ArrowArcHeightCM = FMath::Max(P6ArrowArcHeightCM, 0.0f);
    VisualConfig.P7PaletteReplaceMaterial = P7PaletteReplaceMaterial.Get()
        ? P7PaletteReplaceMaterial.Get()
        : LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/PiecePresentation/Materials/M_TerraPiece_PaletteReplace.M_TerraPiece_PaletteReplace"));
    VisualConfig.P7CommanderBaseTexture = P7CommanderBaseTexture.Get()
        ? P7CommanderBaseTexture.Get()
        : LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/mage_texture.mage_texture"));
    VisualConfig.P7InfantryBaseTexture = P7InfantryBaseTexture.Get()
        ? P7InfantryBaseTexture.Get()
        : LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/knight_texture.knight_texture"));
    VisualConfig.P7CavalryRiderBaseTexture = P7CavalryRiderBaseTexture.Get()
        ? P7CavalryRiderBaseTexture.Get()
        : LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/knight_texture.knight_texture"));
    VisualConfig.P7ArcherBaseTexture = P7ArcherBaseTexture.Get()
        ? P7ArcherBaseTexture.Get()
        : LoadObject<UTexture2D>(nullptr, TEXT("/Game/Animations/Adventurers/Assets/ranger_texture.ranger_texture"));
    VisualConfig.P7FactionPalettes = P7FactionPalettes;
    if (VisualConfig.P7FactionPalettes.Num() == 0)
    {
        BuildDefaultP7FactionPalettes(VisualConfig.P7FactionPalettes);
    }
    VisualConfig.P7PaletteMasksByPieceType = P7PaletteMasksByPieceType;
    if (VisualConfig.P7PaletteMasksByPieceType.Num() == 0)
    {
        BuildDefaultP7PaletteMasks(VisualConfig.P7PaletteMasksByPieceType);
    }
    VisualConfig.P35CommanderAttackDurationSeconds = FMath::Max(P35CommanderAttackDurationSeconds, 0.001f);
    VisualConfig.P35CommanderAttackPlayRateScale = FMath::Max(P35CommanderAttackPlayRateScale, 0.001f);
    VisualConfig.P35ArcherAttackDurationSeconds = FMath::Max(P35ArcherAttackDurationSeconds, 0.001f);
    VisualConfig.P35ArcherAttackPlayRateScale = FMath::Max(P35ArcherAttackPlayRateScale, 0.001f);
    VisualConfig.IdleAnimation = P2IdleAnimation.Get() ? P2IdleAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralIdle_A.Rig_Medium_GeneralIdle_A"));
    VisualConfig.MoveAnimation = P2MoveAnimation.Get() ? P2MoveAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_MovementBasicWalking_A.Rig_Medium_MovementBasicWalking_A"));
    VisualConfig.JumpAnimation = P2JumpAnimation.Get() ? P2JumpAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_MovementBasicJump_Full_Short.Rig_Medium_MovementBasicJump_Full_Short"));
    VisualConfig.CommanderMagicAttackAnimation = P3CommanderMagicAttackAnimation.Get() ? P3CommanderMagicAttackAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralMagic_Spell_Casting.Rig_Medium_GeneralMagic_Spell_Casting"));
    VisualConfig.ArcherRangedAttackAnimation = P3ArcherRangedAttackAnimation.Get() ? P3ArcherRangedAttackAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralShooting_Arrow.Rig_Medium_GeneralShooting_Arrow"));
    VisualConfig.InfantryMeleeAttackAnimation = P3InfantryMeleeAttackAnimation.Get() ? P3InfantryMeleeAttackAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralSword_And_Shield_Slash.Rig_Medium_GeneralSword_And_Shield_Slash"));
    VisualConfig.CavalryMeleeAttackAnimation = P3CavalryMeleeAttackAnimation.Get() ? P3CavalryMeleeAttackAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralUpward_Thrust.Rig_Medium_GeneralUpward_Thrust"));
    VisualConfig.HitAnimation = P3HitAnimation.Get() ? P3HitAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralHit_A.Rig_Medium_GeneralHit_A"));
    VisualConfig.DeathAnimation = P3DeathAnimation.Get() ? P3DeathAnimation.Get() : LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Animations/Adventurers/Animations/Rig_Medium_GeneralDeath_A.Rig_Medium_GeneralDeath_A"));
    VisualConfig.MoveDurationSeconds = FMath::Max(P2MoveDurationSeconds, 0.001f);
    VisualConfig.JumpDurationSeconds = FMath::Max(P2JumpDurationSeconds, 0.001f);
    VisualConfig.JumpHeightCM = FMath::Max(P2JumpHeightCM, 0.0f);
    VisualConfig.P3FacingBlendSeconds = FMath::Max(P3FacingBlendSeconds, 0.0f);
    VisualConfig.P3MeleeRunInSeconds = FMath::Max(P3MeleeRunInSeconds, 0.001f);
    VisualConfig.P3MeleeReturnSeconds = FMath::Max(P3MeleeReturnSeconds, 0.001f);
    VisualConfig.P3AttackAnimationStartOffsetSeconds = FMath::Max(P3AttackAnimationStartOffsetSeconds, 0.0f);
    VisualConfig.P3CommanderAttackToHitSeconds = FMath::Max(P3CommanderAttackToHitSeconds, 0.0f);
    VisualConfig.P3ArcherAttackToHitSeconds = FMath::Max(P3ArcherAttackToHitSeconds, 0.0f);
    VisualConfig.P3InfantryAttackToHitSeconds = FMath::Max(P3InfantryAttackToHitSeconds, 0.0f);
    VisualConfig.P3CavalryAttackToHitSeconds = FMath::Max(P3CavalryAttackToHitSeconds, 0.0f);
    VisualConfig.P3HitReactDelaySeconds = FMath::Max(P3HitReactDelaySeconds, 0.0f);
    VisualConfig.P3HitAnimationStartOffsetSeconds = FMath::Max(P3HitAnimationStartOffsetSeconds, 0.0f);
    VisualConfig.P3DeathAfterHitDelaySeconds = FMath::Max(P3DeathAfterHitDelaySeconds, 0.0f);
    VisualConfig.P3DeathAnimationStartOffsetSeconds = FMath::Max(P3DeathAnimationStartOffsetSeconds, 0.0f);
    VisualConfig.P3CapturedFadeSeconds = FMath::Max(P3CapturedFadeSeconds, 0.0f);
    return VisualConfig;
}

FVector APlanetTessellatedMesh::GetPlanetCenterWorld_() const
{
    return GetActorTransform().TransformPosition(FVector::ZeroVector);
}

bool APlanetTessellatedMesh::SyncOrbitCameraStateFromWorldPosition(const FVector& CameraWorldPosition, float& InOutLongitudeDeg, float& InOutLatitudeDeg, float& InOutHeightOffsetCM) const
{
    const FVector PlanetCenterWorld = GetPlanetCenterWorld_();
    FVector LocalOffset = GetActorTransform().InverseTransformPosition(CameraWorldPosition);
    const float DistanceFromCenter = LocalOffset.Length();
    if (DistanceFromCenter <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const FVector UnitDir = LocalOffset / DistanceFromCenter;
    const float LatitudeRad = FMath::Asin(FMath::Clamp(static_cast<float>(UnitDir.Z), -1.0f, 1.0f));
    const float LongitudeRad = FMath::Atan2(static_cast<float>(UnitDir.Y), static_cast<float>(UnitDir.X));

    InOutLatitudeDeg = FMath::RadiansToDegrees(LatitudeRad);
    InOutLongitudeDeg = FMath::RadiansToDegrees(LongitudeRad);
    InOutHeightOffsetCM = DistanceFromCenter - GlobeRadiusCM;
    return !PlanetCenterWorld.ContainsNaN();
}

bool APlanetTessellatedMesh::ApplyOrbitCameraState(float LongitudeDeg, float LatitudeDeg, float HeightOffsetCM)
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld() || !bEnableG8ManualCameraControl)
    {
        return false;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
    if (!PlayerController)
    {
        return false;
    }

    const float LongitudeRad = FMath::DegreesToRadians(LongitudeDeg);
    const float LatitudeRad = FMath::DegreesToRadians(LatitudeDeg);
    const float Radius = GlobeRadiusCM + HeightOffsetCM;

    const FVector LocalUnitDir(
        FMath::Cos(LatitudeRad) * FMath::Cos(LongitudeRad),
        FMath::Cos(LatitudeRad) * FMath::Sin(LongitudeRad),
        FMath::Sin(LatitudeRad));
    const FVector CameraWorldPosition = GetActorTransform().TransformPosition(LocalUnitDir * Radius);
    const FVector PlanetCenterWorld = GetPlanetCenterWorld_();
    const FVector LookDirection = PlanetCenterWorld - CameraWorldPosition;
    if (LookDirection.IsNearlyZero())
    {
        return false;
    }

    const FRotator LookRotation = LookDirection.Rotation();
    if (AActor* ViewTarget = PlayerController->GetViewTarget())
    {
        ViewTarget->SetActorLocation(CameraWorldPosition);
        ViewTarget->SetActorRotation(LookRotation);
    }
    PlayerController->SetControlRotation(LookRotation);
    return true;
}

bool APlanetTessellatedMesh::SyncFocusCameraStateFromView(
    const FVector& CameraWorldPosition,
    const FRotator& CameraWorldRotation,
    FVector& OutFocusUnitDir,
    float& OutDistanceToFocusCM,
    float& OutTiltDeg,
    float& OutYawAroundFocusDeg) const
{
    const FTransform ActorTransform = GetActorTransform();
    const FVector LocalCameraPosition = ActorTransform.InverseTransformPosition(CameraWorldPosition);
    const FVector LocalCameraForward = ActorTransform.InverseTransformVectorNoScale(CameraWorldRotation.Vector()).GetSafeNormal();
    if (LocalCameraForward.IsNearlyZero())
    {
        return false;
    }

    const float Radius = FMath::Max(1.0f, GlobeRadiusCM);
    const float B = FVector::DotProduct(LocalCameraPosition, LocalCameraForward);
    const float C = FVector::DotProduct(LocalCameraPosition, LocalCameraPosition) - Radius * Radius;
    const float Discriminant = B * B - C;

    FVector LocalFocusPoint = FVector::ZeroVector;
    if (Discriminant >= 0.0f)
    {
        const float SqrtDiscriminant = FMath::Sqrt(Discriminant);
        const float T0 = -B - SqrtDiscriminant;
        const float T1 = -B + SqrtDiscriminant;
        const float T = T0 >= 0.0f ? T0 : T1;
        if (T >= 0.0f)
        {
            LocalFocusPoint = LocalCameraPosition + LocalCameraForward * T;
        }
    }

    if (LocalFocusPoint.IsNearlyZero())
    {
        const FVector LocalCameraDir = LocalCameraPosition.GetSafeNormal();
        if (LocalCameraDir.IsNearlyZero())
        {
            return false;
        }
        LocalFocusPoint = LocalCameraDir * Radius;
    }

    const FVector LocalFocusDir = LocalFocusPoint.GetSafeNormal();
    if (LocalFocusDir.IsNearlyZero())
    {
        return false;
    }

    const FVector FocusWorldPosition = ActorTransform.TransformPosition(LocalFocusDir * Radius);
    const FVector WorldUp = ActorTransform.TransformVectorNoScale(LocalFocusDir).GetSafeNormal();
    const FVector WorldForward = CameraWorldRotation.Vector().GetSafeNormal();
    if (WorldUp.IsNearlyZero() || WorldForward.IsNearlyZero())
    {
        return false;
    }

    OutFocusUnitDir = LocalFocusDir;
    OutDistanceToFocusCM = FMath::Max(1.0f, FVector::Distance(CameraWorldPosition, FocusWorldPosition));

    const float DownDotNormal = FVector::DotProduct(-WorldForward, WorldUp);
    OutTiltDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(DownDotNormal, -1.0f, 1.0f)));
    OutTiltDeg = FMath::Clamp(OutTiltDeg, 5.0f, 85.0f);

    // 在焦点局部基 (East, North) 下反解 Yaw。
    // 与 ApplyFocusCameraState / OffsetFocusCameraStateOnTangent 保持同一约定：
    //     ForwardHint = cos(Yaw) * North + sin(Yaw) * East
    const FVector LocalWorldUp = ActorTransform.InverseTransformVectorNoScale(WorldUp).GetSafeNormal();
    FVector LocalEast = FVector::CrossProduct(FVector::UpVector, LocalWorldUp).GetSafeNormal();
    if (LocalEast.IsNearlyZero())
    {
        LocalEast = FVector::CrossProduct(FVector::RightVector, LocalWorldUp).GetSafeNormal();
    }
    if (LocalEast.IsNearlyZero())
    {
        return false;
    }
    const FVector LocalNorth = FVector::CrossProduct(LocalWorldUp, LocalEast).GetSafeNormal();
    if (LocalNorth.IsNearlyZero())
    {
        return false;
    }

    const FVector LocalCameraToFocus = (LocalFocusDir * Radius - LocalCameraPosition).GetSafeNormal();
    const FVector LocalForwardTangent = FVector::VectorPlaneProject(LocalCameraToFocus, LocalFocusDir).GetSafeNormal();
    if (!LocalForwardTangent.IsNearlyZero())
    {
        const float YawRad = FMath::Atan2(
            static_cast<float>(FVector::DotProduct(LocalForwardTangent, LocalEast)),
            static_cast<float>(FVector::DotProduct(LocalForwardTangent, LocalNorth)));
        OutYawAroundFocusDeg = FRotator::NormalizeAxis(FMath::RadiansToDegrees(YawRad));
    }
    else
    {
        OutYawAroundFocusDeg = 0.0f;
    }

    return true;
}

bool APlanetTessellatedMesh::ApplyFocusCameraState(
    const FVector& FocusUnitDir,
    float DistanceToFocusCM,
    float TiltDeg,
    float YawAroundFocusDeg)
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld() || !bEnableG8ManualCameraControl)
    {
        return false;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
    if (!PlayerController)
    {
        return false;
    }

    const FVector LocalFocusDir = FocusUnitDir.GetSafeNormal();
    if (LocalFocusDir.IsNearlyZero())
    {
        return false;
    }

    // 极区退化补救：|FocusDir × WorldZ| 太小时，改用 WorldY 作辅助基。详见设计稿 §3。
    FVector LocalEast = FVector::CrossProduct(FVector::UpVector, LocalFocusDir).GetSafeNormal();
    if (LocalEast.IsNearlyZero())
    {
        LocalEast = FVector::CrossProduct(FVector::RightVector, LocalFocusDir).GetSafeNormal();
    }
    if (LocalEast.IsNearlyZero())
    {
        return false;
    }

    const FVector LocalNorth = FVector::CrossProduct(LocalFocusDir, LocalEast).GetSafeNormal();
    if (LocalNorth.IsNearlyZero())
    {
        return false;
    }

    const float YawRad = FMath::DegreesToRadians(FRotator::NormalizeAxis(YawAroundFocusDeg));
    const FVector LocalForwardHint = (LocalNorth * FMath::Cos(YawRad) + LocalEast * FMath::Sin(YawRad)).GetSafeNormal();
    if (LocalForwardHint.IsNearlyZero())
    {
        return false;
    }

    const float Distance = FMath::Clamp(
        DistanceToFocusCM,
        G8CameraMinHeightOffsetCM,
        FMath::Max(G8CameraMinHeightOffsetCM, G8CameraMaxHeightOffsetCM));
    const float TiltRad = FMath::DegreesToRadians(FMath::Clamp(TiltDeg, 5.0f, 85.0f));
    const float HorizontalDistance = Distance * FMath::Cos(TiltRad);
    const float VerticalDistance = Distance * FMath::Sin(TiltRad);

    const FTransform ActorTransform = GetActorTransform();
    const FVector FocusWorldPosition = ActorTransform.TransformPosition(LocalFocusDir * GlobeRadiusCM);
    const FVector WorldUp = ActorTransform.TransformVectorNoScale(LocalFocusDir).GetSafeNormal();
    const FVector WorldForwardHint = ActorTransform.TransformVectorNoScale(LocalForwardHint).GetSafeNormal();
    if (WorldUp.IsNearlyZero() || WorldForwardHint.IsNearlyZero())
    {
        return false;
    }

    const FVector CameraWorldPosition = FocusWorldPosition - WorldForwardHint * HorizontalDistance + WorldUp * VerticalDistance;
    const FVector CameraForward = (FocusWorldPosition - CameraWorldPosition).GetSafeNormal();
    if (CameraForward.IsNearlyZero())
    {
        return false;
    }

    const FRotator LookRotation = FRotationMatrix::MakeFromXZ(CameraForward, WorldUp).Rotator();
    if (AActor* ViewTarget = PlayerController->GetViewTarget())
    {
        ViewTarget->SetActorLocation(CameraWorldPosition);
        ViewTarget->SetActorRotation(LookRotation);
    }
    PlayerController->SetControlRotation(LookRotation);
    return true;
}

bool APlanetTessellatedMesh::OffsetFocusCameraStateOnTangent(
    float RightDeltaDeg,
    float ForwardDeltaDeg,
    FVector& InOutFocusUnitDir,
    float& InOutYawAroundFocusDeg) const
{
    // 输入弧度增量。
    const float RightDeltaRad = FMath::DegreesToRadians(RightDeltaDeg);
    const float ForwardDeltaRad = FMath::DegreesToRadians(ForwardDeltaDeg);
    const float DeltaAngleRadSq = RightDeltaRad * RightDeltaRad + ForwardDeltaRad * ForwardDeltaRad;
    if (DeltaAngleRadSq <= KINDA_SMALL_NUMBER * KINDA_SMALL_NUMBER)
    {
        return true; // 本帧无输入，什么都不做。
    }

    const FVector FocusUnitDir = InOutFocusUnitDir.GetSafeNormal();
    if (FocusUnitDir.IsNearlyZero())
    {
        return false;
    }

    // 构造当前焦点的局部切平面基 (East, North)。
    // 极区退化补救：|FocusDir × WorldZ| 太小时改用 WorldY，见设计稿 §3。
    FVector LocalEast = FVector::CrossProduct(FVector::UpVector, FocusUnitDir).GetSafeNormal();
    if (LocalEast.IsNearlyZero())
    {
        LocalEast = FVector::CrossProduct(FVector::RightVector, FocusUnitDir).GetSafeNormal();
    }
    if (LocalEast.IsNearlyZero())
    {
        return false;
    }
    const FVector LocalNorth = FVector::CrossProduct(FocusUnitDir, LocalEast).GetSafeNormal();
    if (LocalNorth.IsNearlyZero())
    {
        return false;
    }

    // 由当前 Yaw 组当前 ForwardTangent 与 RightTangent。
    // 与 Apply/Sync 完全一致的约定：ForwardTangent = cos(Yaw)·North + sin(Yaw)·East。
    const float YawRad = FMath::DegreesToRadians(FRotator::NormalizeAxis(InOutYawAroundFocusDeg));
    const FVector LocalForwardTangent = (LocalNorth * FMath::Cos(YawRad) + LocalEast * FMath::Sin(YawRad)).GetSafeNormal();
    if (LocalForwardTangent.IsNearlyZero())
    {
        return false;
    }
    // 屏幕右方向切向：Cross(FocusDir, ForwardTangent)。
    //   在 Yaw=0 的赤道场景里手工推：Up=(1,0,0), North=(0,0,1), 得
    //     LocalRightTangent = Cross((1,0,0),(0,0,1)) = (0,-1,0) = -East
    //   同时 Apply 里 MakeFromXZ(CameraForward=+North, WorldUp=+Up) 摆出的相机
    //   Actor Y（左手系里的 RightVector = 屏幕右方向）也是 -East。二者一致。
    //   因此 "D 键 = +RightDeltaDeg = 焦点沿 +LocalRightTangent = -East 走 = 屏幕右滑"
    //   等价于用户口中的 "D = 视角向右走"。
    const FVector LocalRightTangent = FVector::CrossProduct(FocusUnitDir, LocalForwardTangent).GetSafeNormal();
    if (LocalRightTangent.IsNearlyZero())
    {
        return false;
    }

    // 组本帧的瞬时移动切向 T 与旋转弧长 θ。
    const FVector LocalTangentDelta = LocalRightTangent * RightDeltaRad + LocalForwardTangent * ForwardDeltaRad;
    const float DeltaAngleRad = LocalTangentDelta.Length();
    if (DeltaAngleRad <= KINDA_SMALL_NUMBER)
    {
        return true;
    }
    const FVector LocalMoveTangent = LocalTangentDelta / DeltaAngleRad;

    // 沿测地线推进焦点。等价于绕轴 A = Cross(FocusDir, T) 旋转 θ 弧度。
    // 因为 |FocusDir| = 1、|T| = 1、FocusDir ⊥ T，故 |A| = 1，可直接作为 Rodrigues 旋转轴。
    const FVector RotationAxis = FVector::CrossProduct(FocusUnitDir, LocalMoveTangent).GetSafeNormal();
    if (RotationAxis.IsNearlyZero())
    {
        return false;
    }

    const float CosTheta = FMath::Cos(DeltaAngleRad);
    const float SinTheta = FMath::Sin(DeltaAngleRad);

    auto Rodrigues = [&](const FVector& V) -> FVector
    {
        // R(k, θ)·v = v·cosθ + (k×v)·sinθ + k·(k·v)·(1 - cosθ)
        const FVector KCrossV = FVector::CrossProduct(RotationAxis, V);
        const float KDotV = FVector::DotProduct(RotationAxis, V);
        return V * CosTheta + KCrossV * SinTheta + RotationAxis * (KDotV * (1.0f - CosTheta));
    };

    const FVector NewFocusUnitDir = Rodrigues(FocusUnitDir).GetSafeNormal();
    if (NewFocusUnitDir.IsNearlyZero())
    {
        return false;
    }

    // 把 ForwardTangent 沿同一根旋转轴平行运输到新焦点。
    const FVector NewForwardTangent = Rodrigues(LocalForwardTangent).GetSafeNormal();
    if (NewForwardTangent.IsNearlyZero())
    {
        InOutFocusUnitDir = NewFocusUnitDir;
        return true; // 焦点更新成功，Yaw 保持原值。
    }

    // 在新焦点的局部基下反解新 Yaw。
    FVector NewLocalEast = FVector::CrossProduct(FVector::UpVector, NewFocusUnitDir).GetSafeNormal();
    if (NewLocalEast.IsNearlyZero())
    {
        NewLocalEast = FVector::CrossProduct(FVector::RightVector, NewFocusUnitDir).GetSafeNormal();
    }
    if (NewLocalEast.IsNearlyZero())
    {
        InOutFocusUnitDir = NewFocusUnitDir;
        return true;
    }
    const FVector NewLocalNorth = FVector::CrossProduct(NewFocusUnitDir, NewLocalEast).GetSafeNormal();
    if (NewLocalNorth.IsNearlyZero())
    {
        InOutFocusUnitDir = NewFocusUnitDir;
        return true;
    }

    // NewForwardTangent 理论上已在新切平面内（因为整个 Rodrigues 保持标架正交），
    // 但受浮点误差影响，先投影到新切平面再反解 atan2 更稳定。
    const FVector ProjectedForward = FVector::VectorPlaneProject(NewForwardTangent, NewFocusUnitDir).GetSafeNormal();
    if (!ProjectedForward.IsNearlyZero())
    {
        const float NewYawRad = FMath::Atan2(
            static_cast<float>(FVector::DotProduct(ProjectedForward, NewLocalEast)),
            static_cast<float>(FVector::DotProduct(ProjectedForward, NewLocalNorth)));
        InOutYawAroundFocusDeg = FRotator::NormalizeAxis(FMath::RadiansToDegrees(NewYawRad));
    }

    InOutFocusUnitDir = NewFocusUnitDir;
    return true;
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
        G2_5LastCameraFocusedTurnIndex = NewTurnIndex;
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
        if (GameplayContainer->TryGetPieceCellId(NewSelectedPieceId, SelectedPieceCellId))
        {
            FocusCameraOnSelectedCellSmart_(SelectedPieceCellId);
        }
    }

    RebuildG1DebugPieces_();
    RequestC6ActionCameraTrackingForMoveEvents_(P2MoveEvents);
    SyncP1PiecePresentation_(P2MoveEvents, P3CaptureEvents);

    if (bTurnChanged
        && !TryRequestC6_5DelayedTurnStartFocus_(
            NewTurnIndex,
            NewFactionId,
            P2MoveEvents,
            P3CaptureEvents))
    {
        UE_LOG(LogPlanetTess, Log,
            TEXT("[Tess][C6.5] Falling back to immediate turn focus. Faction=%d Turn=%d FactionChanged=%d P2MoveEvents=%d P3CaptureEvents=%d"),
            NewFactionId,
            NewTurnIndex,
            bFactionChanged ? 1 : 0,
            P2MoveEvents.Num(),
            P3CaptureEvents.Num());
        FocusCameraOnCurrentFactionBase_();
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
    RequestC6ActionCameraTrackingForMoveEvents_(UndoMoveEvents);
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
    if (bHideG1DebugPiecesWhenP1IsActive && bEnableP1PiecePresentation && World && World->IsGameWorld())
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
