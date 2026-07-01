// Fill out your copyright notice in the Description page of Project Settings.

#include "Render/PlanetTopologyDebugMesh.h"

#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "FCorner.h"
#include "FSphereTopology.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "KismetProceduralMeshLibrary.h"
#include "PixelFormat.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/BulkData.h"
#include "TextureResource.h"
#include "WorldGenerator.h"

// PIE 退出后材质恢复钩子（详见 AgentWorkflow.md §3.11）。
//   FWorldDelegates::OnPostWorldCleanup 在 Engine 模块，无需 UnrealEd。
#include "Engine/World.h"

// W4 引入：从 UTerrainSet 查 UTerrainDefinition->LayerIndex，写入 LUT.R。
#include "TerrainSet.h"
#include "TerrainDefinition.h"
#include "GameplayTagContainer.h"

// R8 引入：ProceduralMesh 几何重建依赖。水面层是本 Actor 下一个子组件（bEnableWaterShell
//   开关 + WaterMaterial，详见 R8_ParametricTint.md §4.5 / AgentWorkflow.md §3.10）。
//   原「Spawn 独立 APlanetWaterShell Actor 」路径已废弃（PIE 深拷贝会造成材质丢失）。

DEFINE_LOG_CATEGORY_STATIC(LogPlanetTopologyDebugMesh, Log, All);

// =====================================================================
// R8：17 种地形配方表（详见 Docs/R8_ParametricTint.md §1.4 / §3.2.1）
//
// 字段顺序：BaseTexIdx, Tint(R,G,B), Sat, Bri, RoughMin, RoughMax,
//           OverlayBlend, NormalStr, TriScale
//
// W4 联调时整体迁移到 UTerrainDefinition::FTerrainMaterialParams DataAsset。
// =====================================================================
namespace
{
    struct FR8Recipe
    {
        uint8  BaseTexIdx;       // 0=Soil, 1=Rock, 2=Forest（不应作为 base）
        float  TintR, TintG, TintB;
        float  SatMul;
        float  BriMul;
        float  RoughMin;
        float  RoughMax;
        float  OverlayBlend;     // 0=纯 base，>0 叠加 Forest
        float  NormalStr;
        float  TriScale;
        float  HeightScaleCM;    // R8.2 新增：SHFRM 高度场缩放（cm，0~50）；详见 R8.2 §4.1
    };

    static const FR8Recipe GR8Recipes[17] = {
        // 0  Plain.Grass         （草根 + 浅碎石，2 cm）
        { 0, 0.40f, 0.70f, 0.30f, 1.0f, 1.0f,  0.5f, 0.8f,  0.15f, 1.0f, 1.0f, 20.0f },
        // 1  Plain.Savanna       （干草地，1.5 cm）
        { 0, 0.70f, 0.60f, 0.30f, 0.9f, 1.1f,  0.6f, 0.9f,  0.0f,  1.0f, 1.0f, 15.0f },
        // 2  Forest.Temperate    （苔藓 + 树根 + 落叶，3 cm）
        { 0, 0.30f, 0.55f, 0.25f, 1.1f, 0.9f,  0.5f, 0.8f,  0.65f, 1.2f, 1.0f, 30.0f },
        // 3  Forest.Tropical     （浓密苔藓，3.5 cm）
        { 0, 0.20f, 0.50f, 0.20f, 1.3f, 0.85f, 0.4f, 0.7f,  0.85f, 1.5f, 1.0f, 35.0f },
        // 4  Forest.Taiga        （针叶林落叶层，3 cm）
        { 0, 0.25f, 0.40f, 0.30f, 0.7f, 0.85f, 0.5f, 0.8f,  0.55f, 1.2f, 1.0f, 30.0f },
        // 5  Wetland             （湿地浅草，1 cm）
        { 0, 0.30f, 0.50f, 0.35f, 1.0f, 0.85f, 0.2f, 0.5f,  0.0f,  0.8f, 1.0f, 10.0f },
        // 6  Desert.Sand         （沙波纹，0.5 cm）
        { 0, 0.95f, 0.85f, 0.60f, 0.9f, 1.2f,  0.7f, 0.95f, 0.0f,  0.5f, 0.5f,  5.0f },
        // 7  Desert.Rocky        （戈壁岩缝，4 cm）
        { 1, 0.70f, 0.60f, 0.45f, 0.7f, 1.0f,  0.7f, 0.95f, 0.0f,  1.0f, 1.0f, 40.0f },
        // 8  Coast.Beach         （沙滩平整；D8.2.6 海岸线特殊处理，强制 0）
        { 0, 0.95f, 0.90f, 0.70f, 0.7f, 1.15f, 0.7f, 0.95f, 0.0f,  0.5f, 0.5f,  0.0f },
        // 9  Coast.Rocky         （岩石海岸，4.5 cm）
        { 1, 0.55f, 0.55f, 0.50f, 0.5f, 0.9f,  0.6f, 0.9f,  0.0f,  1.2f, 1.5f, 45.0f },
        // 10 Mountain.Hill       （丘陵岩裂，4 cm）
        { 1, 0.55f, 0.50f, 0.42f, 0.7f, 0.9f,  0.6f, 0.9f,  0.30f, 1.2f, 1.5f, 40.0f },
        // 11 Mountain.Peak       （峰顶岩裂，5 cm 上限）
        { 1, 0.50f, 0.48f, 0.45f, 0.4f, 0.85f, 0.7f, 0.95f, 0.0f,  1.5f, 2.0f, 50.0f },
        // 12 Mountain.Snow       （雪面平整，0.5 cm）
        { 1, 0.92f, 0.94f, 0.98f, 0.2f, 1.4f,  0.1f, 0.4f,  0.0f,  1.0f, 1.5f,  5.0f },
        // 13 Tundra              （苔原，1.5 cm）
        { 0, 0.70f, 0.70f, 0.65f, 0.3f, 1.05f, 0.7f, 0.95f, 0.0f,  0.8f, 1.0f, 15.0f },
        // 14 Glacier             （冰川接近镜面，0.3 cm）
        { 1, 0.85f, 0.92f, 0.98f, 0.4f, 1.45f, 0.1f, 0.3f,  0.0f,  0.5f, 2.0f,  3.0f },
        // 15 Ocean.Shallow       （D8.2.6：海面 mesh 接管，强制 0）
        { 0, 0.20f, 0.50f, 0.70f, 1.5f, 0.8f,  0.05f, 0.2f, 0.0f,  0.3f, 1.5f,  0.0f },
        // 16 Ocean.Deep          （D8.2.6：海面 mesh 接管，强制 0）
        { 0, 0.05f, 0.15f, 0.40f, 1.5f, 0.5f,  0.05f, 0.2f, 0.0f,  0.2f, 2.0f,  0.0f },
    };
    static_assert(UE_ARRAY_COUNT(GR8Recipes) == 17, "R8 must have exactly 17 recipes");

    // R8 placeholder：CellId → 配方索引（0..16）
    FORCEINLINE int32 R8_PlaceholderRecipeIndex(int32 CellId)
    {
        constexpr uint32 KnuthHashConst = 2654435761u;
        return static_cast<int32>((static_cast<uint32>(CellId) * KnuthHashConst) % 17u);
    }

    // 中性默认配方（W4 联调期 bUseR8PlaceholderRecipes=false 时填入 Cell*LUT，
    //   让 R8 4 通道 LUT 不参与微调，HLSL 端等价于纯 R7 视觉）。
    static const FR8Recipe GR8NeutralRecipe = {
        /*BaseTexIdx*/    0,
        /*Tint*/         1.0f, 1.0f, 1.0f,
        /*Sat*/          1.0f,
        /*Bri*/          1.0f,
        /*RoughMin*/     0.5f,
        /*RoughMax*/     0.8f,
        /*OvlBlend*/     0.0f,
        /*NormalStr*/    1.0f,
        /*TriScale*/     1.0f,
        /*HeightScaleCM*/ 0.0f,    // R8.2 中性：关 SHFRM
    };
}

APlanetTopologyDebugMesh::APlanetTopologyDebugMesh()
{
    // 不需要 Tick；网格在 OnConstruction / Rebuild 时一次性烘焙。
    PrimaryActorTick.bCanEverTick = false;

    // 让 Construction Script 在编辑器内移动 Actor 时也跑：保证编辑器中位置变化能及时刷新 mesh 朝向。
    bRunConstructionScriptOnDrag = true;

    MeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MeshComp"));
    SetRootComponent(MeshComp);

    // R1 阶段不参与碰撞与寻路。
    MeshComp->SetMobility(EComponentMobility::Movable);
    MeshComp->bUseAsyncCooking = true;
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetCanEverAffectNavigation(false);
    MeshComp->bUseComplexAsSimpleCollision = false;

    // R8 水面层子组件（挂在 RootComponent 下）。与主 mesh 同款配置：Movable / NoCollision /
    // 不影响导航 / AsyncCooking。默认 Visibility=false，由 Rebuild() 末尾根据 bEnableWaterShell 切换。
    //
    // 为什么不是 Spawn 独立 Actor：PIE 深拷贝 Editor World 时，`OnConstruction` 中 SpawnActor
    // 出来的临时子 Actor 的材质引用会丢失（变成默认棋盘格）。Component 是 Owner Actor 的
    // SubObject，PIE 拷贝跟随 Owner 一起走——无生命周期错位。
    // 详见 [AgentWorkflow.md §3.10](../../../Docs/AgentWorkflow.md)。
    WaterMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMeshComp"));
    WaterMeshComp->SetupAttachment(MeshComp);
    WaterMeshComp->SetMobility(EComponentMobility::Movable);
    WaterMeshComp->bUseAsyncCooking = true;
    WaterMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WaterMeshComp->SetCanEverAffectNavigation(false);
    WaterMeshComp->bUseComplexAsSimpleCollision = false;
    WaterMeshComp->SetVisibility(false);
    // 水面是半透明球壳：相机会从内部穿过，cull mode 保留默认 back-face culling。
    // Two-Sided 在材质 M_WaterShell 自身设置（详见 R8 §4.5.3），不在 PMC 上开 bUseTwoSided。

#if WITH_EDITOR
    // ===== PIE 退出后材质恢复钩子（AgentWorkflow §3.11）=====
    //
    // 订阅 FWorldDelegates::OnPostWorldCleanup。该委托在任何 UWorld 被 Cleanup 后触发，
    // PIE 退出也走这条路径。重复订阅 / dangling 问题由 BeginDestroy 中 UnRegister 防御。
    //
    // 仅 Editor 构建路径下有效 —— Standalone 不需要这个钩子（只有一个 Game World，
    // 不会发生跨 World duplicate）。
    PostWorldCleanupHandle = FWorldDelegates::OnPostWorldCleanup.AddUObject(
        this, &APlanetTopologyDebugMesh::OnPostWorldCleanup_);
#endif
}

APlanetTopologyDebugMesh::~APlanetTopologyDebugMesh()
{
    // PIE 钩子在析构中取消订阅。AddUObject 路径会自动在 UObject 销毁时表达动作，
    // 但显式 Remove 避免中间状态多次触发与 dangling 风险 —— 详见 AgentWorkflow §3.11。
#if WITH_EDITOR
    if (PostWorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnPostWorldCleanup.Remove(PostWorldCleanupHandle);
        PostWorldCleanupHandle.Reset();
    }
#endif
}
APlanetTopologyDebugMesh::APlanetTopologyDebugMesh(FVTableHelper& Helper) : Super(Helper) {}

void APlanetTopologyDebugMesh::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Rebuild();
}

#if WITH_EDITOR
void APlanetTopologyDebugMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Editor 中改任何属性（NumLayersHint / SubdivisionLevel / Radius / ...）都强制重建。
    // 默认 Actor::PostEditChangeProperty 会调 RerunConstructionScripts() 进而调 OnConstruction，
    // 但我们这里显式再调一次 Rebuild() 作为兜底，保证 NumLayersHint 这种"不影响几何只影响 LUT"
    // 的属性也能立即生效（即便某些边界情况下 RerunConstructionScripts 没被触发）。
    Rebuild();
}
#endif

void APlanetTopologyDebugMesh::Rebuild()
{
    if (!MeshComp)
    {
        return;
    }

    // 1) 构建拓扑（每次 Rebuild 都重新构建：SubdivisionLevel 可能改了）
    //    注意：FSphereTopology(int32) 构造函数内部已经调用 Build()，
    //    这里绝对不能再显式调用 Topology->Build()——否则会在已经构建好的
    //    拓扑数据上叠加细分一次（数组都是 Add/SetNum 追加式），导致
    //    Cells / Corners 数量爆炸式增长（实测 sub=3 会变成 41604 Cells / 83200 Corners）。
    Topology = MakeUnique<FSphereTopology>(SubdivisionLevel);

    const int32 NumCells   = Topology->Cells.Num();
    const int32 NumCorners = Topology->Corners.Num();
    if (NumCells == 0 || NumCorners == 0)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Warning,
            TEXT("[PlanetTopologyDebugMesh] Empty topology (Cells=%d, Corners=%d). Skip build."),
            NumCells, NumCorners);
        MeshComp->ClearAllMeshSections();
        return;
    }

    // ---------------------------------------------------------------
    //  R2 拓扑前置（极重要 —— 这是本项目最容易误读的部分）：
    //
    //  ▸ FSphereTopology 的对偶关系（直接来自 BuildDualFromPrimal 的源码注释）：
    //      Cells     = mesh 顶点 = PrimalVertsUnit 的对偶；每个 Cell 一个位置。
    //      Corners   = mesh 三角形（=primal 三角形）= PrimalTris 的对偶。
    //                  Corner.UnitDir = 三角形 3 个顶点的重心归一化（即外心方向）；
    //                  Corner.CellIds = 三角形的 3 个顶点对应的 Cell ID。
    //      Tris      = 渲染层 RenderTri；本阶段废弃不用。
    //
    //  ▸ 渲染 mesh 的本来面貌：
    //      整个测地线球面 mesh 由且仅由一种几何元素铺成 —— primal 三角形（=Corner）。
    //      sub=3 时共有 642 个 mesh 顶点（=Cells）和 1280 个三角形（=Corners）。
    //      所有三角形地位完全等价。**mesh 上不存在五边形或六边形几何元素**，
    //      也不存在"hex 内部三角形"与"hex 之间过渡三角形"的两类划分。
    //
    //  ▸ 五边形 / 六边形是怎么来的（戈德堡多面体的对偶面）：
    //      每个 Cell（=mesh 顶点）周围有 5 或 6 个相邻 primal 三角形（=Corners）。
    //      每个 primal 三角形被它的"外心-边中点连线"划分为 **3 个 1/3 角块**，
    //      每个角块归属其对应的一个顶点 Cell。
    //      ⇒ 一个 hex Cell 的几何范围 = 它周围 6 个 primal 三角形各贡献的 1/3 角块之并；
    //      ⇒ 一个 pent Cell 的几何范围 = 它周围 5 个 primal 三角形各贡献的 1/3 角块之并。
    //      **每个三角形的 3 个 1/3 角块属于 3 个不同的 hex/pent**——三角形并非"专属"任何
    //      一个 hex/pent；它同时贡献给 3 个 hex/pent 各 1/3 区域。
    //      ⇒ 五/六边形是抽象逻辑面，不是 mesh 几何面；它们由"5/6 个三角形各出 1/3"拼成。
    //
    //  ▸ 之前看图常见的视觉错觉（用户多次纠正过）：
    //      "图上看到 5/6 个三角形围绕一个 Cell 顶点形成一个色块" ≠ "hex 占了 5/6 个三角形"。
    //      实际上每个三角形只**贡献 1/3 角块**给该中心 hex；剩下的 2/3 角块属于另外
    //      2 个邻居 hex。线性插值（VertexColor）让 1/3 角块的边界变成连续渐变，所以视觉上
    //      容易把"靠近中心顶点的亮色优势区"当成"完整三角形被中心 Cell 占用"。
    //
    //  ▸ R2 视觉验收的正确策略 —— PS 端 argmax(λ) 硬边切换：
    //      在每个三角形内部，PS 取 λ₀λ₁λ₂ 中最大者对应的 Cell ID，输出该 Cell 的 hash 色。
    //      数学上这恰好把三角形分割成 3 个清晰的 1/3 角块（重心坐标 argmax 域 = 外心-边中点连线
    //      划出的角块）。视觉效果：
    //      ✓ 每个 hex/pent 是 5/6 个角块拼成的纯色多边形（用户预期的"完整五/六边形"）
    //      ✓ 1280 个 primal 三角形完全无差别（每个都被 3 个 1/3 角块切开）
    //      ✓ 没有"过渡三角形"——三角形之间的所有边界都是 hex/pent 的真实边界
    //      ✓ 没有线性渐变（这正是 R2 区别于"VertexColor 直显"路径的关键）
    //
    //  顶点属性布局（R3 验收 + fp16 精度规避）：
    //      Position, Normal     — 几何
    //      VertexColor          — 该顶点对应 Cell 的 LayerHash 色（fallback 路径：
    //                              "VertexColor → Emissive" 两节点直接看到 R3 软边效果）
    //      UV0.xy = (HiC, LoC)  — Cell C 的 CellId 拆分（高 8 位 / 低 8 位）
    //      UV1.xy = (HiA, LoA)  — Cell A 的 CellId 拆分
    //      UV2.xy = (HiB, LoB)  — Cell B 的 CellId 拆分
    //      UV3.xy = (OneHot.x, OneHot.y)
    //                           — Role 0 顶点写 (1,0)、Role 1 写 (0,1)、Role 2 写 (0,0)
    //                           — 经过光栅化器线性插值后，PS 阶段：
    //                               λ₀ = UV3.x，λ₁ = UV3.y，λ₂ = saturate(1 − λ₀ − λ₁)
    //                               即"硬件免费的重心坐标 / 三方权重"
    //
    //  ★ 为什么把 CellId 拆成 (Hi, Lo) 两个 8-bit 字段（极重要）：
    //  PMC 内部 InitFromDynamicVertex 默认用 fp16（PF_G16R16F）存 UV，
    //  fp16 在 [0, 1024] 范围步长 ≤ 0.5、在 [0, 2048] 步长 ≤ 1，加上光栅化器
    //  的"透视校正插值"会引入小累加误差，PS 端 round 还原大 CellId 时会跨越边界，
    //  导致少数 CellId 被错误映射到同一个 fp16 值，整个 chosen 取值范围被压缩。
    //  把 CellId 拆成 (Hi, Lo)，每个 ≤ 256，fp16 步长 ≤ 1/16，插值误差远小于 0.5，
    //  PS 端 round 能完美还原。PS 端解码：CellId = round(Hi)*256 + round(Lo)。
    //  此编码可支持 sub ≤ 6（NumCells = 40962 < 65536 = 256*256），完全够用。
    //
    //  关键技巧：**三角形的 3 个顶点都写入相同的 (HiA,LoA)/(HiB,LoB)/(HiC,LoC)**——
    //  经过插值后 UV0/UV1/UV2 仍是常量（三个顶点同值，插值结果不变），
    //  只有 UV3 的 OneHot 与 VertexColor 在三个顶点之间不同。
    //
    //  ▸ R3 验收材质（推荐两条路径之一）：
    //
    //    路径 A: VertexColor → Emissive（最简，2 节点，软边）
    //      Shading Model = Unlit；VertexColor RGB 直接连 Emissive Color。
    //      光栅化器对 3 个顶点 LayerHash 色做线性插值 → 三角形内部三色软渐变。
    //      ⊕ 完全旁路 UV/Custom-HLSL，没有 fp16 精度问题，零配置。
    //
    //    路径 B: Custom HLSL（argmax 硬边，需要正确解码 Hi/Lo）
    //      Inputs: UV0(F2), UV1(F2), UV2(F2), UV3(F2), LUT(Texture2D)
    //      Code:
    //          // 8-bit 拆分解码 3 个 CellId
    //          int c0 = (int)round(UV1.x) * 256 + (int)round(UV1.y);
    //          int c1 = (int)round(UV2.x) * 256 + (int)round(UV2.y);
    //          int c2 = (int)round(UV0.x) * 256 + (int)round(UV0.y);
    //          float l0 = saturate(UV3.x), l1 = saturate(UV3.y), l2 = saturate(1 - l0 - l1);
    //          // argmax 选 CellId
    //          int chosen = (l0 >= l1 && l0 >= l2) ? c0 : ((l1 >= l2) ? c1 : c2);
    //          // LUT.Load 取 LayerIndex
    //          int layer = (int)(LUT.Load(int3(chosen, 0, 0)).r * 255.0 + 0.5);
    //          // hash 色
    //          float hue = frac((layer + 1) * 0.6180339887);
    //          return saturate(float3(frac(hue), frac(hue*7.123), frac(hue*13.456)) + 0.25);
    //      详见 R3_CellAttrLUTMaterial.md。
    //
    //  ▸ R4 验收材质（球面 Voronoi 测地线硬边，消除 R3 hex 边折角）：
    //
    //    路径 C: Custom HLSL（argmax(dot(dir, V_i))，纹理化 cell 方向）
    //      问题：R3 路径 B 的 argmax(λ) 等位线 = 外心-边中点折线，相邻三角形外心不重合
    //            ⇒ 在每条 mesh 边的中点处出现一次可见折角，hex 视觉上变成"12 边形"。
    //      方案：把判别准则换成 argmax(dot(dir, V_i))，等位线变为球面 Voronoi 大圆弧（测地线）。
    //            V_i 由新增的 CellDirLUT（1×NumCells、PF_A32B32G32R32F、RGB=UnitCenter）三次 Load 取得。
    //
    //      Inputs: UV0(F2), UV1(F2), UV2(F2), UV3(F2),
    //              WorldPos(F3), PlanetCenter(F3), CellAttrLUT(Texture2D), CellDirLUT(Texture2D)
    //      Code:
    //          // 8-bit 拆分解码 3 个 CellId（与路径 B 完全相同）
    //          int c0 = (int)(UV1.x + 0.5) * 256 + (int)(UV1.y + 0.5);
    //          int c1 = (int)(UV2.x + 0.5) * 256 + (int)(UV2.y + 0.5);
    //          int c2 = (int)(UV0.x + 0.5) * 256 + (int)(UV0.y + 0.5);
    //          // R4 新增：从 CellDirLUT 取三 cell 单位中心方向
    //          float3 V_A = CellDirLUT.Load(int3(c0, 0, 0)).rgb;
    //          float3 V_B = CellDirLUT.Load(int3(c1, 0, 0)).rgb;
    //          float3 V_C = CellDirLUT.Load(int3(c2, 0, 0)).rgb;
    //          // R4 新增：球面 Voronoi 判别（dot 距离 argmax）
    //          float3 dir = normalize(WorldPos - PlanetCenter);
    //          float dA = dot(dir, V_A), dB = dot(dir, V_B), dC = dot(dir, V_C);
    //          int chosen = (dA >= dB && dA >= dC) ? c0 : ((dB >= dC) ? c1 : c2);
    //          // LUT.Load 取 LayerIndex（与路径 B 完全相同）
    //          int layer = (int)(CellAttrLUT.Load(int3(chosen, 0, 0)).r * 255.0 + 0.5);
    //          float hue = frac((layer + 1) * 0.6180339887);
    //          return saturate(float3(frac(hue), frac(hue*7.123), frac(hue*13.456)) + 0.25);
    //      详见 R4_VoronoiBoundary.md。
    // ---------------------------------------------------------------

    // 2) 装填顶点 / 索引缓冲：每个 Corner（三角形）展开为 3 个独立顶点。
    //    Vertices 长度 = NumCorners * 3，Triangles 是 0,1,2,3,4,5,... 的顺序索引。
    const int32 NumVerts = NumCorners * 3;

    TArray<FVector>          Vertices;
    TArray<int32>            Triangles;
    TArray<FVector>          Normals;
    TArray<FVector2D>        UV0;
    TArray<FVector2D>        UV1;     // (c0, c1)
    TArray<FVector2D>        UV2;     // (c2, _ )
    TArray<FVector2D>        UV3;     // OneHot (λ₀, λ₁)
    TArray<FLinearColor>     VertexColors;
    TArray<FProcMeshTangent> Tangents;   // 留空：R2 公式只看 emissive，不需要切线空间

    Vertices.Reserve(NumVerts);
    Triangles.Reserve(NumVerts);
    Normals.Reserve(NumVerts);
    UV0.Reserve(NumVerts);
    UV1.Reserve(NumVerts);
    UV2.Reserve(NumVerts);
    UV3.Reserve(NumVerts);
    VertexColors.Reserve(NumVerts);

    // ------------------------------------------------------------------
    // R3 视觉验收（VertexColor 软边路径）：
    //   每个 Cell 预算 LayerIndex（与 RebuildCellAttrLUT_ 保持完全一致的 Knuth 哈希），
    //   再把 LayerIndex 哈希成颜色，写入该 Cell 所有顶点的 VertexColor。
    //
    //   这样最简单的 "VertexColor → Emissive" 两节点材质就能直接看到 R3 的正确效果：
    //     - 同 LayerIndex 的相邻 hex 颜色相同 → 视觉上融合（无可见边界）
    //     - 不同 LayerIndex 的 hex 颜色不同 → 软边过渡（线性插值）
    //     - 调小 NumLayersHint → 大量 hex 合并为同色大区域（核心验收点）
    //
    //   注意：这是 *软边* 路径（光栅化器线性插值），不同于 R3 文档主推的 PS 端 argmax
    //   硬边路径。两条路径**LayerIndex 来源完全一致**：
    //     1. cpp 端 CellHashColor[Cid] = hash(layerIndex(Cid))，写入 VertexColor
    //     2. RebuildCellAttrLUT_ 把同一 layerIndex 写入 1×NumCells 的 LUT R 通道
    //   用户可以选用任一 fallback：
    //     A. 最简 fallback：材质 = "VertexColor → Emissive" → 软边、立即生效、零配置
    //     B. 主推 R3 路径：Custom HLSL `argmax + LUT.Load` → 硬边（详见 R3 文档 §3.3）
    //
    //   NumLayersHint 改变会同时影响 (A) 和 (B)，因此两条路径视觉上等价（仅"硬边"和"软边"
    //   的区别）。如果 (B) 视觉异常但 (A) 正常 → 一定是 Custom HLSL 节点配置错误。
    //
    //   ⚠ W2 兼容性说明（2026-06）：W2 起 RebuildCellAttrLUT_ 中 R 通道写入逻辑已切换为
    //   "DebugView=None: bIsLand?4:0 / DebugView=PlateId: 板块哈希"，与此处的 Knuth 哈希
    //   *不再一致*。两条路径的"等价性"承诺被有意打破——保持本路径用 Knuth 哈希是为了：
    //     · R7 Triplanar 视觉链路（路径 B 主用）由 LUT 驱动，不依赖 VertexColor → 视觉零回归
    //     · 极简 VertexColor 材质（路径 A 备用）继续显示伪随机色块，便于检视拓扑
    //   若 W3+ 需要让二者重新对齐，可把这段循环也改为按 Generator->GetCellData() 取值。
    // ------------------------------------------------------------------
    constexpr uint32 KnuthHash    = 2654435761u;
    const int32      LayerModCpp  = FMath::Clamp(NumLayersHint, 1, 256);

    TArray<FLinearColor> CellHashColor;
    CellHashColor.SetNumUninitialized(NumCells);
    for (int32 Cid = 0; Cid < NumCells; ++Cid)
    {
        // 步骤 1：CellId → LayerIndex（与 RebuildCellAttrLUT_ 中的公式完全一致）
        const uint32 Hashed     = (uint32)Cid * KnuthHash;
        const uint8  LayerIndex = (uint8)(Hashed % (uint32)LayerModCpp);

        // 步骤 2：LayerIndex → 颜色（黄金分割哈希，与 R3 HLSL `frac((layer+1)*0.618)` 同源）
        // (LayerIndex + 1) 避免 hash(0) = 全黑
        const float Hue  = FMath::Frac((LayerIndex + 1) * 0.6180339887498949f);
        const float HueR = FMath::Frac(Hue * 1.0f);
        const float HueG = FMath::Frac(Hue * 7.123f);
        const float HueB = FMath::Frac(Hue * 13.456f);
        CellHashColor[Cid] = FLinearColor(
            FMath::Clamp(HueR + 0.25f, 0.0f, 1.0f),
            FMath::Clamp(HueG + 0.25f, 0.0f, 1.0f),
            FMath::Clamp(HueB + 0.25f, 0.0f, 1.0f),
            1.0f);
    }

    // PMC 的网格顶点是 Component-Local 空间。Actor 通过 Transform 摆放，
    // 所以这里不要加 ActorLocation——直接用 UnitCenter * Radius 即可。
    for (int32 CornerIdx = 0; CornerIdx < NumCorners; ++CornerIdx)
    {
        const FCorner& Cor = Topology->Corners[CornerIdx];

        // 一个 Corner = 一个 primal 三角形。它的 3 个顶点 = Cor.CellIds[0..2] 对应的 3 个 Cell 中心。
        const int32 CA = Cor.CellIds[0];
        const int32 CB = Cor.CellIds[1];
        const int32 CC = Cor.CellIds[2];
        if (!Topology->Cells.IsValidIndex(CA) ||
            !Topology->Cells.IsValidIndex(CB) ||
            !Topology->Cells.IsValidIndex(CC))
        {
            // 拓扑异常，跳过这条 Corner 以保证 Vertices/Triangles 一致
            continue;
        }

        const FVector PA = Topology->Cells[CA].UnitCenter * Radius;
        const FVector PB = Topology->Cells[CB].UnitCenter * Radius;
        const FVector PC = Topology->Cells[CC].UnitCenter * Radius;

        // 顶点法线：直接用 +UnitCenter（球面外法，朝外）。
        //
        // ★ 经 R8 阶段用户实测确认（修订了早期 [SphereTopologyReference.md §11](../../../Docs/SphereTopologyReference.md)
        //   "应朝球心"的错误结论）：UE5 在球外相机渲染下，漫反射公式
        //   saturate(dot(N, LightDir)) 要求顶点法线 N **朝外**才能被太阳光照亮。
        //   填 -UnitCenter（朝球心）会让所有像素 dot(N, L) ≤ 0 → 整球漆黑。
        //   详见 [AgentWorkflow.md §3.15](../../../Docs/AgentWorkflow.md)（§11 推导错误的复盘）。
        //
        // **不走 KismetProceduralMeshLibrary::CalculateTangentsForMesh 自动法线**——
        // 该工具在"每 Corner 展开 3 独立顶点（不共享）"的几何上等价于 flat shading：
        // 每三角形 3 顶点都拿到面法线 → sub=3 球面在阴影 / 光照边界处出现 1280 个三角
        // 阶梯锯齿（昼夜分割线沿 mesh 三角形边呈尖刺状，详见 AgentWorkflow §3.13 / §3.14）。
        //
        // 替代方案：每个顶点（位置 = Cell 中心）的"球面光滑顶点法线"= UnitCenter（朝外）。
        //   · saturate(dot(UnitCenter, LightDir)) 在朝光半球 > 0 → Lit 正常受光
        //   · 顶点之间法线插值光滑变化 → 阴影 / N·L 边界平滑，无三角形锯齿
        //   · 与水面 SLW 的 +UnitCenter 同向（同一套几何/材质约定）
        const FVector NA = Topology->Cells[CA].UnitCenter;
        const FVector NB = Topology->Cells[CB].UnitCenter;
        const FVector NC = Topology->Cells[CC].UnitCenter;

        // ===================================================================
        //  ★ CellId 编码：高 8 位 + 低 8 位拆分，避开 fp16 精度问题（极其重要！）
        //
        //  问题根源：UE 的 FStaticMeshVertexBuffer 默认 bUseFullPrecisionUVs=false，
        //            意味着 UV 通道在 GPU 顶点缓冲里是 fp16（PF_G16R16F）。
        //
        //  fp16 对大整数的精度损失：
        //    - [0, 2048] 范围内 fp16 步长 ≤ 1（整数精确）
        //    - [0, 1024] 范围内 fp16 步长 ≤ 0.5
        //    - [0, 512]  范围内 fp16 步长 ≤ 0.25
        //  PMC 用 SetVertexUV 写入时会把 fp32 截断到 fp16，已经损失精度；
        //  GPU 光栅化器做"透视校正插值"时还会引入额外的 fp16 累加误差，
        //  最终 PS 端 round 还原 CellId 时，对于 cellId>500 的情况经常跨越边界，
        //  导致少数 CellId 被映射到同一个 fp16 值，chosen 的取值范围被严重压缩。
        //
        //  解决方案：把 CellId（最大 642，需要 10 bits）拆成两个 ≤ 256 的小整数：
        //      Hi = CellId / 256   ∈ [0, 2]   （sub=3 时最大值 642/256=2）
        //      Lo = CellId % 256   ∈ [0, 255]
        //  fp16 对 [0, 256] 范围内的整数和半整数都精确（步长 ≤ 1/16），
        //  插值误差远小于 0.5，PS 端 round 能完美还原。
        //  PS 端解码：CellId = Hi * 256 + Lo
        //
        //  布局：
        //    UV1.xy = (HiA, LoA)   — Cell A 的 CellId 拆分
        //    UV2.xy = (HiB, LoB)   — Cell B 的 CellId 拆分
        //    UV0.xy = (HiC, LoC)   — Cell C 的 CellId 拆分（UV0 原本预留细节纹理 UV，
        //                             R3 阶段还没用到，先借来传 Cell C；R6+ 真要用 UV0
        //                             做 Triplanar 时再迁回 cpp 端预算 LayerColor 到 VC）
        //    UV3.xy = (OneHot.x, OneHot.y) — 重心权重，与原方案一致
        // ===================================================================
        const float HiA = (float)(CA >> 8);    // 高 8 位（0~2 for sub=3，最高支持 sub=6 时 cells=40962<65536）
        const float LoA = (float)(CA & 0xFF);  // 低 8 位（0~255）
        const float HiB = (float)(CB >> 8);
        const float LoB = (float)(CB & 0xFF);
        const float HiC = (float)(CC >> 8);
        const float LoC = (float)(CC & 0xFF);

        // R3 共享给三角形所有顶点的属性：3 个 CellId（拆分编码）
        const FVector2D EncCellA(HiA, LoA);   // 写入 UV1
        const FVector2D EncCellB(HiB, LoB);   // 写入 UV2
        const FVector2D EncCellC(HiC, LoC);   // 写入 UV0（R3 阶段借用，UV0 原是细节纹理 UV 占位）

        // R3 顶点 onehot —— 三角形的三个角色 0/1/2
        //   Role 0 顶点：OneHot = (1, 0) → λ₀ = 1, λ₁ = 0, λ₂ = 0
        //   Role 1 顶点：OneHot = (0, 1) → λ₀ = 0, λ₁ = 1, λ₂ = 0
        //   Role 2 顶点：OneHot = (0, 0) → λ₀ = 0, λ₁ = 0, λ₂ = 1 (= 1 − 0 − 0)
        //   OneHot 的 0/1 值在 fp16 下完全精确，无需拆分。
        const FVector2D OneHotA(1.0f, 0.0f);
        const FVector2D OneHotB(0.0f, 1.0f);
        const FVector2D OneHotC(0.0f, 0.0f);

        // ★ VertexColor fallback 路径：每个顶点写它对应 Cell 的 LayerHash 色。
        //   材质用 "VertexColor → Emissive" 两节点就能直接看到 R3 软边效果，
        //   完全绕开 UV/Custom-HLSL 路径，无 fp16 精度问题。
        const FLinearColor ColA = CellHashColor[CA];
        const FLinearColor ColB = CellHashColor[CB];
        const FLinearColor ColC = CellHashColor[CC];

        const int32 BaseIdx = Vertices.Num();   // 当前三角形第一个顶点的索引

        // Role 0 顶点（位置 = Cell A 中心，OneHot = (1,0)，VertexColor = hash(A)）
        Vertices.Add(PA);  Normals.Add(NA);
        UV0.Add(EncCellC); UV1.Add(EncCellA);  UV2.Add(EncCellB);  UV3.Add(OneHotA);
        VertexColors.Add(ColA);

        // Role 1 顶点（位置 = Cell B 中心，OneHot = (0,1)，VertexColor = hash(B)）
        Vertices.Add(PB);  Normals.Add(NB);
        UV0.Add(EncCellC); UV1.Add(EncCellA);  UV2.Add(EncCellB);  UV3.Add(OneHotB);
        VertexColors.Add(ColB);

        // Role 2 顶点（位置 = Cell C 中心，OneHot = (0,0)，VertexColor = hash(C)）
        Vertices.Add(PC);  Normals.Add(NC);
        UV0.Add(EncCellC); UV1.Add(EncCellA);  UV2.Add(EncCellB);  UV3.Add(OneHotC);
        VertexColors.Add(ColC);

        // ===================================================================
        //  ★ 三角形索引绕序：直接沿用 FCorner.CellIds 的原始顺序
        //
        //  追踪链路（详见 [SphereTopologyReference.md §11](../../../Docs/SphereTopologyReference.md)
        //  与 [SphericalSDFTerrainDesign.md §11.2](../../../Docs/SphericalSDFTerrainDesign.md)）：
        //    1. FSphereTopology::BuildIcosahedronUnit() 中 AddTriangle(0,5,11)
        //       等 20 个根三角形是 CCW from outside（球外向内看逆时针，已用户实测确认）
        //    2. SubdividePrimalOnce() 的 4 个子三角形 (A,A2B,C2A) / (B,B2C,A2B) /
        //       (C,C2A,B2C) / (A2B,B2C,C2A) 全部继承父三角形的绕序方向，**不反转**
        //    3. BuildDualFromPrimal() 中 Corner.CellIds = {A, B, C} 直接来自 PrimalTris[I]
        //       绕序仍为 CCW from outside
        //    4. 这里 Triangles.Add 按 CellIds[0..2] 顺序写入，绕序保持一致
        //
        //  这条链路在 UE5 默认 CullMode（CM_CW → D3D12_CULL_MODE_BACK，剔除背面、保留 CCW
        //  frontface；详见 [D3D12State.cpp:34/356]） 配合下，相机在球外时近端壳呈 CCW
        //  from camera = frontface 被渲染、远端壳呈 CW from camera 被剔除——这正是
        //  「球外看到外壁」的正确视觉。**绕序在 cpp 端是正确的，不要做"朝外校正"**。
        //
        //  ⚠ 历史教训（已避免重蹈）：
        //  曾误以为绕序是 CW from outside（注释滞后）+ 误以为 R7 Lit 漆黑是绕序问题
        //  → 一度引入 bFlipWinding"朝外校正"，反而把对的绕序弄反，出现"看到内壁、
        //  相机移动方向反"等更严重的视觉错误（已撤销）。R7 Lit 漆黑的真正根因是
        //  「手填法线 +UnitCenter 朝外，与 UE5 左手系 CCW 的 face_normal_LH（朝球心）
        //  反向，saturate(dot(N,L)) 大多数像素 ≤ 0 → 漆黑」——已通过 KismetTangents
        //  自动法线修复（见上方 CreateMeshSection 前的法线生成块）。
        // ===================================================================
        Triangles.Add(BaseIdx + 0);
        Triangles.Add(BaseIdx + 1);
        Triangles.Add(BaseIdx + 2);
    }

    // 3) 提交给 PMC。R2 起使用 4 通道 UV 的完整重载。R1 的不创建碰撞约束保持不变。
    //    PMC SceneProxy 内部固定按 4 个 UV 通道初始化（InitFromDynamicVertex 第三参 = 4），
    //    所以 GPU 端材质可直接通过 TexCoord[1]/[2]/[3] 节点读到我们写入的值。
    //
    // ★ Tangents 留空，Normals 已在装填段直接填 -UnitCenter（光滑球面顶点法线）。
    //
    //   不走 KismetTangents 的原因：本路径每 Corner 展开 3 独立顶点（不共享），
    //   KismetTangents 在该几何上等价于 flat shading（每三角形 3 顶点共享面法线）→
    //   sub=3 球面在阴影 / 光照边界呈现 1280 个三角阶梯锯齿。详见 AgentWorkflow §3.13/§3.14。
    //
    //   主材质 PS 端用 normalize(WorldPos - PlanetCenter) 反算球面 dir 自己处理纹理 / Triplanar，
    //   不消费顶点法线——切线空间也无意义。Tangents 留空，PMC 按零向量处理。
    {
        TArray<FProcMeshTangent> AutoTangents;
        Tangents = MoveTemp(AutoTangents);
    }

    MeshComp->ClearAllMeshSections();
    MeshComp->CreateMeshSection_LinearColor(
        /*SectionIndex=*/0,
        Vertices, Triangles, Normals,
        UV0, UV1, UV2, UV3,
        VertexColors, Tangents,
        /*bCreateCollision=*/false);

    // 4) R3：构建 1×NumCells 的 CellAttrLUT，把每 Cell 的 LayerIndex 写入 R 通道。
    //    ★ W2 起：在调用 RebuildCellAttrLUT_ 之前先跑 WorldGen 流水线，
    //      LUT 写入时按 Generator->GetCellData()[i].bIsLand 取值（DebugView=None 默认两色）。
    //      Generator 提升为成员持有，Reset → MakeUnique → Generate 顺序见 Docs/W2_PlatesAndLandSea.md §6 #11。
    {
        Generator.Reset();
        Generator = MakeUnique<FWorldGenerator>(Topology.Get(), WorldGenSettings);
        Generator->Generate();
    }

    RebuildCellAttrLUT_(NumCells);

    // 4.5) R4：构建 1×NumCells 的 CellDirLUT，把每 Cell 的 UnitCenter 写入 RGB 通道。
    //      PS 端用于球面 Voronoi 判别 argmax(dot(dir, V_i))，等位线 = 测地线大圆弧。
    RebuildCellDirLUT_(NumCells);

    // 4.6) R8：3 张 per-cell 多通道参数 LUT（Tint / HSV+Rough / NSpec），由 GR8Recipes 表派生。
    //      详见 Docs/R8_ParametricTint.md §1.3 / §3.2.4。
    //      bUseR8PlaceholderRecipes=true（R8 主验收）时按 RecipeIdx (0..16) 派生；
    //      false（W4 联调）时全部填中性默认值，让 4 LUT 不参与微调。
    RebuildCellTintLUT_(NumCells);
    RebuildCellHSVRoughLUT_(NumCells);
    RebuildCellNSpecLUT_(NumCells);

    // 5) 材质：把外部 Material 包装成 MID，把 CellAttrLUT/CellDirLUT/PlanetCenter 注入 MID 参数。
    //    若 Material 为空：退回 PMC 默认白材质（仍能看到几何 + VertexColor）。
    //    若材质里没有名为 "CellAttrLUT"/"CellDirLUT"/"PlanetCenter" 的参数：MID 创建会成功，
    //    SetXxxParameterValue 对未定义参数是 no-op（UE 不会报错），此时材质等价于 R2/R3 fallback。
    MID = nullptr;
    if (Material)
    {
        MID = UMaterialInstanceDynamic::Create(Material, this);
        if (MID)
        {
            // R3 注入
            if (CellAttrLUT)
            {
                MID->SetTextureParameterValue(TEXT("CellAttrLUT"), CellAttrLUT);
            }
            MID->SetScalarParameterValue(TEXT("NumLayersHint"), (float)NumLayersHint);
            MID->SetScalarParameterValue(TEXT("NumCells"), (float)NumCells);

            // R4 新增注入
            if (CellDirLUT)
            {
                MID->SetTextureParameterValue(TEXT("CellDirLUT"), CellDirLUT);
            }
            // PlanetCenter = Actor 世界坐标，PS 端：dir = normalize(WorldPosition - PlanetCenter)
            const FVector ActorLoc = GetActorLocation();
            MID->SetVectorParameterValue(TEXT("PlanetCenter"),
                FLinearColor((float)ActorLoc.X, (float)ActorLoc.Y, (float)ActorLoc.Z, 0.0f));

            // R5 新增注入：软边过渡带宽度（弧度）
            //   PS 端：w_i = smoothstep(EdgeWidth/2, -EdgeWidth/2, δ_i)
            //   EdgeWidth=0 退化为 R4 硬边；EdgeWidth>0 过渡带总宽度 = EdgeWidth 弧度
            MID->SetScalarParameterValue(TEXT("EdgeWidth"), EdgeWidth);

            // R6 新增注入：边界噪声振幅与频率
            //   PS 端：δ̃_i = δ_i + Noise3D(dir * NoiseScale + V_i * 7.919) * NoiseAmplitude
            //   NoiseAmplitude=0 退化为 R5；NoiseAmplitude>0 令 cell 边呈现蔓蜒曲线
            //   per-cell 采样以保证跨 mesh 边连续（详见 R6_BoundaryNoise.md §1.5）
            MID->SetScalarParameterValue(TEXT("NoiseAmplitude"), NoiseAmplitude);
            MID->SetScalarParameterValue(TEXT("NoiseScale"),     NoiseScale);

            // R7 新增注入：地表 Texture2DArray + Triplanar 参数
            //   PS 端：color_i = SampleTriplanar(TerrainAlbedoArray, layer_i, WorldPos, dir)
            //   - layer_i 从 R3 的 CellAttrLUT.r * 255 读取（slice 下标）
            //   - dir 是未扰动的球面外法（不是 R6 dirP，详见 R7_TerrainTriplanar.md §1.4 / 附录 C）
            //   - TileScale 单位 cm/周期，跨星球大小调这个值而不是缩放坐标
            //   - TriplanarSharpness 推荐 4；SetTextureParameterValue 对 Texture2DArray 适用
            if (TerrainAlbedoArray)
            {
                MID->SetTextureParameterValue(TEXT("TerrainAlbedoArray"), TerrainAlbedoArray);
            }
            if (TerrainNormalArray)
            {
                MID->SetTextureParameterValue(TEXT("TerrainNormalArray"), TerrainNormalArray);
            }
            MID->SetScalarParameterValue(TEXT("TriplanarSharpness"), TriplanarSharpness);
            MID->SetScalarParameterValue(TEXT("TileScale"),         TileScale);

            // R8 新增注入：3 base PBR Array + 3 LUT + 水面参数
            //   PS 端：每 cell 取 BaseTexIdx (LUT0.R) → PBRBaseAlbedo slice，
            //          按 OverlayBlend (LUT0.B) 叠加 Forest（slice 2 固定），
            //          按 LUT1.RGB Tint、LUT2 HSV/Roughness、LUT3 NormalStr/TriScale 微调。
            //   详见 Docs/R8_ParametricTint.md §3.2.6。
            if (PBRBaseAlbedo)    { MID->SetTextureParameterValue(TEXT("PBRBaseAlbedo"),    PBRBaseAlbedo);    }
            if (PBRBaseNormal)    { MID->SetTextureParameterValue(TEXT("PBRBaseNormal"),    PBRBaseNormal);    }
            if (PBRBaseRoughness) { MID->SetTextureParameterValue(TEXT("PBRBaseRoughness"), PBRBaseRoughness); }
            // PBRBaseHeight: R8.2 起激活 SHFRM——HLSL 端读取。R8.0/R8.1 阶段也仍挂上避免 fallback 警告。
            if (PBRBaseHeight)    { MID->SetTextureParameterValue(TEXT("PBRBaseHeight"),    PBRBaseHeight);    }
            if (CellTintLUT)      { MID->SetTextureParameterValue(TEXT("CellTintLUT"),      CellTintLUT);      }
            if (CellHSVRoughLUT)  { MID->SetTextureParameterValue(TEXT("CellHSVRoughLUT"),  CellHSVRoughLUT);  }
            if (CellNSpecLUT)     { MID->SetTextureParameterValue(TEXT("CellNSpecLUT"),     CellNSpecLUT);     }

            // R8.2 新增注入：SHFRM 参数（详见 Docs/R8.2_SphericalHeightFieldRaymarching.md §4.4）。
            //   - GlobeRadiusCM：球半径，HLSL 用 length(P - PlanetCenter) - GlobeRadiusCM 算径向高度
            //   - MaxRaymarchDepthCM：raymarch 沿视线最大深度（cm，R8.2 默认 75 = HeightScale 上限 50 × 1.5）
            //   - MaxRaymarchSteps：线性步数（R8.2 默认 16；HLSL 中是写死的常量，本参数仅提供 Editor 侧可见）
            // PlanetCenter 在 R4 阶段已注入（本函数开头那一行），此处不重复。
            MID->SetScalarParameterValue(TEXT("GlobeRadiusCM"),       Radius);
            MID->SetScalarParameterValue(TEXT("MaxRaymarchDepthCM"),  MaxRaymarchDepthCM);
            MID->SetScalarParameterValue(TEXT("MaxRaymarchSteps"),    static_cast<float>(MaxRaymarchSteps));
        }

        // 注意：不能写 `MID ? (UMaterialInterface*)MID : Material` —— Material 是
        // TObjectPtr<UMaterialInterface>，与裸指针在三元运算符里产生类型歧义，
        // 进而让 SetMaterial 的重载解析失败。这里显式拆成 if/else。
        UMaterialInterface* MatToApply = MID ? static_cast<UMaterialInterface*>(MID) : Material.Get();
        MeshComp->SetMaterial(0, MatToApply);

        // ---- 诊断：枚举材质里的 Custom 节点并打印 Code 字段 ----
        // 因为 .uasset 二进制 dump 看不到 HLSL 关键字，我们必须从 cpp 端反射读
        // 编辑器内存里 UMaterialExpressionCustom::Code 的真实内容来证伪。
        //
        // R8 期望材质里至少有一个 Custom 节点拥有以下 17 个 Inputs（R7 13 项 + R8 新增 4 项，
        // R7 的 terrainalbedoarray 在 R8 已被 pbrbasealbedo + 4 LUT 取代）：
        //   UV0, UV1, UV2, UV3, WorldPos, PlanetCenter, CellAttrLUT, CellDirLUT,
        //   EdgeWidth, NoiseAmplitude, NoiseScale, TriplanarSharpness, TileScale,
        //   PBRBaseAlbedo, CellTintLUT, CellHSVRoughLUT, CellNSpecLUT。
        // PBRBaseNormal/Roughness/Height 是 R8 可选项，反射诊断不强制要求。
        // 详见 Docs/R8_ParametricTint.md §3.2.7。
#if WITH_EDITORONLY_DATA
        if (UMaterial* BaseMat = Material->GetMaterial())
        {
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
                TEXT("pbrbasealbedo"),                                              // R8 替代 terrainalbedoarray
                TEXT("pbrbasenormal"),                                              // R8.1 新增：三通道 Normal
                TEXT("pbrbaseroughness"),                                           // R8.1 新增：三通道 Roughness
                TEXT("celltintlut"), TEXT("cellhsvroughlut"), TEXT("cellnspeclut"), // R8 新增 3 LUT
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
                if (UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expr))
                {
                    ++CustomFound;
                    const FString CodeStr = Custom->Code;
                    UE_LOG(LogPlanetTopologyDebugMesh, Warning,
                        TEXT("[PlanetTopologyDebugMesh] Material '%s' Custom expression #%d  ")
                        TEXT("Description='%s'  OutputType=%d  Code.Length=%d  Inputs.Num=%d"),
                        *BaseMat->GetName(), CustomFound,
                        *Custom->Description,
                        (int32)Custom->OutputType.GetValue(),
                        CodeStr.Len(),
                        Custom->Inputs.Num());

                    // 收集本节点已声明的 Inputs（小写）
                    TSet<FString> PresentLower;
                    TSet<FString> ConnectedLower;

                    // 打印 Inputs 名字与顺序
                    for (int32 I = 0; I < Custom->Inputs.Num(); ++I)
                    {
                        const FString InputName = Custom->Inputs[I].InputName.ToString();
                        const FString Lower     = InputName.ToLower();
                        const bool    bConn     = (Custom->Inputs[I].Input.GetTracedInput().Expression != nullptr);
                        PresentLower.Add(Lower);
                        if (bConn) { ConnectedLower.Add(Lower); }

                        UE_LOG(LogPlanetTopologyDebugMesh, Warning,
                            TEXT("    Input[%d] Name='%s'  Connected=%s"),
                            I, *InputName, bConn ? TEXT("YES") : TEXT("NO"));
                    }

                    // 打印 Code 字段（按 ~200 字节切片打印，避免单行 log 截断）
                    const int32 ChunkSize = 200;
                    for (int32 Off = 0; Off < CodeStr.Len(); Off += ChunkSize)
                    {
                        const int32 Take = FMath::Min(ChunkSize, CodeStr.Len() - Off);
                        UE_LOG(LogPlanetTopologyDebugMesh, Warning,
                            TEXT("    Code[%4d..%4d]: %s"),
                            Off, Off + Take,
                            *CodeStr.Mid(Off, Take).Replace(TEXT("\n"), TEXT("\\n"))
                                                    .Replace(TEXT("\r"), TEXT("\\r")));
                    }

                    // ---- R8 合规性检查 ----
                    TArray<FString> Missing;
                    TArray<FString> Disconnected;
                    for (const FString& Need : ExpectedR8Inputs)
                    {
                        if (!PresentLower.Contains(Need))         { Missing.Add(Need); }
                        else if (!ConnectedLower.Contains(Need))  { Disconnected.Add(Need); }
                    }

                    if (Missing.Num() == 0 && Disconnected.Num() == 0)
                    {
                        bAnyR8Compliant = true;
                        UE_LOG(LogPlanetTopologyDebugMesh, Warning,
                            TEXT("    ✓ R8 compliance: ALL %d expected inputs present & connected"),
                            ExpectedR8Inputs.Num());
                    }
                    else
                    {
                        if (Missing.Num() > 0)
                        {
                            UE_LOG(LogPlanetTopologyDebugMesh, Error,
                                TEXT("    ✗ R8 missing inputs: [%s] — this is likely an R2/R3/R4/R5/R6/R7 material, not R8"),
                                *FString::Join(Missing, TEXT(", ")));
                        }
                        if (Disconnected.Num() > 0)
                        {
                            UE_LOG(LogPlanetTopologyDebugMesh, Error,
                                TEXT("    ✗ R8 inputs declared but NOT connected: [%s]"),
                                *FString::Join(Disconnected, TEXT(", ")));
                        }
                    }
                }
            }
            if (CustomFound == 0)
            {
                UE_LOG(LogPlanetTopologyDebugMesh, Error,
                    TEXT("[PlanetTopologyDebugMesh] Material '%s' contains NO UMaterialExpressionCustom nodes!"),
                    *BaseMat->GetName());
            }
            else if (!bAnyR8Compliant)
            {
                UE_LOG(LogPlanetTopologyDebugMesh, Error,
                    TEXT("[PlanetTopologyDebugMesh] Material '%s' has %d Custom nodes but NONE is R8-compliant. ")
            TEXT("Expected one Custom with inputs: UV0,UV1,UV2,UV3,WorldPos,PlanetCenter,CellAttrLUT,CellDirLUT,EdgeWidth,NoiseAmplitude,NoiseScale,TriplanarSharpness,TileScale,PBRBaseAlbedo,CellTintLUT,CellHSVRoughLUT,CellNSpecLUT,PBRBaseHeight. ")
                    TEXT("See R8_ParametricTint.md §4 for the exact wiring."),
                    *BaseMat->GetName(), CustomFound);
            }
            else
            {
                UE_LOG(LogPlanetTopologyDebugMesh, Log,
                    TEXT("[PlanetTopologyDebugMesh] Material '%s' is R8-compliant ✓"),
                    *BaseMat->GetName());
            }
        }
#endif
    }

    UE_LOG(LogPlanetTopologyDebugMesh, Log,
        TEXT("[PlanetTopologyDebugMesh] Rebuilt (R8: 3-base + 4-LUT parametric tint). ")
        TEXT("SubdivisionLevel=%d  Cells=%d  Corners=%d  Verts=%d  Tris=%d  Radius=%.1f  Smooth=%s  ")
        TEXT("CellAttrLUT=%s  CellDirLUT=%s  CellTintLUT=%s  CellHSVRoughLUT=%s  CellNSpecLUT=%s  ")
        TEXT("PlanetCenter=(%.1f,%.1f,%.1f)  ")
        TEXT("EdgeWidth=%.4f rad (%.2f°)  NoiseAmplitude=%.4f rad (%.2f°)  NoiseScale=%.1f /rad  ")
        TEXT("TerrainAlbedoArray=%s  TerrainNormalArray=%s  ")
        TEXT("PBRBaseAlbedo=%s  PBRBaseNormal=%s  PBRBaseRoughness=%s  PBRBaseHeight=%s  ")
        TEXT("TileScale=%.0f cm  TriplanarSharpness=%.1f  ")
        TEXT("UseR8Placeholder=%s  EnableWaterShell=%s  WaterMaterial=%s  WaterSurfaceOffset=%.1f cm  ")
        TEXT("NumLayersHint=%d"),
        SubdivisionLevel, NumCells, NumCorners,
        Vertices.Num(), Triangles.Num() / 3, Radius,
        bSmoothNormals ? TEXT("true") : TEXT("false"),
        CellAttrLUT     ? TEXT("OK") : TEXT("MISSING"),
        CellDirLUT      ? TEXT("OK") : TEXT("MISSING"),
        CellTintLUT     ? TEXT("OK") : TEXT("MISSING"),
        CellHSVRoughLUT ? TEXT("OK") : TEXT("MISSING"),
        CellNSpecLUT    ? TEXT("OK") : TEXT("MISSING"),
        GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z,
        EdgeWidth,      EdgeWidth      * 180.0 / PI,
        NoiseAmplitude, NoiseAmplitude * 180.0 / PI,
        NoiseScale,
        TerrainAlbedoArray ? TEXT("OK") : TEXT("(none)"),
        TerrainNormalArray ? TEXT("OK") : TEXT("(none)"),
        PBRBaseAlbedo    ? TEXT("OK") : TEXT("MISSING"),
        PBRBaseNormal    ? TEXT("OK") : TEXT("(none)"),
        PBRBaseRoughness ? TEXT("OK") : TEXT("(none)"),
        PBRBaseHeight    ? TEXT("OK") : TEXT("(none)"),
        TileScale,
        TriplanarSharpness,
        bUseR8PlaceholderRecipes ? TEXT("YES") : TEXT("no"),
        bEnableWaterShell ? TEXT("YES") : TEXT("no"),
        WaterMaterial    ? *WaterMaterial->GetName()  : TEXT("(none)"),
        WaterSurfaceOffset,
        NumLayersHint);

    // ===== W2: WorldGen 板块构造 + 海陆分离 =====
    // W2 已在 RebuildCellAttrLUT_(NumCells) 之前完成 Generator 重建与 Generate()，
    // CellAttrLUT 已经按 Generator->GetCellData()[i].bIsLand 写入（DebugView=None 默认两色）；
    // 这里末尾不再重复运行——保留代码块占位，方便 W3+ 在此追加流水线后处理（如
    // 反射诊断、AssetTagSync 等不影响 LUT 的副效应）。
    // 详见 Docs/W2_PlatesAndLandSea.md §4。

    // ===== R8: 水面层重建（Component 子对象路径）=====
    //
    //   bEnableWaterShell=false → ClearAllMeshSections + 隐形；
    //   bEnableWaterShell=true  → 用 (Radius+WaterSurfaceOffset) 重铺 sub=3 球皮 +
    //                              SetMaterial(0, WaterMaterial)。
    //
    //   该路径取代了早期"SpawnActor 独立 APlanetWaterShell"路径——后者在 PIE 启动时
    //   会因 OnConstruction-SpawnActor 时序坑导致材质丢失（变成默认棋盘格球）。详见
    //   Docs/AgentWorkflow.md §3.10。Component 子对象的生命周期由 Owner Actor 管，
    //   PIE 深拷贝 / 编辑器 OnConstruction 重跑都不会丢任何状态。
    //
    //   验收期：
    //     - bEnableWaterShell=false                   → 单看 17 配方地形；
    //     - bEnableWaterShell=true + MeshComp.Visibility=false → 单看水面球。
    //
    //   详见 Docs/R8_ParametricTint.md §3.2.10 / §4.5。
    RebuildWaterMesh_();
}

// =====================================================================
//  R3：CellAttrLUT 构建 / 填充。
//
//  纹理布局：
//    - 大小：1 × NumCells（X 轴是 CellId，Y 轴只有 1 行）
//    - 格式：PF_B8G8R8A8（与 UTexture2D::CreateTransient 默认对齐）
//    - 通道含义：
//        R = LayerIndex（uint8，本阶段为 placeholder：Knuth 哈希取模 NumLayersHint）
//        G = LayerDecor（R8 阶段启用，目前 0）
//        B = Variant   （R8 阶段启用，目前 0）
//        A = Mask      （R8 阶段启用，目前 0）
//    - Filter = TF_Nearest（必须，禁止双线性插值，否则邻居 layer 会被混合）
//    - SRGB   = false（R 通道是离散整数，不能被 sRGB→Linear 解码）
//    - AddressX/Y = TA_Clamp（Cell 编号必须在边界内，越界视为最后一格）
//
//  GPU 端访问范式（材质 Custom 节点）：
//      LUT.Load(int3(CellId, 0, 0)).r * 255.0  →  LayerIndex（float）
//
//  上传方式选择：
//      Rebuild 是低频事件（编辑器拖入 / 改参数 / OnConstruction），不需要 RHI 异步上传，
//      直接走 PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE) → memcpy → Unlock →
//      UpdateResource() 即可。这是 UE 5.x UTexture2D 同步上传的标准范式。
// =====================================================================
void APlanetTopologyDebugMesh::RebuildCellAttrLUT_(int32 NumCells)
{
    if (NumCells <= 0)
    {
        CellAttrLUT = nullptr;
        return;
    }

    const int32 LayerMod = FMath::Clamp(NumLayersHint, 1, 256);

    // 每次都重新创建一张纹理（NumCells 在不同 SubdivisionLevel 下会变，简单做法是重建）。
    UTexture2D* NewLUT = UTexture2D::CreateTransient(NumCells, 1, PF_B8G8R8A8, TEXT("CellAttrLUT_Transient"));
    if (!NewLUT)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Error,
            TEXT("[PlanetTopologyDebugMesh] CreateTransient(CellAttrLUT) failed (NumCells=%d)"), NumCells);
        CellAttrLUT = nullptr;
        return;
    }

    NewLUT->Filter        = TF_Nearest;
    NewLUT->SRGB          = false;
    NewLUT->AddressX      = TA_Clamp;
    NewLUT->AddressY      = TA_Clamp;
    NewLUT->CompressionSettings = TC_VectorDisplacementmap;   // 等效 BGRA8 不压缩
    NewLUT->NeverStream   = true;
    // 用 ColorLookupTable LODGroup —— 这是 UE 引擎源码中明确为 LUT 设计的组：
    // Filter 默认 Nearest、强制 SRGB=false、强制 NoMipmaps、强制不压缩。
    // 比 Pixels2D 更严格、更可靠（Pixels2D 在 cooked 平台上可能被 ini 覆写成 Bilinear）。
    NewLUT->LODGroup      = TEXTUREGROUP_ColorLookupTable;
    NewLUT->MipGenSettings = TMGS_NoMipmaps;

    // 锁 Mip0 写数据。BGRA8 每像素 4 字节。
    FTexturePlatformData* Plat = NewLUT->GetPlatformData();
    if (!Plat || Plat->Mips.Num() == 0)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Error,
            TEXT("[PlanetTopologyDebugMesh] CellAttrLUT has no mip 0; abort fill."));
        CellAttrLUT = nullptr;
        return;
    }

    FByteBulkData& Bulk = Plat->Mips[0].BulkData;
    uint8* Dst = static_cast<uint8*>(Bulk.Lock(LOCK_READ_WRITE));
    if (!Dst)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Error,
            TEXT("[PlanetTopologyDebugMesh] Failed to Lock CellAttrLUT mip 0."));
        CellAttrLUT = nullptr;
        return;
    }

    // Knuth 整数哈希常数（黄金分割比 × 2^32）：保证相邻 CellId 也能落到不同 layer。
    constexpr uint32 KnuthHash = 2654435761u;

    // ★ W4：预构建 Tag → UTerrainDefinition 反查表，供 Biome / None 默认分支按 Def->LayerIndex 写 LUT.R。
    //   - Step_ClassifyBiomes 已把各 cell 的 TerrainTag 写入 CellData[]；
    //   - 本函数仅需查一次 LayerIndex 写入 R 通道。
    //   - 详见 Docs/W4_BiomeClassification.md §A.10。
    TMap<FGameplayTag, UTerrainDefinition*> TagToDefMap;
    if (UTerrainSet* TSet = WorldGenSettings.TerrainSet.LoadSynchronous())
    {
        TSet->LoadSynchronous();
        const TArray<UTerrainDefinition*>& Defs = TSet->GetLoadedDefs();
        TagToDefMap.Reserve(Defs.Num());
        for (UTerrainDefinition* Def : Defs)
        {
            if (Def && Def->TerrainTag.IsValid())
            {
                TagToDefMap.Add(Def->TerrainTag, Def);
            }
        }
    }

    // 小 Lambda：根据 CD 按 DebugView 计算 Layer（身体 + 诊断复用同一份逻辑，避免剧本偏移）。
    auto ComputeLayerForCell = [&](const FCellGeoData& CD) -> uint8
    {
        switch (DebugView)
        {
            case EWorldGenDebugView::PlateId:
            {
                const uint32 Pid    = (uint32)FMath::Max(0, CD.PlateId);
                const uint32 Hashed = (Pid * KnuthHash) >> 24;
                return (uint8)(Hashed % 19u);
            }
            case EWorldGenDebugView::Elevation:
            {
                const float Norm = FMath::Clamp((CD.Elevation + 1.0f) * 0.5f, 0.0f, 1.0f);
                return (uint8)FMath::FloorToInt(Norm * 18.0f);
            }
            case EWorldGenDebugView::Moisture:
            {
                return (uint8)FMath::FloorToInt(FMath::Clamp(CD.Moisture, 0.0f, 1.0f) * 18.0f);
            }
            case EWorldGenDebugView::Temperature:
            {
                const float Norm = FMath::Clamp((CD.Temperature + 1.0f) * 0.5f, 0.0f, 1.0f);
                return (uint8)FMath::FloorToInt(Norm * 18.0f);
            }
            case EWorldGenDebugView::Mountain:
            {
                return CD.bIsMountain ? (uint8)11 : (CD.bIsLand ? (uint8)4 : (uint8)0);
            }
            case EWorldGenDebugView::LandSea:
            {
                return CD.bIsLand ? (uint8)4 : (uint8)0;
            }
            case EWorldGenDebugView::Biome:
            case EWorldGenDebugView::None:
            default:
            {
                // ★ R8：当启用 placeholder 配方时，优先返回 17 配方哈希索引（0..16），
                //   覆盖 W4 真实 Layer。R8 主验收期默认走此分支；W4 联调时把
                //   bUseR8PlaceholderRecipes 关闭即可切回 W4 路径。
                if (bUseR8PlaceholderRecipes)
                {
                    return (uint8)R8_PlaceholderRecipeIndex(CD.CellId);
                }
                // ★ W4：默认视图 = Biome 真实分类。
                // CD.TerrainTag 未设（None）或 Tag 不在表中（TerrainSet 未挂） → fallback LayerIndex 0。
                if (UTerrainDefinition* const* Found = TagToDefMap.Find(CD.TerrainTag))
                {
                    if (*Found)
                    {
                        return (uint8)FMath::Clamp((*Found)->LayerIndex, 0, 255);
                    }
                }
                return (uint8)0;
            }
        }
    };

    // ★ W2：从 WorldGen 取每 Cell 的板块/海陆数据；按 DebugView 切换 R 通道写入逻辑。
    //   - W4 上起：默认视图 升级为 Biome（Def->LayerIndex），LandSea 仍作为独立选项供回归。
    //   - PlateId / Elevation / Moisture / Temperature / Mountain 沿用 W2/W3 语义。
    //   详见 Docs/W4_BiomeClassification.md §A.10 / Docs/W2_PlatesAndLandSea.md §A.3。
    const TArray<FCellGeoData>* CellsPtr =
        (Generator.IsValid() && Generator->GetCellData().Num() == NumCells)
            ? &Generator->GetCellData()
            : nullptr;

    for (int32 CellId = 0; CellId < NumCells; ++CellId)
    {
        uint8 Layer = 0;
        if (CellsPtr)
        {
            Layer = ComputeLayerForCell((*CellsPtr)[CellId]);
        }
        else
        {
            // Fallback：Generator 失效时使用 R8 placeholder（与上面 default 分支一致），
            // 否则使用 W2 之前的 Knuth 哈希按 NumLayersHint 取模。
            if (bUseR8PlaceholderRecipes)
            {
                Layer = (uint8)R8_PlaceholderRecipeIndex(CellId);
            }
            else
            {
                const uint32 Hashed = (uint32)CellId * KnuthHash;
                Layer = (uint8)(Hashed % (uint32)LayerMod);
            }
        }

        // BGRA 顺序写入。R 通道存 Layer，其余先填 0（R8 阶段会启用）。
        const int32 Offset = CellId * 4;
        Dst[Offset + 0] = 0;       // B = Variant   (R8+)
        Dst[Offset + 1] = 0;       // G = LayerDecor (R8+)
        Dst[Offset + 2] = Layer;   // R = LayerBase
        Dst[Offset + 3] = 0;       // A = Mask       (R8+)
    }

    Bulk.Unlock();

    // ---- 诊断（运行时确认写入正确性）----
    // Re-Lock 只读，dump 前 16 个 cell 的 (B,G,R,A) 四个字节，验证 R 通道存的就是 LayerIndex。
    // 如果这里打印的 R 列与上面 for-loop 计算的 Layer 不一致，
    // 说明 PF_B8G8R8A8 的内存字节顺序与我假设的不同，需要调整 Offset+0..3 的赋值方式。
    // ★ W2：Expected 算法跟随 R 通道写入逻辑分支同步，便于在 PIE 排错时一眼判断写入正确性。
    {
        const uint8* Verify = static_cast<const uint8*>(Bulk.LockReadOnly());
        if (Verify)
        {
            const int32 NumDump = FMath::Min(NumCells, 16);
            FString Dump;
            for (int32 i = 0; i < NumDump; ++i)
            {
                uint8 Expected = 0;
                if (CellsPtr)
                {
                    // ★ W4：复用上面的 ComputeLayerForCell lambda，避免主写入与诊断逻辑剧本偏移。
                    Expected = ComputeLayerForCell((*CellsPtr)[i]);
                }
                else if (bUseR8PlaceholderRecipes)
                {
                    Expected = (uint8)R8_PlaceholderRecipeIndex(i);
                }
                else
                {
                    const uint32 Hashed = (uint32)i * KnuthHash;
                    Expected = (uint8)(Hashed % (uint32)LayerMod);
                }
                Dump += FString::Printf(TEXT("  Cell%-3d: B=%3u G=%3u R=%3u A=%3u (expected layer=%u)\n"),
                    i,
                    Verify[i * 4 + 0],
                    Verify[i * 4 + 1],
                    Verify[i * 4 + 2],
                    Verify[i * 4 + 3],
                    Expected);
            }
            const TCHAR* DebugViewName =
                (DebugView == EWorldGenDebugView::PlateId)     ? TEXT("PlateId")
              : (DebugView == EWorldGenDebugView::LandSea)     ? TEXT("LandSea")
              : (DebugView == EWorldGenDebugView::Elevation)   ? TEXT("Elevation")
              : (DebugView == EWorldGenDebugView::Moisture)    ? TEXT("Moisture")
              : (DebugView == EWorldGenDebugView::Temperature) ? TEXT("Temperature")
              : (DebugView == EWorldGenDebugView::Mountain)    ? TEXT("Mountain")
              : (DebugView == EWorldGenDebugView::Biome)       ? TEXT("Biome")
              :                                                  TEXT("None");
            UE_LOG(LogPlanetTopologyDebugMesh, Log,
                TEXT("[PlanetTopologyDebugMesh] CellAttrLUT first %d cells (NumCells=%d, DebugView=%s, NumLayersHint=%d, LayerMod=%d):\n%s"),
                NumDump, NumCells, DebugViewName, NumLayersHint, LayerMod, *Dump);
            Bulk.Unlock();
        }
        else
        {
            UE_LOG(LogPlanetTopologyDebugMesh, Warning,
                TEXT("[PlanetTopologyDebugMesh] LockReadOnly failed for verification."));
        }
    }

    NewLUT->UpdateResource();   // 把 Mip0 的字节流提交到 GPU

    CellAttrLUT = NewLUT;
}

// =====================================================================
//  R4：CellDirLUT 构建 / 填充。
//
//  纹理布局：
//    - 大小：1 × NumCells（X 轴是 CellId，Y 轴只有 1 行）
//    - 格式：PF_A32B32G32R32F（FP32 RGBA，每像素 16 字节）
//    - 通道含义：
//        R = UnitCenter.x   （cell 中心方向，单位向量）
//        G = UnitCenter.y
//        B = UnitCenter.z
//        A = bIsPentagon ? 1.0 : 0.0   （预留给 R5+ pent 特判）
//    - Filter = TF_Nearest（必须，禁止双线性插值）
//    - SRGB   = false（线性几何数据，不是颜色）
//    - AddressX/Y = TA_Clamp
//
//  GPU 端访问范式（材质 Custom 节点）：
//      float3 V_i = CellDirLUT.Load(int3(CellId, 0, 0)).rgb;
//      float  d_i = dot(dir, V_i);
//      // chosen = argmax_i(d_i)
//
//  为什么必须用 FP32（不能用 R16F / RGBA8）：
//    R16F 的 mantissa 只有 10 bit，对单位球上相邻 cell 中心方向（其差 ≤ 1°）的精度
//    不足以区分两个 cell 的 dot 距离差异，导致 argmax 在 cell 边界附近频繁误判。
//    RGBA8 更糟（量化步长 1/255）。FP32 在 sub=3 时只占 642×16 = ~10 KB，
//    常驻 GPU L2 cache，性能完全没问题。
//
//  纹理压缩设置：
//    - CompressionSettings = TC_HDR：保留 FP32 不被 BC* 压缩破坏（FP32 LUT 常用设置）
//    - LODGroup = TEXTUREGROUP_HDR / 自定义：但因为是 Transient 纹理，LODGroup 影响有限
//    - 关键是 PixelFormat 必须保持 PF_A32B32G32R32F，不能被 platform data 转码
//
//  FP32 / 与 RebuildCellAttrLUT_ 的实现差异：
//    - Bulk.Lock 后写入的是 float* 而不是 uint8*，每像素 4 个 float
//    - 不需要 BGRA 顺序倒序（FP32 RGBA 内存布局就是 RGBA）
//    - 上传机制完全相同（Lock → memcpy → Unlock → UpdateResource）
// =====================================================================
void APlanetTopologyDebugMesh::RebuildCellDirLUT_(int32 NumCells)
{
    if (NumCells <= 0 || !Topology.IsValid())
    {
        CellDirLUT = nullptr;
        return;
    }

    UTexture2D* NewLUT = UTexture2D::CreateTransient(NumCells, 1, PF_A32B32G32R32F, TEXT("CellDirLUT_Transient"));
    if (!NewLUT)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Error,
            TEXT("[PlanetTopologyDebugMesh] CreateTransient(CellDirLUT) failed (NumCells=%d)"), NumCells);
        CellDirLUT = nullptr;
        return;
    }

    NewLUT->Filter        = TF_Nearest;
    NewLUT->SRGB          = false;
    NewLUT->AddressX      = TA_Clamp;
    NewLUT->AddressY      = TA_Clamp;
    NewLUT->NeverStream   = true;
    NewLUT->CompressionSettings = TC_HDR;        // FP32 RGBA，不压缩
    NewLUT->MipGenSettings      = TMGS_NoMipmaps;

    FTexturePlatformData* Plat = NewLUT->GetPlatformData();
    if (!Plat || Plat->Mips.Num() == 0)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Error,
            TEXT("[PlanetTopologyDebugMesh] CellDirLUT has no mip 0; abort fill."));
        CellDirLUT = nullptr;
        return;
    }

    FByteBulkData& Bulk = Plat->Mips[0].BulkData;
    float* Dst = static_cast<float*>(Bulk.Lock(LOCK_READ_WRITE));
    if (!Dst)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Error,
            TEXT("[PlanetTopologyDebugMesh] Failed to Lock CellDirLUT mip 0."));
        CellDirLUT = nullptr;
        return;
    }

    for (int32 CellId = 0; CellId < NumCells; ++CellId)
    {
        const FCell&   Cell = Topology->Cells[CellId];
        const FVector& U    = Cell.UnitCenter;

        const int32 Off = CellId * 4;
        Dst[Off + 0] = (float)U.X;                       // R = UnitCenter.x
        Dst[Off + 1] = (float)U.Y;                       // G = UnitCenter.y
        Dst[Off + 2] = (float)U.Z;                       // B = UnitCenter.z
        Dst[Off + 3] = Cell.bIsPentagon ? 1.0f : 0.0f;   // A = isPentagon
    }

    Bulk.Unlock();

    // ---- 诊断（运行时确认写入正确性）----
    // dump 前 16 个 cell 的 (R, G, B, A) 四个 float，以及 |UnitCenter| 是否 ≈ 1.0
    {
        const float* Verify = static_cast<const float*>(Bulk.LockReadOnly());
        if (Verify)
        {
            const int32 NumDump = FMath::Min(NumCells, 16);
            FString Dump;
            for (int32 i = 0; i < NumDump; ++i)
            {
                const float Rx = Verify[i * 4 + 0];
                const float Gy = Verify[i * 4 + 1];
                const float Bz = Verify[i * 4 + 2];
                const float Aw = Verify[i * 4 + 3];
                const float Mag = FMath::Sqrt(Rx*Rx + Gy*Gy + Bz*Bz);
                Dump += FString::Printf(
                    TEXT("  Cell%-3d: R=%+.4f G=%+.4f B=%+.4f A=%.1f  |V|=%.4f  isPent=%s\n"),
                    i, Rx, Gy, Bz, Aw, Mag,
                    (Aw > 0.5f) ? TEXT("YES") : TEXT("no"));
            }
            UE_LOG(LogPlanetTopologyDebugMesh, Log,
                TEXT("[PlanetTopologyDebugMesh] CellDirLUT first %d cells (NumCells=%d, FP32 RGBA):\n%s"),
                NumDump, NumCells, *Dump);
            Bulk.Unlock();
        }
        else
        {
            UE_LOG(LogPlanetTopologyDebugMesh, Warning,
                TEXT("[PlanetTopologyDebugMesh] LockReadOnly failed for CellDirLUT verification."));
        }
    }

    NewLUT->UpdateResource();

    CellDirLUT = NewLUT;
}

// =====================================================================
//  R8：CellTintLUT / CellHSVRoughLUT / CellNSpecLUT 构建 / 填充。
//
//  3 张 LUT 共享相同的纹理布局，仅 RGBA 通道含义不同：
//    - 大小：1 × NumCells
//    - 格式：PF_FloatRGBA（FP16x4，每像素 8 字节）
//    - Filter = TF_Nearest（必须，禁止双线性插值）
//    - SRGB   = false（线性参数，不是颜色）
//    - AddressX/Y = TA_Clamp
//
//  数据来源：bUseR8PlaceholderRecipes=true 时按 R8_PlaceholderRecipeIndex(CellId) 查 GR8Recipes 表；
//            false 时全部填 GR8NeutralRecipe（中性默认值，让 R8 4 LUT 不参与微调）。
//
//  GPU 端访问范式（材质 Custom 节点）：
//      float4 lut1 = CellTintLUT.Load(int3(CellId, 0, 0));     // RGB=Tint, A=HueShift
//      float4 lut2 = CellHSVRoughLUT.Load(int3(CellId, 0, 0)); // R=Sat, G=Bri, B=RMin, A=RMax
//      float4 lut3 = CellNSpecLUT.Load(int3(CellId, 0, 0));    // R=NormalStr, G=HeightScale, B=Spec, A=TriScale
//
//  详见 Docs/R8_ParametricTint.md §1.3 / §3.2.4。
// =====================================================================

// 内部 helper：根据 bUseR8PlaceholderRecipes 与 CellId 解析出该 cell 应使用的配方。
static const FR8Recipe& R8_PickRecipe(bool bUsePlaceholder, int32 CellId)
{
    if (bUsePlaceholder)
    {
        return GR8Recipes[R8_PlaceholderRecipeIndex(CellId)];
    }
    return GR8NeutralRecipe;
}

// 内部 helper：创建一张 1×NumCells 的 PF_FloatRGBA 动态纹理（用 R8 三 LUT 共用模板）。
static UTexture2D* R8_CreateFloatRGBALUT(int32 NumCells, const TCHAR* DebugName)
{
    UTexture2D* NewLUT = UTexture2D::CreateTransient(NumCells, 1, PF_FloatRGBA, DebugName);
    if (!NewLUT)
    {
        return nullptr;
    }

    NewLUT->Filter        = TF_Nearest;
    NewLUT->SRGB          = false;
    NewLUT->AddressX      = TA_Clamp;
    NewLUT->AddressY      = TA_Clamp;
    NewLUT->NeverStream   = true;
    NewLUT->CompressionSettings = TC_HDR;
    NewLUT->LODGroup      = TEXTUREGROUP_ColorLookupTable;
    NewLUT->MipGenSettings = TMGS_NoMipmaps;
    return NewLUT;
}

void APlanetTopologyDebugMesh::RebuildCellTintLUT_(int32 NumCells)
{
    if (NumCells <= 0)
    {
        CellTintLUT = nullptr;
        return;
    }

    UTexture2D* NewLUT = R8_CreateFloatRGBALUT(NumCells, TEXT("CellTintLUT_Transient"));
    if (!NewLUT)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Error,
            TEXT("[PlanetTopologyDebugMesh] CreateTransient(CellTintLUT) failed (NumCells=%d)"), NumCells);
        CellTintLUT = nullptr;
        return;
    }

    FTexturePlatformData* Plat = NewLUT->GetPlatformData();
    if (!Plat || Plat->Mips.Num() == 0)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Error,
            TEXT("[PlanetTopologyDebugMesh] CellTintLUT has no mip 0; abort fill."));
        CellTintLUT = nullptr;
        return;
    }

    FByteBulkData& Bulk = Plat->Mips[0].BulkData;
    FFloat16* Dst = static_cast<FFloat16*>(Bulk.Lock(LOCK_READ_WRITE));
    if (!Dst)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Error,
            TEXT("[PlanetTopologyDebugMesh] Failed to Lock CellTintLUT mip 0."));
        CellTintLUT = nullptr;
        return;
    }

    for (int32 c = 0; c < NumCells; ++c)
    {
        const FR8Recipe& R = R8_PickRecipe(bUseR8PlaceholderRecipes, c);
        Dst[c * 4 + 0] = FFloat16(R.TintR);
        Dst[c * 4 + 1] = FFloat16(R.TintG);
        Dst[c * 4 + 2] = FFloat16(R.TintB);
        Dst[c * 4 + 3] = FFloat16(0.0f);   // HueShift 预留 0
    }

    Bulk.Unlock();

    // 诊断：dump 前 8 个 cell（FP16 → float 转回打印），验证写入正确性
    {
        const FFloat16* Verify = static_cast<const FFloat16*>(Bulk.LockReadOnly());
        if (Verify)
        {
            const int32 NumDump = FMath::Min(NumCells, 8);
            FString Dump;
            for (int32 i = 0; i < NumDump; ++i)
            {
                const int32 RecipeIdx = bUseR8PlaceholderRecipes
                    ? R8_PlaceholderRecipeIndex(i)
                    : -1;
                Dump += FString::Printf(
                    TEXT("  Cell%-3d: Tint=(%.3f, %.3f, %.3f, %.3f)  RecipeIdx=%d\n"),
                    i,
                    Verify[i * 4 + 0].GetFloat(),
                    Verify[i * 4 + 1].GetFloat(),
                    Verify[i * 4 + 2].GetFloat(),
                    Verify[i * 4 + 3].GetFloat(),
                    RecipeIdx);
            }
            UE_LOG(LogPlanetTopologyDebugMesh, Log,
                TEXT("[PlanetTopologyDebugMesh] CellTintLUT first %d cells (NumCells=%d, UsePlaceholder=%s):\n%s"),
                NumDump, NumCells,
                bUseR8PlaceholderRecipes ? TEXT("YES") : TEXT("no"),
                *Dump);
            Bulk.Unlock();
        }
    }

    NewLUT->UpdateResource();
    CellTintLUT = NewLUT;
}

void APlanetTopologyDebugMesh::RebuildCellHSVRoughLUT_(int32 NumCells)
{
    if (NumCells <= 0)
    {
        CellHSVRoughLUT = nullptr;
        return;
    }

    UTexture2D* NewLUT = R8_CreateFloatRGBALUT(NumCells, TEXT("CellHSVRoughLUT_Transient"));
    if (!NewLUT)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Error,
            TEXT("[PlanetTopologyDebugMesh] CreateTransient(CellHSVRoughLUT) failed (NumCells=%d)"), NumCells);
        CellHSVRoughLUT = nullptr;
        return;
    }

    FTexturePlatformData* Plat = NewLUT->GetPlatformData();
    if (!Plat || Plat->Mips.Num() == 0)
    {
        CellHSVRoughLUT = nullptr;
        return;
    }

    FByteBulkData& Bulk = Plat->Mips[0].BulkData;
    FFloat16* Dst = static_cast<FFloat16*>(Bulk.Lock(LOCK_READ_WRITE));
    if (!Dst)
    {
        CellHSVRoughLUT = nullptr;
        return;
    }

    for (int32 c = 0; c < NumCells; ++c)
    {
        const FR8Recipe& R = R8_PickRecipe(bUseR8PlaceholderRecipes, c);
        Dst[c * 4 + 0] = FFloat16(R.SatMul);
        Dst[c * 4 + 1] = FFloat16(R.BriMul);
        Dst[c * 4 + 2] = FFloat16(R.RoughMin);
        Dst[c * 4 + 3] = FFloat16(R.RoughMax);
    }

    Bulk.Unlock();
    NewLUT->UpdateResource();
    CellHSVRoughLUT = NewLUT;
}

void APlanetTopologyDebugMesh::RebuildCellNSpecLUT_(int32 NumCells)
{
    if (NumCells <= 0)
    {
        CellNSpecLUT = nullptr;
        return;
    }

    UTexture2D* NewLUT = R8_CreateFloatRGBALUT(NumCells, TEXT("CellNSpecLUT_Transient"));
    if (!NewLUT)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Error,
            TEXT("[PlanetTopologyDebugMesh] CreateTransient(CellNSpecLUT) failed (NumCells=%d)"), NumCells);
        CellNSpecLUT = nullptr;
        return;
    }

    FTexturePlatformData* Plat = NewLUT->GetPlatformData();
    if (!Plat || Plat->Mips.Num() == 0)
    {
        CellNSpecLUT = nullptr;
        return;
    }

    FByteBulkData& Bulk = Plat->Mips[0].BulkData;
    FFloat16* Dst = static_cast<FFloat16*>(Bulk.Lock(LOCK_READ_WRITE));
    if (!Dst)
    {
        CellNSpecLUT = nullptr;
        return;
    }

    for (int32 c = 0; c < NumCells; ++c)
    {
        const FR8Recipe& R = R8_PickRecipe(bUseR8PlaceholderRecipes, c);
        // R8.2 激活：LUT3.G 存 [0, 1] 归一化 HeightScale，HLSL 端用 lut3.g * MaxRaymarchDepthCM 还原 cm。
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

// =====================================================================
//  R8：水面层 mesh 构建（Component 子对象路径）
//
//  几何方案：
//    复用 Grid 模块的 FSphereTopology(SubdivisionLevel=3)，得到 642 mesh 顶点 / 1280
//    primal 三角形（icosphere sub=3）。每个 Corner（=primal 三角形）展开为 3 个独立顶点
//    提交给 WaterMeshComp，与 R8 主 mesh 走完全一致的几何路径，便于 R8.5 自研球面网格
//    上线后无歧义切换。
//
//  选择 sub=3 而非更高细分的理由：
//    · 水面材质只需要 WorldPos + Time 做 fbm 法线扰动，几何精度无关；
//    · sub=3 仅 1280 三角形，OnConstruction 冷路径成本 < 1 ms；
//    · 顶点法线**直接用 UnitCenter**（球面外法）实现数学完美光滑球面着色——
//      不走 KismetProceduralMeshLibrary::CalculateTangentsForMesh，因为本路径每
//      Corner 展开 3 独立顶点（不共享），KismetTangents 在该几何上等价于 flat
//      shading（每三角形 3 顶点都拿到面法线），sub=3 球面会出现 1280 个清晰小棱
//      面。详见 AgentWorkflow.md §3.13"独立顶点 + KismetTangents 的 flat-shading 陷阱"。
//
//  与早期 SpawnActor 路径的差分：
//    早期方案是在 OnConstruction 中 SpawnActor<APlanetWaterShell>，把 mesh 包在独立 Actor
//    里。该路径在 PIE 启动时存在生命周期错位（Editor World → PIE World 深拷贝过程中
//    SpawnActor 出来的 Editor 临时 Actor 的材质引用会丢失，变成默认棋盘格球；退 PIE
//    时 Actor 被回收）。Component 是 Owner Actor 的 SubObject，PIE 拷贝跟随 Owner 走，
//    无任何额外管理代码。详见 [AgentWorkflow.md §3.10](../../../Docs/AgentWorkflow.md)。
// =====================================================================
void APlanetTopologyDebugMesh::RebuildWaterMesh_()
{
    if (!WaterMeshComp)
    {
        return;
    }

    // bEnableWaterShell=false 路径：清空 mesh + 隐形即可，不做几何工作
    if (!bEnableWaterShell)
    {
        WaterMeshComp->ClearAllMeshSections();
        WaterMeshComp->SetVisibility(false);
        return;
    }

    // ---- 1) 构建 / 复用 sub=3 拓扑 ----
    //
    // 使用固定 SubdivisionLevel=3。理由：
    //   · 642 verts / 1280 tris 已经足够把 fbm 噪声采样空间撑开（顶点间球面距 ≈ 8°）
    //   · 升到 sub=4 会让顶点数 ×4（→ 2562 verts），但水面 material 完全在 PS 端做扰动，
    //     几何细节无收益
    //   · sub=3 与 R8 主 mesh sub=3 顶点位置 1:1 对齐，将来 R8.5 接入自研球面网格后
    //     可直接共享拓扑实例
    if (!WaterTopology.IsValid())
    {
        WaterTopology = MakeUnique<FSphereTopology>(3);
    }

    const int32 NumCells   = WaterTopology->Cells.Num();
    const int32 NumCorners = WaterTopology->Corners.Num();
    if (NumCells == 0 || NumCorners == 0)
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Warning,
            TEXT("[PlanetTopologyDebugMesh] Empty water topology (Cells=%d, Corners=%d). Skip water build."),
            NumCells, NumCorners);
        WaterMeshComp->ClearAllMeshSections();
        WaterMeshComp->SetVisibility(false);
        return;
    }

    const float WaterRadius = Radius + WaterSurfaceOffset;

    // ---- 2) 装填顶点 / 索引缓冲 ----
    //
    // 每个 Corner 展开 3 个独立顶点（不共享），与 R8 主 mesh 路径一致。
    const int32 NumVerts = NumCorners * 3;

    TArray<FVector>          Vertices;
    TArray<int32>            Triangles;
    TArray<FVector>          Normals;
    TArray<FVector2D>        UV0;
    TArray<FVector2D>        UV1;
    TArray<FVector2D>        UV2;
    TArray<FVector2D>        UV3;
    TArray<FLinearColor>     VertexColors;
    TArray<FProcMeshTangent> Tangents;

    Vertices.Reserve(NumVerts);
    Triangles.Reserve(NumVerts);
    Normals.Reserve(NumVerts);
    UV0.Reserve(NumVerts);
    UV1.Reserve(NumVerts);
    UV2.Reserve(NumVerts);
    UV3.Reserve(NumVerts);
    VertexColors.Reserve(NumVerts);

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

        // 法线：直接用每个顶点的 UnitCenter（球面外法）作为顶点法线，达到
        // 数学上完美光滑的球面着色——水面在视觉上就该是连续光滑球面。
        //
        // 这里**故意不走** KismetProceduralMeshLibrary::CalculateTangentsForMesh
        // 自动法线路径——原因：水面 mesh 的几何与主 mesh 同款，每个 Corner 展开
        // 3 个独立顶点（不共享）。在这种"顶点完全独立"的拓扑上，KismetTangents
        // 算出来的法线等价于 flat shading（每个三角形的 3 个顶点都拿到面法线），
        // 球面渲染会出现 1280 个三角小棱面——这正是 R8 验收期实测看到的"水面有棱
        // 角"现象。详见 AgentWorkflow.md §3.6 / §3.13。
        //
        // 主 mesh 不能这么做（材质要展示 cell 边界 SDF），但水面 SLW 视觉就是
        // 一颗光滑反射球——直接用 UnitCenter 作为顶点法线既物理正确又零棱角。
        const FVector NA = WaterTopology->Cells[CA].UnitCenter;
        const FVector NB = WaterTopology->Cells[CB].UnitCenter;
        const FVector NC = WaterTopology->Cells[CC].UnitCenter;

        // 水面 material 不需要 cell 编码 UV / OneHot 重心 / VertexColor LayerHash，
        // 全部留默认 0（SLW 路径下 UV 也不会被材质消费）。
        const FVector2D    UVZero(0.0f, 0.0f);
        const FLinearColor ColWhite(1.0f, 1.0f, 1.0f, 1.0f);

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

        // ---- 3) 三角形索引 ----
        //
        // 沿用 FCorner.CellIds 原始顺序（CCW from outside）；不做 bFlipWinding。
        // 详见 R8 主 Rebuild() 的"绕序与法线坑"块和 SphereTopologyReference.md §11。
        Triangles.Add(BaseIdx + 0);
        Triangles.Add(BaseIdx + 1);
        Triangles.Add(BaseIdx + 2);
    }

    // ---- 4) Tangents 留空 ----
    //
    // Normals 已在装填顶点时直接用 UnitCenter 填好（球面外法 → 完美光滑球面着色，
    // 见上方装填段注释）。Tangent 不再调用 KismetTangents 自动计算——
    // KismetTangents 会**先算面法线再覆盖 Normals 数组**，把我们刚填好的光滑
    // UnitCenter 法线砸回 flat shading；其副产物 Tangent 又只在 SLW 路径下被忽略。
    //
    // 留空 Tangents 数组，PMC 端会按零向量处理；SLW Shading Model 通过 Normal 引脚
    // 直接消费 World Space 法线，不走 Tangent 空间反算 → 无视觉差异。
    Tangents.Reset();

    // ---- 5) 提交到 PMC ----
    WaterMeshComp->ClearAllMeshSections();
    WaterMeshComp->CreateMeshSection_LinearColor(
        /*SectionIndex*/ 0,
        Vertices,
        Triangles,
        Normals,
        UV0,
        UV1,
        UV2,
        UV3,
        VertexColors,
        Tangents,
        /*bCreateCollision*/ false);

    // ---- 6) 应用材质 ----
    if (WaterMaterial)
    {
        WaterMeshComp->SetMaterial(0, WaterMaterial);
    }
    else
    {
        UE_LOG(LogPlanetTopologyDebugMesh, Warning,
            TEXT("[PlanetTopologyDebugMesh] WaterMaterial slot is empty; water mesh will render with default checker. ")
            TEXT("Assign M_WaterShell in Details > PlanetTopology|R8 > WaterMaterial."));
    }

    WaterMeshComp->SetVisibility(true);

    UE_LOG(LogPlanetTopologyDebugMesh, Log,
        TEXT("[PlanetTopologyDebugMesh] Rebuilt water mesh: Radius=%.1f cm  Cells=%d  Corners=%d  Verts=%d  Tris=%d  Material=%s"),
        WaterRadius, NumCells, NumCorners, Vertices.Num(), Triangles.Num() / 3,
        WaterMaterial ? *WaterMaterial->GetName() : TEXT("(none)"));
}

// =====================================================================
//  PIE 退出后材质恢复钩子（详见 Docs/AgentWorkflow.md §3.11）
//
//  问题现象：
//    Editor 中放置 APlanetTopologyDebugMesh 后 Rebuild 正常显示；进入 PIE 一切正常；
//    退出 PIE 后回到 Editor —— 主 mesh 与水面 mesh 都变成不透明的默认白材质 / 棋盘格
//    （Material Stats 仍正常）；再 Rebuild 或者再次进入 PIE 又恢复。
//
//  根因：
//    PIE 启动时 UE 把整个 Editor World 深拷贝（DuplicateWorld）成 PIE World，
//    Editor 端的 APlanetTopologyDebugMesh 实例 A_editor 与其 MID_editor 都被
//    duplicate 出 A_pie / MID_pie。MeshComp / WaterMeshComp 的 SceneProxy 在 PIE
//    期间持有的是 PIE 端的 MID_pie 引用——PIE 退出时 PIE World 整体 Cleanup，MID_pie
//    被 GC 后，Editor 端 MeshComp 的 RenderProxy 再次刷新时拿不到合法 material 引用，
//    fallback 到 UEngine::DefaultMaterial（白色不透明）。
//
//  修复：
//    监听 FWorldDelegates::OnPostWorldCleanup —— 在任何 UWorld 被 cleanup 后触发。
//    若 cleanup 的 World 不是 *本 Actor 所在 World*（即被 cleanup 的是 PIE World，
//    而本 Actor 还活在 Editor World），就调一次 Rebuild() 把 MID 重建并 SetMaterial。
//
//    跳过 *本 Actor 所在 World* 被 cleanup 的情况——那时本 Actor 即将被销毁，
//    Rebuild 会触发悬挂访问（GetWorld() 返回 null 或半释放状态）。
//
//    仅 Editor 构建路径有效（Standalone 不会发生跨 World duplicate）。
// =====================================================================
void APlanetTopologyDebugMesh::OnPostWorldCleanup_(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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
    //   —— 切关卡 / 编辑器关闭等场景为 false，不需要也不应该 Rebuild
    if (!bSessionEnded)
    {
        return;
    }

    // 防御 4：仅当本 Actor 在 Editor World 中时才需要恢复（PIE 退出 → Editor 端材质失效）。
    //   如果本 Actor 自己就在 PIE/Game World 里，不会发生跨 World 引用失效问题。
    if (MyWorld->WorldType != EWorldType::Editor && MyWorld->WorldType != EWorldType::EditorPreview)
    {
        return;
    }

    UE_LOG(LogPlanetTopologyDebugMesh, Log,
        TEXT("[PlanetTopologyDebugMesh] PIE world cleaned up; rebuilding to restore Editor MID + materials. "
             "(MyWorld=%s, CleanedWorld=%s, bSessionEnded=%d, bCleanupResources=%d)"),
        *MyWorld->GetName(),
        World ? *World->GetName() : TEXT("(null)"),
        bSessionEnded ? 1 : 0, bCleanupResources ? 1 : 0);

    Rebuild();
#endif
}