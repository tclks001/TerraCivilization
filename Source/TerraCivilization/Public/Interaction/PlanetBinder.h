// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/UniquePtr.h"
#include "PlanetBinder.generated.h"

// 前向声明，避免在头文件中拉入 PTG / Grid / Render 的实现细节
class APtgManager;
class APlanetTessellatedMesh;
class FSphereTopology;
class FSphereTopologyQuery;
class UCellHighlightComponent;

/**
 * APlanetBinder
 *
 * 桥接 PTG 球面地形（几何层）与 FSphereTopology 正二十面体细分网格（拓扑层）的 Actor。
 *
 * 设计原则（详见 Docs/HexHighlightInteractionPlan.md）：
 * - 几何层与拓扑层完全解耦，仅共享"球心 + 名义半径"两个标量；
 * - 拓扑数据在 BeginPlay 一次性构建，运行时只读；
 * - 鼠标交互由 PlayerController 走 LineTrace 拿到 WorldHit，再交给本 Binder 的 OnHoverWorldPoint。
 *
 * 当前实现状态（Step 2）：仅完成拓扑构建与日志验证；hover/click 接口为占位，留作后续 Step。
 */
UCLASS()
class TERRACIVILIZATION_API APlanetBinder : public AActor
{
    GENERATED_BODY()

public:
    APlanetBinder();

    /**
     * 显式声明析构与 VTableHelper 构造，并在 .cpp 中 = default。
     *
     * 原因：本类持有 TUniquePtr<FSphereTopology> 和 TUniquePtr<FSphereTopologyQuery>，
     * 而这两个类型在本头文件中仅前向声明。如果让编译器在 .gen.cpp 里自动生成析构，
     * 由于 .gen.cpp 看不到完整类型，TDefaultDelete::operator() 中的 `delete Ptr;`
     * 会触发 C4150「删除指向不完整类型的指针；没有调用析构函数」。
     *
     * 解决方案见 Engine/Source/Runtime/Core/Public/Templates/UniquePtr.h
     * 中 TDefaultDelete::operator() 的注释。
     */
    virtual ~APlanetBinder();
    APlanetBinder(FVTableHelper& Helper);

    /**
     * 关联的 PTG Manager（旧几何源，R11 后续废弃路径，fallback 使用）。
     * R11 推荐使用 TessellatedMeshRef；PtgManagerRef 留为向后兼容与调试。
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
    TObjectPtr<APtgManager> PtgManagerRef;

    /**
     * R11 主几何源：T 阶段自研球面网格 actor。鼠标射线、球心位置、半径都从它取。
     * 优先级：该字段 != null 时走 Tess 路径；否则 fallback 到 PtgManagerRef（详见 GetHostActor）。
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
    TObjectPtr<APlanetTessellatedMesh> TessellatedMeshRef;

    /** 正二十面体细分层数。Cells 数 = 10*4^N + 2。N=3 -> 642 cells。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet", meta = (ClampMin = "1", ClampMax = "6"))
    int32 SubdivisionLevel = 3;

    /**
     * Step 3 调试用：在命中 Cell 中心绘制 DebugSphere 的半径（cm）。
     * 这只是肉眼校准用的视觉标识，与 PTG 的 Radius 无关。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Debug", meta = (ClampMin = "1.0"))
    float DebugSphereRadius = 200.0f;

    /** Step 3 调试用：DebugSphere 的颜色（hover 命中时）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Debug")
    FColor DebugHoverColor = FColor::Yellow;

    /**
     * 是否在 BeginPlay 自动调用 PtgManager->GenerateEverything()。
     *
     * APtgManager 没有运行时自动生成机制，且 RuntimeMeshComponent 的几何数据
     * 不会随关卡 .umap 序列化保存。打开此开关可保证 PIE 第一帧就有可见的球，
     * 避免编辑器里看得见、PIE 中却空空如也的常见困惑。
     *
     * 缺点：每次 PIE 都要重新生成（sub=3、Resolution=80 大约 100~300 ms）。
     * 如果你已经在编辑器里手动 Generate 过且不想重复，关掉此开关即可。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    bool bAutoGenerateOnBeginPlay = true;

    /**
     * 是否强制把 PtgManager 的 Shape 字段改为 Sphere。
     *
     * APtgManager::Shape 默认是 Plane，自带 BP_PTG_Manager 也没有改这个默认值，
     * 因此在不主动覆盖的情况下，关卡里看到的就是平面而不是球。
     * 打开此开关后，BeginPlay 与编辑器中 OnConstruction 都会通过反射强制设置成 Sphere
     * 再触发 GenerateEverything()。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    bool bForceSphereShape = true;

    /**
     * 是否在编辑器（Construction Script）阶段自动生成 PTG。
     *
     * 打开后，把 BP_PlanetBinder 拖入关卡或 PtgManagerRef 被赋值的瞬间，
     * 就会自动让 PTG 生成一次，免去手动点 "Generate Everything" 的步骤。
     * 仅在编辑器中生效；PIE / 打包包里不会触发（避免重复生成）。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
    bool bAutoGenerateInEditor = true;

    //~ AActor
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    //~ End AActor

    /** 由 PlayerController 在每帧 hover 时调用。WorldHit 为 PTG 表面命中点（世界坐标）。 */
    void OnHoverWorldPoint(const FVector& WorldHit);

    /** 由 PlayerController 在点击时调用。当前为占位。 */
    void OnClickWorldPoint(const FVector& WorldHit);

    /** 鼠标移出球体时调用。 */
    void OnLeavePlanet();

    /** 拓扑层只读访问（供 UCellHighlightComponent / 上层系统使用）。 */
    APtgManager*                GetPtgManager()      const { return PtgManagerRef; }
    APlanetTessellatedMesh*     GetTessellatedMesh() const { return TessellatedMeshRef; }
    UCellHighlightComponent*    GetHighlightComp()   const { return HighlightComp; }
    const FSphereTopology*      GetTopology()        const { return Topology.Get(); }
    const FSphereTopologyQuery* GetQuery()           const { return Query.Get(); }

    /**
     * R11 几何源优先级：优先 Tess actor、次选 PTG manager。
     * 鼠标射线命中判定 + 球心 GetActorLocation 均取该 actor。两者都为 null 时返回 nullptr。
     */
    AActor* GetHostActor() const;

    /**
     * 取球半径（cm）。
     *
     * R11 优先从 TessellatedMeshRef->GlobeRadiusCM 取；fallback 走旧 PTG 反射路径。
     * 两者都不可用时返回兌底值 15000.0f（与 APtgManager::Radius / Tess GlobeRadiusCM
     * 默认值一致）。
     */
    float GetRadius() const;

private:
    /**
     * 通过反射强制把 PtgManagerRef 的 Shape 改为 Sphere（值 = 2）。
     * APtgManager::Shape 是 protected UPROPERTY 且没有 setter，
     * 仅能用反射或子类（不可行）访问。返回 true 表示发生了实际修改。
     */
    bool EnsureSphereShape();

    /** 安全包装：若 PtgManagerRef 有效则调用 GenerateEverything，并打印日志。 */
    void RegeneratePtg(const TCHAR* Reason);

    /** 拓扑层（正二十面体细分），BeginPlay 构造，EndPlay 释放。 */
    TUniquePtr<FSphereTopology>      Topology;

    /** 球面拓扑查询器（O(log N) 球面方向 -> CellId）。 */
    TUniquePtr<FSphereTopologyQuery> Query;

    /** 高亮渲染组件（在构造函数里 CreateDefaultSubobject）。 */
    UPROPERTY(VisibleAnywhere, Category = "Planet")
    TObjectPtr<UCellHighlightComponent> HighlightComp;

    /** 上一帧 hover 命中的 CellId，用于抖动抑制（Step 3 起开始使用）。 */
    int32 LastHoveredCellId = INDEX_NONE;
};