// Fill out your copyright notice in the Description page of Project Settings.

#include "Render/PlanetTessellatedMesh.h"
#include "Render/MeshDisplacementBuilder.h"
#include "Render/R8RecipeTable.h"

#include "TerraGameplayContainer.h"
#include "TerraNpcMcpGameplayBridge.h"
#include "TerraPiecePresentationManager.h"

#include "FSphereTopology.h"
#include "FCell.h"
#include "FCorner.h"

// T4：WorldGen 接入——仅在 cpp 侧 include（头文件仅使用前向声明）
#include "WorldGenerator.h"
#include "CellGeoData.h"

#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PixelFormat.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/BulkData.h"
#include "TextureResource.h"

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
    /** Knuth 整数哈希常数（黄金分割比 × 2^32），用于 VertexColor 预查时的 layer 哈希。 */
    constexpr uint32 GKnuthHashConst = 2654435761u;

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

    // R11：鼠标射线需要在 TerrainMeshComp 上命中复杂碰撞（§8.1 / §13 Step 1）。
    // 设为 QueryOnly（允许 LineTrace、不参与物理模拟） + ComplexAsSimple（让简单 trace
    // 也走复杂网格）。水面 mesh 保留 NoCollision，鼠标射线穿过水面击中地表是
    // 预期行为。
    TerrainMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMeshComp"));
    TerrainMeshComp->SetupAttachment(RootScene);
    TerrainMeshComp->bUseAsyncCooking = false;
    TerrainMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TerrainMeshComp->SetCollisionObjectType(ECC_WorldStatic);
    TerrainMeshComp->SetCollisionResponseToAllChannels(ECR_Block);
    TerrainMeshComp->bUseComplexAsSimpleCollision = true;
    TerrainMeshComp->SetCanEverAffectNavigation(false);

    WaterMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMeshComp"));
    WaterMeshComp->SetupAttachment(RootScene);
    WaterMeshComp->bUseAsyncCooking = false;
    WaterMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WaterMeshComp->SetCanEverAffectNavigation(false);
    WaterMeshComp->bUseComplexAsSimpleCollision = false;
    WaterMeshComp->SetVisibility(false);

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

    PrepareHISMHighlightComponent_(PlainTileHISMComp);
    PrepareHISMHighlightComponent_(ForestTileHISMComp);
    PrepareHISMHighlightComponent_(MountainTileHISMComp);

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
            || !MeshTopology.IsValid()
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

    if (HISMHoverFadeTimer > 0.0f)
    {
        HISMHoverFadeTimer -= DeltaSeconds;
        if (HISMHoverFadeTimer <= 0.0f)
        {
            HISMHoverFadeTimer = 0.0f;
            const int32 OldHover = HISMCurrentHoverCellId;
            HISMCurrentHoverCellId = INDEX_NONE;
            LastHISMPickedCellId = INDEX_NONE;
            if (OldHover != INDEX_NONE)
            {
                WriteHISMHighlightForCell_(OldHover, false);
                RefreshG4CapturePreviewCellsForActionTarget_(OldHover, true);
            }
        }
    }

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

// ===================================================================
//  RebuildAll_：T3 完整执行序列
//
//  详见 Docs/T3_TerrainMeshRender.md §2 总体执行序列：
//    1. RebuildTopologies_ —— 双 FSphereTopology
//    2. new FMeshDisplacementBuilder + BuildVertexToCoarseTris (T2)
//    3. ComputeCellElevation_                                  (T3 §3.5)
//    4. 5x RebuildCell?LUT_                                    (T3 §4)
//    5. RebuildTerrainMesh_                                    (T3 §3.4)
//    6. ApplyTerrainMaterial_ + DiagnoseR8Material_            (T3 §5)
//    7. RebuildWaterMesh_                                      (T3 §6)
//    8. RunSelfCheckT2 + LogTopologyStats_                     (T1/T2 保留)
// ===================================================================
void APlanetTessellatedMesh::RebuildAll_()
{
    // 设计稿 §6 风险点 7：必须整体重建双拓扑 + Builder，不允许部分跳过。
    RebuildTopologies_();

    // T2：BuildVertexToCoarseTris 真实填表（5/6 个 Corner 爬升 → 反查粗 Tri）
    Displacement = MakeUnique<FMeshDisplacementBuilder>(CellTopology.Get(), MeshTopology.Get());
    Displacement->BuildVertexToCoarseTris();

    // T2 验收自检（Output Log 打印 PASS/FAIL）。
    Displacement->RunSelfCheckT2();

    // ===== T4：在所有 LUT / mesh 灌装之前先跑 WorldGen 流水线 =====
    //
    // 与 R8 actor 在 RebuildAll_ 中的同款时序（PlanetTopologyDebugMesh.cpp L597~L605）：
    //   Generator.Reset() → MakeUnique<FWorldGenerator>(CellTopology, Settings) → Generate()
    //
    // ⚠ 必须用 CellTopology（玩法层 sub=3 / 642 cells），不是 MeshTopology（渲染层 sub=4）。
    //    WorldGen 在 cell 拓扑上算板块/海陆/三标量场——cell 数 = 642 是 WorldGen 的契约
    //    （详见 WorldGenDesign.md §4.2）。FCellGeoData[].Elevation 也是 cell 级、不是顶点级。
    //
    // 详见 Docs/T4_RealElevation.md §4.2。
    {
        Generator.Reset();
        if (CellTopology.IsValid())
        {
            Generator = MakeUnique<FWorldGenerator>(CellTopology.Get(), WorldGenSettings);
            Generator->Generate();
        }
    }

    // T3：5 张 LUT 必须在 ApplyTerrainMaterial_ 之前完成（§2 执行序列硬约束）。
    const int32 NumCells = CellTopology ? CellTopology->Cells.Num() : 0;
    RebuildCellAttrLUT_(NumCells);
    RebuildCellDirLUT_(NumCells);
    RebuildCellTintLUT_(NumCells);
    RebuildCellHSVRoughLUT_(NumCells);
    RebuildCellNSpecLUT_(NumCells);

    // T3：陆地 mesh + 材质（共享顶点路径，详见 §3.4）。
    RebuildTerrainMesh_();
    ApplyTerrainMaterial_(NumCells);

    // T3：水面层独立 sub=3 mesh（详见 §6.1）。
    RebuildWaterMesh_();

    // SimpleGameplay：主视觉 HISM 静态网格瓦片。
    RebuildHISMTileInstances_();
    ApplyRenderModeVisibility_();

    // SimpleGameplay G2：Gameplay 容器复制 Cell 拓扑与 WorldGen 地形，并持有棋子/回合状态。
    RebuildGameplay_();

    // SimpleGameplay G1/G2：根据 Gameplay 容器棋子状态生成调试球缓存，Tick 中绘制。
    RebuildG1DebugPieces_();
}

void APlanetTessellatedMesh::RebuildTopologies_()
{
    // ----- D8：CellTopology 与 MeshTopology 是两个完全独立的 FSphereTopology 实例 -----

    // Clamp 到 UPROPERTY 区间外侧再做一次防御（编辑器面板的 ClampMin/Max 仅是 UI 限制）。
    const int32 CellSub = FMath::Clamp(CellSubdivisionLevel, 1, 5);
    const int32 MeshSub = FMath::Clamp(FMath::Max(MeshSubdivisionLevel, CellSub), CellSub, 6);

    if (MeshSub != MeshSubdivisionLevel)
    {
        UE_LOG(LogPlanetTess, Warning,
               TEXT("MeshSubdivisionLevel(%d) < CellSubdivisionLevel(%d) is invalid, forcing MeshSub=%d"),
               MeshSubdivisionLevel, CellSub, MeshSub);
    }

    CellTopology = MakeUnique<FSphereTopology>(CellSub);
    CellTopology->Build();

    MeshTopology = MakeUnique<FSphereTopology>(MeshSub);
    MeshTopology->Build();

    // D13：水面层独立 sub=3 拓扑——lazy-build，复用一旦构建。
    if (!WaterTopology.IsValid())
    {
        WaterTopology = MakeUnique<FSphereTopology>(3);
        WaterTopology->Build();
    }
}

void APlanetTessellatedMesh::LogTopologyStats_() const
{
    const int32 CellCount  = CellTopology ? CellTopology->Cells.Num()             : 0;
    const int32 MeshVerts  = MeshTopology ? MeshTopology->PrimalVertsUnit.Num()   : 0;
    const int32 MeshTris   = MeshTopology ? MeshTopology->PrimalTris.Num()        : 0;

    // T1 验收锚点：默认参数（CellSub=3 / MeshSub=4）下应当输出
    //   "CellTopo: 642 cells / MeshTopo: 2562 verts / 5120 tris"
    UE_LOG(LogPlanetTess, Log,
           TEXT("[Tess] CellTopo: %d cells / MeshTopo: %d verts / %d tris  (CellSub=%d, MeshSub=%d)"),
           CellCount, MeshVerts, MeshTris,
           CellTopology ? CellTopology->SubdivisionLevel : -1,
           MeshTopology ? MeshTopology->SubdivisionLevel : -1);
}

// ===================================================================
//  T3 §3.5 / T4 §4.3：ComputeCellElevation_
//
//  T4 三段式数据源检测（D22）：
//    1) bUsePlaceholderElevation=true   → 强制 placeholder（D21 回归对照）
//    2) Generator 未 ready              → fallback placeholder（父稿 §6 风险点 5）
//    3) 否则走 Generator->GetCellData()[c].Elevation 真实数据
//
//  Placeholder = 赤道 +1、两极 -1 的余弦 ramp（详见 D12），与 T3 本封语义一致。
// ===================================================================
void APlanetTessellatedMesh::ComputeCellElevation_(TArray<float>& OutElev) const
{
    const int32 NumCells = CellTopology ? CellTopology->Cells.Num() : 0;
    OutElev.SetNumUninitialized(NumCells);

    // D22 三段式数据源检测
    const bool bUseReal =
        !bUsePlaceholderElevation
        && Generator.IsValid()
        && Generator->GetCellData().Num() == NumCells;

    if (bUseReal)
    {
        // T4 主路径：直接消费 FCellGeoData.Elevation（[-1, +1]）
        const TArray<FCellGeoData>& Cells = Generator->GetCellData();
        for (int32 c = 0; c < NumCells; ++c)
        {
            OutElev[c] = Cells[c].Elevation;
        }
        UE_LOG(LogPlanetTess, Verbose,
            TEXT("[Tess] ComputeCellElevation: WorldGen real path (NumCells=%d)"), NumCells);
    }
    else
    {
        // Fallback：T3 placeholder（赤道 +1，两极 -1）
        // 触发条件：编辑器初次打开 / WorldGen 失败 / bUsePlaceholderElevation=true
        for (int32 c = 0; c < NumCells; ++c)
        {
            const FVector& U = CellTopology->Cells[c].UnitCenter;
            const float AbsZ = FMath::Abs(static_cast<float>(U.Z));
            OutElev[c] = 1.0f - 2.0f * AbsZ;
        }

        if (bUsePlaceholderElevation)
        {
            UE_LOG(LogPlanetTess, Log,
                TEXT("[Tess] ComputeCellElevation: PLACEHOLDER (bUsePlaceholderElevation=true)"));
        }
        else
        {
            UE_LOG(LogPlanetTess, Warning,
                TEXT("[Tess] ComputeCellElevation: PLACEHOLDER fallback "
                     "(Generator.IsValid=%d, GotCells=%d, Expected=%d)"),
                Generator.IsValid() ? 1 : 0,
                Generator.IsValid() ? Generator->GetCellData().Num() : -1,
                NumCells);
        }
    }
}

// ===================================================================
//  T3 §3.2（第二版）：FindCoarseCellsForMeshTri_ —— 按 mesh 三角形索引爬升到粗 Cell 三角形
//
//  R8 PS 端的球面 Voronoi argmax(dot(dir, V_i)) 在 (c0, c1, c2) 间仲裁，要求一个
//  渲染三角形内部的 (c0, c1, c2) 必须保持常量（不允许光栅化插值）。
//
//  T3 mesh sub=4 的每个渲染三角形 T_render 唯一从属于一个 sub=3 粗 Cell 三角形 T_coarse
//  （由 FSphereTopology::SubdividePrimalOnce 的 4 子 → 1 父结构保证）。本函数沿
//  PrimalTriTreeNodes[T_render]->Father 爬升 (MeshSub - CellSub) 步，得到 T_coarse
//  及其 (cA, cB, cC) —— 该 mesh 三角形的 3 个独立顶点全部写这一组 c。
//
//  关键不变量：
//    · MeshTopology->PrimalTriTreeNodes[T] 是 PrimalTris[T] 的叶子节点（FSphereTopology.cpp L463）
//    · 爬升到 CellTopology 层级后 Node->CellIds[0..2] 是 CellTopology 的 cell 索引
//      （依赖 sub=N 与 sub=M (M>N) 的子图同构性，与 T2 BuildVertexToCoarseTris 同根）
//
//  详见 Docs/AgentWorkflow.md §3.18（T3 共享顶点路径破碎踩坑）。
// ===================================================================
bool APlanetTessellatedMesh::FindCoarseCellsForMeshTri_(int32 MeshTriIdx,
    int32& OutCA, int32& OutCB, int32& OutCC) const
{
    OutCA = OutCB = OutCC = INDEX_NONE;

    if (!CellTopology.IsValid() || !MeshTopology.IsValid())
    {
        return false;
    }
    if (!MeshTopology->PrimalTriTreeNodes.IsValidIndex(MeshTriIdx))
    {
        return false;
    }

    FTriTreeNode* Node = MeshTopology->PrimalTriTreeNodes[MeshTriIdx];
    if (Node == nullptr)
    {
        return false;
    }

    const int32 ClimbSteps = MeshTopology->SubdivisionLevel - CellTopology->SubdivisionLevel;
    if (ClimbSteps < 0)
    {
        return false;
    }

    for (int32 K = 0; K < ClimbSteps && Node && Node->Father; ++K)
    {
        Node = Node->Father;
    }
    if (Node == nullptr)
    {
        return false;
    }

    OutCA = Node->CellIds[0];
    OutCB = Node->CellIds[1];
    OutCC = Node->CellIds[2];

    return CellTopology->Cells.IsValidIndex(OutCA)
        && CellTopology->Cells.IsValidIndex(OutCB)
        && CellTopology->Cells.IsValidIndex(OutCC);
}

// ===================================================================
//  T3：VertexColor 预查（17 配方 placeholder）
// ===================================================================
FLinearColor APlanetTessellatedMesh::ComputeVertexLayerColor_(int32 CellId) const
{
    // R8 路径：17 配方哈希索引 → Tint。VertexColor 仅作为材质回退底色（材质未挂时仍可见 cell 着色）。
    const int32 RecipeIdx = TerraCivilization::R8::PlaceholderRecipeIndex(CellId);
    const TerraCivilization::R8::FRecipe& R = TerraCivilization::R8::GRecipes[RecipeIdx];
    return FLinearColor(
        FMath::Clamp(R.TintR, 0.0f, 1.0f),
        FMath::Clamp(R.TintG, 0.0f, 1.0f),
        FMath::Clamp(R.TintB, 0.0f, 1.0f),
        1.0f);
}

// ===================================================================
//  T3 §4：5 张 LUT 重建（从 R8 整段搬运 + Topology→CellTopology + T3 placeholder 路径）
// ===================================================================

// ----- §4 LUT 1/5：CellAttrLUT（PF_B8G8R8A8，R 通道写 LayerIndex）-----
//
// T3 简化版：仅走 placeholder 路径（17 配方哈希）——T3 阶段 WorldGenerator 字段为空，
// 不读 FCellGeoData。T4 起再扩展真实数据分支。详见 T3_TerrainMeshRender.md §10 风险点 2。
void APlanetTessellatedMesh::RebuildCellAttrLUT_(int32 NumCells)
{
    if (NumCells <= 0)
    {
        CellAttrLUT = nullptr;
        return;
    }

    const int32 LayerMod = FMath::Clamp(NumLayersHint, 1, 256);

    UTexture2D* NewLUT = UTexture2D::CreateTransient(NumCells, 1, PF_B8G8R8A8, TEXT("Tess_CellAttrLUT_Transient"));
    if (!NewLUT)
    {
        UE_LOG(LogPlanetTess, Error, TEXT("[Tess] CreateTransient(CellAttrLUT) failed (NumCells=%d)"), NumCells);
        CellAttrLUT = nullptr;
        return;
    }

    NewLUT->Filter        = TF_Nearest;
    NewLUT->SRGB          = false;
    NewLUT->AddressX      = TA_Clamp;
    NewLUT->AddressY      = TA_Clamp;
    NewLUT->CompressionSettings = TC_VectorDisplacementmap;
    NewLUT->NeverStream   = true;
    NewLUT->LODGroup      = TEXTUREGROUP_ColorLookupTable;
#if WITH_EDITORONLY_DATA
    NewLUT->MipGenSettings = TMGS_NoMipmaps;
#endif

    FTexturePlatformData* Plat = NewLUT->GetPlatformData();
    if (!Plat || Plat->Mips.Num() == 0)
    {
        UE_LOG(LogPlanetTess, Error, TEXT("[Tess] CellAttrLUT has no mip 0; abort fill."));
        CellAttrLUT = nullptr;
        return;
    }

    FByteBulkData& Bulk = Plat->Mips[0].BulkData;
    uint8* Dst = static_cast<uint8*>(Bulk.Lock(LOCK_READ_WRITE));
    if (!Dst)
    {
        UE_LOG(LogPlanetTess, Error, TEXT("[Tess] Failed to Lock CellAttrLUT mip 0."));
        CellAttrLUT = nullptr;
        return;
    }

    for (int32 CellId = 0; CellId < NumCells; ++CellId)
    {
        // T3 placeholder：bUseR8PlaceholderRecipes=true → R8 17 配方哈希；
        //                 false → Knuth 哈希按 NumLayersHint 取模（R3 朴素路径）。
        uint8 Layer = 0;
        if (bUseR8PlaceholderRecipes)
        {
            Layer = static_cast<uint8>(TerraCivilization::R8::PlaceholderRecipeIndex(CellId));
        }
        else
        {
            const uint32 Hashed = static_cast<uint32>(CellId) * GKnuthHashConst;
            Layer = static_cast<uint8>(Hashed % static_cast<uint32>(LayerMod));
        }

        // BGRA 顺序写入。R 通道存 Layer，其余 0。
        const int32 Offset = CellId * 4;
        Dst[Offset + 0] = 0;       // B
        Dst[Offset + 1] = 0;       // G
        Dst[Offset + 2] = Layer;   // R = LayerBase
        Dst[Offset + 3] = 0;       // A
    }

    Bulk.Unlock();
    NewLUT->UpdateResource();
    CellAttrLUT = NewLUT;
}

// ----- §4 LUT 2/5：CellDirLUT（PF_A32B32G32R32F，RGB = UnitCenter，A = isPentagon）-----
void APlanetTessellatedMesh::RebuildCellDirLUT_(int32 NumCells)
{
    if (NumCells <= 0 || !CellTopology.IsValid())
    {
        CellDirLUT = nullptr;
        return;
    }

    UTexture2D* NewLUT = UTexture2D::CreateTransient(NumCells, 1, PF_A32B32G32R32F, TEXT("Tess_CellDirLUT_Transient"));
    if (!NewLUT)
    {
        UE_LOG(LogPlanetTess, Error, TEXT("[Tess] CreateTransient(CellDirLUT) failed (NumCells=%d)"), NumCells);
        CellDirLUT = nullptr;
        return;
    }

    NewLUT->Filter        = TF_Nearest;
    NewLUT->SRGB          = false;
    NewLUT->AddressX      = TA_Clamp;
    NewLUT->AddressY      = TA_Clamp;
    NewLUT->NeverStream   = true;
    NewLUT->CompressionSettings = TC_HDR;
#if WITH_EDITORONLY_DATA
    NewLUT->MipGenSettings      = TMGS_NoMipmaps;
#endif

    FTexturePlatformData* Plat = NewLUT->GetPlatformData();
    if (!Plat || Plat->Mips.Num() == 0)
    {
        CellDirLUT = nullptr;
        return;
    }

    FByteBulkData& Bulk = Plat->Mips[0].BulkData;
    float* Dst = static_cast<float*>(Bulk.Lock(LOCK_READ_WRITE));
    if (!Dst)
    {
        CellDirLUT = nullptr;
        return;
    }

    for (int32 CellId = 0; CellId < NumCells; ++CellId)
    {
        const FCell&   Cell = CellTopology->Cells[CellId];
        const FVector& U    = Cell.UnitCenter;

        const int32 Off = CellId * 4;
        Dst[Off + 0] = static_cast<float>(U.X);                  // R = UnitCenter.x
        Dst[Off + 1] = static_cast<float>(U.Y);                  // G = UnitCenter.y
        Dst[Off + 2] = static_cast<float>(U.Z);                  // B = UnitCenter.z
        Dst[Off + 3] = Cell.bIsPentagon ? 1.0f : 0.0f;           // A = isPentagon
    }

    Bulk.Unlock();
    NewLUT->UpdateResource();
    CellDirLUT = NewLUT;
}

// ----- §4 LUT 3/5：CellTintLUT（PF_FloatRGBA，RGB = Tint，A = HueShift）-----
//
// T3 共用 helper（namespace-scoped 避免与 R8 actor 的 file-static 撞名）。
namespace
{
    UTexture2D* T3_CreateFloatRGBALUT(int32 NumCells, const TCHAR* DebugName)
    {
        UTexture2D* NewLUT = UTexture2D::CreateTransient(NumCells, 1, PF_FloatRGBA, DebugName);
        if (!NewLUT) return nullptr;

        NewLUT->Filter        = TF_Nearest;
        NewLUT->SRGB          = false;
        NewLUT->AddressX      = TA_Clamp;
        NewLUT->AddressY      = TA_Clamp;
        NewLUT->NeverStream   = true;
        NewLUT->CompressionSettings = TC_HDR;
        NewLUT->LODGroup      = TEXTUREGROUP_ColorLookupTable;
#if WITH_EDITORONLY_DATA
        NewLUT->MipGenSettings = TMGS_NoMipmaps;
#endif
        return NewLUT;
    }
}

void APlanetTessellatedMesh::RebuildCellTintLUT_(int32 NumCells)
{
    if (NumCells <= 0)
    {
        CellTintLUT = nullptr;
        return;
    }

    UTexture2D* NewLUT = T3_CreateFloatRGBALUT(NumCells, TEXT("Tess_CellTintLUT_Transient"));
    if (!NewLUT)
    {
        UE_LOG(LogPlanetTess, Error, TEXT("[Tess] CreateTransient(CellTintLUT) failed (NumCells=%d)"), NumCells);
        CellTintLUT = nullptr;
        return;
    }

    FTexturePlatformData* Plat = NewLUT->GetPlatformData();
    if (!Plat || Plat->Mips.Num() == 0) { CellTintLUT = nullptr; return; }

    FByteBulkData& Bulk = Plat->Mips[0].BulkData;
    FFloat16* Dst = static_cast<FFloat16*>(Bulk.Lock(LOCK_READ_WRITE));
    if (!Dst) { CellTintLUT = nullptr; return; }

    for (int32 c = 0; c < NumCells; ++c)
    {
        const TerraCivilization::R8::FRecipe& R =
            TerraCivilization::R8::PickRecipe(bUseR8PlaceholderRecipes, c);
        Dst[c * 4 + 0] = FFloat16(R.TintR);
        Dst[c * 4 + 1] = FFloat16(R.TintG);
        Dst[c * 4 + 2] = FFloat16(R.TintB);
        Dst[c * 4 + 3] = FFloat16(0.0f);   // HueShift 预留 0
    }

    Bulk.Unlock();
    NewLUT->UpdateResource();
    CellTintLUT = NewLUT;
}

// ----- §4 LUT 4/5：CellHSVRoughLUT（R=Sat, G=Bri, B=RoughMin, A=RoughMax）-----
void APlanetTessellatedMesh::RebuildCellHSVRoughLUT_(int32 NumCells)
{
    if (NumCells <= 0) { CellHSVRoughLUT = nullptr; return; }

    UTexture2D* NewLUT = T3_CreateFloatRGBALUT(NumCells, TEXT("Tess_CellHSVRoughLUT_Transient"));
    if (!NewLUT) { CellHSVRoughLUT = nullptr; return; }

    FTexturePlatformData* Plat = NewLUT->GetPlatformData();
    if (!Plat || Plat->Mips.Num() == 0) { CellHSVRoughLUT = nullptr; return; }

    FByteBulkData& Bulk = Plat->Mips[0].BulkData;
    FFloat16* Dst = static_cast<FFloat16*>(Bulk.Lock(LOCK_READ_WRITE));
    if (!Dst) { CellHSVRoughLUT = nullptr; return; }

    for (int32 c = 0; c < NumCells; ++c)
    {
        const TerraCivilization::R8::FRecipe& R =
            TerraCivilization::R8::PickRecipe(bUseR8PlaceholderRecipes, c);
        Dst[c * 4 + 0] = FFloat16(R.SatMul);
        Dst[c * 4 + 1] = FFloat16(R.BriMul);
        Dst[c * 4 + 2] = FFloat16(R.RoughMin);
        Dst[c * 4 + 3] = FFloat16(R.RoughMax);
    }

    Bulk.Unlock();
    NewLUT->UpdateResource();
    CellHSVRoughLUT = NewLUT;
}

// ----- §4 LUT 5/5：CellNSpecLUT（R=NormalStr, G=HeightScale, B=Spec, A=TriScale）-----
void APlanetTessellatedMesh::RebuildCellNSpecLUT_(int32 NumCells)
{
    if (NumCells <= 0) { CellNSpecLUT = nullptr; return; }

    UTexture2D* NewLUT = T3_CreateFloatRGBALUT(NumCells, TEXT("Tess_CellNSpecLUT_Transient"));
    if (!NewLUT) { CellNSpecLUT = nullptr; return; }

    FTexturePlatformData* Plat = NewLUT->GetPlatformData();
    if (!Plat || Plat->Mips.Num() == 0) { CellNSpecLUT = nullptr; return; }

    FByteBulkData& Bulk = Plat->Mips[0].BulkData;
    FFloat16* Dst = static_cast<FFloat16*>(Bulk.Lock(LOCK_READ_WRITE));
    if (!Dst) { CellNSpecLUT = nullptr; return; }

    for (int32 c = 0; c < NumCells; ++c)
    {
        const TerraCivilization::R8::FRecipe& R =
            TerraCivilization::R8::PickRecipe(bUseR8PlaceholderRecipes, c);
        // R8.2 激活：LUT3.G 存 [0, 1] 归一化 HeightScale，HLSL 端用 lut3.g * MaxRaymarchDepthCM 还原 cm。
        //
        //   T 阶段与 R8.2 的位移分层（D8.2.7）：
        //     - 几何域 macro Elevation（~500 cm 公里级）走 cpp 顶点位移（已由 MeshDisplacementBuilder 完成）
        //     - 材质域 micro SHFRM（~50 cm 厘米级）走 GPU raymarch（本 LUT 提供 HeightScale）
        //   二者在量纲上完全正交，同时处于不同管线阶段、互不覆盖。
        //
        //   归一化上界 50.0f = R8.2 §4.2 拍板的 HeightScaleCM 上限（Mountain.Peak）。
        const float HScaleNormalized = FMath::Clamp(R.HeightScaleCM / 50.0f, 0.0f, 1.0f);
        Dst[c * 4 + 0] = FFloat16(R.NormalStr);
        Dst[c * 4 + 1] = FFloat16(HScaleNormalized);   // R8.2 SHFRM HeightScale（0..1，HLSL 端还原 cm）
        Dst[c * 4 + 2] = FFloat16(1.0f);          // SpecularBoost: 中性 1.0
        Dst[c * 4 + 3] = FFloat16(R.TriScale);
    }

    Bulk.Unlock();
    NewLUT->UpdateResource();
    CellNSpecLUT = NewLUT;
}

// ===================================================================
//  T3 §3.4（第二版）：RebuildTerrainMesh_ —— 独立顶点 + 顶点位移 + 三角形级 c0/c1/c2 一致
//
//  ⚠ 与 T3 第一版的本质差异：
//    第一版用共享顶点（NumVerts = MeshTopology->PrimalVertsUnit.Num() = 2562）。
//    问题：mesh 顶点 V 周围的多个三角形对它的代表组 (c0, c1, c2) 不同，但共享顶点
//    只能保留一组。光栅化插值时，相邻三角形从同一个 V 拿到"插值后"的 (c0, c1, c2)
//    实际上是 3 个不同顶点 (c0, c1, c2) 的线性 blend——解出的 cell ID 完全错乱，
//    PS 端 argmax 选到不属于该位置的 cell → 整球按 mesh 三角形粒度破碎。
//
//    第二版采用 R8 同款独立顶点路径（NumVerts = NumMeshTris * 3 = 15360）。
//    每个 mesh 三角形的 3 个顶点写同一组 (HiC,LoC) / (HiA,LoA) / (HiB,LoB) ——
//    这一组 (cA, cB, cC) 是该 mesh 三角形通过 FindCoarseCellsForMeshTri_ 爬升到
//    粗 CellTopology 三角形得到的。光栅化插值后 UV0/UV1/UV2 在三角形内部仍是常量，
//    PS 端 argmax(dot(dir, V_i)) 在 (cA, cB, cC) 间仲裁，dir 必落在该粗 Tri 球面区域
//    内 → 仲裁结果稳定为 cA/cB/cC 之一，cell 内所有 mesh 三角形看到的颜色完全一致。
//
//  顶点位移 / 法线连续性的保证：
//    虽然每个三角形展开 3 独立顶点，但每个顶点的 Position 与 Normal 仍按它对应的
//    mesh 顶点 v 取值（Position = PrimalVertsUnit[v] * (R + ElevCM[v]); Normal =
//    PrimalVertsUnit[v]）—— 也就是说"独立顶点"仅在 UV/VertexColor 等"语义属性"
//    上独立，几何属性仍跨三角形共享同一份 mesh 顶点的 Position/Normal。这保证了
//    相邻三角形交界处位置 / 法线**完全一致** → 视觉上仍是光滑共享顶点 mesh，无
//    flat shading 锯齿。
//
//  详见 Docs/AgentWorkflow.md §3.18 与 R8 PlanetTopologyDebugMesh.cpp L470~L545。
// ===================================================================
void APlanetTessellatedMesh::RebuildTerrainMesh_()
{
    if (!TerrainMeshComp || !MeshTopology.IsValid() || !CellTopology.IsValid() || !Displacement.IsValid())
    {
        return;
    }

    const int32 NumMeshVerts = MeshTopology->PrimalVertsUnit.Num();
    const int32 NumMeshTris  = MeshTopology->PrimalTris.Num();

    if (NumMeshVerts <= 0 || NumMeshTris <= 0)
    {
        TerrainMeshComp->ClearAllMeshSections();
        return;
    }

    // ---- 1) Cell elevation（T3 placeholder = 余弦 ramp）+ 顶点位移 ----
    TArray<float> CellElev;
    ComputeCellElevation_(CellElev);

    TArray<float> VertexElevCM;
    Displacement->ComputeVertexElevationCM(CellElev, ElevationScaleCM, VertexElevCM);
    if (VertexElevCM.Num() != NumMeshVerts)
    {
        // 兜底：T2 自检失败时不让 mesh 整体崩。补 0 让球面退化为光面球。
        VertexElevCM.Init(0.0f, NumMeshVerts);
    }

    // ---- 2) 顶点缓冲（独立顶点：每 mesh 三角形展开 3 顶点）----
    const int32 NumOutVerts = NumMeshTris * 3;

    TArray<FVector>          Vertices;       Vertices.Reserve(NumOutVerts);
    TArray<int32>            Triangles;      Triangles.Reserve(NumOutVerts);
    TArray<FVector>          Normals;        Normals.Reserve(NumOutVerts);
    TArray<FVector2D>        UV0, UV1, UV2, UV3;
    UV0.Reserve(NumOutVerts);
    UV1.Reserve(NumOutVerts);
    UV2.Reserve(NumOutVerts);
    UV3.Reserve(NumOutVerts);
    TArray<FLinearColor>     VertexColors;   VertexColors.Reserve(NumOutVerts);
    TArray<FProcMeshTangent> Tangents;       // 留空，与 R8 主 mesh 一致（D17）

    int32 OrphanTriCount = 0;

    for (int32 T = 0; T < NumMeshTris; ++T)
    {
        // 爬升到粗 Cell 三角形，拿 (cA, cB, cC) —— 三角形内全体顶点共享这一组
        int32 cA = INDEX_NONE, cB = INDEX_NONE, cC = INDEX_NONE;
        if (!FindCoarseCellsForMeshTri_(T, cA, cB, cC))
        {
            ++OrphanTriCount;
            // 兜底：用 (0, 0, 0) 占位（PS 仲裁会都选 cell 0，但起码不崩）
            cA = cB = cC = 0;
        }

        // CellId 拆 Hi/Lo 编码（与 R8 完全一致；PS 端 c = Hi*256 + Lo）
        const float HiA = static_cast<float>((cA >> 8) & 0xFF);
        const float LoA = static_cast<float>(cA & 0xFF);
        const float HiB = static_cast<float>((cB >> 8) & 0xFF);
        const float LoB = static_cast<float>(cB & 0xFF);
        const float HiC = static_cast<float>((cC >> 8) & 0xFF);
        const float LoC = static_cast<float>(cC & 0xFF);

        const FVector2D EncCellA(HiA, LoA);   // → UV1 (c0)
        const FVector2D EncCellB(HiB, LoB);   // → UV2 (c1)
        const FVector2D EncCellC(HiC, LoC);   // → UV0 (c2)

        // 三角形 3 个角色的 OneHot（与 R8 完全一致）
        const FVector2D OneHotA(1.0f, 0.0f);
        const FVector2D OneHotB(0.0f, 1.0f);
        const FVector2D OneHotC(0.0f, 0.0f);

        // 三角形 3 个 mesh 顶点（共享原始位置 / 法线 / 位移）
        const FIntVector& Tri = MeshTopology->PrimalTris[T];
        const int32 V0 = Tri.X, V1 = Tri.Y, V2 = Tri.Z;
        if (!MeshTopology->PrimalVertsUnit.IsValidIndex(V0) ||
            !MeshTopology->PrimalVertsUnit.IsValidIndex(V1) ||
            !MeshTopology->PrimalVertsUnit.IsValidIndex(V2))
        {
            continue;
        }

        const FVector Dir0 = MeshTopology->PrimalVertsUnit[V0];
        const FVector Dir1 = MeshTopology->PrimalVertsUnit[V1];
        const FVector Dir2 = MeshTopology->PrimalVertsUnit[V2];

        const FVector P0 = Dir0 * (GlobeRadiusCM + VertexElevCM[V0]);
        const FVector P1 = Dir1 * (GlobeRadiusCM + VertexElevCM[V1]);
        const FVector P2 = Dir2 * (GlobeRadiusCM + VertexElevCM[V2]);

        // VertexColor 用三角形 3 个 mesh 顶点各自的代表 cell 色（视觉差异微小，且材质
        // 走 R8 Custom 节点时不消费 VertexColor）。这里简化为 (cA, cB, cC) hash 色，
        // 与 R8 的 hash(A)/hash(B)/hash(C) 等价（虽然 mesh 顶点不一定就在 cell 中心）。
        const FLinearColor ColA = ComputeVertexLayerColor_(cA);
        const FLinearColor ColB = ComputeVertexLayerColor_(cB);
        const FLinearColor ColC = ComputeVertexLayerColor_(cC);

        const int32 BaseIdx = Vertices.Num();

        // Role 0：mesh 顶点 V0，OneHot=(1,0)，VertexColor=hash(cA)
        Vertices.Add(P0); Normals.Add(Dir0);
        UV0.Add(EncCellC); UV1.Add(EncCellA); UV2.Add(EncCellB); UV3.Add(OneHotA);
        VertexColors.Add(ColA);

        // Role 1：mesh 顶点 V1，OneHot=(0,1)，VertexColor=hash(cB)
        Vertices.Add(P1); Normals.Add(Dir1);
        UV0.Add(EncCellC); UV1.Add(EncCellA); UV2.Add(EncCellB); UV3.Add(OneHotB);
        VertexColors.Add(ColB);

        // Role 2：mesh 顶点 V2，OneHot=(0,0)，VertexColor=hash(cC)
        Vertices.Add(P2); Normals.Add(Dir2);
        UV0.Add(EncCellC); UV1.Add(EncCellA); UV2.Add(EncCellB); UV3.Add(OneHotC);
        VertexColors.Add(ColC);

        // 沿用 PrimalTris 原始绕序（CCW from outside，与 R8 一致）
        Triangles.Add(BaseIdx + 0);
        Triangles.Add(BaseIdx + 1);
        Triangles.Add(BaseIdx + 2);
    }

    if (OrphanTriCount > 0)
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess] %d mesh tris failed coarse-cell lookup (likely sub-isomorphism break). "
                 "They were filled with cell=(0,0,0) fallback."),
            OrphanTriCount);
    }

    // ---- 3) 提交 PMC ----
    TerrainMeshComp->ClearAllMeshSections();
    TerrainMeshComp->CreateMeshSection_LinearColor(
        /*SectionIndex*/ 0,
        Vertices, Triangles, Normals,
        UV0, UV1, UV2, UV3,
        VertexColors, Tangents,
        /*bCreateCollision*/ true);   // R11：必须开同复杂碰撞才能被鼠标射线命中（§8.1）

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess] Rebuilt terrain mesh: MeshTris=%d  OutVerts=%d  Radius=%.1f cm  ElevScale=%.1f cm"),
        NumMeshTris, Vertices.Num(), GlobeRadiusCM, ElevationScaleCM);
}

// ===================================================================
//  T3 §5.1：ApplyTerrainMaterial_ —— 17 参数 MID 注入（与 R8 完全对齐）
// ===================================================================
void APlanetTessellatedMesh::ApplyTerrainMaterial_(int32 NumCells)
{
    if (!TerrainMeshComp)
    {
        return;
    }

    if (!TerrainMaterial)
    {
        TerrainMID = nullptr;
        TerrainMeshComp->SetMaterial(0, nullptr);
        return;
    }

    UMaterialInstanceDynamic* NewMID = UMaterialInstanceDynamic::Create(TerrainMaterial, this);
    if (NewMID)
    {
        // R3 注入
        if (CellAttrLUT)  NewMID->SetTextureParameterValue(TEXT("CellAttrLUT"), CellAttrLUT);
        NewMID->SetScalarParameterValue(TEXT("NumLayersHint"), static_cast<float>(NumLayersHint));
        NewMID->SetScalarParameterValue(TEXT("NumCells"),      static_cast<float>(NumCells));

        // R4 注入：CellDirLUT + PlanetCenter
        if (CellDirLUT)   NewMID->SetTextureParameterValue(TEXT("CellDirLUT"), CellDirLUT);
        const FVector ActorLoc = GetActorLocation();
        NewMID->SetVectorParameterValue(TEXT("PlanetCenter"),
            FLinearColor(static_cast<float>(ActorLoc.X), static_cast<float>(ActorLoc.Y),
                         static_cast<float>(ActorLoc.Z), 0.0f));

        // R5/R6 注入
        NewMID->SetScalarParameterValue(TEXT("EdgeWidth"),      EdgeWidth);
        NewMID->SetScalarParameterValue(TEXT("NoiseAmplitude"), NoiseAmplitude);
        NewMID->SetScalarParameterValue(TEXT("NoiseScale"),     NoiseScale);

        // R7 注入：Triplanar
        if (TerrainAlbedoArray) NewMID->SetTextureParameterValue(TEXT("TerrainAlbedoArray"), TerrainAlbedoArray);
        if (TerrainNormalArray) NewMID->SetTextureParameterValue(TEXT("TerrainNormalArray"), TerrainNormalArray);
        NewMID->SetScalarParameterValue(TEXT("TriplanarSharpness"), TriplanarSharpness);
        NewMID->SetScalarParameterValue(TEXT("TileScale"),         TileScale);

        // R8 注入：3 PBR base + 3 LUT
        if (PBRBaseAlbedo)    NewMID->SetTextureParameterValue(TEXT("PBRBaseAlbedo"),    PBRBaseAlbedo);
        if (PBRBaseNormal)    NewMID->SetTextureParameterValue(TEXT("PBRBaseNormal"),    PBRBaseNormal);
        if (PBRBaseRoughness) NewMID->SetTextureParameterValue(TEXT("PBRBaseRoughness"), PBRBaseRoughness);
        if (PBRBaseHeight)    NewMID->SetTextureParameterValue(TEXT("PBRBaseHeight"),    PBRBaseHeight);
        if (CellTintLUT)      NewMID->SetTextureParameterValue(TEXT("CellTintLUT"),      CellTintLUT);
        if (CellHSVRoughLUT)  NewMID->SetTextureParameterValue(TEXT("CellHSVRoughLUT"),  CellHSVRoughLUT);
        if (CellNSpecLUT)     NewMID->SetTextureParameterValue(TEXT("CellNSpecLUT"),     CellNSpecLUT);

        // R8.2 注入：SHFRM 参数（详见 Docs/R8.2_SphericalHeightFieldRaymarching.md §4.4）。
        //   - GlobeRadiusCM：球半径，HLSL 用 length(P - PlanetCenter) - GlobeRadiusCM 算径向高度
        //   - MaxRaymarchDepthCM：raymarch 沿视线最大深度（cm，R8.2 默认 75 = HeightScale 上限 50 × 1.5）
        //   - MaxRaymarchSteps：线性步数（R8.2 默认 16；HLSL 中是写死的常量，本参数仅提供 Editor 侧可见）
        // 注意：T 阶段的 GlobeRadiusCM 取自 actor UPROPERTY GlobeRadiusCM；R8 actor 取自 Radius。
        NewMID->SetScalarParameterValue(TEXT("GlobeRadiusCM"),       GlobeRadiusCM);
        NewMID->SetScalarParameterValue(TEXT("MaxRaymarchDepthCM"),  MaxRaymarchDepthCM);
        NewMID->SetScalarParameterValue(TEXT("MaxRaymarchSteps"),    static_cast<float>(MaxRaymarchSteps));

        // R11 注入：hover/select 描边颜色 + 描边带宽度 + 全局强度（详见 HexHighlightInteractionPlan.md §10.2）。
        //   - HighlightHoverColor 默认 (1.00, 0.85, 0.10) 暖金
        //   - HighlightSelectColor 默认 (0.20, 0.90, 1.00) 青蓝
        //   - HighlightPadding 默认 0.15（cell 张角 15%）
        //   - HighlightStrength 默认 1.5（加性叠加到 albedo）
        // CellHighlightLUT 本身由 UCellHighlightComponent::BeginPlay 负责创建 + SetHighlightLUT 注入；
        // 这里 Rebuild 到这一步可能还没拿到 LUT，需要从保存的成员变量重填一份。
        NewMID->SetVectorParameterValue(TEXT("HoverColor"),  HighlightHoverColor);
        NewMID->SetVectorParameterValue(TEXT("SelectColor"), HighlightSelectColor);
        NewMID->SetScalarParameterValue(TEXT("HighlightPadding"),  HighlightPadding);
        NewMID->SetScalarParameterValue(TEXT("HighlightStrength"), HighlightStrength);
        if (HighlightLUT)
        {
            NewMID->SetTextureParameterValue(TEXT("CellHighlightLUT"), HighlightLUT);
        }
    }

    TerrainMID = NewMID;
    UMaterialInterface* MatToApply = TerrainMID
        ? static_cast<UMaterialInterface*>(TerrainMID)
        : TerrainMaterial.Get();
    TerrainMeshComp->SetMaterial(0, MatToApply);

    // D16：R8 合规性反射诊断（17 inputs 检查，仅 Editor）
    DiagnoseR8Material_();
}

// ===================================================================
//  T3 §5.2：DiagnoseR8Material_ —— R8 合规性反射诊断（17 inputs）
// ===================================================================
void APlanetTessellatedMesh::DiagnoseR8Material_() const
{
#if WITH_EDITORONLY_DATA
    if (!TerrainMaterial) return;
    UMaterial* BaseMat = TerrainMaterial->GetMaterial();
    if (!BaseMat) return;

    const TConstArrayView<TObjectPtr<UMaterialExpression>> Exprs = BaseMat->GetExpressions();
    int32 CustomFound = 0;

    // R8.1 完整 19 项 + R8.2 新增 5 项 = 24 项（与 R8.2 §5.2 完全对应）；
    // R11 在 Step 8 末尾追加 5 项 Inputs：CellHighlightLUT + HoverColor + SelectColor
    // + HighlightPadding + HighlightStrength（详见 HexHighlightInteractionPlan.md §9）。
    // → 反射诊断期望表共 29 inputs（小写比较）。
    const TArray<FString> ExpectedR8Inputs = {
        // ---- R8.1 baseline 19 项 ----
        TEXT("uv0"), TEXT("uv1"), TEXT("uv2"), TEXT("uv3"),
        TEXT("worldpos"), TEXT("planetcenter"),
        TEXT("cellattrlut"), TEXT("celldirlut"),
        TEXT("edgewidth"),
        TEXT("noiseamplitude"), TEXT("noisescale"),
        TEXT("triplanarsharpness"), TEXT("tilescale"),
        TEXT("pbrbasealbedo"),
        TEXT("pbrbasenormal"),                                              // R8.1 新增：三通道 Normal
        TEXT("pbrbaseroughness"),                                           // R8.1 新增：三通道 Roughness
        TEXT("celltintlut"), TEXT("cellhsvroughlut"), TEXT("cellnspeclut"),
        // ---- R8.2 新增 5 项（SHFRM）----
        TEXT("pbrbaseheight"),                                              // R8.2 新增：SHFRM 高度场
        TEXT("cameravector"),                                               // R8.2 新增：视线方向
        TEXT("globeradiuscm"),                                              // R8.2 新增：球半径显式暴露
        TEXT("maxraymarchdepthcm"),                                         // R8.2 新增：raymarch 深度上限
        TEXT("cameraworldpos"),                                             // R8.2 新增：相机世界位置
        // ---- R11 新增 5 项（HexHighlight）----
        TEXT("cellhighlightlut"),                                           // R11 新增：hover/select 双通道描边 LUT
        TEXT("hovercolor"),                                                 // R11 新增：hover 描边色（Vector3）
        TEXT("selectcolor"),                                                // R11 新增：select 描边色（Vector3）
        TEXT("highlightpadding"),                                           // R11 新增：描边带宽（Scalar）
        TEXT("highlightstrength"),                                          // R11 新增：全局描边强度（Scalar）
    };
    bool bAnyR8Compliant = false;

    for (UMaterialExpression* Expr : Exprs)
    {
        UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr);
        if (!Custom) continue;
        ++CustomFound;

        TSet<FString> PresentLower;
        TSet<FString> ConnectedLower;
        for (int32 I = 0; I < Custom->Inputs.Num(); ++I)
        {
            const FString InputName = Custom->Inputs[I].InputName.ToString();
            const FString Lower     = InputName.ToLower();
            const bool    bConn     = (Custom->Inputs[I].Input.GetTracedInput().Expression != nullptr);
            PresentLower.Add(Lower);
            if (bConn) { ConnectedLower.Add(Lower); }
        }

        TArray<FString> Missing, Disconnected;
        for (const FString& Need : ExpectedR8Inputs)
        {
            if (!PresentLower.Contains(Need))         { Missing.Add(Need); }
            else if (!ConnectedLower.Contains(Need))  { Disconnected.Add(Need); }
        }

        if (Missing.Num() == 0 && Disconnected.Num() == 0)
        {
            bAnyR8Compliant = true;
            UE_LOG(LogPlanetTess, Log,
                TEXT("[Tess] Material '%s' Custom #%d: ✓ R8 compliance (all %d inputs present & connected)"),
                *BaseMat->GetName(), CustomFound, ExpectedR8Inputs.Num());
        }
        else
        {
            if (Missing.Num() > 0)
            {
                UE_LOG(LogPlanetTess, Error,
                    TEXT("[Tess] Material '%s' Custom #%d: ✗ missing inputs [%s]"),
                    *BaseMat->GetName(), CustomFound, *FString::Join(Missing, TEXT(", ")));
            }
            if (Disconnected.Num() > 0)
            {
                UE_LOG(LogPlanetTess, Error,
                    TEXT("[Tess] Material '%s' Custom #%d: ✗ inputs declared but NOT connected [%s]"),
                    *BaseMat->GetName(), CustomFound, *FString::Join(Disconnected, TEXT(", ")));
            }
        }
    }

    if (CustomFound == 0)
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess] Material '%s' contains NO UMaterialExpressionCustom nodes — "
                 "T3 falls back to vertex color preview only."),
            *BaseMat->GetName());
    }
    else if (!bAnyR8Compliant)
    {
        UE_LOG(LogPlanetTess, Error,
            TEXT("[Tess] Material '%s' has %d Custom nodes but NONE is R8-compliant "
                 "(see R8_ParametricTint.md §4 for the 17-input wiring)."),
            *BaseMat->GetName(), CustomFound);
    }
#endif
}

// ===================================================================
//  T3 §6：RebuildWaterMesh_ —— 独立 sub=3 拓扑（与 R8 RebuildWaterMesh_ 等价）
// ===================================================================
void APlanetTessellatedMesh::RebuildWaterMesh_()
{
    if (!WaterMeshComp)
    {
        return;
    }

    if (!bEnableWaterShell)
    {
        WaterMeshComp->ClearAllMeshSections();
        WaterMeshComp->SetVisibility(false);
        return;
    }

    if (!WaterTopology.IsValid())
    {
        // 防御：RebuildTopologies_ 中已经 lazy-build；保险起见再补一次。
        WaterTopology = MakeUnique<FSphereTopology>(3);
        WaterTopology->Build();
    }

    const int32 NumCells   = WaterTopology->Cells.Num();
    const int32 NumCorners = WaterTopology->Corners.Num();
    if (NumCells == 0 || NumCorners == 0)
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess] Empty water topology (Cells=%d, Corners=%d). Skip water build."),
            NumCells, NumCorners);
        WaterMeshComp->ClearAllMeshSections();
        WaterMeshComp->SetVisibility(false);
        return;
    }

    // 水面半径 = WaterRadiusCM（编辑器直接调）。R8 actor 用的是 Radius + WaterSurfaceOffset，
    // T3 简化为单字段。详见 T3_TerrainMeshRender.md §6.1。
    const float WaterRadius = FMath::Max(WaterRadiusCM, 1.0f);

    // 与 R8 RebuildWaterMesh_ 几何路径一致：每 Corner 展开 3 独立顶点（不共享）
    const int32 NumVerts = NumCorners * 3;

    TArray<FVector>          Vertices;       Vertices.Reserve(NumVerts);
    TArray<int32>            Triangles;      Triangles.Reserve(NumVerts);
    TArray<FVector>          Normals;        Normals.Reserve(NumVerts);
    TArray<FVector2D>        UV0, UV1, UV2, UV3;
    UV0.Reserve(NumVerts); UV1.Reserve(NumVerts); UV2.Reserve(NumVerts); UV3.Reserve(NumVerts);
    TArray<FLinearColor>     VertexColors;   VertexColors.Reserve(NumVerts);
    TArray<FProcMeshTangent> Tangents;       // 留空（SLW Shading Model 走 World Space Normal）

    const FVector2D    UVZero(0.0f, 0.0f);
    const FLinearColor ColWhite(1.0f, 1.0f, 1.0f, 1.0f);

    for (int32 CornerIdx = 0; CornerIdx < NumCorners; ++CornerIdx)
    {
        const FCorner& Cor = WaterTopology->Corners[CornerIdx];
        const int32 CA = Cor.CellIds[0];
        const int32 CB = Cor.CellIds[1];
        const int32 CC = Cor.CellIds[2];
        if (!WaterTopology->Cells.IsValidIndex(CA) ||
            !WaterTopology->Cells.IsValidIndex(CB) ||
            !WaterTopology->Cells.IsValidIndex(CC))
        {
            continue;
        }

        const FVector PA = WaterTopology->Cells[CA].UnitCenter * WaterRadius;
        const FVector PB = WaterTopology->Cells[CB].UnitCenter * WaterRadius;
        const FVector PC = WaterTopology->Cells[CC].UnitCenter * WaterRadius;

        // 法线 = UnitCenter（球面外法）。**故意不走** KismetTangents 自动法线——
        // 独立顶点路径下 KismetTangents 等价于 flat shading（1280 个三角棱面）。
        // 详见 AgentWorkflow.md §3.13。
        const FVector NA = WaterTopology->Cells[CA].UnitCenter;
        const FVector NB = WaterTopology->Cells[CB].UnitCenter;
        const FVector NC = WaterTopology->Cells[CC].UnitCenter;

        const int32 BaseIdx = Vertices.Num();

        Vertices.Add(PA); Normals.Add(NA);
        UV0.Add(UVZero); UV1.Add(UVZero); UV2.Add(UVZero); UV3.Add(UVZero);
        VertexColors.Add(ColWhite);

        Vertices.Add(PB); Normals.Add(NB);
        UV0.Add(UVZero); UV1.Add(UVZero); UV2.Add(UVZero); UV3.Add(UVZero);
        VertexColors.Add(ColWhite);

        Vertices.Add(PC); Normals.Add(NC);
        UV0.Add(UVZero); UV1.Add(UVZero); UV2.Add(UVZero); UV3.Add(UVZero);
        VertexColors.Add(ColWhite);

        // 沿用 FCorner.CellIds 原始顺序（CCW from outside）；不做 bFlipWinding。
        Triangles.Add(BaseIdx + 0);
        Triangles.Add(BaseIdx + 1);
        Triangles.Add(BaseIdx + 2);
    }

    WaterMeshComp->ClearAllMeshSections();
    WaterMeshComp->CreateMeshSection_LinearColor(
        /*SectionIndex*/ 0,
        Vertices,
        Triangles,
        Normals,
        UV0, UV1, UV2, UV3,
        VertexColors,
        Tangents,
        /*bCreateCollision*/ false);

    if (WaterMaterial)
    {
        WaterMeshComp->SetMaterial(0, WaterMaterial);
    }

    WaterMeshComp->SetVisibility(true);

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess] Rebuilt water mesh: Radius=%.1f cm  Cells=%d  Corners=%d  Verts=%d  Tris=%d  Material=%s"),
        WaterRadius, NumCells, NumCorners, Vertices.Num(), Triangles.Num() / 3,
        WaterMaterial ? *WaterMaterial->GetName() : TEXT("(none)"));
}

void APlanetTessellatedMesh::RebuildHISMTileInstances_()
{
    if (!PlainTileHISMComp || !ForestTileHISMComp || !MountainTileHISMComp)
    {
        return;
    }

    PlainTileHISMComp->ClearInstances();
    ForestTileHISMComp->ClearInstances();
    MountainTileHISMComp->ClearInstances();

    PlainInstanceToCellId.Reset();
    ForestInstanceToCellId.Reset();
    MountainInstanceToCellId.Reset();
    CellIdToHISMInstance.Reset();
    HISMCurrentHoverCellId = INDEX_NONE;
    HISMPendingHoverCellId = INDEX_NONE;
    HISMHoverFadeTimer = 0.0f;
    LastHISMPickedCellId = INDEX_NONE;
    LastHISMClickedCellId = INDEX_NONE;

    PlainTileHISMComp->SetStaticMesh(PlainTileStaticMesh);
    ForestTileHISMComp->SetStaticMesh(ForestTileStaticMesh);
    MountainTileHISMComp->SetStaticMesh(MountainTileStaticMesh);

    PrepareHISMHighlightComponent_(PlainTileHISMComp);
    PrepareHISMHighlightComponent_(ForestTileHISMComp);
    PrepareHISMHighlightComponent_(MountainTileHISMComp);

    if (!bEnableHISMTileRendering)
    {
        return;
    }

    if (!CellTopology.IsValid() || !Generator.IsValid())
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess] Skip HISM spherical tiles: CellTopology or WorldGen is not ready."));
        return;
    }

    const TArray<FCellGeoData>& Cells = Generator->GetCellData();
    const int32 NumCells = CellTopology->Cells.Num();
    CellIdToHISMInstance.SetNum(NumCells);
    if (Cells.Num() != NumCells)
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess] Skip HISM spherical tiles: WorldGen cell count mismatch. Got=%d Expected=%d"),
            Cells.Num(), NumCells);
        return;
    }

    if (!PlainTileStaticMesh || !ForestTileStaticMesh || !MountainTileStaticMesh)
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess] HISM spherical tiles need all three StaticMesh assets. Plain=%s Forest=%s Mountain=%s"),
            *GetNameSafe(PlainTileStaticMesh),
            *GetNameSafe(ForestTileStaticMesh),
            *GetNameSafe(MountainTileStaticMesh));
    }

    const float SourceRadius = FMath::Max(HISMTileSourceRadiusCM, 1.0f);
    const float TargetRadius = FMath::Max(GlobeRadiusCM + HISMTileRadiusOffsetCM, 1.0f);
    const float UniformScale = (TargetRadius / SourceRadius) * FMath::Max(HISMTileAdditionalUniformScale, 0.001f);
    const FVector Scale3D(UniformScale);
    const FVector LocalTileUp = FVector::UpVector;

    int32 PlainCount = 0;
    int32 ForestCount = 0;
    int32 MountainCount = 0;
    int32 MissingMeshCount = 0;

    for (int32 CellId = 0; CellId < NumCells; ++CellId)
    {
        const FVector UnitCenter = CellTopology->Cells[CellId].UnitCenter.GetSafeNormal();
        if (UnitCenter.IsNearlyZero())
        {
            continue;
        }

        UHierarchicalInstancedStaticMeshComponent* TargetComp = nullptr;
        switch (Cells[CellId].SimpleTerrainType)
        {
        case ETerraSimpleTerrainType::Forest:
            TargetComp = ForestTileHISMComp;
            ++ForestCount;
            break;
        case ETerraSimpleTerrainType::Mountain:
            TargetComp = MountainTileHISMComp;
            ++MountainCount;
            break;
        case ETerraSimpleTerrainType::Plain:
        default:
            TargetComp = PlainTileHISMComp;
            ++PlainCount;
            break;
        }

        if (!TargetComp || !TargetComp->GetStaticMesh())
        {
            ++MissingMeshCount;
            continue;
        }

        const FQuat Rotation = FQuat::FindBetweenNormals(LocalTileUp, UnitCenter);
        const FTransform InstanceTransform(Rotation, FVector::ZeroVector, Scale3D);
        const int32 InstanceIndex = TargetComp->AddInstance(InstanceTransform, /*bWorldSpace=*/false);
        if (InstanceIndex == INDEX_NONE)
        {
            continue;
        }

        TArray<int32>* InstanceToCellId = nullptr;
        if (TargetComp == PlainTileHISMComp)
        {
            InstanceToCellId = &PlainInstanceToCellId;
        }
        else if (TargetComp == ForestTileHISMComp)
        {
            InstanceToCellId = &ForestInstanceToCellId;
        }
        else if (TargetComp == MountainTileHISMComp)
        {
            InstanceToCellId = &MountainInstanceToCellId;
        }

        if (InstanceToCellId)
        {
            if (InstanceToCellId->Num() <= InstanceIndex)
            {
                InstanceToCellId->SetNum(InstanceIndex + 1);
            }
            (*InstanceToCellId)[InstanceIndex] = CellId;
        }

        if (CellIdToHISMInstance.IsValidIndex(CellId))
        {
            CellIdToHISMInstance[CellId].Component = TargetComp;
            CellIdToHISMInstance[CellId].InstanceIndex = InstanceIndex;
        }

        if (bEnableHISMInstanceHighlight)
        {
            TargetComp->SetCustomDataValue(InstanceIndex, 0, 0.0f, false);
            TargetComp->SetCustomDataValue(InstanceIndex, 1, 0.0f, false);
            TargetComp->SetCustomDataValue(InstanceIndex, 2, 0.0f, false);
            TargetComp->SetCustomDataValue(InstanceIndex, 3, 0.0f, false);
        }
    }

    UE_LOG(LogPlanetTess, Log,
        TEXT("[Tess] Rebuilt HISM spherical tiles: Plain=%d Forest=%d Mountain=%d MissingMeshSkipped=%d Radius=%.1fcm SourceRadius=%.1fcm Scale=%.4f"),
        PlainCount, ForestCount, MountainCount, MissingMeshCount, TargetRadius, SourceRadius, UniformScale);
}

void APlanetTessellatedMesh::PrepareHISMHighlightComponent_(UHierarchicalInstancedStaticMeshComponent* Comp)
{
    if (!Comp)
    {
        return;
    }

    Comp->SetNumCustomDataFloats(4);

    const int32 MaterialCount = Comp->GetNumMaterials();
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
    {
        if (UMaterialInterface* Material = Comp->GetMaterial(MaterialIndex))
        {
            UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material);
            if (!MID)
            {
                MID = Comp->CreateDynamicMaterialInstance(MaterialIndex, Material);
            }
            if (MID)
            {
                MID->SetScalarParameterValue(TEXT("HighlightStrength"), HighlightStrength);
                MID->SetScalarParameterValue(TEXT("HighlightInnerRadius"), HISMHighlightInnerRadius);
                MID->SetScalarParameterValue(TEXT("HighlightOuterRadius"), HISMHighlightOuterRadius);
            }
        }
    }
}

void APlanetTessellatedMesh::WriteHISMHighlightForCell_(int32 CellId, bool bMarkRenderStateDirty)
{
    if (!bEnableHISMInstanceHighlight || !CellIdToHISMInstance.IsValidIndex(CellId))
    {
        return;
    }

    const FTerraHISMCellInstanceRef& Ref = CellIdToHISMInstance[CellId];
    if (!Ref.IsValid())
    {
        return;
    }

    const float HoverIntensity = (HISMCurrentHoverCellId == CellId) ? 1.0f : 0.0f;

    FLinearColor FinalHighlightColor = FLinearColor::Black;
    float FinalHighlightIntensity = 0.0f;

    FTerraGameplayCellHighlight GameplayHighlight;
    const bool bHasGameplayActionHighlight = GameplayContainer.IsValid()
        && GameplayContainer->GetHighlightForCell(CellId, GameplayHighlight);
    const bool bIsCurrentFactionPieceCell = GameplayContainer.IsValid()
        && GameplayContainer->IsCurrentFactionPieceCell(CellId);

    if (bHasGameplayActionHighlight)
    {
        const bool bIsG4CaptureTargetHover = GameplayContainer.IsValid()
            && HISMCurrentHoverCellId != INDEX_NONE
            && GameplayContainer->IsCapturePreviewCellForActionTarget(CellId, HISMCurrentHoverCellId);
        const bool bIsActionTargetHover = HoverIntensity > KINDA_SMALL_NUMBER
            && GameplayContainer.IsValid()
            && GameplayContainer->IsCurrentActionTargetCell(CellId);
        FinalHighlightColor = bIsG4CaptureTargetHover
            ? G4CaptureTargetHoverColor
            : (bIsActionTargetHover ? G3ActionTargetHoverColor : GameplayHighlight.Color);
        FinalHighlightIntensity = GameplayHighlight.Intensity;
    }
    else if (bIsCurrentFactionPieceCell && HoverIntensity > KINDA_SMALL_NUMBER)
    {
        FinalHighlightColor = G2_5CurrentFactionPieceHoverColor;
        FinalHighlightIntensity = 1.0f;
    }
    else if (bIsCurrentFactionPieceCell)
    {
        FinalHighlightColor = G2_5CurrentFactionPieceColor;
        FinalHighlightIntensity = 1.0f;
    }
    else if (HoverIntensity > KINDA_SMALL_NUMBER)
    {
        FinalHighlightColor = HighlightHoverColor;
        FinalHighlightIntensity = HoverIntensity;
    }
    FinalHighlightIntensity = FMath::Clamp(FinalHighlightIntensity, 0.0f, 1.0f);

    const bool bRedWritten = Ref.Component->SetCustomDataValue(Ref.InstanceIndex, 0, FMath::Clamp(FinalHighlightColor.R, 0.0f, 1.0f), false);
    const bool bGreenWritten = Ref.Component->SetCustomDataValue(Ref.InstanceIndex, 1, FMath::Clamp(FinalHighlightColor.G, 0.0f, 1.0f), false);
    const bool bBlueWritten = Ref.Component->SetCustomDataValue(Ref.InstanceIndex, 2, FMath::Clamp(FinalHighlightColor.B, 0.0f, 1.0f), false);
    const bool bIntensityWritten = Ref.Component->SetCustomDataValue(Ref.InstanceIndex, 3, FinalHighlightIntensity, bMarkRenderStateDirty);

    if (bMarkRenderStateDirty)
    {
        Ref.Component->MarkRenderInstancesDirty();
    }

    if (!bRedWritten || !bGreenWritten || !bBlueWritten || !bIntensityWritten)
    {
        UE_LOG(LogPlanetTess, Warning,
            TEXT("[Tess] Failed to write HISM highlight custom data. Cell=%d Instance=%d Component=%s RWritten=%d GWritten=%d BWritten=%d IntensityWritten=%d Hover=%.1f Gameplay=%.1f Final=(%.3f, %.3f, %.3f, %.3f)"),
            CellId,
            Ref.InstanceIndex,
            *GetNameSafe(Ref.Component),
            bRedWritten ? 1 : 0,
            bGreenWritten ? 1 : 0,
            bBlueWritten ? 1 : 0,
            bIntensityWritten ? 1 : 0,
            HoverIntensity,
            GameplayHighlight.Intensity,
            FinalHighlightColor.R,
            FinalHighlightColor.G,
            FinalHighlightColor.B,
            FinalHighlightIntensity);
    }
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
    OutCellId = INDEX_NONE;

    if (!bEnableHISMInstanceHighlight || !bEnableHISMTileRendering || !bEnableHISMTileCollision)
    {
        return false;
    }

    const UPrimitiveComponent* HitComp = Hit.GetComponent();
    const int32 InstanceIndex = Hit.Item;
    if (!HitComp || InstanceIndex == INDEX_NONE)
    {
        return false;
    }

    const TArray<int32>* InstanceToCellId = nullptr;
    if (HitComp == PlainTileHISMComp)
    {
        InstanceToCellId = &PlainInstanceToCellId;
    }
    else if (HitComp == ForestTileHISMComp)
    {
        InstanceToCellId = &ForestInstanceToCellId;
    }
    else if (HitComp == MountainTileHISMComp)
    {
        InstanceToCellId = &MountainInstanceToCellId;
    }

    if (!InstanceToCellId || !InstanceToCellId->IsValidIndex(InstanceIndex))
    {
        return false;
    }

    const int32 CellId = (*InstanceToCellId)[InstanceIndex];
    if (!CellIdToHISMInstance.IsValidIndex(CellId))
    {
        return false;
    }

    OutCellId = CellId;
    return true;
}

bool APlanetTessellatedMesh::HandleHISMHoverHit(const FHitResult& Hit)
{
    int32 CellId = INDEX_NONE;
    if (!TryResolveHISMHitToCellId(Hit, CellId))
    {
        return false;
    }

UpdateHISMHoverCell_(CellId);

    if (GEngine && CellId != INDEX_NONE && CellId != LastHISMClickedCellId)
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

    LastHISMClickedCellId = CellId;
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

    LastHISMClickedCellId = TargetCellId;
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
    RefreshG4CapturePreviewCellsForActionTarget_(HISMCurrentHoverCellId);

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
    RefreshG4CapturePreviewCellsForActionTarget_(HISMCurrentHoverCellId);
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
    if (HISMCurrentHoverCellId != INDEX_NONE)
    {
        const int32 OldHover = HISMCurrentHoverCellId;
        HISMCurrentHoverCellId = INDEX_NONE;
        HISMPendingHoverCellId = INDEX_NONE;
        HISMHoverFadeTimer = 0.0f;
        LastHISMPickedCellId = INDEX_NONE;
        WriteHISMHighlightForCell_(OldHover, false);
        RefreshG4CapturePreviewCellsForActionTarget_(OldHover, true);
    }
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
    if (TerrainMeshComp)
    {
        TerrainMeshComp->SetVisibility(bShowDebugProceduralSurface, true);
        TerrainMeshComp->SetHiddenInGame(!bShowDebugProceduralSurface);
        TerrainMeshComp->SetCollisionEnabled(bUseDebugProceduralCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }

    if (WaterMeshComp)
    {
        const bool bShowDebugWater = bShowDebugProceduralSurface && bEnableWaterShell;
        WaterMeshComp->SetVisibility(bShowDebugWater, true);
        WaterMeshComp->SetHiddenInGame(!bShowDebugWater);
        WaterMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    const ECollisionEnabled::Type HISMCollision =
        (bEnableHISMTileRendering && bEnableHISMTileCollision)
        ? ECollisionEnabled::QueryOnly
        : ECollisionEnabled::NoCollision;

    auto ApplyHISMState = [this, HISMCollision](UHierarchicalInstancedStaticMeshComponent* Comp)
    {
        if (!Comp)
        {
            return;
        }

        Comp->SetVisibility(bEnableHISMTileRendering, true);
        Comp->SetHiddenInGame(!bEnableHISMTileRendering);
        Comp->SetCollisionEnabled(HISMCollision);
        Comp->SetCollisionObjectType(ECC_WorldStatic);
        Comp->SetCollisionResponseToAllChannels(ECR_Block);
    };

    ApplyHISMState(PlainTileHISMComp);
    ApplyHISMState(ForestTileHISMComp);
    ApplyHISMState(MountainTileHISMComp);

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

// ===================================================================
//  R11 §10.2：SetHighlightLUT —— 由 UCellHighlightComponent::BeginPlay 调用
//
//  把刚则建好的 1×NumCells / R8G8 LUT 注入现有 TerrainMID。同时把 LUT 指针保存到
//  HighlightLUT 字段——下次 Rebuild 时（OnConstruction / PIE 退出钩子）会重建 MID，
//  ApplyTerrainMaterial_ 末尾会反向填回新 MID。
//
//  详见 Docs/HexHighlightInteractionPlan.md §8.4。
// ===================================================================
void APlanetTessellatedMesh::SetHighlightLUT(UTexture2D* InLUT)
{
    HighlightLUT = InLUT;
    if (TerrainMID && InLUT)
    {
        TerrainMID->SetTextureParameterValue(TEXT("CellHighlightLUT"), InLUT);
    }
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
    if (!bEnableHISMInstanceHighlight)
    {
        return;
    }

    if (NewCellId != INDEX_NONE && !CellIdToHISMInstance.IsValidIndex(NewCellId))
    {
        return;
    }

    if (NewCellId == INDEX_NONE)
    {
        LastHISMPickedCellId = INDEX_NONE;
        if (HISMCurrentHoverCellId == INDEX_NONE)
        {
            return;
        }
        if (HISMPendingHoverCellId != INDEX_NONE)
        {
            HISMPendingHoverCellId = INDEX_NONE;
            HISMHoverFadeTimer = HISMHoverFadeDuration;
        }
        return;
    }

    LastHISMPickedCellId = NewCellId;

    if (HISMCurrentHoverCellId == INDEX_NONE)
    {
        HISMCurrentHoverCellId = NewCellId;
        HISMPendingHoverCellId = NewCellId;
        HISMHoverFadeTimer = 0.0f;
        WriteHISMHighlightForCell_(NewCellId, false);
        RefreshG4CapturePreviewCellsForActionTarget_(NewCellId, true);
        return;
    }

    if (NewCellId == HISMCurrentHoverCellId)
    {
        HISMPendingHoverCellId = NewCellId;
        HISMHoverFadeTimer = 0.0f;
        return;
    }

    const int32 OldHover = HISMCurrentHoverCellId;
    HISMCurrentHoverCellId = NewCellId;
    HISMPendingHoverCellId = NewCellId;
    HISMHoverFadeTimer = 0.0f;
    WriteHISMHighlightForCell_(OldHover, false);
    RefreshG4CapturePreviewCellsForActionTarget_(OldHover, false);
    WriteHISMHighlightForCell_(NewCellId, false);
    RefreshG4CapturePreviewCellsForActionTarget_(NewCellId, true);
}
