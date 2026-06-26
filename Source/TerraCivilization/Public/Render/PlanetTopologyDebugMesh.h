// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/UniquePtr.h"
#include "PlanetTopologyDebugMesh.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class FSphereTopology;

/**
 * APlanetTopologyDebugMesh
 *
 * R1 Step：直接把 FSphereTopology 的 primal mesh（测地线球面）通过
 * UProceduralMeshComponent 渲染出来。
 *
 * 网格构造方式：对每个 Corner（三角形）展开成 3 个独立顶点，
 *   pos_k = Cells[Corner.CellIds[k]].UnitCenter * Radius + ActorLocation
 * 索引缓冲为顺序 (0,1,2,3,4,5,...)，三角形之间不共享顶点。
 *
 * 之所以不复用顶点：
 * - 后续 R2/R3 阶段需要把"我属于哪个 Cell"作为 per-vertex 属性写入；
 *   每三角形顶点独立可以让一个顶点只对应一个 Cell（onehot），
 *   光栅化插值出的重心坐标 (λ₀,λ₁,λ₂) 就是它到三个 Cell 的权重，
 *   这正是 SDF 设计稿 §1~§13 IsoSphere 直渲方案的拓扑前置约定。
 *
 * R1：纯白材质，验证 mesh 几何与拓扑构建。
 * R2：把 (TriCellId0, TriCellId1, TriCellId2, OneHot) 写入 UV1/UV2/UV3，材质里用 λ₀×hash(c₀) 着色。
 * R3：构建 1×NumCells 的 CellAttrLUT，材质里 3 次 Load(LUT) 取每 Cell 的 LayerIndex，按 λᵢ 加权混合。
 * R4：新增 1×NumCells 的 CellDirLUT（FP32 RGBA = (UnitCenter.xyz, isPentagon)），
 *     材质里把判别准则从 argmax(λ) 改为球面 Voronoi argmax(dot(dir, V_i))，
 *     消除 R3 hex/pent 边在 mesh 边中点处的折角，使 cell 边视觉上是测地线大圆弧。
 *     PlanetCenter 通过 MID Vector Parameter 注入，PS 端 dir = normalize(WorldPos - PlanetCenter)。
 *     详见 R4_VoronoiBoundary.md。
 */
UCLASS()
class TERRACIVILIZATION_API APlanetTopologyDebugMesh : public AActor
{
    GENERATED_BODY()

public:
    APlanetTopologyDebugMesh();

    /** 显式声明析构 + VTableHelper 构造，原因同 APlanetBinder：持有 TUniquePtr<前向声明类型>。 */
    virtual ~APlanetTopologyDebugMesh();
    APlanetTopologyDebugMesh(FVTableHelper& Helper);

    /** 正二十面体细分层数。Cells 数 = 10*4^N + 2、Tris 数 = 20*4^N。N=3 -> 642 Cells / 1280 Tris。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlanetTopology", meta = (ClampMin = "1", ClampMax = "6"))
    int32 SubdivisionLevel = 3;

    /** 渲染用的名义球半径（cm）。仅做几何缩放，不参与拓扑。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlanetTopology", meta = (ClampMin = "1.0"))
    float Radius = 15000.0f;

    /**
     * 渲染材质。可在编辑器里指定为任意 Material；为空则使用 PMC 默认白材质
     * （UEngine::DefaultMaterial）。
     *
     * R3 阶段：应指向一个引用了名为 "CellAttrLUT" 的 Texture2D 参数的材质。
     * 本 Actor 会在 Rebuild 时通过 MID 把 CellAttrLUT 这张动态纹理传给材质。
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlanetTopology")
    TObjectPtr<UMaterialInterface> Material;

    /**
     * R3 placeholder：每 Cell 的 LayerIndex 用伪随机值填充时的层数上限。
     *
     * 真正的 LayerIndex 应该由 WorldGen 根据 TerrainTag 决定（详见 SDF 设计稿）；
     * 在 R3 阶段我们还没接入 WorldGen，所以用 (CellId * 2654435761u) %% NumLayersHint
     * 这种 Knuth 哈希做 placeholder。
     *
     * 调小这个值（比如 8）可以让相同 LayerIndex 的 Cell 更频繁出现，便于看到
     * "两个 hex 相邻地形相同时无可见边界" 的合并效果。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlanetTopology|R3", meta = (ClampMin = "1", ClampMax = "256"))
    int32 NumLayersHint = 32;

    /**
     * 是否开启光滑法线。
     * 关闭时每三角形使用面法线（Flat Shading），可以清楚看到正二十面体细分的三角形结构；
     * 开启时把每个顶点的法线设为该 Cell 的 UnitCenter，得到光滑球面（但同顶点位置 = Cell 中心，
     * 球面感比较明显，三角形分界也几乎看不见）。R1 默认关闭，便于检视拓扑。
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlanetTopology")
    bool bSmoothNormals = false;

    //~ AActor
    virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
    //~ End AActor

    /**
     * 重建网格。Editor 中拖入关卡或修改属性时（OnConstruction）会自动调用，
     * 也可由外部代码主动触发（例如改 SubdivisionLevel 后）。
     */
    UFUNCTION(CallInEditor, Category = "PlanetTopology")
    void Rebuild();

    UProceduralMeshComponent* GetMeshComponent() const { return MeshComp; }

private:
    /** 渲染组件。在构造函数里 CreateDefaultSubobject。 */
    UPROPERTY(VisibleAnywhere, Category = "PlanetTopology")
    TObjectPtr<UProceduralMeshComponent> MeshComp;

    /** 拓扑数据。OnConstruction 时按 SubdivisionLevel 构建。 */
    TUniquePtr<FSphereTopology> Topology;

    /**
     * R3：每 Cell 一个像素的 1×N 动态纹理（PF_B8G8R8A8）。
     *   - R 通道（内存第 2 字节）= LayerIndex（uint8）
     *   - GBA 通道留作 R3+ 扩展（LayerDecor / Variant / Mask）
     *
     * 由 RebuildCellAttrLUT_ 创建并填充 placeholder 数据。
     * Filter=Nearest、SRGB=false、AddressX=Clamp。
     */
    UPROPERTY(VisibleAnywhere, Transient, Category = "PlanetTopology|R3")
    TObjectPtr<UTexture2D> CellAttrLUT;

    /**
     * R4：每 Cell 一个像素的 1×N 动态纹理（PF_A32B32G32R32F）。
     *   - R/G/B 通道 = Cells[Cid].UnitCenter（单位向量，FP32 精度）
     *   - A     通道 = bIsPentagon ? 1.0 : 0.0（预留给 R5+ pent 特判）
     *
     * 用途：PS 端用 c0/c1/c2 三次 Load 取得三个 cell 的中心方向，
     * 计算 dot(dir, V_i) 做球面 Voronoi 判别，等位线为测地线大圆弧。
     *
     * 由 RebuildCellDirLUT_ 创建并填充。Filter=Nearest、SRGB=false、AddressX=Clamp。
     * 必须 FP32 —— R16F 对方向夹角精度不够，会让相邻 cell 中心方向被截断到同一格。
     */
    UPROPERTY(VisibleAnywhere, Transient, Category = "PlanetTopology|R4")
    TObjectPtr<UTexture2D> CellDirLUT;

    /**
     * R3：包装外部 Material 的动态实例，用于按 Cell 数动态绑定 CellAttrLUT 参数。
     * 每次 Rebuild 都会重建一次（保证 LUT 大小变化时材质始终绑到正确尺寸的纹理）。
     */
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> MID;

    /**
     * R3：构建/刷新 CellAttrLUT。每次 Rebuild() 都会调用一次。
     *
     * Placeholder 策略：LayerIndex = (CellId * 2654435761u) % NumLayersHint
     *   —— 这是 Knuth 整数哈希常数（黄金分割），保证相邻 CellId 也能落在不同 Layer。
     *
     * R7 接入 WorldGen 后，本函数会被替换为 "按 FCellGeoData[].TerrainTag 的 LayerIndex 填表"。
     */
    void RebuildCellAttrLUT_(int32 NumCells);

    /**
     * R4：构建/刷新 CellDirLUT（1×NumCells、PF_A32B32G32R32F）。每次 Rebuild() 都会调用一次。
     *
     * 像素 (R, G, B, A) = (UnitCenter.x, UnitCenter.y, UnitCenter.z, isPentagon ? 1 : 0)
     *
     * PS 端用法：
     *   float3 V_i = CellDirLUT.Load(int3(c_i, 0, 0)).rgb;
     *   float  dot_i = dot(dir, V_i);
     *   int    chosen = argmax_i(dot_i);
     *
     * 必须用 PF_A32B32G32R32F：R16F 对方向夹角精度不够（cell 中心方向会被截断到同一格，
     * 导致 dot 距离判别失效）。FP32 在 sub=3 时只占 642×16 = ~10 KB，常驻 GPU L2 cache。
     */
    void RebuildCellDirLUT_(int32 NumCells);
};
