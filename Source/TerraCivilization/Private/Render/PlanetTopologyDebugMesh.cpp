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
    const int32 NumVerts = NumCorners * 3;

    TArray<FVector>          Vertices;
    TArray<int32>            Triangles;
    TArray<FVector>          Normals;
    TArray<FVector2D>        UV0;
    TArray<FLinearColor>     VertexColors;
    TArray<FProcMeshTangent> Tangents;   // 留空：纯白材质不需要切线空间

    Vertices.Reserve(NumVerts);
    Triangles.Reserve(NumVerts);
    Normals.Reserve(NumVerts);
    UV0.Reserve(NumVerts);
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

        // R1 阶段 UV 用不到（纯白材质），统一塞 (0,0)；后续 R2 可以把它替换成
        //   onehot 的"我属于哪个 Cell"或者重心权重等 per-vertex 属性。
        const FVector2D ZeroUV(0.0f, 0.0f);

        // R1 顶点色统一白；R3+ 可写入 LayerIndex 等数据。
        const FLinearColor WhiteColor(1.0f, 1.0f, 1.0f, 1.0f);

        const int32 BaseIdx = Vertices.Num();   // 当前三角形第一个顶点的索引

        Vertices.Add(PA);   Normals.Add(NA);   UV0.Add(ZeroUV);   VertexColors.Add(WhiteColor);
        Vertices.Add(PB);   Normals.Add(NB);   UV0.Add(ZeroUV);   VertexColors.Add(WhiteColor);
        Vertices.Add(PC);   Normals.Add(NC);   UV0.Add(ZeroUV);   VertexColors.Add(WhiteColor);

        Triangles.Add(BaseIdx + 0);
        Triangles.Add(BaseIdx + 1);
        Triangles.Add(BaseIdx + 2);
    }

    // 3) 提交给 PMC。R1 不创建碰撞（Actor 仅为可视化）。
    MeshComp->ClearAllMeshSections();
    MeshComp->CreateMeshSection_LinearColor(
        /*SectionIndex=*/0,
        Vertices, Triangles, Normals, UV0,
        VertexColors, Tangents,
        /*bCreateCollision=*/false);

    // 4) 材质：用户指定优先；否则交给 PMC 默认（=引擎 DefaultMaterial，纯白漫反射）。
    if (Material)
    {
        MeshComp->SetMaterial(0, Material);
    }

    UE_LOG(LogPlanetTopologyDebugMesh, Log,
        TEXT("[PlanetTopologyDebugMesh] Rebuilt. SubdivisionLevel=%d  Cells=%d  Corners=%d  Verts=%d  Tris=%d  Radius=%.1f  Smooth=%s"),
        SubdivisionLevel, NumCells, NumCorners,
        Vertices.Num(), Triangles.Num() / 3, Radius,
        bSmoothNormals ? TEXT("true") : TEXT("false"));
}
