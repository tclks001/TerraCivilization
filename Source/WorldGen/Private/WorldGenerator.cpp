// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldGenerator.h"
#include "WorldGenLog.h"
#include "FSphereTopology.h"
#include "FCell.h"

FWorldGenerator::FWorldGenerator(const FSphereTopology* InTopology, const FWorldGenSettings& InSettings)
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

    Step_PartitionPlates();       // W2  - W1 空体
    Step_ComputeElevation();      // W3  - W1 空体
    Step_DetermineLandSea();      // W2  - W1 空体
    Step_SimulateMoisture();      // W3  - W1 空体
    Step_ComputeTemperature();    // W3  - W1 空体
    Step_ClassifyBiomes();        // W4  - W1 空体
    Step_TraceRivers();           // W5  - W1 空体
    Step_AssignBaseCells();       // W6  - W1 空体

    UE_LOG(LogWorldGen, Log,
        TEXT("[WorldGen] Skeleton OK, %d cells, no-op generate"), N);
}

// ─────────────────────────────────────────────────────────────
// W1 阶段：以下 8 个 Step_* 全部空体；后续阶段逐个填充实现。
// 实现路线见 Docs/WorldGenDesign.md §11 W-step Roadmap。
// ─────────────────────────────────────────────────────────────

void FWorldGenerator::Step_PartitionPlates()    { /* W2 实现：球面 Lloyd + BFS Voronoi */ }
void FWorldGenerator::Step_ComputeElevation()   { /* W3 实现：板块基础高程 + 边界抬升 + fbm */ }
void FWorldGenerator::Step_DetermineLandSea()   { /* W2 实现：海陆判定 + 海岸标记 */ }
void FWorldGenerator::Step_SimulateMoisture()   { /* W3 实现：风带 + 海距 + 雨影 */ }
void FWorldGenerator::Step_ComputeTemperature() { /* W3 实现：纬度 + 高程 + 全局偏移 */ }
void FWorldGenerator::Step_ClassifyBiomes()     { /* W4 实现：Whittaker 双轴查表 */ }
void FWorldGenerator::Step_TraceRivers()        { /* W5 实现：D6/D5 最陡下降 + 汇流 */ }
void FWorldGenerator::Step_AssignBaseCells()    { /* W6 实现：12 五边形势力基地分配 */ }
