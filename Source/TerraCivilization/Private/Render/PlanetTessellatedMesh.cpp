// Fill out your copyright notice in the Description page of Project Settings.

#include "Render/PlanetTessellatedMesh.h"
#include "Render/PlanetCameraComponent.h"
#include "Render/PlanetGameplayComponent.h"
#include "Render/PlanetHISMInteractionComponent.h"
#include "Render/PlanetPiecePresentationComponent.h"
#include "TerrainVisualCoordinator.h"
#include "TerrainVisualSurfaceComponent.h"
#include "TerraNpcMcpGameplayBridge.h"

#include "FSphereTopology.h"
#include "FSphereTopologyQuery.h"
#include "FCell.h"

// T4：WorldGen 接入——仅在 cpp 侧 include（头文件仅使用前向声明）
#include "WorldGenerator.h"
#include "CellGeoData.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"

#include "Components/SceneComponent.h"
#include "Logging/LogMacros.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Interaction/PlanetInteractionController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tutorial/TerraTutorialScenarioData.h"
#include "UObject/ObjectSaveContext.h"
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

    TerrainVisualSurfaceComp = CreateDefaultSubobject<UTerrainVisualSurfaceComponent>(TEXT("TerrainVisualSurfaceComp"));
    TerrainVisualSurfaceComp->SetupAttachment(RootScene);
    TerrainVisualSurfaceComp->SetVisibility(false);
    TerrainVisualSurfaceComp->SetHiddenInGame(true);

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

    FString TutorialScenarioPath;
    const bool bHasTutorialScenario = FParse::Value(FCommandLine::Get(), TEXT("TutorialScenario="), TutorialScenarioPath);
    if (bHasTutorialScenario)
    {
        UTerraTutorialScenarioData* Scenario = LoadObject<UTerraTutorialScenarioData>(nullptr, *TutorialScenarioPath);
        if (!Scenario)
        {
            UE_LOG(LogPlanetTess, Error, TEXT("[Tutorial] Failed to load TutorialScenario '%s'; using default initialization."), *TutorialScenarioPath);
        }
        else
        {
            CellSubdivisionLevel = Scenario->CellSubdivisionLevel;
            RebuildAll_();
            FString Error;
            if (PlanetGameplayComponent && PlanetGameplayComponent->InitializeTutorialScenario(*Scenario, Error))
            {
                UE_LOG(LogPlanetTess, Log, TEXT("[Tutorial] Loaded scenario '%s'."), *Scenario->ScenarioId.ToString());
                return;
            }
            UE_LOG(LogPlanetTess, Error, TEXT("[Tutorial] Failed to initialize scenario '%s': %s; using default initialization."), *Scenario->ScenarioId.ToString(), *Error);
        }
    }

    const UWorld* World = GetWorld();
    if (World && World->IsGameWorld()
        && (!CellTopology.IsValid()
            || !Generator.IsValid()
            || !PlanetGameplayComponent
            || !PlanetGameplayComponent->GetGameplayContainer()
            || !PlanetGameplayComponent->GetGameplayContainer()->IsInitialized()
            // ProceduralMesh sections are intentionally stripped before map save.
            // Rebuild all runtime-only state when loading such a map into PIE/game.
            || (TerrainVisualMode == ETerrainVisualMode::ContinuousSurface
                && (!TerrainVisualSurfaceComp || !TerrainVisualSurfaceComp->HasBuiltSurface()))))
    {
        RebuildAll_();
    }
}

#if WITH_EDITOR
void APlanetTessellatedMesh::PreSave(FObjectPreSaveContext SaveContext)
{
    Super::PreSave(SaveContext);

    bRestoreTerrainVisualAfterSave =
        TerrainVisualMode == ETerrainVisualMode::ContinuousSurface
        && TerrainVisualSurfaceComp
        && TerrainVisualSurfaceComp->HasBuiltSurface();

    if (bRestoreTerrainVisualAfterSave)
    {
        // UProceduralMeshComponent serializes ProcMeshSections by default. They are
        // deterministically rebuilt from topology/worldgen after the save instead.
        TerrainVisualSurfaceComp->ClearSurface();
        UE_LOG(LogPlanetTess, Verbose, TEXT("[TerrainVisual] Stripped runtime surface mesh before saving '%s'."), *GetPathName());
    }
}

void APlanetTessellatedMesh::PostSaveRoot(FObjectPostSaveRootContext SaveContext)
{
    Super::PostSaveRoot(SaveContext);

    if (!bRestoreTerrainVisualAfterSave)
    {
        return;
    }

    bRestoreTerrainVisualAfterSave = false;
    RebuildTerrainVisualSurface_();
    ApplyRenderModeVisibility_();
    UE_LOG(LogPlanetTess, Verbose, TEXT("[TerrainVisual] Restored runtime surface mesh after saving '%s'."), *GetPathName());
}
#endif

void APlanetTessellatedMesh::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const int32 HoverBeforeFadeTick = IsContinuousTerrainVisualActive()
        ? HISMTileRenderer.GetCurrentHoverCellId()
        : INDEX_NONE;
    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->TickHoverFade(DeltaSeconds);
    }
    if (IsContinuousTerrainVisualActive())
    {
        const int32 HoverAfterFadeTick = HISMTileRenderer.GetCurrentHoverCellId();
        if (HoverBeforeFadeTick != HoverAfterFadeTick)
        {
            WriteTerrainVisualHighlightForCell_(HoverBeforeFadeTick);
            RefreshTerrainVisualCapturePreviewCells_(HoverBeforeFadeTick);
            WriteTerrainVisualHighlightForCell_(HoverAfterFadeTick);
            RefreshTerrainVisualCapturePreviewCells_(HoverAfterFadeTick);
        }
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
        if (PlanetGameplayComponent)
        {
            PlanetGameplayComponent->SetMountainRidgeSegments(Generator->GetMountainRidgeSegments());
        }
    }

    RebuildHISMTileInstances_();
    RebuildTerrainVisualSurface_();
    ApplyRenderModeVisibility_();
    if (PlanetGameplayComponent)
    {
        PlanetGameplayComponent->RebuildGameplay();
        PlanetGameplayComponent->RebuildG1DebugPieces();
    }
    RefreshAllTerrainVisualHighlights_();
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
    if (UsesTerrainVisualHighlightLUT_())
    {
        WriteTerrainVisualHighlightForCell_(CellId);
        if (IsContinuousTerrainVisualActive())
        {
            return;
        }
    }

    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->WriteHISMHighlightForCell(CellId, bMarkRenderStateDirty);
    }
}

void APlanetTessellatedMesh::RebuildTerrainVisualSurface_()
{
    if (!TerrainVisualSurfaceComp || !CellTopology.IsValid() || !Generator.IsValid())
    {
        return;
    }

    FTerrainVisualConfig VisualConfig;
    VisualConfig.VisualMode = TerrainVisualMode;
    VisualConfig.SurfaceSubdivisionLevel = FMath::Clamp(TerrainVisualSurfaceSubdivisionLevel, 1, 7);
    // SV1 must share the HISM nominal radius while P2.5 still resolves piece height from HISM.
    VisualConfig.GlobeRadiusCM = FMath::Max(GlobeRadiusCM + HISMTileRadiusOffsetCM, 1.0f);
    VisualConfig.GlobalVisualSeed = WorldGenSettings.RandomSeed;
    VisualConfig.PlanetCenterWorld = GetPlanetCenterWorld_();
    VisualConfig.MountainHeightCM = TerrainVisualMountainHeightCM;
    VisualConfig.MountainFalloffExponent = TerrainVisualMountainFalloffExponent;
    VisualConfig.ForestHeightCM = TerrainVisualForestHeightCM;
    VisualConfig.ForestSigmoidSteepness = TerrainVisualForestSigmoidSteepness;
    VisualConfig.bEnableMediumFrequencyErosion = bEnableTerrainVisualMediumFrequencyErosion;
    VisualConfig.CrestNoiseAmplitudeCM = TerrainVisualCrestNoiseAmplitudeCM;
    VisualConfig.ErosionAmplitudeCM = TerrainVisualErosionAmplitudeCM;
    VisualConfig.LowlandNoiseAmplitudeCM = TerrainVisualLowlandNoiseAmplitudeCM;
    VisualConfig.MediumFrequencyNoiseFrequency = TerrainVisualMediumFrequencyNoiseFrequency;
    VisualConfig.ErosionDomainWarpAmplitude = TerrainVisualErosionDomainWarpAmplitude;
    VisualConfig.ErosionValleySharpness = TerrainVisualErosionValleySharpness;
    VisualConfig.bEnableDecorativeRivers = bEnableTerrainVisualDecorativeRivers;
    VisualConfig.RiverSourceCount = TerrainVisualRiverSourceCount;
    VisualConfig.RiverTerminalBasinCount = TerrainVisualRiverTerminalBasinCount;
    VisualConfig.RiverMinPathCells = TerrainVisualRiverMinPathCells;
    VisualConfig.RiverMaxPathCells = TerrainVisualRiverMaxPathCells;
    VisualConfig.RiverTerminalLakeMinDischarge = TerrainVisualRiverTerminalLakeMinDischarge;
    VisualConfig.RiverMinWidthCM = TerrainVisualRiverMinWidthCM;
    VisualConfig.RiverWidthScaleCM = TerrainVisualRiverWidthScaleCM;
    VisualConfig.RiverLengthWidthGrowthCM = TerrainVisualRiverLengthWidthGrowthCM;
    VisualConfig.RiverLakeLengthMultiplier = TerrainVisualRiverLakeLengthMultiplier;
    VisualConfig.RiverLakeWidthMultiplier = TerrainVisualRiverLakeWidthMultiplier;
    VisualConfig.RiverMaxLakeRadiusFraction = TerrainVisualRiverMaxLakeRadiusFraction;

    if (!TerrainVisualCoordinator.IsValid())
    {
        TerrainVisualCoordinator = MakeUnique<FTerrainVisualCoordinator>();
    }

    FString InitError;
    const TArray<FIntPoint>& MountainRidges = PlanetGameplayComponent
        ? PlanetGameplayComponent->GetMountainRidgeSegments()
        : Generator->GetMountainRidgeSegments();
    if (!TerrainVisualCoordinator->Initialize(*CellTopology, Generator->GetCellData(), MountainRidges, VisualConfig, InitError))
    {
        UE_LOG(LogPlanetTess, Warning, TEXT("[TerrainVisual][SV1] Initialization failed: %s"), *InitError);
        TerrainVisualSurfaceComp->ClearSurface();
        return;
    }

    const bool bContinuousSurface = TerrainVisualMode == ETerrainVisualMode::ContinuousSurface;
    const bool bHISMSDFExperiment = TerrainVisualMode == ETerrainVisualMode::HISMSDFExperiment;
    if (!bContinuousSurface && !bHISMSDFExperiment)
    {
        TerrainVisualSurfaceComp->ClearSurface();
        TerrainVisualCoordinator->SetContinuousSurfaceAvailable(false);
        ApplyHISMSDFExperimentMaterials_();
        return;
    }

    int32 SurfaceTriangleCount = 0;
    if (bContinuousSurface)
    {
        FSphereTopology SurfaceTopology(VisualConfig.SurfaceSubdivisionLevel);
        SurfaceTopology.Build();
        const bool bBuilt = TerrainVisualSurfaceComp->RebuildBaseSphere(
            SurfaceTopology,
            *CellTopology,
            *TerrainVisualCoordinator,
            VisualConfig.GlobeRadiusCM);
        TerrainVisualCoordinator->SetContinuousSurfaceAvailable(bBuilt);
        if (!bBuilt)
        {
            UE_LOG(LogPlanetTess, Warning,
                TEXT("[TerrainVisual][SV1] Failed to build base surface (Sub=%d Radius=%.1fcm)."),
                VisualConfig.SurfaceSubdivisionLevel,
                GlobeRadiusCM);
            ApplyHISMSDFExperimentMaterials_();
            return;
        }
        SurfaceTriangleCount = SurfaceTopology.PrimalTris.Num();
    }
    else
    {
        TerrainVisualSurfaceComp->ClearSurface();
        TerrainVisualCoordinator->SetContinuousSurfaceAvailable(false);
    }

    if (bContinuousSurface && TerrainVisualBaseMaterial)
    {
        TerrainVisualSurfaceComp->SetMaterial(0, TerrainVisualBaseMaterial);
    }
    if (!TerrainVisualSurfaceComp->InitializeHighlightResources(*CellTopology))
    {
        UE_LOG(LogPlanetTess, Warning, TEXT("[TerrainVisual][SV2] Failed to initialize highlight LUT resources."));
    }
    if (!TerrainVisualSurfaceComp->InitializeTerrainResources(Generator->GetCellData(), VisualConfig.GlobalVisualSeed))
    {
        UE_LOG(LogPlanetTess, Warning, TEXT("[TerrainVisual][SV3] Failed to initialize terrain LUT resources."));
    }
    if (!TerrainVisualSurfaceComp->InitializeRiverResources(TerrainVisualCoordinator->GetRiverSystem()))
    {
        UE_LOG(LogPlanetTess, Warning, TEXT("[TerrainVisual][SV5] Failed to initialize river LUT resources."));
    }
    if (bContinuousSurface)
    {
        TerrainVisualSurfaceComp->SetHighlightMaterial(
            TerrainVisualSurfaceMaterial ? TerrainVisualSurfaceMaterial.Get() : TerrainVisualHighlightMaterial.Get());
    }
    TerrainVisualSurfaceComp->SetSurfaceEnhancementParameters(
        TerrainVisualGravelColor, TerrainVisualGravelNormal, TerrainVisualGravelRoughness,
        TerrainVisualMossColor, TerrainVisualMossNormal, TerrainVisualMossRoughness,
        TerrainVisualRockColor, TerrainVisualRockNormal, TerrainVisualRockRoughness,
        TerrainVisualPlainTint, TerrainVisualForestTint, TerrainVisualMountainTint,
        TerrainVisualTileScaleCM, TerrainVisualTriplanarSharpness, TerrainVisualNormalStrength);
    TerrainVisualSurfaceComp->SetHighlightParameters(
        GetPlanetCenterWorld_(),
        TerrainVisualBaseGroundColor,
        TerrainVisualHighlightPaddingRad,
        TerrainVisualHighlightStrength);
    ApplyHISMSDFExperimentMaterials_();

    if (bContinuousSurface)
    {
        UE_LOG(LogPlanetTess, Log,
            TEXT("[TerrainVisual][SV1] Rebuilt base surface: Sub=%d Tris=%d Radius=%.1fcm."),
            VisualConfig.SurfaceSubdivisionLevel,
            SurfaceTriangleCount,
            VisualConfig.GlobeRadiusCM);
    }
    else
    {
        UE_LOG(LogPlanetTess, Log,
            TEXT("[TerrainVisual][SV6-B] Initialized HISM SDF experiment: Cells=%d ProjectionRadius=%.1fcm Material=%s."),
            CellTopology->Cells.Num(),
            VisualConfig.GlobeRadiusCM,
            *GetNameSafe(TerrainVisualHISMSDFMaterial));
    }
}

void APlanetTessellatedMesh::ApplyHISMSDFExperimentMaterials_()
{
    const bool bExperiment = TerrainVisualMode == ETerrainVisualMode::HISMSDFExperiment
        && TerrainVisualHISMSDFMaterial
        && TerrainVisualSurfaceComp
        && CellTopology.IsValid();

    const auto RestoreStaticMeshMaterials = [](UHierarchicalInstancedStaticMeshComponent* Component)
    {
        if (!Component || !Component->GetStaticMesh())
        {
            return;
        }
        UStaticMesh* StaticMesh = Component->GetStaticMesh();
        for (int32 MaterialIndex = 0; MaterialIndex < StaticMesh->GetStaticMaterials().Num(); ++MaterialIndex)
        {
            Component->SetMaterial(MaterialIndex, StaticMesh->GetMaterial(MaterialIndex));
        }
    };

    if (!bExperiment)
    {
        PlainHISMSDFMID = nullptr;
        ForestHISMSDFMID = nullptr;
        MountainHISMSDFMID = nullptr;
        RestoreStaticMeshMaterials(PlainTileHISMComp);
        RestoreStaticMeshMaterials(ForestTileHISMComp);
        RestoreStaticMeshMaterials(MountainTileHISMComp);
        if (PlanetHISMInteractionComponent)
        {
            PlanetHISMInteractionComponent->PrepareHighlightComponents();
        }
        return;
    }

    const auto ApplyExperimentMaterial = [this](
        UHierarchicalInstancedStaticMeshComponent* Component,
        TObjectPtr<UMaterialInstanceDynamic>& OutMID)
    {
        if (!Component || !Component->GetStaticMesh())
        {
            OutMID = nullptr;
            return;
        }

        OutMID = UMaterialInstanceDynamic::Create(TerrainVisualHISMSDFMaterial, this);
        if (!OutMID)
        {
            return;
        }

        TerrainVisualSurfaceComp->ApplySharedMaterialParameters(
            OutMID,
            GetPlanetCenterWorld_(),
            TerrainVisualBaseGroundColor,
            TerrainVisualHighlightPaddingRad,
            TerrainVisualHighlightStrength);
        OutMID->SetScalarParameterValue(
            TEXT("HISMSDFProjectionRadiusCM"),
            FMath::Max(GlobeRadiusCM + HISMTileRadiusOffsetCM, 1.0f));
        OutMID->SetScalarParameterValue(TEXT("SurfaceCellCount"), CellTopology->Cells.Num());

        const int32 MaterialCount = FMath::Max(Component->GetStaticMesh()->GetStaticMaterials().Num(), 1);
        for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
        {
            Component->SetMaterial(MaterialIndex, OutMID);
        }
    };

    ApplyExperimentMaterial(PlainTileHISMComp, PlainHISMSDFMID);
    ApplyExperimentMaterial(ForestTileHISMComp, ForestHISMSDFMID);
    ApplyExperimentMaterial(MountainTileHISMComp, MountainHISMSDFMID);
}

void APlanetTessellatedMesh::RefreshG4CapturePreviewCellsForActionTarget_(int32 ActionTargetCellId, bool bMarkLastRenderStateDirty)
{
    if (UsesTerrainVisualHighlightLUT_())
    {
        RefreshTerrainVisualCapturePreviewCells_(ActionTargetCellId);
        if (IsContinuousTerrainVisualActive())
        {
            return;
        }
    }

    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->RefreshCapturePreviewCellsForActionTarget(ActionTargetCellId, bMarkLastRenderStateDirty);
    }
}

void APlanetTessellatedMesh::UpdateHISMHoverCell_(int32 NewCellId)
{
    const int32 OldHoverCellId = HISMTileRenderer.GetCurrentHoverCellId();
    if (PlanetHISMInteractionComponent)
    {
        PlanetHISMInteractionComponent->UpdateHISMHoverCell(NewCellId);
    }

    if (UsesTerrainVisualHighlightLUT_())
    {
        WriteTerrainVisualHighlightForCell_(OldHoverCellId);
        RefreshTerrainVisualCapturePreviewCells_(OldHoverCellId);
        WriteTerrainVisualHighlightForCell_(NewCellId);
        RefreshTerrainVisualCapturePreviewCells_(NewCellId);
    }
}

void APlanetTessellatedMesh::WriteTerrainVisualHighlightForCell_(int32 CellId)
{
    if (!UsesTerrainVisualHighlightLUT_()
        || !TerrainVisualSurfaceComp
        || !CellTopology.IsValid()
        || !CellTopology->Cells.IsValidIndex(CellId))
    {
        return;
    }

    const FTerraGameplayContainer* Gameplay = PlanetGameplayComponent
        ? PlanetGameplayComponent->GetGameplayContainer()
        : nullptr;
    const FPlanetHISMHighlightConfig Config = BuildHISMHighlightConfig_();
    const float HoverIntensity = HISMTileRenderer.GetCurrentHoverCellId() == CellId ? 1.0f : 0.0f;
    FLinearColor FinalColor = FLinearColor::Black;
    float FinalIntensity = 0.0f;

    FTerraGameplayCellHighlight GameplayHighlight;
    const bool bHasGameplayHighlight = Gameplay && Gameplay->GetHighlightForCell(CellId, GameplayHighlight);
    const bool bIsCurrentFactionPieceCell = Gameplay && Gameplay->IsCurrentFactionPieceCell(CellId);
    if (bHasGameplayHighlight)
    {
        const bool bCaptureTargetHover = Gameplay
            && HISMTileRenderer.GetCurrentHoverCellId() != INDEX_NONE
            && Gameplay->IsCapturePreviewCellForActionTarget(CellId, HISMTileRenderer.GetCurrentHoverCellId());
        const bool bActionTargetHover = Gameplay
            && HoverIntensity > KINDA_SMALL_NUMBER
            && Gameplay->IsCurrentActionTargetCell(CellId);
        FinalColor = bCaptureTargetHover
            ? Config.CaptureTargetHoverColor
            : (bActionTargetHover ? Config.ActionTargetHoverColor : GameplayHighlight.Color);
        FinalIntensity = GameplayHighlight.Intensity;
    }
    else if (bIsCurrentFactionPieceCell && HoverIntensity > KINDA_SMALL_NUMBER)
    {
        FinalColor = Config.CurrentFactionPieceHoverColor;
        FinalIntensity = 1.0f;
    }
    else if (bIsCurrentFactionPieceCell)
    {
        FinalColor = Config.CurrentFactionPieceColor;
        FinalIntensity = 1.0f;
    }
    else if (HoverIntensity > KINDA_SMALL_NUMBER)
    {
        FinalColor = Config.HoverColor;
        FinalIntensity = HoverIntensity;
    }

    TerrainVisualSurfaceComp->WriteHighlightCell(
        CellId,
        FinalColor,
        FMath::Clamp(FinalIntensity, 0.0f, 1.0f));
}

void APlanetTessellatedMesh::RefreshTerrainVisualCapturePreviewCells_(int32 ActionTargetCellId)
{
    const FTerraGameplayContainer* Gameplay = PlanetGameplayComponent
        ? PlanetGameplayComponent->GetGameplayContainer()
        : nullptr;
    if (!Gameplay || ActionTargetCellId == INDEX_NONE)
    {
        return;
    }

    TArray<int32> CaptureCellIds;
    if (Gameplay->CollectCapturePreviewCellIdsForActionTarget(ActionTargetCellId, CaptureCellIds))
    {
        for (const int32 CellId : CaptureCellIds)
        {
            WriteTerrainVisualHighlightForCell_(CellId);
        }
    }
}

void APlanetTessellatedMesh::RefreshAllTerrainVisualHighlights_()
{
    if (!UsesTerrainVisualHighlightLUT_() || !CellTopology.IsValid())
    {
        return;
    }

    for (int32 CellId = 0; CellId < CellTopology->Cells.Num(); ++CellId)
    {
        WriteTerrainVisualHighlightForCell_(CellId);
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
    if (UsesTerrainVisualHighlightLUT_())
    {
        for (const int32 CellId : DirtyCellIds)
        {
            WriteTerrainVisualHighlightForCell_(CellId);
        }
    }
}

void APlanetTessellatedMesh::RefreshFactionPieceHighlights_(int32 FactionId)
{
    if (PlanetGameplayComponent)
    {
        PlanetGameplayComponent->RefreshFactionPieceHighlights(FactionId);
    }
    RefreshAllTerrainVisualHighlights_();
}

void APlanetTessellatedMesh::RefreshCurrentFactionPieceHighlights_()
{
    if (PlanetGameplayComponent)
    {
        PlanetGameplayComponent->RefreshCurrentFactionPieceHighlights();
    }
    RefreshAllTerrainVisualHighlights_();
}

bool APlanetTessellatedMesh::GetCellSurfaceWorldPosition_(int32 CellId, float RadiusOffsetCM, FVector& OutWorldPosition) const
{
    if (!CellTopology.IsValid() || !CellTopology->Cells.IsValidIndex(CellId))
    {
        return false;
    }

    // Camera focus deliberately remains on the unmodified reference sphere. Macro
    // terrain height and slope normals are presentation data, not camera orbit data.
    const FVector LocalPosition = CellTopology->Cells[CellId].UnitCenter * (GlobeRadiusCM + RadiusOffsetCM);
    OutWorldPosition = GetActorTransform().TransformPosition(LocalPosition);
    return true;
}

bool APlanetTessellatedMesh::QueryTerrainSurface_(const FVector& LocalUnitDirection, FTerrainSurfaceQueryResult& OutSurface) const
{
    OutSurface = FTerrainSurfaceQueryResult();
    const FVector SafeLocalDirection = LocalUnitDirection.GetSafeNormal();
    if (SafeLocalDirection.IsNearlyZero() || !TerrainVisualCoordinator.IsValid())
    {
        return false;
    }

    const FTerrainSurfaceQueryResult LocalSurface = TerrainVisualCoordinator->QueryBaseSurface(SafeLocalDirection);
    if (!LocalSurface.bIsValid)
    {
        return false;
    }

    const FTransform ActorTransform = GetActorTransform();
    OutSurface = LocalSurface;
    OutSurface.WorldPosition = ActorTransform.TransformPosition(SafeLocalDirection * LocalSurface.SurfaceRadiusCM);
    OutSurface.WorldNormal = ActorTransform.TransformVectorNoScale(LocalSurface.WorldNormal).GetSafeNormal();
    return !OutSurface.WorldNormal.IsNearlyZero();
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

bool APlanetTessellatedMesh::IsContinuousTerrainVisualActive() const
{
    return TerrainVisualMode == ETerrainVisualMode::ContinuousSurface
        && TerrainVisualSurfaceComp
        && TerrainVisualSurfaceComp->HasBuiltSurface()
        && TerrainVisualCoordinator.IsValid()
        && TerrainVisualCoordinator->GetDiagnostics().bCanActivateContinuousSurface;
}

bool APlanetTessellatedMesh::UsesTerrainVisualHighlightLUT_() const
{
    return IsContinuousTerrainVisualActive()
        || (TerrainVisualMode == ETerrainVisualMode::HISMSDFExperiment
            && TerrainVisualSurfaceComp
            && TerrainVisualSurfaceComp->HasHighlightResources());
}

bool APlanetTessellatedMesh::TryResolveContinuousSurfaceHitToCellId_(const FHitResult& Hit, int32& OutCellId) const
{
    OutCellId = INDEX_NONE;
    if (!IsContinuousTerrainVisualActive() || Hit.GetComponent() != TerrainVisualSurfaceComp)
    {
        return false;
    }

    const FVector LocalImpact = GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
    OutCellId = TerrainVisualCoordinator->ResolveCellId(LocalImpact.GetSafeNormal());
    return OutCellId != INDEX_NONE;
}

bool APlanetTessellatedMesh::HandleContinuousSurfaceHoverHit(const FHitResult& Hit)
{
    int32 CellId = INDEX_NONE;
    if (!TryResolveContinuousSurfaceHitToCellId_(Hit, CellId))
    {
        return false;
    }

    UpdateHISMHoverCell_(CellId);
    return true;
}

bool APlanetTessellatedMesh::HandleContinuousSurfaceClickHit(const FHitResult& Hit)
{
    int32 CellId = INDEX_NONE;
    if (!TryResolveContinuousSurfaceHitToCellId_(Hit, CellId))
    {
        return false;
    }

    return HandleGameplayCellClick_(
        CellId,
        TEXT("ContinuousSurface"),
        INDEX_NONE,
        GetNameSafe(TerrainVisualSurfaceComp));
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
    UpdateHISMHoverCell_(INDEX_NONE);
}

void APlanetTessellatedMesh::ClearAllHISMHighlights()
{
    if (UsesTerrainVisualHighlightLUT_())
    {
        if (TerrainVisualSurfaceComp)
        {
            TerrainVisualSurfaceComp->ClearHighlightCells();
        }
        RefreshAllTerrainVisualHighlights_();
        if (IsContinuousTerrainVisualActive())
        {
            return;
        }
    }

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
        PlanetGameplayComponent->TickTutorialNpcScript();
    }
}

void APlanetTessellatedMesh::ApplyRenderModeVisibility_()
{
    HISMTileRenderer.ApplyVisibility(bEnableHISMTileRendering, bEnableHISMTileCollision);
    ApplyTerrainVisualMode_();

    const bool bNeedTick = (PlanetHISMInteractionComponent
            && PlanetHISMInteractionComponent->bEnableHISMInstanceHighlight
            && (bEnableHISMTileRendering || IsContinuousTerrainVisualActive()))
        || (PlanetGameplayComponent && PlanetGameplayComponent->bEnableG1DebugPieces);
    PrimaryActorTick.SetTickFunctionEnable(bNeedTick);
}

void APlanetTessellatedMesh::ApplyTerrainVisualMode_()
{
    const bool bContinuous = IsContinuousTerrainVisualActive();
    if (TerrainVisualSurfaceComp)
    {
        TerrainVisualSurfaceComp->SetVisibility(bContinuous, true);
        TerrainVisualSurfaceComp->SetHiddenInGame(!bContinuous);
        TerrainVisualSurfaceComp->SetCollisionEnabled(
            bContinuous ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        TerrainVisualSurfaceComp->SetCollisionObjectType(ECC_WorldStatic);
        TerrainVisualSurfaceComp->SetCollisionResponseToAllChannels(ECR_Ignore);
        TerrainVisualSurfaceComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        TerrainVisualSurfaceComp->SetHighlightParameters(
            GetPlanetCenterWorld_(),
            TerrainVisualBaseGroundColor,
            TerrainVisualHighlightPaddingRad,
            TerrainVisualHighlightStrength);
    }

    if (!bContinuous)
    {
        return;
    }

    const ECollisionEnabled::Type LegacyCollision = bEnableHISMTileCollision
        ? ECollisionEnabled::QueryOnly
        : ECollisionEnabled::NoCollision;
    const auto ApplyLegacyHISM = [this, LegacyCollision](UHierarchicalInstancedStaticMeshComponent* Comp)
    {
        if (!Comp)
        {
            return;
        }

        Comp->SetVisibility(bShowLegacyHISMDebugInContinuousSurfaceMode, true);
        Comp->SetHiddenInGame(!bShowLegacyHISMDebugInContinuousSurfaceMode);
        Comp->SetCollisionEnabled(LegacyCollision);
        Comp->SetCollisionObjectType(ECC_WorldStatic);
        Comp->SetCollisionResponseToAllChannels(ECR_Block);
        // P2.5 keeps using ECC_WorldStatic. Cursor input uses ECC_Visibility and must hit the surface.
        Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    };

    ApplyLegacyHISM(PlainTileHISMComp);
    ApplyLegacyHISM(ForestTileHISMComp);
    ApplyLegacyHISM(MountainTileHISMComp);
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
