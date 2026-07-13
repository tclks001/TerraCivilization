// Fill out your copyright notice in the Description page of Project Settings.

#include "Render/PlanetTessellatedMesh.h"
#include "Render/PlanetCameraComponent.h"
#include "Render/PlanetGameplayComponent.h"
#include "Render/PlanetHISMInteractionComponent.h"
#include "Render/PlanetPiecePresentationComponent.h"
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
    PlanetGameplayComponent = CreateDefaultSubobject<UPlanetGameplayComponent>(TEXT("PlanetGameplayComponent"));
    PlanetHISMInteractionComponent = CreateDefaultSubobject<UPlanetHISMInteractionComponent>(TEXT("PlanetHISMInteractionComponent"));
    PlanetPiecePresentationComponent = CreateDefaultSubobject<UPlanetPiecePresentationComponent>(TEXT("PlanetPiecePresentationComponent"));

    HISMTileRenderer.Initialize(PlainTileHISMComp, ForestTileHISMComp, MountainTileHISMComp);
    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->PrepareHighlightComponents();
    }

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
    if (PlanetGameplayComponent && PlanetGameplayComponent->GetGameplayContainer())
    {
        FTerraNpcMcpGameplayBridge::UnregisterExecuteValidatedActionDelegate();
        FTerraNpcMcpGameplayBridge::UnregisterGameplayContainer(PlanetGameplayComponent->GetGameplayContainer());
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

    // Blueprint SCS previews construct temporary actors while PropertyEditor builds
    // the component tree. Do not populate runtime HISM/Gameplay state in that path.
    const UWorld* World = GetWorld();
    if (IsTemplate() || (World && World->WorldType == EWorldType::EditorPreview))
    {
        return;
    }

    RebuildAll_();
}

void APlanetTessellatedMesh::BeginPlay()
{
    Super::BeginPlay();

    const UWorld* World = GetWorld();
    if (World && World->IsGameWorld()
        && (!CellTopology.IsValid()
            || !Generator.IsValid()
            || !PlanetGameplayComponent
            || !PlanetGameplayComponent->GetGameplayContainer()
            || !PlanetGameplayComponent->GetGameplayContainer()->IsInitialized()))
    {
        RebuildAll_();
    }
}

void APlanetTessellatedMesh::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->TickHoverFade(DeltaSeconds);
    }

    if (PlanetCameraComponent)
    {
        PlanetCameraComponent->TickCamera(DeltaSeconds);
    }

    if (PlanetGameplayComponent)
    {
        PlanetGameplayComponent->DrawG1DebugPieces();
    }
}

void APlanetTessellatedMesh::Rebuild()
{
    RebuildAll_();
}

#if WITH_EDITOR
bool APlanetTessellatedMesh::ShouldTickIfViewportsOnly() const
{
    return PlanetGameplayComponent ? PlanetGameplayComponent->bEnableG1DebugPieces : false;
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
    if (PlanetGameplayComponent)
    {
        PlanetGameplayComponent->RebuildGameplay();
        PlanetGameplayComponent->RebuildG1DebugPieces();
    }
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
    Config.bEnableInstanceHighlight = PlanetHISMInteractionComponent
        ? PlanetHISMInteractionComponent->bEnableHISMInstanceHighlight
        : false;
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
    return PlanetHISMInteractionComponent
        ? PlanetHISMInteractionComponent->BuildHighlightConfig()
        : FPlanetHISMHighlightConfig();
}

void APlanetTessellatedMesh::RebuildHISMTileInstances_()
{
    if (!CellTopology.IsValid() || !Generator.IsValid())
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess] Skip HISM spherical tiles: CellTopology or WorldGen is not ready."));
        return;
    }

    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->PrepareHighlightComponents();
    }
    HISMTileRenderer.RebuildInstances(
        *CellTopology,
        Generator->GetCellData(),
        BuildHISMTileRenderConfig_());
}

void APlanetTessellatedMesh::WriteHISMHighlightForCell_(int32 CellId, bool bMarkRenderStateDirty)
{
    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->WriteHISMHighlightForCell(CellId, bMarkRenderStateDirty);
    }
}

void APlanetTessellatedMesh::RefreshG4CapturePreviewCellsForActionTarget_(int32 ActionTargetCellId, bool bMarkLastRenderStateDirty)
{
    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->RefreshCapturePreviewCellsForActionTarget(ActionTargetCellId, bMarkLastRenderStateDirty);
    }
}

void APlanetTessellatedMesh::UpdateHISMHoverCell_(int32 NewCellId)
{
    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->UpdateHISMHoverCell(NewCellId);
    }
}

void APlanetTessellatedMesh::RebuildGameplay_()
{
    if (PlanetGameplayComponent)
    {
        PlanetGameplayComponent->RebuildGameplay();
    }
}

void APlanetTessellatedMesh::RefreshGameplayHighlights_(const TArray<int32>& DirtyCellIds)
{
    if (PlanetGameplayComponent)
    {
        PlanetGameplayComponent->RefreshGameplayHighlights(DirtyCellIds);
    }
}

void APlanetTessellatedMesh::RefreshFactionPieceHighlights_(int32 FactionId)
{
    if (PlanetGameplayComponent)
    {
        PlanetGameplayComponent->RefreshFactionPieceHighlights(FactionId);
    }
}

void APlanetTessellatedMesh::RefreshCurrentFactionPieceHighlights_()
{
    if (PlanetGameplayComponent)
    {
        PlanetGameplayComponent->RefreshCurrentFactionPieceHighlights();
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
    return PlanetHISMInteractionComponent
        ? PlanetHISMInteractionComponent->TryResolveHISMHitToCellId(Hit, OutCellId)
        : false;
}

bool APlanetTessellatedMesh::HandleHISMHoverHit(const FHitResult& Hit)
{
    return PlanetHISMInteractionComponent
        ? PlanetHISMInteractionComponent->HandleHISMHoverHit(Hit)
        : false;
}

bool APlanetTessellatedMesh::HandleHISMClickHit(const FHitResult& Hit)
{
    return PlanetHISMInteractionComponent
        ? PlanetHISMInteractionComponent->HandleHISMClickHit(Hit)
        : false;
}

int32 APlanetTessellatedMesh::GetLastHISMPickedCellId() const
{
    return PlanetHISMInteractionComponent
        ? PlanetHISMInteractionComponent->GetLastHISMPickedCellId()
        : INDEX_NONE;
}

int32 APlanetTessellatedMesh::GetLastHISMClickedCellId() const
{
    return PlanetHISMInteractionComponent
        ? PlanetHISMInteractionComponent->GetLastHISMClickedCellId()
        : INDEX_NONE;
}

bool APlanetTessellatedMesh::HandleC5NavigateCurrentFactionPiece(bool bReverse)
{
    return PlanetGameplayComponent
        ? PlanetGameplayComponent->HandleC5NavigateCurrentFactionPiece(bReverse)
        : false;
}

bool APlanetTessellatedMesh::HandleGameplayCellClick_(int32 CellId, const TCHAR* SourceLabel, int32 InstanceIndex, const FString& ComponentName)
{
    return PlanetGameplayComponent
        ? PlanetGameplayComponent->HandleGameplayCellClick(CellId, SourceLabel, InstanceIndex, ComponentName)
        : true;
}

bool APlanetTessellatedMesh::TryExecuteNpcMcpValidatedAction_(int32 ExpectedTurnIndex, int32 ExpectedFactionId, int32 PieceId, int32 ToCellId, FTerraGameplayContainer::FValidatedActionExecutionResult& OutResult)
{
    return PlanetGameplayComponent
        ? PlanetGameplayComponent->TryExecuteNpcMcpValidatedAction(ExpectedTurnIndex, ExpectedFactionId, PieceId, ToCellId, OutResult)
        : false;
}

bool APlanetTessellatedMesh::HandleHISMUndo()
{
    return PlanetGameplayComponent
        ? PlanetGameplayComponent->HandleHISMUndo()
        : false;
}

void APlanetTessellatedMesh::ClearHISMHover()
{
    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->ClearHISMHover();
    }
}

void APlanetTessellatedMesh::ClearAllHISMHighlights()
{
    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->ClearAllHISMHighlights();
    }
}

void APlanetTessellatedMesh::RebuildG1DebugPieces_()
{
    if (PlanetGameplayComponent)
    {
        PlanetGameplayComponent->RebuildG1DebugPieces();
    }
}

void APlanetTessellatedMesh::DrawG1DebugPieces_() const
{
    if (PlanetGameplayComponent)
    {
        PlanetGameplayComponent->DrawG1DebugPieces();
    }
}

void APlanetTessellatedMesh::ApplyRenderModeVisibility_()
{
    HISMTileRenderer.ApplyVisibility(bEnableHISMTileRendering, bEnableHISMTileCollision);

    const bool bNeedTick = (PlanetHISMInteractionComponent
            && PlanetHISMInteractionComponent->bEnableHISMInstanceHighlight
            && bEnableHISMTileRendering)
        || (PlanetGameplayComponent && PlanetGameplayComponent->bEnableG1DebugPieces);
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
    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->SetHighlightLUT(InLUT);
    }
}
