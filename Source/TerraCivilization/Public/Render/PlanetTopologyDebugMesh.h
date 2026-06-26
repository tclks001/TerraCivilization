// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/UniquePtr.h"
#include "PlanetTopologyDebugMesh.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
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
 * 当前 R1 仅渲染纯白材质，不写入任何 per-vertex 属性。
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
     * （UEngine::DefaultMaterial）。R1 阶段保持纯白即可。
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlanetTopology")
    TObjectPtr<UMaterialInterface> Material;

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
};
