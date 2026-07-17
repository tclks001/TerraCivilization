#include "TerrainVisualRiverSystem.h"

#include "FSphereTopology.h"
#include "CellGeoData.h"
#include "TerrainSurfaceQuery.h"
#include "TerrainVisualTypes.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerrainVisualRivers, Log, All);

namespace
{
    FVector SmoothSpherePathPoint(const TArray<int32>& Path, int32 SegmentIndex, float T, const FSphereTopology& Topology)
    {
        const FVector P0 = Topology.Cells[Path[FMath::Max(0, SegmentIndex - 1)]].UnitCenter;
        const FVector P1 = Topology.Cells[Path[SegmentIndex]].UnitCenter;
        const FVector P2 = Topology.Cells[Path[SegmentIndex + 1]].UnitCenter;
        const FVector P3 = Topology.Cells[Path[FMath::Min(Path.Num() - 1, SegmentIndex + 2)]].UnitCenter;
        const float T2 = T * T;
        const float T3 = T2 * T;
        return (0.5f * ((2.0f * P1) + (-P0 + P2) * T + (2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * T2 + (-P0 + 3.0f * P1 - 3.0f * P2 + P3) * T3)).GetSafeNormal();
    }
}

bool FTerrainVisualRiverSystem::Build(
    const FSphereTopology& Topology,
    const TArray<FCellGeoData>& GeoCells,
    const ITerrainSurfaceQuery& SurfaceQuery,
    const FTerrainVisualConfig& Config)
{
    Reset();
    if (!Config.bEnableDecorativeRivers || Topology.Cells.IsEmpty() || GeoCells.Num() != Topology.Cells.Num() || Config.GlobeRadiusCM <= KINDA_SMALL_NUMBER)
    {
        return true;
    }

    TArray<float> Heights;
    Heights.SetNum(Topology.Cells.Num());
    TArray<int32> SortedCells;
    TArray<int32> MountainSources;
    for (int32 CellId = 0; CellId < Topology.Cells.Num(); ++CellId)
    {
        Heights[CellId] = SurfaceQuery.QueryBaseSurface(Topology.Cells[CellId].UnitCenter).SurfaceRadiusCM;
        SortedCells.Add(CellId);
        if (GeoCells[CellId].SimpleTerrainType == ETerraSimpleTerrainType::Mountain)
        {
            MountainSources.Add(CellId);
        }
    }

    // Select a few separated lowland basins. These are the only river terminals;
    // ordinary local minima are crossed by the filled drainage surface below.
    SortedCells.Sort([&Heights](int32 A, int32 B) { return Heights[A] < Heights[B]; });
    TArray<int32> Terminals;
    const int32 TerminalCount = FMath::Clamp(Config.RiverTerminalBasinCount, 1, 4);
    for (const int32 Candidate : SortedCells)
    {
        if (GeoCells[Candidate].SimpleTerrainType == ETerraSimpleTerrainType::Mountain)
        {
            continue;
        }
        bool bSeparated = true;
        for (const int32 Existing : Terminals)
        {
            const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Topology.Cells[Candidate].UnitCenter, Topology.Cells[Existing].UnitCenter), -1.0f, 1.0f));
            if (Angle < 0.35f)
            {
                bSeparated = false;
                break;
            }
        }
        if (bSeparated)
        {
            Terminals.Add(Candidate);
            if (Terminals.Num() >= TerminalCount) break;
        }
    }

    // Priority-Flood on the Cell graph. Parent always points toward a selected
    // terminal, so rivers continue through local visual depressions and merge.
    TArray<float> FilledHeight;
    FilledHeight.Init(UE_BIG_NUMBER, Topology.Cells.Num());
    TArray<int32> Parent;
    Parent.Init(INDEX_NONE, Topology.Cells.Num());
    TArray<bool> Closed;
    Closed.Init(false, Topology.Cells.Num());
    for (const int32 Terminal : Terminals) FilledHeight[Terminal] = Heights[Terminal];
    for (int32 Iteration = 0; Iteration < Topology.Cells.Num(); ++Iteration)
    {
        int32 Current = INDEX_NONE;
        for (int32 CellId = 0; CellId < Topology.Cells.Num(); ++CellId)
        {
            if (!Closed[CellId] && (Current == INDEX_NONE || FilledHeight[CellId] < FilledHeight[Current])) Current = CellId;
        }
        if (Current == INDEX_NONE || FilledHeight[Current] == UE_BIG_NUMBER) break;
        Closed[Current] = true;
        for (const int32 Neighbor : Topology.Cells[Current].NeighborCellIds)
        {
            if (!Topology.Cells.IsValidIndex(Neighbor) || Closed[Neighbor]) continue;
            const float CandidateHeight = FMath::Max(Heights[Neighbor], FilledHeight[Current]);
            if (CandidateHeight < FilledHeight[Neighbor])
            {
                FilledHeight[Neighbor] = CandidateHeight;
                Parent[Neighbor] = Current;
            }
        }
    }

    MountainSources.Sort([&Heights](int32 A, int32 B) { return Heights[A] > Heights[B]; });
    TArray<TArray<int32>> Paths;
    const int32 SourceCount = FMath::Clamp(Config.RiverSourceCount, 0, 16);
    for (const int32 SourceCell : MountainSources)
    {
        if (Paths.Num() >= SourceCount || Parent[SourceCell] == INDEX_NONE)
        {
            continue;
        }
        TArray<int32> Path;
        Path.Add(SourceCell);
        while (Parent[Path.Last()] != INDEX_NONE && Path.Num() < Config.RiverMaxPathCells)
        {
            Path.Add(Parent[Path.Last()]);
        }
        const bool bReachedTerminal = Parent[Path.Last()] == INDEX_NONE && Terminals.Contains(Path.Last());
        if (bReachedTerminal && Path.Num() >= FMath::Max(Config.RiverMinPathCells, 2))
        {
            Paths.Add(MoveTemp(Path));
        }
    }

    TMap<int32, int32> Discharge;
    TMap<int32, int32> LongestUpstreamDistance;
    for (const TArray<int32>& Path : Paths)
    {
        for (int32 PathIndex = 0; PathIndex < Path.Num(); ++PathIndex)
        {
            const int32 CellId = Path[PathIndex];
            Discharge.FindOrAdd(CellId)++;
            int32& LongestDistance = LongestUpstreamDistance.FindOrAdd(CellId);
            LongestDistance = FMath::Max(LongestDistance, PathIndex);
        }
    }

    TMap<int32, float> NodeWidths;
    for (const TPair<int32, int32>& Pair : Discharge)
    {
        const float WidthCM = Config.RiverMinWidthCM
            + Config.RiverWidthScaleCM * FMath::Sqrt(static_cast<float>(Pair.Value))
            + Config.RiverLengthWidthGrowthCM * LongestUpstreamDistance.FindRef(Pair.Key);
        NodeWidths.Add(Pair.Key, WidthCM / Config.GlobeRadiusCM);
    }

    TSet<int32> LakeTerminals;
    TSet<uint64> RenderedCellEdges;
    int32 UniqueLogicalEdgeCount = 0;
    for (const TArray<int32>& Path : Paths)
    {
        for (int32 SegmentIndex = 0; SegmentIndex < Path.Num() - 1; ++SegmentIndex)
        {
            const uint64 EdgeKey = (static_cast<uint64>(Path[SegmentIndex]) << 32) | static_cast<uint32>(Path[SegmentIndex + 1]);
            if (RenderedCellEdges.Contains(EdgeKey))
            {
                continue;
            }
            RenderedCellEdges.Add(EdgeKey);
            ++UniqueLogicalEdgeCount;
            // Each DAG node owns one width. Adjacent curve edges must share that
            // value at their joint, otherwise the SDF union produces circular bulbs.
            const float WidthA = NodeWidths.FindRef(Path[SegmentIndex]);
            const float WidthB = NodeWidths.FindRef(Path[SegmentIndex + 1]);
            constexpr int32 Subdivisions = 2;
            FVector Previous = SmoothSpherePathPoint(Path, SegmentIndex, 0.0f, Topology);
            for (int32 Sample = 1; Sample <= Subdivisions; ++Sample)
            {
                const float T = static_cast<float>(Sample) / Subdivisions;
                const FVector Current = SmoothSpherePathPoint(Path, SegmentIndex, T, Topology);
                FTerrainVisualRiverSegment& Out = Segments.AddDefaulted_GetRef();
                Out.StartUnit = Previous;
                Out.EndUnit = Current;
                Out.StartWidthRad = FMath::Lerp(WidthA, WidthB, static_cast<float>(Sample - 1) / Subdivisions);
                Out.EndWidthRad = FMath::Lerp(WidthA, WidthB, T);
                Previous = Current;
            }
        }

        const int32 TerminalCell = Path.Last();
        if (Terminals.Contains(TerminalCell)
            && Discharge.FindRef(TerminalCell) >= Config.RiverTerminalLakeMinDischarge
            && !LakeTerminals.Contains(TerminalCell))
        {
            LakeTerminals.Add(TerminalCell);
            const FVector Center = Topology.Cells[TerminalCell].UnitCenter;
            const FVector Previous = Topology.Cells[Path[Path.Num() - 2]].UnitCenter;
            FVector FlowAxis = FVector::VectorPlaneProject(Center - Previous, Center).GetSafeNormal();
            if (FlowAxis.IsNearlyZero())
            {
                FlowAxis = FVector::VectorPlaneProject(FVector::ForwardVector, Center).GetSafeNormal();
            }
            const float TerminalWidth = NodeWidths.FindRef(TerminalCell);
            float CellSpacing = UE_BIG_NUMBER;
            for (const int32 NeighborId : Topology.Cells[TerminalCell].NeighborCellIds)
            {
                if (Topology.Cells.IsValidIndex(NeighborId))
                {
                    CellSpacing = FMath::Min(CellSpacing, FMath::Acos(FMath::Clamp(FVector::DotProduct(Center, Topology.Cells[NeighborId].UnitCenter), -1.0f, 1.0f)));
                }
            }
            const float MaxLakeRadius = CellSpacing == UE_BIG_NUMBER ? TerminalWidth * 3.0f : CellSpacing * Config.RiverMaxLakeRadiusFraction;
            FTerrainVisualRiverLake& Lake = TerminalLakes.AddDefaulted_GetRef();
            Lake.CenterUnit = Center;
            Lake.FlowAxisUnit = FlowAxis;
            Lake.RadiusAlongRad = FMath::Min(TerminalWidth * Config.RiverLakeLengthMultiplier, MaxLakeRadius);
            Lake.RadiusAcrossRad = FMath::Min(TerminalWidth * Config.RiverLakeWidthMultiplier, MaxLakeRadius);
        }
    }
    UE_LOG(LogTerrainVisualRivers, Log,
        TEXT("[TerrainVisual][SV5] MountainSources=%d Requested=%d AcceptedPaths=%d UniqueEdges=%d RenderSegments=%d Lakes=%d"),
        MountainSources.Num(),
        SourceCount,
        Paths.Num(),
        UniqueLogicalEdgeCount,
        Segments.Num(),
        TerminalLakes.Num());
    return true;
}

void FTerrainVisualRiverSystem::Reset()
{
    Segments.Reset();
    TerminalLakes.Reset();
}
