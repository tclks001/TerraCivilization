// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldGenerator.h"
#include "WorldGenLog.h"
#include "FSphereTopology.h"
#include "FCell.h"
#include "FCellEdge.h"

// W4 引入：TerrainTags 模块（UTerrainSet / UTerrainDefinition / FClimateSample）。
// Step_ClassifyBiomes 仅作为评分器；不依赖任何具体 Tag 名。
#include "TerrainSet.h"
#include "TerrainDefinition.h"

// W3 引入：FastNoiseLite C++ 库（namespace 内的 class FastNoiseLite）；用于 fbm 高程噪声。
// 详见 Docs/W3_ScalarFields.md §A.1 / §3.1.4。
#include "FastNoiseLite.h"

namespace
{
    /**
     * FWorldGenNoise
     *
     * W3 内部噪声包装：球面 3D fbm 采样。仅在 WorldGenerator.cpp 翻译单元内可见，
     * 不暴露到头文件，避免 WorldGen 头文件传染 FastNoiseLite。
     * 详见 Docs/W3_ScalarFields.md §A.1。
     */
    class FWorldGenNoise
    {
    public:
        FWorldGenNoise(int32 Seed, float Frequency, int32 Octaves = 4)
        {
            Noise.SetSeed(Seed);
            Noise.SetFrequency(Frequency);
            Noise.SetNoiseType(FastNoiseLite::NoiseType::NoiseType_OpenSimplex2);
            Noise.SetFractalType(FastNoiseLite::FractalType::FractalType_FBm);
            Noise.SetFractalOctaves(Octaves);
            Noise.SetFractalLacunarity(2.0f);
            Noise.SetFractalGain(0.5f);
        }
        /**
         * 球面 3D 采样：Dir 是单位向量；FastNoiseLite 内部归一化到 [-1, 1]。
         *
         * 注意：本方法**故意非 const**——FastNoiseLite::GetNoise(x,y,z) 自身是非 const 方法
         * （内部维护 fractal 累加器等可变状态），无法在 const 上下文中调用。
         */
        float Fbm(const FVector& Dir)
        {
            return Noise.GetNoise(static_cast<float>(Dir.X),
                                   static_cast<float>(Dir.Y),
                                   static_cast<float>(Dir.Z));
        }
    private:
        FastNoiseLite Noise;
    };
}

FWorldGenerator::FWorldGenerator(FSphereTopology* InTopology, const FWorldGenSettings& InSettings)
    : Topology(InTopology)
    , Settings(InSettings)
{
}

void FWorldGenerator::Generate()
{
    check(Topology);

    const int32 N = Topology->Cells.Num();
    CellData.SetNum(N);
    PlateIdField.SetNumZeroed(N);
    ElevationField.SetNumZeroed(N);
    MoistureField.SetNumZeroed(N);
    TemperatureField.SetNumZeroed(N);
    DischargeField.SetNumZeroed(N);

    // W1 唯一实际写入：CellId + bIsPentagon
    for (int32 i = 0; i < N; ++i)
    {
        CellData[i].CellId      = i;
        CellData[i].bIsPentagon = Topology->Cells[i].bIsPentagon ? 1 : 0;
    }

    Rng.Initialize(Settings.RandomSeed);

    Step_PartitionPlates();       // W2  ✅
    Step_ComputeElevation();      // W3  ✅ （覆写 Step_PartitionPlates 写的板块基础值，叠加边界抬升 + fbm）
    Step_DetermineLandSea();      // W2  ✅
    Step_SimulateMoisture();      // W3  ✅
    Step_ComputeTemperature();    // W3  ✅
    Step_ClassifyBiomes();        // W4  ✅ （UTerrainDefinition::ScoreFor 评分器）
    Step_TraceRivers();           // W5  - W4 阶段仍空体
    Step_AssignBaseCells();       // W6  - W4 阶段仍空体

    UE_LOG(LogWorldGen, Log,
        TEXT("[WorldGen] W4 OK, %d cells, %d plates, %d land / %d ocean (%.0f%%), %d coast, %d mountain ")
        TEXT("(Elev∈[%.2f,%.2f], Moist∈[%.2f,%.2f], Temp∈[%.2f,%.2f]), biome-sentinel=%d"),
        N, Plates.Num(),
        LastLandCount, N - LastLandCount,
        N > 0 ? 100.0f * LastLandCount / N : 0.0f,
        LastCoastCount, LastMountainCount,
        LastElevMin, LastElevMax,
        LastMoistMin, LastMoistMax,
        LastTempMin, LastTempMax,
        LastBiomeSentinelCount);
}

// ──────────────────────────────────────────────────────────
// W2 阶段：Step_PartitionPlates / Step_DetermineLandSea 实体化（✅ 已验收）。
// W3 阶段：Step_ComputeElevation / Step_SimulateMoisture / Step_ComputeTemperature 实体化。
// 其他 3 个 Step_*（ClassifyBiomes / TraceRivers / AssignBaseCells）仍保持空体。
// 详稿：Docs/W2_PlatesAndLandSea.md §3 / Docs/W3_ScalarFields.md §3。
// ──────────────────────────────────────────────────────────

void FWorldGenerator::Step_PartitionPlates()
{
    const int32 N = Topology->Cells.Num();
    Plates.SetNum(Settings.PlateCount);
    PlateIdField.SetNumUninitialized(N);

    // ────── 1. 初始种子（球面随机单位向量；unit Gaussian 等价快速法）──────
    TArray<FVector> SeedDirs;
    SeedDirs.Reserve(Settings.PlateCount);
    for (int32 p = 0; p < Settings.PlateCount; ++p)
    {
        FVector v;
        do
        {
            v = FVector(Rng.FRandRange(-1.0f, 1.0f),
                        Rng.FRandRange(-1.0f, 1.0f),
                        Rng.FRandRange(-1.0f, 1.0f));
        } while (v.SizeSquared() < 1e-8f);
        SeedDirs.Add(v.GetSafeNormal());
    }

    // ────── 2. 球面 Lloyd 松弛（详见 Docs/WorldGenDesign.md §7.1）──────
    constexpr int32 LloydIters = 5;
    TArray<int32>   NearestSeed; NearestSeed.SetNumUninitialized(N);
    TArray<FVector> NewSeeds;
    for (int32 Iter = 0; Iter < LloydIters; ++Iter)
    {
        // a) 每 cell 找最近种子（球面 dot 最大）
        for (int32 i = 0; i < N; ++i)
        {
            const FVector& C = Topology->Cells[i].UnitCenter;
            int32 BestP = 0;
            float BestDot = -2.0f;
            for (int32 p = 0; p < SeedDirs.Num(); ++p)
            {
                const float D = FVector::DotProduct(C, SeedDirs[p]);
                if (D > BestDot) { BestDot = D; BestP = p; }
            }
            NearestSeed[i] = BestP;
        }
        // b) 区内质心 → 移种子
        NewSeeds.Reset();
        NewSeeds.SetNumZeroed(SeedDirs.Num());
        for (int32 i = 0; i < N; ++i)
        {
            NewSeeds[NearestSeed[i]] += Topology->Cells[i].UnitCenter;
        }
        for (int32 p = 0; p < SeedDirs.Num(); ++p)
        {
            if (NewSeeds[p].SizeSquared() > 1e-8f)
            {
                SeedDirs[p] = NewSeeds[p].GetSafeNormal();
            }
            // 否则保持原种子（空区保护）
        }
    }

    // ────── 3. PlateIdField = Lloyd 末轮 NearestSeed ──────
    // 说明：N >> P² 且 UnitCenter 可用时，该结果与主稿 §7.2 "多源 BFS 扩张"产生完全相同的分区。
    PlateIdField = MoveTemp(NearestSeed);
    for (int32 i = 0; i < N; ++i)
    {
        CellData[i].PlateId = PlateIdField[i];
    }

    // ────── 4. 板块漂移向量 + 海洋/大陆类型 + 基础高程 ──────
    // 4.a 海洋/大陆 Fisher-Yates 洗牌（使用 Rng，保证可重现）
    TArray<int32> Indices;
    Indices.Reserve(Settings.PlateCount);
    for (int32 p = 0; p < Settings.PlateCount; ++p) Indices.Add(p);
    for (int32 i = Indices.Num() - 1; i > 0; --i)
    {
        const int32 j = Rng.RandRange(0, i);
        Indices.Swap(i, j);
    }
    const int32 NumOceanic = FMath::RoundToInt(Settings.OceanicPlateRatio * Settings.PlateCount);

    // 4.b 每板块填字段（PlateId / SeedCellId / DriftAxis / DriftSpeed）
    for (int32 p = 0; p < Plates.Num(); ++p)
    {
        Plates[p].PlateId = p;

        // SeedCellId：取本板块中最接近 SeedDirs[p] 的 cell
        int32 BestI = INDEX_NONE;
        float BestDot = -2.0f;
        for (int32 i = 0; i < N; ++i)
        {
            if (PlateIdField[i] != p) continue;
            const float D = FVector::DotProduct(Topology->Cells[i].UnitCenter, SeedDirs[p]);
            if (D > BestDot) { BestDot = D; BestI = i; }
        }
        if (BestI == INDEX_NONE)
        {
            UE_LOG(LogWorldGen, Warning,
                TEXT("[WorldGen] Plate %d has no cells (degenerate); falling back to cell 0"), p);
            BestI = 0;
        }
        Plates[p].SeedCellId = BestI;

        // DriftAxis：球面切平面单位向量。随机 R 在种子点切平面上的投影后归一化。
        FVector R;
        do
        {
            R = FVector(Rng.FRandRange(-1.0f, 1.0f),
                        Rng.FRandRange(-1.0f, 1.0f),
                        Rng.FRandRange(-1.0f, 1.0f));
        } while (R.SizeSquared() < 1e-8f);
        R = R.GetSafeNormal();
        const FVector& SeedDir = SeedDirs[p];
        FVector Tangent = R - FVector::DotProduct(R, SeedDir) * SeedDir;
        if (Tangent.SizeSquared() < 1e-6f)
        {
            // R 与 SeedDir 共线：用任意正交基 fallback
            Tangent = FVector::CrossProduct(SeedDir, FVector::UpVector);
            if (Tangent.SizeSquared() < 1e-6f)
            {
                Tangent = FVector::CrossProduct(SeedDir, FVector::ForwardVector);
            }
        }
        Plates[p].DriftAxis  = Tangent.GetSafeNormal();
        Plates[p].DriftSpeed = Rng.FRandRange(0.3f, 1.0f);
    }

    // 4.c 按洗牌顺序分配海洋/大陆二值 + 基础高程
    for (int32 k = 0; k < Indices.Num(); ++k)
    {
        const int32 PIdx = Indices[k];
        const bool  bOcn = (k < NumOceanic);
        Plates[PIdx].bIsOceanic    = bOcn;
        Plates[PIdx].BaseElevation = bOcn ? -0.3f : +0.2f;
    }

    // 4.d 把板块基础高程写到 ElevationField（W3 启动时会被 Step_ComputeElevation 完全覆写）
    for (int32 i = 0; i < N; ++i)
    {
        ElevationField[i]      = Plates[PlateIdField[i]].BaseElevation;
        CellData[i].Elevation  = ElevationField[i];   // 临时；W3 覆写
    }
}

// ──────────────────────────────────────────────────────────
// W3 阶段：Step_ComputeElevation / Step_SimulateMoisture / Step_ComputeTemperature 实体化。
// 详稿：Docs/W3_ScalarFields.md §3（算法） / §A（可粘贴 cpp）。
// ──────────────────────────────────────────────────────────

void FWorldGenerator::Step_ComputeElevation()
{
    const int32 N        = Topology->Cells.Num();
    const int32 NumEdges = Topology->Edges.Num();

    // 1) 板块基础值已在 Step_PartitionPlates §4.d 写入 ElevationField（W2 占位）；
    //    本函数把它当作初始值起步——不重置。

    // 2) 扫描边：判别板块边界，计算 Δv_n，按强度抬升/俯冲两侧 cell。
    //    （详见 Docs/WorldGenDesign.md §2.1 板块边界类型公式）
    for (int32 e = 0; e < NumEdges; ++e)
    {
        FCellEdge& Edge = Topology->Edges[e];
        const int32 CA = Edge.CellIds[0];
        const int32 CB = Edge.CellIds[1];
        if (CA == INDEX_NONE || CB == INDEX_NONE) continue;

        const int32 PA = PlateIdField[CA];
        const int32 PB = PlateIdField[CB];
        if (PA == PB)
        {
            Edge.bIsPlateBoundary = false;
            Edge.BoundaryStrength = 0.0f;
            continue;
        }
        Edge.bIsPlateBoundary = true;

        // 边界法向：A→B 方向的球面切向（用 UnitCenter 差归一化即可，无需精确投影到切平面）
        const FVector& UA = Topology->Cells[CA].UnitCenter;
        const FVector& UB = Topology->Cells[CB].UnitCenter;
        const FVector  Nab = (UB - UA).GetSafeNormal();

        const FVector Va = Plates[PA].DriftAxis * Plates[PA].DriftSpeed;
        const FVector Vb = Plates[PB].DriftAxis * Plates[PB].DriftSpeed;
        const float DeltaVn = FVector::DotProduct(Va - Vb, Nab);

        Edge.BoundaryStrength = FMath::Abs(DeltaVn);

        // 抬升/俯冲：DeltaVn > 0 = 汇聚（A 推向 B，山脉抬升）；< 0 = 张裂（裂谷下沉）。
        // 仅当 |DeltaVn| > Threshold 时才显著影响地形（小于阈值视为平移边界，不抬升）。
        if (Edge.BoundaryStrength > Settings.PlateBoundaryThreshold)
        {
            // 0.5 经验缩放：避免单条强边主宰整个 Elevation 范围（多边相加几何爆量）。
            const float UpliftMagnitude = (DeltaVn > 0.0f ? +1.0f : -1.0f)
                                        * Edge.BoundaryStrength
                                        * Settings.MountainBoundaryStrength
                                        * 0.5f;
            ElevationField[CA] += UpliftMagnitude;
            ElevationField[CB] += UpliftMagnitude;
        }
    }

    // 3) fbm 噪声叠加（种子与板块种子解耦：异或一个魔法常数）
    {
        FWorldGenNoise DetailNoise(
            Settings.RandomSeed ^ 0x1A2B3C4D,
            Settings.ElevationNoiseFrequency,
            /*Octaves=*/4);
        for (int32 i = 0; i < N; ++i)
        {
            const FVector& U = Topology->Cells[i].UnitCenter;
            const float Noise = DetailNoise.Fbm(U);                  // ∈ [-1, 1]
            ElevationField[i] += Noise * Settings.ElevationNoiseAmplitude;
        }
    }

    // 4) clamp + 写回 CellData + bIsMountain 标记 + 统计 min/max
    int32 MountainCount = 0;
    LastElevMin = +1e9f;
    LastElevMax = -1e9f;
    for (int32 i = 0; i < N; ++i)
    {
        ElevationField[i] = FMath::Clamp(ElevationField[i], -1.0f, 1.0f);
        CellData[i].Elevation = ElevationField[i];

        const bool bMountain = (ElevationField[i] > Settings.MountainThreshold);
        CellData[i].bIsMountain = bMountain ? 1 : 0;
        if (bMountain) ++MountainCount;

        LastElevMin = FMath::Min(LastElevMin, ElevationField[i]);
        LastElevMax = FMath::Max(LastElevMax, ElevationField[i]);
    }
    LastMountainCount = MountainCount;
}

void FWorldGenerator::Step_DetermineLandSea()
{
    const int32 N = Topology->Cells.Num();

    // 1) bIsLand = Elevation > SeaLevel
    int32 LandCount = 0;
    for (int32 i = 0; i < N; ++i)
    {
        const bool bLand = (ElevationField[i] > Settings.SeaLevel);
        CellData[i].bIsLand = bLand ? 1 : 0;
        if (bLand) ++LandCount;
    }

    // 2) bIsCoast = bIsLand && (∃ 邻居 !bIsLand)
    int32 CoastCount = 0;
    for (int32 i = 0; i < N; ++i)
    {
        if (!CellData[i].bIsLand) { CellData[i].bIsCoast = 0; continue; }
        bool bAnyOceanNeighbor = false;
        for (const int32 NId : Topology->Cells[i].NeighborCellIds)
        {
            if (NId != INDEX_NONE && !CellData[NId].bIsLand)
            {
                bAnyOceanNeighbor = true;
                break;
            }
        }
        CellData[i].bIsCoast = bAnyOceanNeighbor ? 1 : 0;
        if (bAnyOceanNeighbor) ++CoastCount;
    }

#if WITH_EDITOR
    check(LandCount  >= 0 && LandCount  <= N);
    check(CoastCount >= 0 && CoastCount <= LandCount);
#endif

    LastLandCount  = LandCount;
    LastCoastCount = CoastCount;
}

void FWorldGenerator::Step_SimulateMoisture()
{
    const int32 N = Topology->Cells.Num();

    // ¶W3.5 修订（2026-06-28）：上风向 SSSP（Bellman-Ford 多轮松弛）+ 高程加权边权。
    // 雨影不再独立处理——高地 cell 边权 ×AlphaElevation 后跨过山脉后西侧自然更干。
    // Settings.RainShadowFactor 在 W3 中废弃（W3 deprecated），保留字段不读；W4+ 可能重启用。
    // 完整设计见 Docs/W3_ScalarFields.md §3.2 / §4.0。

    // 1) 上风 SSSP 多源初始化：所有 ocean cell + bIsCoast cell 累积距离 = 0
    TArray<float> AccumDist;
    AccumDist.Init(TNumericLimits<float>::Max(), N);
    for (int32 i = 0; i < N; ++i)
    {
        if (!CellData[i].bIsLand || CellData[i].bIsCoast)
        {
            AccumDist[i] = 0.0f;
        }
    }

    // 2) Bellman-Ford 多轮松弛
    //    每轮扫描所有 cell；对每 cur 仅从"上风邻居"（东侧 = dot(nbr - cur, East) > 0）取后续距离。
    //    边权 = 1 + AlphaElevation * max(0, Elev_up - SeaLevel) - AlphaCoast * (NId.bIsCoast ? 1 : 0)，下限 0.1。
    //    极区 |East| < ε 退化为各向同性（所有邻居都视为上风）。
    //
    //    ⚠ 东向公式（UE5 左手系 + Z up）：
    //      UE5 是左手系，俯视 +Z 看，+X→+Y 是顺时针；
    //      地球北极俯视自转方向是逆时针（自西向东），故 UE5 中"自西向东"= +X→-Y。
    //      赤道点 P=(cosλ, -sinλ, 0)（λ 为经度，λ=0 时 P=+X，向东转→-Y）；
    //      东向切向量 dP/dλ = (-sinλ, -cosλ, 0) = (P.y, -P.x, 0)。
    //      故 East = (U.Y, -U.X, 0).Normalized()——而非直觉的 (-U.Y, U.X, 0)（那是向西）。
    //      历史教训见 Docs/AgentWorkflow.md §3.7。
    bool bChanged = true;
    int32 Round = 0;
    while (bChanged && Round < N)
    {
        bChanged = false;
        for (int32 cur = 0; cur < N; ++cur)
        {
            const FVector& U = Topology->Cells[cur].UnitCenter;
            FVector East(U.Y, -U.X, 0.0f);
            const float EastLen2 = East.SizeSquared();
            const bool bPolar = EastLen2 < 1e-6f;
            if (!bPolar) East = East.GetSafeNormal();

            // ⚠ 真实字段名：NeighborCellIds（TStaticArray<int32, 6>，pent 第 6 项 = INDEX_NONE）
            for (const int32 NId : Topology->Cells[cur].NeighborCellIds)
            {
                if (NId == INDEX_NONE) continue;

                // 上风邻居判别（极区跳过该过滤）
                if (!bPolar)
                {
                    const FVector D = (Topology->Cells[NId].UnitCenter - U).GetSafeNormal();
                    if (FVector::DotProduct(D, East) <= 0.0f) continue;
                }

                // 边权取决于"上游"节点 NId 的高程/海岸状态——"湿气走过 NId 这块地"的成本
                const float ElevUp = ElevationField[NId];
                const float ExcessUp = FMath::Max(0.0f, ElevUp - Settings.SeaLevel);
                float EdgeCost = 1.0f
                    + Settings.AlphaElevation * ExcessUp
                    - Settings.AlphaCoast * (CellData[NId].bIsCoast ? 1.0f : 0.0f);
                EdgeCost = FMath::Max(EdgeCost, 0.1f);   // 下限避免负/零

                const float NewDist = AccumDist[NId] + EdgeCost;
                if (NewDist < AccumDist[cur])
                {
                    AccumDist[cur] = NewDist;
                    bChanged = true;
                }
            }
        }
        ++Round;
    }

    // 3) 基础湿度：海洋满湿；陆地按上风累积距离指数衰减
    LastMoistMin = +1e9f;
    LastMoistMax = -1e9f;
    for (int32 i = 0; i < N; ++i)
    {
        if (!CellData[i].bIsLand)
        {
            MoistureField[i] = Settings.MoistureScale;
        }
        else
        {
            const float Dist = AccumDist[i];
            // SSSP 未覆盖（孤岛？极区退化不及？）的 cell 以默认干谷距离 50 兜底
            const float Safe = FMath::IsFinite(Dist) ? Dist : 50.0f;
            MoistureField[i] = Settings.MoistureScale * FMath::Exp(-Settings.MoistureCoastalFalloff * Safe);
        }
        MoistureField[i] = FMath::Clamp(MoistureField[i], 0.0f, 1.0f);
        CellData[i].Moisture = MoistureField[i];
        LastMoistMin = FMath::Min(LastMoistMin, MoistureField[i]);
        LastMoistMax = FMath::Max(LastMoistMax, MoistureField[i]);
    }
}

void FWorldGenerator::Step_ComputeTemperature()
{
    const int32 N = Topology->Cells.Num();

    LastTempMin = +1e9f;
    LastTempMax = -1e9f;
    for (int32 i = 0; i < N; ++i)
    {
        const FVector& U = Topology->Cells[i].UnitCenter;

        // 1) 纬度基础：z=0（赤道）= 1，z=±1（极点）= -1
        const float LatBase = 1.0f - 2.0f * FMath::Abs(static_cast<float>(U.Z));

        // 2) 全局偏移
        const float Biased = LatBase + Settings.TemperatureBias;

        // 3) 高程 lapse rate（仅陆地高于海平面才降温；海洋不衰减）
        const float ExcessElev = FMath::Max(0.0f, ElevationField[i] - Settings.SeaLevel);
        const float Final      = Biased - ExcessElev * Settings.TemperatureLapseRate;

        TemperatureField[i] = FMath::Clamp(Final, -1.0f, 1.0f);
        CellData[i].Temperature = TemperatureField[i];

        LastTempMin = FMath::Min(LastTempMin, TemperatureField[i]);
        LastTempMax = FMath::Max(LastTempMax, TemperatureField[i]);
    }
}
void FWorldGenerator::Step_ClassifyBiomes()
{
    // ──────────────────────────────────────────────────────────────────
    // W4：评分器实体化。本函数完全不知道任何具体生物群系语义（"什么是沙漠/森林/冰原"）
    // 都交给 UTerrainDefinition DataAsset 的 ClimateRules[] 表达。
    // 走 ini 注册的 17 个 Terrain.* Tag 在本函数中仅以 FGameplayTag 句柄出现（不出现字符串）。
    //
    // 详见 Docs/W4_BiomeClassification.md §3.2 / §A.8。
    // ──────────────────────────────────────────────────────────────────

    LastBiomeSentinelCount = 0;

    // 0) 解析 TerrainSet：SoftObjectPtr 需要显式 LoadSynchronous。
    UTerrainSet* Set = Settings.TerrainSet.LoadSynchronous();
    if (!Set)
    {
        UE_LOG(LogWorldGen, Warning,
            TEXT("[WorldGen] Step_ClassifyBiomes: TerrainSet 未指定；跳过分类，TerrainTag 保持为 None。"));
        return;
    }
    Set->LoadSynchronous();
    const TArray<UTerrainDefinition*>& Defs = Set->GetLoadedDefs();
    if (Defs.Num() == 0)
    {
        UE_LOG(LogWorldGen, Warning,
            TEXT("[WorldGen] Step_ClassifyBiomes: TerrainSet 内含 0 个 Def；跳过分类。"));
        return;
    }

    // 1) Sentinel：覆盖盲区 fallback。
    //    SentinelTag 由 FWorldGenSettings 持有（FGameplayTag 字段），
    //    设计师在编辑器内挂 Terrain.Plain.Grass；cpp 不出现具体 Tag 字符串。
    UTerrainDefinition* SentinelDef = nullptr;
    if (Settings.SentinelTerrainTag.IsValid())
    {
        for (UTerrainDefinition* Def : Defs)
        {
            if (Def && Def->TerrainTag == Settings.SentinelTerrainTag)
            {
                SentinelDef = Def;
                break;
            }
        }
        if (!SentinelDef)
        {
            UE_LOG(LogWorldGen, Warning,
                TEXT("[WorldGen] Step_ClassifyBiomes: SentinelTerrainTag '%s' 未在 TerrainSet 中找到对应 Def。"),
                *Settings.SentinelTerrainTag.ToString());
        }
    }

    // 2) 双层遍历× ScoreFor × argmax。
    const int32 N = CellData.Num();
    int32 SentinelCount = 0;
    for (int32 i = 0; i < N; ++i)
    {
        FCellGeoData& CD = CellData[i];

        FClimateSample Sample;
        Sample.Elevation   = ElevationField[i];
        Sample.Moisture    = MoistureField[i];
        Sample.Temperature = TemperatureField[i];
        Sample.bIsLand     = (CD.bIsLand     != 0);
        Sample.bIsCoast    = (CD.bIsCoast    != 0);
        Sample.bIsMountain = (CD.bIsMountain != 0);

        UTerrainDefinition* Best = nullptr;
        float BestScore = 0.0f;
        for (UTerrainDefinition* Def : Defs)
        {
            if (!Def) continue;
            const float Score = Def->ScoreFor(Sample);
            if (Score > BestScore)
            {
                BestScore = Score;
                Best = Def;
            }
        }

        if (Best)
        {
            CD.TerrainTag = Best->TerrainTag;
            CD.Resources  = Best->DefaultResources;
        }
        else
        {
            ++SentinelCount;
            if (SentinelDef)
            {
                CD.TerrainTag = SentinelDef->TerrainTag;
                CD.Resources  = SentinelDef->DefaultResources;
            }
            // else：保留 None Tag——渲染端遇到 None 会 fallback 到 LayerIndex 0
        }
    }

    LastBiomeSentinelCount = SentinelCount;

#if WITH_EDITOR
    // 临时自检（1÷N > 10%% 则提示覆盖盲区）。
    ensureMsgf(N == 0 || SentinelCount * 10 < N,
        TEXT("[WorldGen] W4: sentinel-fallback ratio %d/%d > 10%%\uff1bClimateRules[] 区间存在显著覆盖盲区"),
        SentinelCount, N);
#endif

    UE_LOG(LogWorldGen, Log,
        TEXT("[WorldGen] W4 ClassifyBiomes: %d cells classified, sentinel-fallback=%d (%d defs in set)"),
        N, SentinelCount, Defs.Num());
}
void FWorldGenerator::Step_TraceRivers()        { /* W5 实现：D6/D5 最陡下降 + 汇流 */ }
void FWorldGenerator::Step_AssignBaseCells()    { /* W6 实现：12 五边形势力基地分配 */ }