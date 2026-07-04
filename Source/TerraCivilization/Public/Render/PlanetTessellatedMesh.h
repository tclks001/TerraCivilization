// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "TerraPiecePresentationTypes.h"
#include "Templates/UniquePtr.h"
#include "WorldGenSettings.h"   // T4：UPROPERTY 直接持有 FWorldGenSettings → 完整类型可见
#include "PlanetTessellatedMesh.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UTexture2DArray;
class UAnimationAsset;
class FSphereTopology;
class FMeshDisplacementBuilder;
class FWorldGenerator;   // T4：TUniquePtr<FWorldGenerator>，避免在头文件 include "WorldGenerator.h"
class FTerraGameplayContainer;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class USkeletalMesh;
class UTerraPiecePresentationManager;
struct FHitResult;

struct FTerraHISMCellInstanceRef
{
    UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
    int32 InstanceIndex = INDEX_NONE;

    bool IsValid() const { return Component != nullptr && InstanceIndex != INDEX_NONE; }
};

enum class ETerraG1DebugPieceType : uint8
{
    Base,
    Infantry,
    Cavalry,
    Archer,
};

struct FTerraG1DebugPiece
{
    int32 FactionId = INDEX_NONE;
    int32 CellId = INDEX_NONE;
    ETerraG1DebugPieceType PieceType = ETerraG1DebugPieceType::Infantry;
};

/**
 * APlanetTessellatedMesh
 *
 * T 阶段（TessellatedMesh）主验收 actor —— 详见 Docs/TessellatedMeshDesign.md §2.1。
 *
 * 与 R8 的 APlanetTopologyDebugMesh 的关系：两个 actor 并存。Debug 用作 R8 验收的基准回归对照，
 * Tessellated 是 T 阶段开始的"逻辑/渲染拓扑解耦"主路线（默认 sub=3 逻辑 + sub=4 渲染）。
 *
 * 子里程碑 T1~T5（逐文件验收）：
 *   - T1 ✅：FMeshDisplacementBuilder + 双拓扑骨架
 *   - T2 ✅：BuildVertexToCoarseTris + ComputeVertexElevationCM + 自检 PASS
 *   - T3 ✅：5 张 LUT + 17 参数 MID + 共享顶点 mesh + 水面 SLW + PIE 退出钩子
 *   - T4（本里程碑）：接入 FCellGeoData.Elevation 真实数据源（详见 Docs/T4_RealElevation.md）
 *   - T5：整体验收清单 A~J
 *
 * 设计稿锚点：
 *   - D1：MeshSubdivisionLevel 默认 4（与 CellSubdivisionLevel 解耦）；
 *   - D2：顶点高度走 cpp 路径（不走 GPU WPO）；
 *   - D5：顶点法线 = +UnitCenter 朝外，Tangent 留空；
 *   - D7：直接消费 FCellGeoData.Elevation（T4）；
 *   - D8：CellTopology 与 MeshTopology 是两个完全独立的 FSphereTopology 实例；
 *   - D11~D17：T3 子里程碑追加（详见 Docs/T3_TerrainMeshRender.md §1）。
 */
UCLASS()
class TERRACIVILIZATION_API APlanetTessellatedMesh : public AActor
{
    GENERATED_BODY()

public:
    APlanetTessellatedMesh();

    /**
     * 显式声明 FVTableHelper 构造函数 + 析构函数 + 在 .cpp 中实现。
     * 必须放 .cpp，否则 UHT 生成的 `DEFINE_VTABLE_PTR_HELPER_CTOR_NS` 会在头文件语境里
     * 内联化 `TUniquePtr<FSphereTopology>` / `TUniquePtr<FMeshDisplacementBuilder>` 的析构器，
     * 触发 "C4150: 删除指向不完整类型的指针" 编译错。
     * 详见 Docs/AgentWorkflow.md §3.9（R8 水面 Actor 经典踩坑）。
     */
    APlanetTessellatedMesh(FVTableHelper& Helper);
    virtual ~APlanetTessellatedMesh();

    //----------------------------------------------------------
    // §2.1 / §1.3 拍板参数（编辑器可调）
    //----------------------------------------------------------

    /** Cell 拓扑细分层级（玩法 / WorldGen 用）。默认 sub=3 → 642 cells。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess",
              meta = (ClampMin = "1", ClampMax = "5"))
    int32 CellSubdivisionLevel = 3;

    /**
     * Mesh 渲染拓扑细分层级（仅渲染用）。默认 sub=4 → 2562 verts / 5120 tris。
     * 必须 >= CellSubdivisionLevel（设计稿 §3.3 同构性约束）；
     * 上限 6 是因为 sub=7+ 的 OnConstruction 重建会卡顿（设计稿 §6 风险点 4）。
     */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess",
              meta = (ClampMin = "1", ClampMax = "6"))
    int32 MeshSubdivisionLevel = 4;

    /** 球半径（cm）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess",
              meta = (ClampMin = "100.0"))
    float GlobeRadiusCM = 15000.0f;

    /** Elevation [-1, 1] → cm 的缩放（详见 D7）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess",
              meta = (ClampMin = "0.0", ClampMax = "5000.0"))
    float ElevationScaleCM = 500.0f;

    /** 水面 mesh 半径（cm，相对球心）。暂行常量，W3 后由 WorldGen 注入。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess",
              meta = (ClampMin = "100.0"))
    float WaterRadiusCM = 15100.0f;

    /** 启用水面层（同 R8 接口；T3 起 RebuildWaterMesh_ 真实创建水面 mesh）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess")
    bool bEnableWaterShell = true;

    /** 地形材质（T3 起接入到 TerrainMeshComp）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess")
    TObjectPtr<UMaterialInterface> TerrainMaterial;

    /** 水面材质（T3 起接入到 WaterMeshComp）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess")
    TObjectPtr<UMaterialInterface> WaterMaterial;

    //----------------------------------------------------------
    // T3：R8 全套材质参数（与 APlanetTopologyDebugMesh 完全对齐）
    //
    // 详见 Docs/T3_TerrainMeshRender.md §7 头文件增量。
    //----------------------------------------------------------

    /** R5 软边过渡带宽度（绝对弧度，与 R8 actor 同制）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R8",
              meta = (ClampMin = "0.0001", ClampMax = "0.5"))
    float EdgeWidth = 0.05f;

    /** R6 边界噪声振幅（弧度）。0 退化为 R5 直边；与 EdgeWidth 正交。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R8",
              meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float NoiseAmplitude = 0.0f;

    /** R6 边界噪声频率（每弧度周期数）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R8",
              meta = (ClampMin = "0.1", ClampMax = "100.0"))
    float NoiseScale = 10.0f;

    /** R7 Triplanar 三平面混合锐度（默认 4.0）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R8",
              meta = (ClampMin = "1.0", ClampMax = "16.0"))
    float TriplanarSharpness = 4.0f;

    /** R7 Triplanar 纹理周期长度（cm/周期，跨星球大小不需要变）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R8",
              meta = (ClampMin = "10.0", ClampMax = "10000.0"))
    float TileScale = 100.0f;

    /** R3 placeholder：每 Cell 的 LayerIndex 用伪随机值填充时的层数上限。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R8",
              meta = (ClampMin = "1", ClampMax = "256"))
    int32 NumLayersHint = 32;

    /** R8 placeholder 配方哈希（17 配方）。T3 阶段默认 true；T4 联调 W4 时关闭。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R8")
    bool bUseR8PlaceholderRecipes = true;

    /**
     * R8.2：SHFRM raymarch 沿视线最大深度（cm）。
     *
     *   T 阶段几何域已通过 cpp 顶点位移完成 macro Elevation（公里级）；
     *   R8.2 在材质域追加 micro SHFRM（厘米级）。两者量纲完全正交，无需互改。
     *
     *   HLSL 端把 LUT3.G [0,1] 归一化值乘以本参数还原为实际 HeightScaleCM。
     *   默认 75 = HeightScaleCM 上限 50（Mountain.Peak）× 1.5 倍冗余。
     *
     *   详见 Docs/R8.2_SphericalHeightFieldRaymarching.md §4.6 / §8.2 D8.2.7。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R8.2",
              meta = (ClampMin = "10.0", ClampMax = "500.0"))
    float MaxRaymarchDepthCM = 75.0f;

    /**
     * R8.2：raymarch 线性步数（Editor 侧可见值）。
     *
     *   ⚠ HLSL Custom 节点 `[unroll]` 必须编译期常量，所以 R8.2 的 HLSL 写死 16 步。
     *   本 UPROPERTY 仅作为 MID Scalar Parameter 暴露给 Editor 面板。
     *
     *   详见 Docs/R8.2_SphericalHeightFieldRaymarching.md §4.6。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R8.2",
              meta = (ClampMin = "4", ClampMax = "32"))
    int32 MaxRaymarchSteps = 16;

    /** R8：3-slice BaseColor 数组（Soil / Rock / Forest，详见 R8 §3.1）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|R8")
    TObjectPtr<UTexture2DArray> PBRBaseAlbedo;

    /** R8：3-slice Normal 数组（NormalDX）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|R8")
    TObjectPtr<UTexture2DArray> PBRBaseNormal;

    /** R8：3-slice Roughness 数组。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|R8")
    TObjectPtr<UTexture2DArray> PBRBaseRoughness;

    /** R8：3-slice Height 数组（T 阶段径向位移走 cpp 路径，本字段挂上避免 fallback 警告即可）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|R8")
    TObjectPtr<UTexture2DArray> PBRBaseHeight;

    /** R7 旧路径 Texture2DArray（向后兼容；T3 起首选 PBRBase* 三件套，本字段供历史 R7 材质消费）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|R8")
    TObjectPtr<UTexture2DArray> TerrainAlbedoArray;

    /** R7 旧路径 Normal 数组（与 TerrainAlbedoArray 配套）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|R8")
    TObjectPtr<UTexture2DArray> TerrainNormalArray;

    // R8.1 baseline = 19 inputs，R8.2 追加 5 项（pbrbaseheight/cameravector/globeradiuscm/
    // maxraymarchdepthcm/cameraworldpos）= 24 inputs；R11 在 Step 8 末尾追加 5 项 Inputs
    // （CellHighlightLUT + HoverColor + SelectColor + HighlightPadding + HighlightStrength）
    // → 反射诊断期望表共 29 inputs（详见 R8.2 §5.2 + HexHighlightInteractionPlan.md §9 + DiagnoseR8Material_）。

    //----------------------------------------------------------
    // R11：Hex/Pent 高亮描边带（hover + select 统一管线）
    //
    // 详见 Docs/HexHighlightInteractionPlan.md §6。这 4 个参数仅以 MID Vector / Scalar
    // 参数形式注入 TerrainMID，**不走 LUT**——颜色 / 描边宽度 / 全局强度都是
    // 跨 cell 全局设定，提供运行期调色能力（例如"我方选中蓝 / 敌方选中红"未来可在
    // 必要时上升为多个 SelectColor + LUT.B 分组编号）。
    //
    // 调色取值推荐：
    //   - HighlightHoverColor  = 暖金（(1.00, 0.85, 0.10)）——Civ 系默认黑金色调。
    //   - HighlightSelectColor = 青蓝（(0.20, 0.90, 1.00)）——与 hover 反差明显，且在合色
    //                            17 酒色 tint 上均可辨识。
    //   - HighlightPadding     = 0.03（弧度，约 1.7°；R11.1 修订：新算法下 padding 是从 hex 边向内延伸的
    //                                    角距离带宽——sub=3 时 cell 张角约 0.2 rad ≈ 12°，0.03 rad 描边带占约 15%）。
    //   - HighlightStrength    = 1.5（加性叠加到 albedo，1.5 倍令描边越过环境光仔细看得出）。
    //----------------------------------------------------------

    /** R11：hover 状态 cell 描边颜色（RGB）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R11 Highlight")
    FLinearColor HighlightHoverColor = FLinearColor(1.0f, 0.85f, 0.10f, 1.0f);

    /** R11：select 状态 cell 描边颜色（RGB）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R11 Highlight")
    FLinearColor HighlightSelectColor = FLinearColor(0.20f, 0.90f, 1.00f, 1.0f);

    /** R11：描边带宽度（**弧度**语义，R11.1 修订）——从 hex 边向内延伸的角距离带宽。
     *  sub=3 时 cell 张角约 0.2 rad；推荐 0.02~0.05 rad（cell 张角的 10%~25%），默认 0.03。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R11 Highlight",
              meta = (ClampMin = "0.005", ClampMax = "0.15"))
    float HighlightPadding = 0.03f;

    /** R11：全局描边强度倍率（加性叠加到 albedo，>1 表示超过环境光多亮）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|R11 Highlight",
              meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float HighlightStrength = 1.50f;

    /**
     * R11：由 UCellHighlightComponent::BeginPlay() 调用，把刚则建好的 1×NumCells / R8G8
     * 高亮 LUT 注入现有 TerrainMID。该函数会在内部保存 LUT 指针，以便后续 Rebuild
     * 后重建 MID 时反向填回。
     */
    void SetHighlightLUT(class UTexture2D* InLUT);

    //----------------------------------------------------------
    // SimpleGameplay：WorldGen 三地形参数
    //----------------------------------------------------------

    /**
     * SimpleGameplay WorldGen 参数（RandomSeed / 山脉数量与平均节点数 / 森林数量与平均节点数 / TerrainSet）。
     * 改动后 OnConstruction 自动重跑 Generator->Generate() → ComputeCellElevation_ 取新 Elevation。
     * 与 APlanetTopologyDebugMesh 的 WorldGenSettings 字段语义完全一致（两 actor 各跑各自的实例）。
     */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|WorldGen")
    FWorldGenSettings WorldGenSettings;

    /**
     * D21：true → 退回 T3 余弦 ramp（赤道 +1，两极 -1，回归对照路径）；
     *      false（默认）→ 走 Generator->GetCellData()[c].Elevation 真实数据。
     * 联调期排查 mesh 形变异常时一键回到 T3 baseline。
     */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|WorldGen")
    bool bUsePlaceholderElevation = false;

    //----------------------------------------------------------
    // SimpleGameplay：HISM 静态网格瓦片渲染
    //----------------------------------------------------------

    /** true：主视觉使用每 Cell 一个 StaticMesh 实例的 HISM 瓦片渲染。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    bool bEnableHISMTileRendering = true;

    /** true：HISM 组件开启 QueryOnly 碰撞，可直接参与鼠标拾取。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    bool bEnableHISMTileCollision = true;

    /** 平原 Cell 使用的烘焙 StaticMesh 资产。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UStaticMesh> PlainTileStaticMesh;

    /** 森林 Cell 使用的烘焙 StaticMesh 资产。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UStaticMesh> ForestTileStaticMesh;

    /** 山脉 Cell 使用的烘焙 StaticMesh 资产。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UStaticMesh> MountainTileStaticMesh;

    /** 生成瓦片资产时使用的 BaseRadius。默认 100，与 TerraSphericalTileGenerator 默认值一致。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles", meta = (ClampMin = "1.0"))
    float HISMTileSourceRadiusCM = 100.0f;

    /** HISM 瓦片相对 GlobeRadiusCM 的额外半径偏移。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    float HISMTileRadiusOffsetCM = 0.0f;

    /** HISM 瓦片额外统一缩放。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|HISM Tiles", meta = (ClampMin = "0.001"))
    float HISMTileAdditionalUniformScale = 1.0f;

    /** true：启用 HISM 实例拾取与 PerInstanceCustomData tile 边缘高亮。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight")
    bool bEnableHISMInstanceHighlight = true;

    /** HISM hover 离开球体后的防抖保留时间；语义与旧 CellHighlightComponent 保持一致。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight",
              meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float HISMHoverFadeDuration = 0.5f;

    /** 材质侧 UV 边缘高亮内半径建议值；C++ 会写入 HISM 动态材质参数。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HISMHighlightInnerRadius = 0.40f;

    /** 材质侧 UV 边缘高亮外半径建议值；C++ 会写入 HISM 动态材质参数。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|HISM Highlight",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HISMHighlightOuterRadius = 0.50f;

    /** 最近一次 HISM hover 解析出的 CellId，供编辑器 / 蓝图调试查看。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Tess|HISM Highlight")
    int32 LastHISMPickedCellId = INDEX_NONE;

    /** 最近一次 HISM click 解析出的 CellId，供编辑器 / 蓝图调试查看。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlanetTopology|Tess|HISM Highlight")
    int32 LastHISMClickedCellId = INDEX_NONE;

    /** true：显示旧 ProceduralMesh 整球地表，用作 debug 对照。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|Debug Render")
    bool bShowDebugProceduralSurface = false;

    /** true：即使旧整球地表不可见，也保留 QueryOnly 复杂碰撞作为鼠标拾取兜底。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|Debug Render")
    bool bUseDebugProceduralCollision = true;

    //----------------------------------------------------------
    // SimpleGameplay G1：棋子初始化调试绘制
    //----------------------------------------------------------

    /** true：用 DrawDebugSphere 显示 G1 初始棋子布局。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|SimpleGameplay G1")
    bool bEnableG1DebugPieces = true;

    /** G1 调试棋子球半径（cm）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|SimpleGameplay G1", meta = (ClampMin = "10.0", ClampMax = "1000.0"))
    float G1DebugPieceRadiusCM = 140.0f;

    /** G1 调试棋子相对球面外抬高度（cm）。 */
    UPROPERTY(EditAnywhere, Category = "PlanetTopology|Tess|SimpleGameplay G1", meta = (ClampMin = "0.0", ClampMax = "5000.0"))
    float G1DebugPieceHeightOffsetCM = 260.0f;

    //----------------------------------------------------------
    // SimpleGameplay P1：真实棋子模型表现
    //----------------------------------------------------------

    /** true：PIE / 游戏运行时启用真实棋子 Actor 表现。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    bool bEnableP1PiecePresentation = true;

    /** true：P1 真实棋子启用时隐藏 G1 调试球，避免模型与球重叠。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    bool bHideG1DebugPiecesWhenP1IsActive = true;

    /** P1 棋子模型相对球面的外抬高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
    float P1PieceRadiusOffsetCM = 260.0f;

    /** P1 所有人物模型统一缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "100.0"))
    float P1PieceUniformScale = 1.0f;

    /** P1 模型组件相对棋子 Actor 根节点的位置修正。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FVector P1MeshRelativeLocation = FVector::ZeroVector;

    /** P1 模型组件相对棋子 Actor 根节点的导入朝向修正。默认 Yaw=90，用于修正当前人物模型“逻辑朝前实际朝右”的资源坐标差异。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    FRotator P1MeshRelativeRotation = FRotator(0.0f, 90.0f, 0.0f);

    /** 主将模型。推荐挂 Content/Animations/Adventurers/Characters/Mage1。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1CommanderMesh;

    /** 步兵模型。推荐挂 Content/Animations/Adventurers/Characters/Knight1。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1InfantryMesh;

    /** 骑兵占位人物模型。P1 暂不挂马，推荐先挂 Knight1。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1CavalryMesh;

    /** 弓兵模型。推荐挂 Content/Animations/Adventurers/Characters/Ranger1。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<USkeletalMesh> P1ArcherMesh;

    /** P2 待机动画。推荐 Rig_Medium_GeneralIdle_A。为空时只更新驱动状态，不强制播放动画。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P2IdleAnimation;

    /** P2 普通移动动画。推荐 Rig_Medium_MovementBasicWalking_A。为空时仍播放位移插值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P2MoveAnimation;

    /** P2 跳跃动画。推荐 Rig_Medium_MovementBasicJump_Full_Short。为空时仍播放跳跃弧线。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UAnimationAsset> P2JumpAnimation;

    /** P2 普通移动单段时长（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P2MoveDurationSeconds = 0.35f;

    /** P2 跳跃单段时长（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.001", ClampMax = "10.0"))
    float P2JumpDurationSeconds = 0.55f;

    /** P2 跳跃最高额外外抬高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
    float P2JumpHeightCM = 650.0f;

    /** P2.5：true 时通过 HISM 碰撞射线修正棋子 Actor 的球面高度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    bool bEnableP2_5HISMPieceHeightTrace = true;

    /** P2.5：棋子高度射线从球面外侧额外多远开始（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float P2_5PieceHeightTraceStartOffsetCM = 5000.0f;

    /** P2.5：棋子高度射线穿过球心后继续多远（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float P2_5PieceHeightTracePastCenterOffsetCM = 1000.0f;

    /** P2.5：true 时输出棋子 HISM 高度射线诊断日志。验收后可关闭。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    bool bDebugP2_5HISMPieceHeightTrace = true;

    //----------------------------------------------------------
    // SimpleGameplay G2.5：视角与当前阵营提示
    //----------------------------------------------------------

    /** true：回合开始自动切到当前阵营大本营上方，选中棋子时自动旋转视角对准。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G2.5")
    bool bEnableG2_5CameraAssist = true;

    /** 回合开始时摄像机位于大本营球面外侧的额外高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G2.5", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float G2_5TurnStartCameraHeightCM = 8000.0f;

    /** C2：true 时回合开始切到当前阵营活棋子的战区中心斜俯视；false 时保留 G2.5 主将/大本营正上方视角。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C2 Camera", meta = (DisplayName = "Enable C2 Turn Start War Zone Camera"))
    bool bEnableC2TurnStartWarZoneCamera = true;

    /** C2：回合开始斜俯视时，相机到战区中心目标点的距离（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C2 Camera", meta = (ClampMin = "1000.0", ClampMax = "200000.0"))
    float C2TurnStartCameraDistanceCM = 18000.0f;

    /** C2：回合开始斜俯视时，视线中心方向与目标点地面切平面的夹角（度）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay C2 Camera", meta = (ClampMin = "5.0", ClampMax = "85.0"))
    float C2TurnStartCameraTiltDeg = 55.0f;

    /** 当前阵营所有棋子脚下 Cell 的淡粉色提示。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G2.5")
    FLinearColor G2_5CurrentFactionPieceColor = FLinearColor(1.0f, 0.45f, 0.68f, 1.0f);

    /** hover 到当前阵营棋子时的加红提示色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G2.5")
    FLinearColor G2_5CurrentFactionPieceHoverColor = FLinearColor(1.0f, 0.22f, 0.32f, 1.0f);

    //----------------------------------------------------------
    // SimpleGameplay G3：跳跃与调试
    //----------------------------------------------------------

    /** true：调试时回合结束不跳到下一个玩家，下一回合仍保持当前玩家。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G3|Debug")
    bool bG3DebugKeepSameFactionOnEndTurn = false;

    /** hover 到可行走 / 可跳跃淡蓝落点时使用的加深颜色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G3")
    FLinearColor G3ActionTargetHoverColor = FLinearColor(0.08f, 0.45f, 1.0f, 1.0f);

    /** hover 到某个可吃子落点时，该落点对应可吃目标使用的加深红色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G4")
    FLinearColor G4CaptureTargetHoverColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

    //----------------------------------------------------------
    // SimpleGameplay G8：手动视角轨道控制
    //----------------------------------------------------------

    /** true：允许 PlayerController 通过 WSAD + 滚轮驱动球面轨道相机。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8")
    bool bEnableG8ManualCameraControl = true;

    /** G8：每秒经纬度变化速度（度 / 秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "1.0", ClampMax = "360.0"))
    float G8CameraOrbitDegreesPerSecond = 45.0f;

    /** G8：每次滚轮缩放的高度步长（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "10.0", ClampMax = "100000.0"))
    float G8CameraZoomStepCM = 800.0f;

    /** G8：相机允许的最小离地高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
    float G8CameraMinHeightOffsetCM = 2500.0f;

    /** G8：相机允许的最大离地高度（cm）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|Tess|SimpleGameplay G8", meta = (ClampMin = "0.0", ClampMax = "200000.0"))
    float G8CameraMaxHeightOffsetCM = 30000.0f;

    //----------------------------------------------------------
    // 生命周期
    //----------------------------------------------------------
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
    virtual bool ShouldTickIfViewportsOnly() const override;
#endif

    // 注：不 override BeginDestroy()。
    //   1) BeginDestroy 是 UObject GC 的早期回调（对象逻辑销毁、但 C++ 内存还活着），
    //      此时手动 Reset() TUniquePtr 等于过早释放非 UObject 字段，与 GC 的后续阶段
    //      （以及编辑器关闭路径下的 TSparseArray / TMap 重定位）冲突，崩溃在 ~TMap::ReallocTo。
    //   2) TUniquePtr<FSphereTopology> / TUniquePtr<FMeshDisplacementBuilder> 完全由
    //      ~APlanetTessellatedMesh()（在 .cpp 中显式定义、可见完整类型）自动释放，无需提前 Reset。
    //   3) 与 APlanetTopologyDebugMesh 写法对齐（构造函数末尾绑定 PIE 钩子 + 析构函数中解绑）。

    /**
     * 重建网格。Editor 中拖入关卡或修改属性时（OnConstruction）会自动调用，
     * 也可由外部代码主动触发（例如改 MeshSubdivisionLevel 后）。
     */
    UFUNCTION(CallInEditor, Category = "PlanetTopology|Tess")
    void Rebuild();

    /** 尝试把鼠标命中的 HISM 实例解析成 CellId。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    bool TryResolveHISMHitToCellId(const FHitResult& Hit, int32& OutCellId) const;

    /** 输入层 hover 命中 HISM 时调用；成功处理返回 true。 */
    bool HandleHISMHoverHit(const FHitResult& Hit);

    /** 输入层 click 命中 HISM 时调用；成功处理返回 true。 */
    bool HandleHISMClickHit(const FHitResult& Hit);

    /** 输入层右键撤销当前未提交行动时调用；成功处理返回 true。 */
    bool HandleHISMUndo();

    /** 鼠标离开 HISM 瓦片时调用，只清 hover，不清 select。 */
    void ClearHISMHover();

    /** 清空全部 HISM hover / select 高亮。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    void ClearAllHISMHighlights();

    /** 获取当前 HISM hover CellId。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    int32 GetLastHISMPickedCellId() const { return LastHISMPickedCellId; }

    /** 获取最近一次 HISM click CellId。 */
    UFUNCTION(BlueprintCallable, Category = "PlanetTopology|Tess|HISM Highlight")
    int32 GetLastHISMClickedCellId() const { return LastHISMClickedCellId; }

private:
    /** 整体重建：双拓扑 + Builder + 5 张 LUT + 双 mesh + 材质（T3 完整路径）。 */
    void RebuildAll_();

    /** 私有：构造（或重建）两个独立 FSphereTopology 实例 + 水面 sub=3 拓扑（lazy）。 */
    void RebuildTopologies_();

    /** 私有：T1 验收用 —— 输出 "CellTopo: X cells / MeshTopo: Y verts / Z tris" 到 Output Log。 */
    void LogTopologyStats_() const;

    //----------------------------------------------------------
    // T3 新增：算法 / 数据 helper（详见 Docs/T3_TerrainMeshRender.md §3）
    //----------------------------------------------------------

    /** D12：placeholder cell elevation = 余弦 ramp（赤道 +1，两极 -1）；T4 切到真实数据。 */
    void ComputeCellElevation_(TArray<float>& OutElev) const;

    /**
     * 给定 mesh 渲染三角形索引（MeshTopology->PrimalTris[T] 的 T），通过 PrimalTriTreeNodes
     * 爬升到粗 CellTopology 三角形，输出该粗 Tri 的 3 个 cell ID。
     *
     * ⚠ T3 第二版核心算法：R8 PS 端的球面 Voronoi argmax(dot(dir, V_i)) 在 c0/c1/c2 间仲裁，
     *    要求一个渲染三角形内部的 (c0, c1, c2) 必须保持常量（不允许光栅化插值），即
     *    "三个顶点写同一组 (cA, cB, cC)"。这就要求渲染三角形必须是**独立顶点路径**——
     *    共享顶点会让相邻三角形在共享顶点处的 (c0, c1, c2) 互相覆盖、插值后产生混乱的
     *    cell ID，PS 端选错 cell。详见 AgentWorkflow.md §3.18。
     *
     *    本函数输出的 (cA, cB, cC) 即为该 mesh 三角形所属的粗 Cell 三角形（dir 落在该
     *    粗 Tri 内时 PS 仲裁结果**100% 是 cA/cB/cC 之一**）。返回 false 表示失败。
     */
    bool FindCoarseCellsForMeshTri_(int32 MeshTriIdx,
        int32& OutCA, int32& OutCB, int32& OutCC) const;

    /** VertexColor 预查（17 配方哈希，与 R8 公式一致）。 */
    FLinearColor ComputeVertexLayerColor_(int32 CellId) const;

    //----------------------------------------------------------
    // T3 新增：5 张 LUT + 双 mesh + 材质（详见 Docs/T3_TerrainMeshRender.md §4 / §5 / §6）
    //----------------------------------------------------------

    /** R3 LUT：1×NumCells、PF_B8G8R8A8、R 通道写 LayerIndex（T3 placeholder = R8 17 配方哈希）。 */
    void RebuildCellAttrLUT_(int32 NumCells);

    /** R4 LUT：1×NumCells、PF_A32B32G32R32F、RGB = UnitCenter，A = isPentagon。 */
    void RebuildCellDirLUT_(int32 NumCells);

    /** R8 LUT：1×NumCells、PF_FloatRGBA、RGB = Tint, A = HueShift。 */
    void RebuildCellTintLUT_(int32 NumCells);

    /** R8 LUT：1×NumCells、PF_FloatRGBA、R=Sat, G=Bri, B=RoughMin, A=RoughMax。 */
    void RebuildCellHSVRoughLUT_(int32 NumCells);

    /** R8 LUT：1×NumCells、PF_FloatRGBA、R=NormalStr, G=HeightScale, B=Specular, A=TriScale。 */
    void RebuildCellNSpecLUT_(int32 NumCells);

    /** 陆地 mesh 灌装（共享顶点路径，详见 §3.4）。 */
    void RebuildTerrainMesh_();

    /** 水面 mesh 灌装（独立 sub=3 拓扑，详见 §6.1，与 R8 RebuildWaterMesh_ 等价）。 */
    void RebuildWaterMesh_();

    /** SimpleGameplay：按 WorldGen 三地形输出重建平原 / 森林 / 山脉三套 HISM 实例。 */
    void RebuildHISMTileInstances_();

    /** SimpleGameplay：统一应用 HISM 与旧 ProceduralMesh 的显示 / 碰撞开关。 */
    void ApplyRenderModeVisibility_();

    /** SimpleGameplay：统一设置 HISM 组件的最终高亮颜色自定义数据通道、动态材质参数与高亮初值。 */
    void PrepareHISMHighlightComponent_(UHierarchicalInstancedStaticMeshComponent* Comp);

    /** SimpleGameplay：把单个 Cell 当前 Gameplay/hover 逻辑合成为最终 RGB + Intensity 自定义数据。 */
    void WriteHISMHighlightForCell_(int32 CellId, bool bMarkRenderStateDirty = true);
    void RefreshG4CapturePreviewCellsForActionTarget_(int32 ActionTargetCellId, bool bMarkLastRenderStateDirty = true);
    void UpdateHISMHoverCell_(int32 NewCellId);

    void RebuildGameplay_();

    /** SimpleGameplay G2：刷新 Gameplay 容器报告的脏 Cell 高亮。 */
    void RefreshGameplayHighlights_(const TArray<int32>& DirtyCellIds);

    /** SimpleGameplay G2.5：刷新指定阵营所有棋子所在 Cell 的 HISM 高亮。 */
    void RefreshFactionPieceHighlights_(int32 FactionId);

    /** SimpleGameplay G2.5：刷新当前阵营所有棋子所在 Cell 的 HISM 高亮。 */
    void RefreshCurrentFactionPieceHighlights_();

    /** SimpleGameplay G2.5：根据 CellId 计算球面 Cell 中心世界坐标。 */
    bool GetCellSurfaceWorldPosition_(int32 CellId, float RadiusOffsetCM, FVector& OutWorldPosition) const;

    /** SimpleGameplay G2.5：回合开始时移动到 Cell 上方并朝向 Cell，或仅旋转当前视角对准 Cell。 */
    void FocusCameraOnCell_(int32 CellId, bool bMoveCamera);

    /** SimpleGameplay C2：回合开始时视角切到当前阵营活棋子战区中心斜俯视。成功处理返回 true。 */
    bool FocusCameraOnCurrentFactionWarZone_();

    /** SimpleGameplay C2：计算当前阵营活棋子的平均球面方向。 */
    bool TryBuildCurrentFactionWarZoneDirection_(FVector& OutLocalWarZoneDir) const;

    /** SimpleGameplay C2：计算当前阵营主将/大本营方向，供斜俯视方位参考。 */
    bool TryBuildCurrentFactionCommanderDirection_(FVector& OutLocalCommanderDir) const;

    /** SimpleGameplay G2.5：回合开始时视角切到当前阵营大本营正上方。 */
    void FocusCameraOnCurrentFactionBase_();

    /** SimpleGameplay P1/P2：根据当前 Gameplay 快照增量同步真实棋子 Actor，可选播放 P2 移动事件。 */
    void SyncP1PiecePresentation_(const TArray<FTerraPiecePresentationMoveEvent>& MoveEvents = TArray<FTerraPiecePresentationMoveEvent>());

    /** SimpleGameplay P1：清空真实棋子 Actor 表现。 */
    void ClearP1PiecePresentation_();

    /** SimpleGameplay P1：把 CellId 转为棋子 Actor 的球面世界 Transform。 */
    bool BuildP1PieceWorldTransform_(int32 CellId, FTransform& OutWorldTransform) const;

    /** SimpleGameplay P2.5：从 Cell 外侧向球心方向射线命中 HISM，以命中点修正棋子高度。 */
    bool TryResolveP2_5PieceHeightFromHISM_(int32 CellId, const FVector& WorldUp, FVector& OutWorldPosition) const;

    /** SimpleGameplay P1/P2：按“背向本阵营主将”规则计算初始棋子朝向。 */
    bool BuildP1PieceWorldTransformForPiece_(const FTerraGameplayPieceState& Piece, const TArray<FTerraGameplayPieceState>& Pieces, FTransform& OutWorldTransform) const;

    /** SimpleGameplay P1：从 Details 面板资产槽生成表现配置。 */
    FTerraPieceVisualConfig BuildP1PieceVisualConfig_() const;

public:
    /** G8：取球心世界坐标。 */
    FVector GetPlanetCenterWorld_() const;

    /** G8：取球半径（cm）。 */
    float GetPlanetRadiusCM_() const { return GlobeRadiusCM; }

    /** G8：把某个世界位置同步成球面轨道参数。C3 后仅保留兼容旧路径。 */
    bool SyncOrbitCameraStateFromWorldPosition(const FVector& CameraWorldPosition, float& InOutLongitudeDeg, float& InOutLatitudeDeg, float& InOutHeightOffsetCM) const;

    /** G8：按球面轨道参数把视角应用到当前 PlayerController / ViewTarget。C3 后仅保留兼容旧路径。 */
    bool ApplyOrbitCameraState(float LongitudeDeg, float LatitudeDeg, float HeightOffsetCM);

    /**
     * C3：从当前相机位置和旋转反推焦点式手动相机状态。仅在进入手动模式的首帧被调用一次；
     * 后续帧不再调用（详见 C3FocusCameraManualControlDesign.md §4.2）。
     */
    bool SyncFocusCameraStateFromView(
        const FVector& CameraWorldPosition,
        const FRotator& CameraWorldRotation,
        FVector& OutFocusUnitDir,
        float& OutDistanceToFocusCM,
        float& OutTiltDeg,
        float& OutYawAroundFocusDeg) const;

    /** C3：按焦点式相机状态把视角应用到当前 PlayerController / ViewTarget。 */
    bool ApplyFocusCameraState(
        const FVector& FocusUnitDir,
        float DistanceToFocusCM,
        float TiltDeg,
        float YawAroundFocusDeg);

    /**
     * C3：在当前焦点局部切平面上沿测地线（大圆）推进焦点，同时用 Rodrigues 旋转把 Yaw
     * 平行运输到新焦点。这样连续按住 A/D 或 W/S 会走一整条大圆，不会退化成纬线。
     * 详见 C3FocusCameraManualControlDesign.md §5.1.3。
     */
    bool OffsetFocusCameraStateOnTangent(
        float RightDeltaDeg,
        float ForwardDeltaDeg,
        FVector& InOutFocusUnitDir,
        float& InOutYawAroundFocusDeg) const;

    /** SimpleGameplay G1/G2：按当前 Gameplay 棋子状态重建调试棋子缓存。 */
    void RebuildG1DebugPieces_();

    /** SimpleGameplay G1：用 DrawDebugSphere 绘制当前调试棋子缓存。 */
    void DrawG1DebugPieces_() const;

    /** 应用 17 参数 MID 到 TerrainMeshComp（详见 §5.1）。 */
    void ApplyTerrainMaterial_(int32 NumCells);

    /** D16：R8 合规性反射诊断（17 inputs 检查），仅 WITH_EDITORONLY_DATA。 */
    void DiagnoseR8Material_() const;

    /** D15：PIE 退出后材质恢复钩子（详见 AgentWorkflow.md §3.11）。 */
    void OnPostWorldCleanup_(class UWorld* World, bool bSessionEnded, bool bCleanupResources);

    //----------------------------------------------------------
    // 运行时持有
    //----------------------------------------------------------

    /** 逻辑层 sub=CellSubdivisionLevel 拓扑实例（玩法 / WorldGen 共用）。 */
    TUniquePtr<FSphereTopology> CellTopology;

    /** 渲染层 sub=MeshSubdivisionLevel 拓扑实例（仅渲染用）。 */
    TUniquePtr<FSphereTopology> MeshTopology;

    /**
     * D13：水面层独立 sub=3 拓扑实例。仅在首次 RebuildWaterMesh_ 时 lazy-build，
     * 之后复用——sub 不变 → 顶点位置只是按 (GlobeRadiusCM + Offset) 缩放。
     */
    TUniquePtr<FSphereTopology> WaterTopology;

    /** 顶点位移预计算器（T2 起填充实际算法）。 */
    TUniquePtr<FMeshDisplacementBuilder> Displacement;

    /**
     * D18 / D20：WorldGen 主类实例。Rebuild() 中按顺序：
     *   RebuildTopologies_ → new Displacement + BuildVertexToCoarseTris → RunSelfCheckT2
     *     → Generator.Reset() → MakeUnique<FWorldGenerator>(CellTopology, Settings) → Generate()
     *     → 5x LUT → RebuildTerrainMesh_（内部消费 ComputeCellElevation_ → Generator->GetCellData()）
     *
     * 生命周期必须于 CellTopology 之后、于 ComputeCellElevation_ 之前
     * （详见 Docs/T4_RealElevation.md §1 D20）。
     * 与 APlanetTopologyDebugMesh::Generator 字段语义完全一致。
     */
    TUniquePtr<FWorldGenerator> Generator;

    //----------------------------------------------------------
    // T3：5 张动态 LUT + MID（运行期重建）
    //----------------------------------------------------------

    UPROPERTY(VisibleAnywhere, Transient, Category = "PlanetTopology|Tess|R8")
    TObjectPtr<UTexture2D> CellAttrLUT;

    UPROPERTY(VisibleAnywhere, Transient, Category = "PlanetTopology|Tess|R8")
    TObjectPtr<UTexture2D> CellDirLUT;

    UPROPERTY(VisibleAnywhere, Transient, Category = "PlanetTopology|Tess|R8")
    TObjectPtr<UTexture2D> CellTintLUT;

    UPROPERTY(VisibleAnywhere, Transient, Category = "PlanetTopology|Tess|R8")
    TObjectPtr<UTexture2D> CellHSVRoughLUT;

    UPROPERTY(VisibleAnywhere, Transient, Category = "PlanetTopology|Tess|R8")
    TObjectPtr<UTexture2D> CellNSpecLUT;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> TerrainMID;

    /** R11：高亮 LUT（UCellHighlightComponent 拥有，这里仅保存 weak 引用以便 Rebuild 后重套 MID）。 */
    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> HighlightLUT;

    //----------------------------------------------------------
    // T3+ 字段
    //----------------------------------------------------------
    /** 地形 mesh 子组件（T3 起接入）。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess")
    TObjectPtr<UProceduralMeshComponent> TerrainMeshComp;

    /** 水面层 mesh 子组件（T3 起接入）。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess")
    TObjectPtr<UProceduralMeshComponent> WaterMeshComp;

    /** SimpleGameplay：平原瓦片 HISM 组件。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PlainTileHISMComp;

    /** SimpleGameplay：森林瓦片 HISM 组件。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ForestTileHISMComp;

    /** SimpleGameplay：山脉瓦片 HISM 组件。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess|HISM Tiles")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MountainTileHISMComp;

    /** SimpleGameplay P1：真实棋子模型表现管理器。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess|SimpleGameplay P1 Piece Presentation")
    TObjectPtr<UTerraPiecePresentationManager> PiecePresentationManager;

    /** HISM 反查：Plain 组件 InstanceIndex -> CellId。运行期缓存，不暴露给 Details 面板。 */
    TArray<int32> PlainInstanceToCellId;

    /** HISM 反查：Forest 组件 InstanceIndex -> CellId。运行期缓存，不暴露给 Details 面板。 */
    TArray<int32> ForestInstanceToCellId;

    /** HISM 反查：Mountain 组件 InstanceIndex -> CellId。运行期缓存，不暴露给 Details 面板。 */
    TArray<int32> MountainInstanceToCellId;

    /** HISM 正查：CellId -> HISM 组件 + InstanceIndex。运行期缓存，不暴露给 Details 面板。 */
    TArray<FTerraHISMCellInstanceRef> CellIdToHISMInstance;

    /** HISM 当前 hover 高亮 CellId。 */
    int32 HISMCurrentHoverCellId = INDEX_NONE;

    /** HISM 当前鼠标真实指向 CellId；INDEX_NONE 表示处于离开 / fade 状态。 */
    int32 HISMPendingHoverCellId = INDEX_NONE;

    /** HISM hover 离开后的防抖倒计时。 */
    float HISMHoverFadeTimer = 0.0f;

    /** SimpleGameplay G2：棋子、Cell 逻辑、回合和 Gameplay 高亮的总容器。 */
    TUniquePtr<FTerraGameplayContainer> GameplayContainer;

    /** SimpleGameplay G2.5：上一次已刷新底色提示的当前阵营。 */
    int32 G2_5LastHighlightedFactionId = INDEX_NONE;

    /** SimpleGameplay G2.5：上一次已经执行回合开始相机切换的 TurnIndex。 */
    int32 G2_5LastCameraFocusedTurnIndex = INDEX_NONE;

    /** SimpleGameplay G1/G2：当前初始化出的调试棋子缓存。 */
    TArray<FTerraG1DebugPiece> G1DebugPieces;

    //----------------------------------------------------------
    // D15：PIE 退出后材质恢复（详见 AgentWorkflow.md §3.11）
    //----------------------------------------------------------
    /** FWorldDelegates::OnPostWorldCleanup 委托句柄（构造函数末尾注册，析构中解绑）。 */
    FDelegateHandle PostWorldCleanupHandle;
};
