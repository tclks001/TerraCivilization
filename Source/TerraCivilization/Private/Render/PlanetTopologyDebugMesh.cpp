// Fill out your copyright notice in the Description page of Project Settings.

#include "Render/PlanetTopologyDebugMesh.h"

#include "FCorner.h"
#include "FSphereTopology.h"
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

    // 2) 装填顶点 / 索引缓冲：每个 Corner（三角形）展开为 3 个独立顶点。
    //    Vertices 长度 = NumCorners * 3，Triangles 是 0,1,2,3,4,5,... 的顺序索引。
    //
    //    R2 顶点属性布局（详见 SDF 设计稿 §4.3 方案 A、§14.7）：
    //      Position, Normal     — 几何
    //      UV0  = (0, 0)        — 预留给后续细节纹理 fallback
    //      UV1.xy = (c0, c1)    — 当前三角形 3 个 Cell 中第 0、第 1 个 CellId（float 存 int，<= 16M 精确）
    //      UV2.xy = (c2, _ )    — 第 2 个 CellId；y 通道空着（留作未来"五边形 mask"等用途）
    //      UV3.xy = (OneHot.x, OneHot.y)
    //                           — Role 0 顶点写 (1,0)、Role 1 写 (0,1)、Role 2 写 (0,0)
    //                           — 经过光栅化器线性插值后，PS 阶段：
    //                               λ₀ = UV3.x，λ₁ = UV3.y，λ₂ = saturate(1 − λ₀ − λ₁)
    //                               即"硬件免费的重心坐标 / 三方权重"
    //      VertexColor          — R2 暂留作纯白；R3+ 可复用为其它通道
    //
    //    关键技巧：**三角形的 3 个顶点都写入相同的 (c0, c1, c2)**——
    //    经过插值后 UV1/UV2 仍是常量（三个顶点同值，插值结果不变），PS 用 round() 还原。
    //    只有 UV3 的 OneHot 在三个顶点之间不同，插值后形成重心权重。
    const int32 NumVerts = NumCorners * 3;

    TArray<FVector>          Vertices;
    TArray<int32>            Triangles;
    TArray<FVector>          Normals;
    TArray<FVector2D>        UV0;
    TArray<FVector2D>        UV1;     // (c0, c1)
    TArray<FVector2D>        UV2;     // (c2, _ )
    TArray<FVector2D>        UV3;     // OneHot (λ₀, λ₁)
    TArray<FLinearColor>     VertexColors;
    TArray<FProcMeshTangent> Tangents;   // 留空：纯白材质不需要切线空间

    Vertices.Reserve(NumVerts);
    Triangles.Reserve(NumVerts);
    Normals.Reserve(NumVerts);
    UV0.Reserve(NumVerts);
    UV1.Reserve(NumVerts);
    UV2.Reserve(NumVerts);
    UV3.Reserve(NumVerts);
    VertexColors.Reserve(NumVerts);

    // PMC 的网格顶点是 Component-Local 空间。Actor 通过 Transform 摆放，
    // 所以这里不要加 ActorLocation——直接用 UnitCenter * Radius 即可。
    for (int32 CornerIdx = 0; CornerIdx < NumCorners; ++CornerIdx)
    {
        const FCorner& Cor = Topology->Corners[CornerIdx];

        // 取三个 Cell 中心，组成本三角形的 3 个独立顶点
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

        // R2 共享给三角形所有顶点的属性：3 个 CellId（顺序固定为 Cor.CellIds 自身）
        const FVector2D TriCellId01((float)CA, (float)CB);   // 写入 UV1
        const FVector2D TriCellId2_((float)CC, 0.0f);        // 写入 UV2

        // R2 顶点 onehot —— 三角形的三个角色 0/1/2
        //   Role 0 顶点：OneHot = (1, 0) → λ₀ = 1
        //   Role 1 顶点：OneHot = (0, 1) → λ₁ = 1
        //   Role 2 顶点：OneHot = (0, 0) → λ₂ = 1 (= 1 − 0 − 0)
        const FVector2D OneHotA(1.0f, 0.0f);
        const FVector2D OneHotB(0.0f, 1.0f);
        const FVector2D OneHotC(0.0f, 0.0f);

        // R2 阶段 UV0 仍然预留（细节纹理 fallback 暂不使用）；顶点色统一白。
        const FVector2D    ZeroUV(0.0f, 0.0f);
        const FLinearColor WhiteColor(1.0f, 1.0f, 1.0f, 1.0f);

        const int32 BaseIdx = Vertices.Num();   // 当前三角形第一个顶点的索引

        // Role 0
        Vertices.Add(PA);  Normals.Add(NA);  UV0.Add(ZeroUV);
        UV1.Add(TriCellId01);  UV2.Add(TriCellId2_);  UV3.Add(OneHotA);
        VertexColors.Add(WhiteColor);

        // Role 1
        Vertices.Add(PB);  Normals.Add(NB);  UV0.Add(ZeroUV);
        UV1.Add(TriCellId01);  UV2.Add(TriCellId2_);  UV3.Add(OneHotB);
        VertexColors.Add(WhiteColor);

        // Role 2
        Vertices.Add(PC);  Normals.Add(NC);  UV0.Add(ZeroUV);
        UV1.Add(TriCellId01);  UV2.Add(TriCellId2_);  UV3.Add(OneHotC);
        VertexColors.Add(WhiteColor);

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

    // 4) 材质：用户指定优先；否则交给 PMC 默认（=引擎 DefaultMaterial，纯白漫反射）。
    if (Material)
    {
        MeshComp->SetMaterial(0, Material);
    }

    UE_LOG(LogPlanetTopologyDebugMesh, Log,
        TEXT("[PlanetTopologyDebugMesh] Rebuilt (R2: per-vertex CellIds + OneHot in UV1/UV2/UV3). ")
        TEXT("SubdivisionLevel=%d  Cells=%d  Corners=%d  Verts=%d  Tris=%d  Radius=%.1f  Smooth=%s"),
        SubdivisionLevel, NumCells, NumCorners,
        Vertices.Num(), Triangles.Num() / 3, Radius,
        bSmoothNormals ? TEXT("true") : TEXT("false"));
}
