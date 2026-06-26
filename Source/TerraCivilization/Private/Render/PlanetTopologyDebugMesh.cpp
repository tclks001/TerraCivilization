// Fill out your copyright notice in the Description page of Project Settings.

#include "Render/PlanetTopologyDebugMesh.h"

#include "Engine/Texture2D.h"
#include "FCorner.h"
#include "FSphereTopology.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

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
    //  顶点属性布局（R2 验收必须）：
    //      Position, Normal     — 几何
    //      VertexColor          — 该顶点对应 Cell 的 hash 色（备用：argmax 硬边路径需要此色作 PS 输出源）
    //      UV0  = (0, 0)        — 预留给后续细节纹理 fallback
    //      UV1.xy = (c0, c1)    — 当前三角形 3 个 Cell 中第 0、第 1 个 CellId
    //                             （float 存 int，<= 16M 精确；三个顶点写同样值，PS 用 round 还原）
    //      UV2.xy = (c2, _ )    — 第 2 个 CellId；y 通道空着
    //      UV3.xy = (OneHot.x, OneHot.y)
    //                           — Role 0 顶点写 (1,0)、Role 1 写 (0,1)、Role 2 写 (0,0)
    //                           — 经过光栅化器线性插值后，PS 阶段：
    //                               λ₀ = UV3.x，λ₁ = UV3.y，λ₂ = saturate(1 − λ₀ − λ₁)
    //                               即"硬件免费的重心坐标 / 三方权重"
    //      VertexColor          — 备用：写入了每顶点对应 Cell 的 hash 色，
    //                             方便 fallback 路径（VertexColor → Emissive）也能看到结构
    //
    //  关键技巧：**三角形的 3 个顶点都写入相同的 (c0, c1, c2)**——
    //  经过插值后 UV1/UV2 仍是常量（三个顶点同值，插值结果不变），PS 用 round() 还原 int CellId。
    //  只有 UV3 的 OneHot 与 VertexColor 在三个顶点之间不同。
    //
    //  ▸ R2 验收材质：M_TopologyDebug_R2 用 Custom HLSL 节点实现 argmax 硬边：
    //      // 还原 3 个 CellId
    //      int c0 = (int)(UV1.x + 0.5), c1 = (int)(UV1.y + 0.5), c2 = (int)(UV2.x + 0.5);
    //      // 还原重心坐标
    //      float l0 = saturate(UV3.x), l1 = saturate(UV3.y), l2 = saturate(1 - l0 - l1);
    //      // argmax 选 CellId
    //      int chosen = (l0 >= l1 && l0 >= l2) ? c0 : ((l1 >= l2) ? c1 : c2);
    //      // hash 色
    //      float hue = frac((chosen + 1) * 0.6180339887);
    //      return saturate(float3(frac(hue), frac(hue*7.123), frac(hue*13.456)) + 0.25);
    //  连到 EmissiveColor，Shading Model = Unlit。详见 R2_TopologyDebugMaterial.md。
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
    // R2 视觉验收：为每个 Cell 预算一个 hash 色。
    // 算法：用 Knuth 黄金分割整数哈希 + 三种素数倍频拆 RGB，
    //       再加 0.25 偏置避免接近黑色，最后 saturate 在 [0,1]。
    // 这样相邻 CellId 之间的色差足够大，肉眼能立刻分辨每个 hex/pent。
    // ------------------------------------------------------------------
    TArray<FLinearColor> CellHashColor;
    CellHashColor.SetNumUninitialized(NumCells);
    for (int32 Cid = 0; Cid < NumCells; ++Cid)
    {
        // (Cid + 1) 避免 sin(0)=0 → 黑色伪影
        const float    Hue  = FMath::Frac((Cid + 1) * 0.6180339887498949f); // 黄金分割，均匀分布
        const float    HueR = FMath::Frac(Hue * 1.0f);
        const float    HueG = FMath::Frac(Hue * 7.123f);
        const float    HueB = FMath::Frac(Hue * 13.456f);
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

        // R2 共享给三角形所有顶点的属性：3 个 CellId（顺序固定为 Cor.CellIds 自身，与 Role 一一对应）
        const FVector2D TriCellId01((float)CA, (float)CB);   // 写入 UV1
        const FVector2D TriCellId2_((float)CC, 0.0f);        // 写入 UV2

        // R2 顶点 onehot —— 三角形的三个角色 0/1/2（仅 R3 阶段会用，R2 验收无需读）
        //   Role 0 顶点：OneHot = (1, 0) → λ₀ = 1, λ₁ = 0, λ₂ = 0
        //   Role 1 顶点：OneHot = (0, 1) → λ₀ = 0, λ₁ = 1, λ₂ = 0
        //   Role 2 顶点：OneHot = (0, 0) → λ₀ = 0, λ₁ = 0, λ₂ = 1 (= 1 − 0 − 0)
        const FVector2D OneHotA(1.0f, 0.0f);
        const FVector2D OneHotB(0.0f, 1.0f);
        const FVector2D OneHotC(0.0f, 0.0f);

        // ★ R2 视觉验收的核心：每个顶点的 VertexColor = 它对应的那个 Cell 的 hash 色。
        //   同一 Cell 在它周围 5~6 个三角形里出现时 VertexColor 完全相同
        //   → hex/pent 内部 5~6 个 1/3 角块全部被同一颜色填满 → 视觉上是一个完整均匀色块。
        const FLinearColor ColA = CellHashColor[CA];
        const FLinearColor ColB = CellHashColor[CB];
        const FLinearColor ColC = CellHashColor[CC];

        // R2 阶段 UV0 仍然预留（细节纹理 fallback 暂不使用）。
        const FVector2D    ZeroUV(0.0f, 0.0f);

        const int32 BaseIdx = Vertices.Num();   // 当前三角形第一个顶点的索引

        // Role 0 顶点（位置 = Cell A 中心，OneHot = (1,0)，VertexColor = hash(A)）
        Vertices.Add(PA);  Normals.Add(NA);  UV0.Add(ZeroUV);
        UV1.Add(TriCellId01);  UV2.Add(TriCellId2_);  UV3.Add(OneHotA);
        VertexColors.Add(ColA);

        // Role 1 顶点（位置 = Cell B 中心，OneHot = (0,1)，VertexColor = hash(B)）
        Vertices.Add(PB);  Normals.Add(NB);  UV0.Add(ZeroUV);
        UV1.Add(TriCellId01);  UV2.Add(TriCellId2_);  UV3.Add(OneHotB);
        VertexColors.Add(ColB);

        // Role 2 顶点（位置 = Cell C 中心，OneHot = (0,0)，VertexColor = hash(C)）
        Vertices.Add(PC);  Normals.Add(NC);  UV0.Add(ZeroUV);
        UV1.Add(TriCellId01);  UV2.Add(TriCellId2_);  UV3.Add(OneHotC);
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

    // 4) 材质。R2 验收使用最朴素的 Unlit 材质：直接把 VertexColor 节点接到 EmissiveColor 即可。
    //    若用户没指定材质，PMC 退回默认白材质——此时也能看到几何，只是看不到 hash 色。
    //    R3 阶段会重新激活 RebuildCellAttrLUT_ + MID 路径以让 PS 能 Load(LUT) 取 LayerIndex；
    //    本次 R2 之后这两段代码处于待命状态。
    if (Material)
    {
        MeshComp->SetMaterial(0, Material);
    }

    // R3 字段保持空，避免遗留 LUT/MID 影响 R2 验收。
    CellAttrLUT = nullptr;
    MID = nullptr;

    UE_LOG(LogPlanetTopologyDebugMesh, Log,
        TEXT("[PlanetTopologyDebugMesh] Rebuilt (R2: per-vertex CellIds + OneHot in UV1/UV2/UV3; ")
        TEXT("VertexColor=hash(CellId) per vertex; PS-side argmax hard-edge expected; no LUT). ")
        TEXT("SubdivisionLevel=%d  Cells=%d  Corners=%d  Verts=%d  Tris=%d  Radius=%.1f  Smooth=%s"),
        SubdivisionLevel, NumCells, NumCorners,
        Vertices.Num(), Triangles.Num() / 3, Radius,
        bSmoothNormals ? TEXT("true") : TEXT("false"));
}

// =====================================================================
//  R3：CellAttrLUT 构建 / 填充（R2 阶段不调用，待 R3 重做时再启用）
//
//  保留实现是因为 .h 里已经声明了 CellAttrLUT/MID UPROPERTY，
//  R3 重做时只需在 Rebuild() 末尾恢复对此函数的调用、再加上 MID 包装即可。
// =====================================================================
void APlanetTopologyDebugMesh::RebuildCellAttrLUT_(int32 NumCells)
{
    // 显式空实现：R2 阶段保持 LUT 为空，避免 R3 placeholder 数据混入 R2 验收。
    (void)NumCells;
    CellAttrLUT = nullptr;
}