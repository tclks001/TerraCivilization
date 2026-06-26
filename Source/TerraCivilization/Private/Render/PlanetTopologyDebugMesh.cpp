// Fill out your copyright notice in the Description page of Project Settings.

#include "Render/PlanetTopologyDebugMesh.h"

#include "Engine/Texture2D.h"
#include "FCorner.h"
#include "FSphereTopology.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PixelFormat.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/BulkData.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlanetTopologyDebugMesh, Log, All);

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
}

APlanetTopologyDebugMesh::~APlanetTopologyDebugMesh() = default;
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

        // 法线：
        //   - 光滑：每顶点法线 = 该 Cell 的 UnitCenter（球面外法向）
        //   - 平面：整个三角形使用面法线（叉乘后归一化），3 个顶点用同一个值
        FVector NA, NB, NC;
        if (bSmoothNormals)
        {
            NA = Topology->Cells[CA].UnitCenter;
            NB = Topology->Cells[CB].UnitCenter;
            NC = Topology->Cells[CC].UnitCenter;
        }
        else
        {
            const FVector Face = FVector::CrossProduct(PB - PA, PC - PA).GetSafeNormal();
            NA = NB = NC = Face;
        }

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

        Triangles.Add(BaseIdx + 0);
        Triangles.Add(BaseIdx + 1);
        Triangles.Add(BaseIdx + 2);
    }

    // 3) 提交给 PMC。R2 起使用 4 通道 UV 的完整重载。R1 的不创建碰撞约束保持不变。
    //    PMC SceneProxy 内部固定按 4 个 UV 通道初始化（InitFromDynamicVertex 第三参 = 4），
    //    所以 GPU 端材质可直接通过 TexCoord[1]/[2]/[3] 节点读到我们写入的值。
    MeshComp->ClearAllMeshSections();
    MeshComp->CreateMeshSection_LinearColor(
        /*SectionIndex=*/0,
        Vertices, Triangles, Normals,
        UV0, UV1, UV2, UV3,
        VertexColors, Tangents,
        /*bCreateCollision=*/false);

    // 4) R3：构建 1×NumCells 的 CellAttrLUT，把每 Cell 的 LayerIndex 写入 R 通道。
    RebuildCellAttrLUT_(NumCells);

    // 4.5) R4：构建 1×NumCells 的 CellDirLUT，把每 Cell 的 UnitCenter 写入 RGB 通道。
    //      PS 端用于球面 Voronoi 判别 argmax(dot(dir, V_i))，等位线 = 测地线大圆弧。
    RebuildCellDirLUT_(NumCells);

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
        // R6 期望材质里至少有一个 Custom 节点同时拥有以下 11 个 Inputs：
        //   UV0, UV1, UV2, UV3, WorldPos, PlanetCenter, CellAttrLUT, CellDirLUT, EdgeWidth, NoiseAmplitude, NoiseScale
        // 缺失任意一项 → 打印 Error 级别日志（说明用户挂了 R3/R4/R5 旧材质，或新材质没接全）
#if WITH_EDITORONLY_DATA
        if (UMaterial* BaseMat = Material->GetMaterial())
        {
            const TConstArrayView<TObjectPtr<UMaterialExpression>> Exprs = BaseMat->GetExpressions();
            int32 CustomFound = 0;
            // R6 期望 Inputs 集合（小写比较，容错命名风格）
            const TArray<FString> ExpectedR6Inputs = {
                TEXT("uv0"), TEXT("uv1"), TEXT("uv2"), TEXT("uv3"),
                TEXT("worldpos"), TEXT("planetcenter"),
                TEXT("cellattrlut"), TEXT("celldirlut"),
                TEXT("edgewidth"),
                TEXT("noiseamplitude"), TEXT("noisescale")
            };
            bool bAnyR6Compliant = false;

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

                    // ---- R6 合规性检查 ----
                    TArray<FString> Missing;
                    TArray<FString> Disconnected;
                    for (const FString& Need : ExpectedR6Inputs)
                    {
                        if (!PresentLower.Contains(Need))         { Missing.Add(Need); }
                        else if (!ConnectedLower.Contains(Need))  { Disconnected.Add(Need); }
                    }

                    if (Missing.Num() == 0 && Disconnected.Num() == 0)
                    {
                        bAnyR6Compliant = true;
                        UE_LOG(LogPlanetTopologyDebugMesh, Warning,
                            TEXT("    ✓ R6 compliance: ALL %d expected inputs present & connected"),
                            ExpectedR6Inputs.Num());
                    }
                    else
                    {
                        if (Missing.Num() > 0)
                        {
                            UE_LOG(LogPlanetTopologyDebugMesh, Error,
                                TEXT("    ✗ R6 missing inputs: [%s] — this is likely an R2/R3/R4/R5 material, not R6"),
                                *FString::Join(Missing, TEXT(", ")));
                        }
                        if (Disconnected.Num() > 0)
                        {
                            UE_LOG(LogPlanetTopologyDebugMesh, Error,
                                TEXT("    ✗ R6 inputs declared but NOT connected: [%s]"),
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
            else if (!bAnyR6Compliant)
            {
                UE_LOG(LogPlanetTopologyDebugMesh, Error,
                    TEXT("[PlanetTopologyDebugMesh] Material '%s' has %d Custom nodes but NONE is R6-compliant. ")
                    TEXT("Expected one Custom with inputs: UV0,UV1,UV2,UV3,WorldPos,PlanetCenter,CellAttrLUT,CellDirLUT,EdgeWidth,NoiseAmplitude,NoiseScale. ")
                    TEXT("See R6_BoundaryNoise.md §4 for the exact wiring."),
                    *BaseMat->GetName(), CustomFound);
            }
            else
            {
                UE_LOG(LogPlanetTopologyDebugMesh, Log,
                    TEXT("[PlanetTopologyDebugMesh] Material '%s' is R6-compliant ✓"),
                    *BaseMat->GetName());
            }
        }
#endif
    }

    UE_LOG(LogPlanetTopologyDebugMesh, Log,
        TEXT("[PlanetTopologyDebugMesh] Rebuilt (R6: per-cell noise on δ + soft edge). ")
        TEXT("SubdivisionLevel=%d  Cells=%d  Corners=%d  Verts=%d  Tris=%d  Radius=%.1f  Smooth=%s  ")
        TEXT("CellAttrLUT=%s  CellDirLUT=%s  PlanetCenter=(%.1f,%.1f,%.1f)  ")
        TEXT("EdgeWidth=%.4f rad (%.2f°)  NoiseAmplitude=%.4f rad (%.2f°)  NoiseScale=%.1f /rad  ")
        TEXT("NumLayersHint=%d"),
        SubdivisionLevel, NumCells, NumCorners,
        Vertices.Num(), Triangles.Num() / 3, Radius,
        bSmoothNormals ? TEXT("true") : TEXT("false"),
        CellAttrLUT ? TEXT("OK") : TEXT("MISSING"),
        CellDirLUT  ? TEXT("OK") : TEXT("MISSING"),
        GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z,
        EdgeWidth,      EdgeWidth      * 180.0 / PI,
        NoiseAmplitude, NoiseAmplitude * 180.0 / PI,
        NoiseScale,
        NumLayersHint);
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

    for (int32 CellId = 0; CellId < NumCells; ++CellId)
    {
        const uint32 Hashed = (uint32)CellId * KnuthHash;
        const uint8  Layer  = (uint8)(Hashed % (uint32)LayerMod);

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
    {
        const uint8* Verify = static_cast<const uint8*>(Bulk.LockReadOnly());
        if (Verify)
        {
            const int32 NumDump = FMath::Min(NumCells, 16);
            FString Dump;
            for (int32 i = 0; i < NumDump; ++i)
            {
                const uint32 Hashed   = (uint32)i * KnuthHash;
                const uint8  Expected = (uint8)(Hashed % (uint32)LayerMod);
                Dump += FString::Printf(TEXT("  Cell%-3d: B=%3u G=%3u R=%3u A=%3u (expected layer=%u)\n"),
                    i,
                    Verify[i * 4 + 0],
                    Verify[i * 4 + 1],
                    Verify[i * 4 + 2],
                    Verify[i * 4 + 3],
                    Expected);
            }
            UE_LOG(LogPlanetTopologyDebugMesh, Log,
                TEXT("[PlanetTopologyDebugMesh] CellAttrLUT first %d cells (NumCells=%d, NumLayersHint=%d, LayerMod=%d):\n%s"),
                NumDump, NumCells, NumLayersHint, LayerMod, *Dump);
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