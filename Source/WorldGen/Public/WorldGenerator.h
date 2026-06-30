// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "WorldGenSettings.h"
#include "CellGeoData.h"
#include "PlateInfo.h"

class FSphereTopology;

/**
 * FWorldGenerator
 *
 * WorldGen 主类。一次构造，调一次 Generate() 跑完 8 步流水线。
 * - W1：所有 Step_* 都是空体；Generate() 仅写 CellId + bIsPentagon 并打印诊断日志。
 * - W2 ✅：Step_PartitionPlates / Step_DetermineLandSea 实体化（板块构造 + 海陆分离）。
 * - W3 ✅：Step_ComputeElevation / Step_SimulateMoisture / Step_ComputeTemperature 实体化（三标量场）；
 *           同时反向写入 FCellEdge.bIsPlateBoundary / BoundaryStrength（与 SphereTopologyReference §8.1 契约一致）。
 * - W4 ✅：Step_ClassifyBiomes 实体化（UTerrainDefinition::ScoreFor 评分器）；原 W7 计划合并进 W4。
 *           cpp 不知道任何具体生物群系语义；规则、LayerIndex、玩法属性全在 DataAsset。
 * - W5~W6：TraceRivers / AssignBaseCells 仍为空体。
 * 详见 Docs/WorldGenDesign.md §13.1 / Docs/W2_PlatesAndLandSea.md / Docs/W3_ScalarFields.md / Docs/W4_BiomeClassification.md。
 */
class WORLDGEN_API FWorldGenerator
{
public:
    explicit FWorldGenerator(FSphereTopology* InTopology, const FWorldGenSettings& InSettings);

    /** 跑完整 8 步流水线；W1 阶段所有 step 都是空函数；执行后 GetCellData() 仅有 CellId + bIsPentagon。 */
    void Generate();

    const TArray<FCellGeoData>& GetCellData()    const { return CellData; }
    const TArray<FPlateInfo>&   GetPlates()      const { return Plates; }
    const TArray<int32>&        GetBaseCellIds() const { return BaseCellIds; }
    
    /** W2 起暴露：Debug 视图按 PlateId 染色用；以及供下游模块（W3 板块边界判定等）只读访问。 */
    const TArray<int32>&        GetPlateIdField() const { return PlateIdField; }

    /** Debug：单步运行（W8 编辑器视图用，W1 阶段全部空体）。 */
    void Step_PartitionPlates();        // W2
    void Step_ComputeElevation();       // W3
    void Step_DetermineLandSea();       // W2
    void Step_SimulateMoisture();       // W3
    void Step_ComputeTemperature();     // W3
    void Step_ClassifyBiomes();         // W4
    void Step_TraceRivers();            // W5
    void Step_AssignBaseCells();        // W6

private:
    // W3 起：WorldGen 需要向 Topology 反向写入边属性（FCellEdge.bIsPlateBoundary / BoundaryStrength），
    // 因此持有非 const 指针。Topology 本身的 Cells/Edges/Corners 拓扑在 W3+ 仍严格只读，
    // WorldGen 仅写 §8.1 契约中划定的两个边字段。详见 Docs/SphereTopologyReference.md §8.1。
    FSphereTopology*       Topology = nullptr;
    FWorldGenSettings      Settings;
    FRandomStream          Rng;

    TArray<FCellGeoData>   CellData;
    TArray<FPlateInfo>     Plates;
    TArray<int32>          BaseCellIds;

    // 中间标量场（详见 Docs/WorldGenDesign.md §4.4）；W1 仅 SetNumZeroed 不写入。
    TArray<float>          ElevationField;
    TArray<float>          MoistureField;
    TArray<float>          TemperatureField;
    TArray<int32>          PlateIdField;
    TArray<float>          DischargeField;

    // W2 引入：Step_DetermineLandSea() 缓存计数，供 Generate() 末尾日志使用。
    int32 LastLandCount  = 0;
    int32 LastCoastCount = 0;

    // W3 引入：三标量场 min/max 与山脉计数，供 Generate() 末尾日志使用（详见 W3_ScalarFields.md §A.8）。
    int32 LastMountainCount = 0;
    float LastElevMin  = 0.f, LastElevMax  = 0.f;
    float LastMoistMin = 0.f, LastMoistMax = 0.f;
    float LastTempMin  = 0.f, LastTempMax  = 0.f;

    // W4 引入：覆盖盲区 fallback 计数（未被任何 UTerrainDefinition 命中的 cell 数），
    // 供 Generate() 末尾日志使用。详见 W4_BiomeClassification.md §3.2。
    int32 LastBiomeSentinelCount = 0;
};
