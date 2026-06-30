// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/UniquePtr.h"
#include "WorldGenSettings.h"   // T4：UPROPERTY 直接持有 FWorldGenSettings → 完整类型可见
#include "PlanetTessellatedMesh.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UTexture2DArray;
class FSphereTopology;
class FMeshDisplacementBuilder;
class FWorldGenerator;   // T4：TUniquePtr<FWorldGenerator>，避免在头文件 include "WorldGenerator.h"

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

    //----------------------------------------------------------
    // T4：WorldGen 流水线参数（详见 Docs/T4_RealElevation.md §3.2）
    //----------------------------------------------------------

    /**
     * D19：WorldGen 流水线参数（RandomSeed / PlateCount / SeaLevel / TerrainSet / DebugView 等都在内部）。
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
    // 生命周期
    //----------------------------------------------------------
    virtual void OnConstruction(const FTransform& Transform) override;

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

    //----------------------------------------------------------
    // T3+ 字段
    //----------------------------------------------------------
    /** 地形 mesh 子组件（T3 起接入）。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess")
    TObjectPtr<UProceduralMeshComponent> TerrainMeshComp;

    /** 水面层 mesh 子组件（T3 起接入）。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology|Tess")
    TObjectPtr<UProceduralMeshComponent> WaterMeshComp;

    //----------------------------------------------------------
    // D15：PIE 退出后材质恢复（详见 AgentWorkflow.md §3.11）
    //----------------------------------------------------------
    /** FWorldDelegates::OnPostWorldCleanup 委托句柄（构造函数末尾注册，析构中解绑）。 */
    FDelegateHandle PostWorldCleanupHandle;
};
