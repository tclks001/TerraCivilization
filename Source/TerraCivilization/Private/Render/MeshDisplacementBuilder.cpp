// Fill out your copyright notice in the Description page of Project Settings.

#include "Render/MeshDisplacementBuilder.h"
#include "FSphereTopology.h"
#include "FCell.h"

#include "Logging/LogMacros.h"
#include "Math/RandomStream.h"

DEFINE_LOG_CATEGORY_STATIC(LogMeshDisp, Log, All);

FMeshDisplacementBuilder::FMeshDisplacementBuilder(const FSphereTopology* InCellTopo,
                                                   const FSphereTopology* InMeshTopo)
    : CellTopo(InCellTopo)
    , MeshTopo(InMeshTopo)
{
    if (CellTopo && MeshTopo)
    {
        checkf(MeshTopo->SubdivisionLevel >= CellTopo->SubdivisionLevel,
               TEXT("FMeshDisplacementBuilder: MeshSub(%d) must be >= CellSub(%d) (TessellatedMeshDesign.md §3.3)"),
               MeshTopo->SubdivisionLevel, CellTopo->SubdivisionLevel);
    }
}

FMeshDisplacementBuilder::~FMeshDisplacementBuilder() = default;

//==============================================================================
// 私有 LUT：CellTopology 三元组 → 粗 PrimalTri 索引
//==============================================================================

uint64 FMeshDisplacementBuilder::PackTripleKey_(int32 A, int32 B, int32 C)
{
    // sub=5 时 NumCells=10242，索引 < 2^14；用 21 位/分量已经留足余量。
    int32 Sorted[3] = { A, B, C };
    if (Sorted[0] > Sorted[1]) Swap(Sorted[0], Sorted[1]);
    if (Sorted[1] > Sorted[2]) Swap(Sorted[1], Sorted[2]);
    if (Sorted[0] > Sorted[1]) Swap(Sorted[0], Sorted[1]);

    return  (static_cast<uint64>(static_cast<uint32>(Sorted[0])) & 0x1FFFFFu)
         | ((static_cast<uint64>(static_cast<uint32>(Sorted[1])) & 0x1FFFFFu) << 21)
         | ((static_cast<uint64>(static_cast<uint32>(Sorted[2])) & 0x1FFFFFu) << 42);
}

void FMeshDisplacementBuilder::RebuildCoarseTriLUT_()
{
    CoarseTriIdByCellTriple.Reset();
    if (!CellTopo) return;

    const int32 NumTris = CellTopo->PrimalTris.Num();
    CoarseTriIdByCellTriple.Reserve(NumTris);

    for (int32 T = 0; T < NumTris; ++T)
    {
        const FIntVector& Tri = CellTopo->PrimalTris[T];
        const uint64 Key = PackTripleKey_(Tri.X, Tri.Y, Tri.Z);
        // 同三角形不应重复出现；以 Add（而非 Emplace）保留首次为主。
        CoarseTriIdByCellTriple.Add(Key, T);
    }
}

int32 FMeshDisplacementBuilder::FindCoarseTriIdByCells_(int32 A, int32 B, int32 C) const
{
    const uint64 Key = PackTripleKey_(A, B, C);
    if (const int32* Found = CoarseTriIdByCellTriple.Find(Key))
    {
        return *Found;
    }
    return INDEX_NONE;
}

//==============================================================================
// T2 主路径
//==============================================================================

void FMeshDisplacementBuilder::BuildVertexToCoarseTris()
{
    VertexToCoarseTriIds.Reset();

    if (!CellTopo || !MeshTopo)
    {
        UE_LOG(LogMeshDisp, Warning, TEXT("BuildVertexToCoarseTris: topo not bound, skip."));
        return;
    }

    // §3.4：先建 LUT；查询 O(1) 平均（TMap 哈希）。
    RebuildCoarseTriLUT_();

    const int32 NumMeshVerts = MeshTopo->PrimalVertsUnit.Num();
    VertexToCoarseTriIds.SetNum(NumMeshVerts);

    const int32 ClimbSteps = MeshTopo->SubdivisionLevel - CellTopo->SubdivisionLevel;
    checkf(ClimbSteps >= 0,
           TEXT("BuildVertexToCoarseTris: ClimbSteps(%d) must be >= 0; MeshSub=%d CellSub=%d"),
           ClimbSteps, MeshTopo->SubdivisionLevel, CellTopo->SubdivisionLevel);

    // §3.4 主循环：mesh 顶点 v 在 dual 层是 MeshTopo->Cells[v]
    // —— FCell.CornerIds 是该顶点周围的 5/6 个 Corner（INDEX_NONE 表示五边形第 6 位）。
    const int32 NumMeshCells = MeshTopo->Cells.Num();
    checkf(NumMeshCells == NumMeshVerts,
           TEXT("BuildVertexToCoarseTris: MeshTopo Cells(%d) != PrimalVerts(%d); dual/primal misalignment"),
           NumMeshCells, NumMeshVerts);

    int32 OrphanVertexCount = 0;

    for (int32 V = 0; V < NumMeshVerts; ++V)
    {
        TArray<int32>& OutTriIds = VertexToCoarseTriIds[V];
        OutTriIds.Reset();

        const FCell& MeshCell = MeshTopo->Cells[V];

        // §3.2：5 (五边形 Cell) 或 6 (六边形 Cell) 个 Corner
        for (int32 K = 0; K < MeshCell.CornerIds.Num(); ++K)
        {
            const int32 CornerId = MeshCell.CornerIds[K];
            if (CornerId == INDEX_NONE) continue; // 五边形第 6 位
            if (!MeshTopo->PrimalTriTreeNodes.IsValidIndex(CornerId)) continue;

            FTriTreeNode* Node = MeshTopo->PrimalTriTreeNodes[CornerId];
            if (Node == nullptr) continue;

            // §3.3：向上爬 ClimbSteps 次到达 CellTopology 层级。
            for (int32 Step = 0; Step < ClimbSteps && Node && Node->Father; ++Step)
            {
                Node = Node->Father;
            }
            if (Node == nullptr) continue;

            // 同构性约束（§3.3）：Node->CellIds 直接是 CellTopology cell 索引。
            const int32 CoarseTriId = FindCoarseTriIdByCells_(
                Node->CellIds[0], Node->CellIds[1], Node->CellIds[2]);
            if (CoarseTriId != INDEX_NONE)
            {
                OutTriIds.AddUnique(CoarseTriId);
            }
        }

        if (OutTriIds.Num() == 0)
        {
            ++OrphanVertexCount;
        }
    }

    UE_LOG(LogMeshDisp, Log,
           TEXT("[T2] BuildVertexToCoarseTris done: %d mesh verts, ClimbSteps=%d, LUT=%d coarse Tris, orphan=%d"),
           NumMeshVerts, ClimbSteps, CoarseTriIdByCellTriple.Num(), OrphanVertexCount);

    if (OrphanVertexCount > 0)
    {
        UE_LOG(LogMeshDisp, Warning,
               TEXT("[T2] %d mesh verts have empty coarse-tri lists (expect 0). Likely sub-isomorphism break."),
               OrphanVertexCount);
    }
}

void FMeshDisplacementBuilder::ComputeVertexElevationCM(
    const TArray<float>& CellElevation,
    float ElevationScaleCM,
    TArray<float>& OutVertexElevationCM) const
{
    OutVertexElevationCM.Reset();

    if (!CellTopo || !MeshTopo)
    {
        return;
    }

    const int32 NumMeshVerts = MeshTopo->PrimalVertsUnit.Num();
    OutVertexElevationCM.SetNumZeroed(NumMeshVerts);

    const int32 NumCells = CellTopo->Cells.Num();
    if (CellElevation.Num() != NumCells)
    {
        UE_LOG(LogMeshDisp, Warning,
               TEXT("ComputeVertexElevationCM: CellElevation.Num()=%d != CellTopo->Cells.Num()=%d. Output stays 0."),
               CellElevation.Num(), NumCells);
        return;
    }

    if (VertexToCoarseTriIds.Num() != NumMeshVerts)
    {
        UE_LOG(LogMeshDisp, Warning,
               TEXT("ComputeVertexElevationCM: VertexToCoarseTriIds not built. Output stays 0."));
        return;
    }

    for (int32 V = 0; V < NumMeshVerts; ++V)
    {
        const FVector Dir = MeshTopo->PrimalVertsUnit[V];
        const TArray<int32>& TriIds = VertexToCoarseTriIds[V];

        if (TriIds.Num() == 0)
        {
            OutVertexElevationCM[V] = 0.f;
            continue;
        }

        float SumH = 0.f;
        for (int32 TriId : TriIds)
        {
            const FIntVector& Tri = CellTopo->PrimalTris[TriId];
            const int32 c0 = Tri.X;
            const int32 c1 = Tri.Y;
            const int32 c2 = Tri.Z;

            // D3：三点 dot 权重的线性插值（max(0, dot) 后归一化）
            float w0 = FMath::Max(0.f, FVector::DotProduct(Dir, CellTopo->Cells[c0].UnitCenter));
            float w1 = FMath::Max(0.f, FVector::DotProduct(Dir, CellTopo->Cells[c1].UnitCenter));
            float w2 = FMath::Max(0.f, FVector::DotProduct(Dir, CellTopo->Cells[c2].UnitCenter));
            const float WSum = w0 + w1 + w2;
            if (WSum > KINDA_SMALL_NUMBER)
            {
                const float InvW = 1.f / WSum;
                w0 *= InvW; w1 *= InvW; w2 *= InvW;
            }
            else
            {
                // 异常：Dir 与三 Cell 中心都几乎正交（不应该发生在一致同构网格内）；权重均分。
                w0 = w1 = w2 = 1.f / 3.f;
            }

            const float HContrib =
                w0 * CellElevation[c0] +
                w1 * CellElevation[c1] +
                w2 * CellElevation[c2];
            SumH += HContrib;
        }

        // D4：算术平均
        const float AvgH = SumH / static_cast<float>(TriIds.Num());
        OutVertexElevationCM[V] = AvgH * ElevationScaleCM;
    }

    // D9：fbm 微细节占位（T 阶段主验收不接入）
    // ComputeMicroDetail(OutVertexElevationCM); // 当前为空体，不主动调用以省一次遍历
}

//==============================================================================
// 验收自检（设计稿 §5.1）
//==============================================================================

bool FMeshDisplacementBuilder::RunSelfCheckT2(int32 RandomSampleCount, int32 RandomSeed) const
{
    if (!CellTopo || !MeshTopo)
    {
        UE_LOG(LogMeshDisp, Warning, TEXT("[T2-Check] topo not bound."));
        return false;
    }

    const int32 NumMeshVerts = MeshTopo->PrimalVertsUnit.Num();
    if (VertexToCoarseTriIds.Num() != NumMeshVerts)
    {
        UE_LOG(LogMeshDisp, Warning, TEXT("[T2-Check] VertexToCoarseTriIds not built."));
        return false;
    }

    // (1) 全量扫描：VertexToCoarseTriIds[v].Num() ∈ [1, 6]
    int32 MinCount = INT32_MAX;
    int32 MaxCount = 0;
    int32 BadVerts = 0;
    for (int32 V = 0; V < NumMeshVerts; ++V)
    {
        const int32 N = VertexToCoarseTriIds[V].Num();
        MinCount = FMath::Min(MinCount, N);
        MaxCount = FMath::Max(MaxCount, N);
        if (N < 1 || N > 6)
        {
            ++BadVerts;
        }
    }

    // (2) 随机采样 RandomSampleCount 个 mesh 顶点 × 首个粗 Tri，
    //     检查 dot 权重归一化后的和 ∈ [0.99, 1.01]。
    FRandomStream Rng(RandomSeed);
    int32 WeightSamples = 0;
    int32 WeightFails = 0;
    float WorstSum = 1.f;
    const int32 SampleN = FMath::Min(RandomSampleCount, NumMeshVerts);
    for (int32 S = 0; S < SampleN; ++S)
    {
        const int32 V = Rng.RandRange(0, NumMeshVerts - 1);
        const TArray<int32>& TriIds = VertexToCoarseTriIds[V];
        if (TriIds.Num() == 0) continue;

        const int32 TriId = TriIds[0];
        const FIntVector& Tri = CellTopo->PrimalTris[TriId];
        const FVector Dir = MeshTopo->PrimalVertsUnit[V];

        float w0 = FMath::Max(0.f, FVector::DotProduct(Dir, CellTopo->Cells[Tri.X].UnitCenter));
        float w1 = FMath::Max(0.f, FVector::DotProduct(Dir, CellTopo->Cells[Tri.Y].UnitCenter));
        float w2 = FMath::Max(0.f, FVector::DotProduct(Dir, CellTopo->Cells[Tri.Z].UnitCenter));
        const float WSum = w0 + w1 + w2;
        if (WSum <= KINDA_SMALL_NUMBER) continue;

        const float Norm = (w0 + w1 + w2) / WSum; // 归一化后的和（理论 == 1）
        ++WeightSamples;

        if (Norm < 0.99f || Norm > 1.01f)
        {
            ++WeightFails;
        }
        if (FMath::Abs(Norm - 1.f) > FMath::Abs(WorstSum - 1.f))
        {
            WorstSum = Norm;
        }
    }

    const bool bCountOK = (BadVerts == 0) && (MinCount >= 1) && (MaxCount <= 6);
    const bool bWeightOK = (WeightFails == 0);
    const bool bAllOK = bCountOK && bWeightOK;

    UE_LOG(LogMeshDisp, Log,
           TEXT("[T2-Check] verts=%d, TriIdsPerVert range=[%d,%d], badCount=%d | weight samples=%d, fails=%d, worstNormSum=%.6f | %s"),
           NumMeshVerts, MinCount, MaxCount, BadVerts,
           WeightSamples, WeightFails, WorstSum,
           bAllOK ? TEXT("PASS") : TEXT("FAIL"));

    return bAllOK;
}

//==============================================================================
// 简单访问器
//==============================================================================

int32 FMeshDisplacementBuilder::GetNumMeshVerts() const
{
    return MeshTopo ? MeshTopo->PrimalVertsUnit.Num() : 0;
}

int32 FMeshDisplacementBuilder::GetNumCells() const
{
    return CellTopo ? CellTopo->Cells.Num() : 0;
}
