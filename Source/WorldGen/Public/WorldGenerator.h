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
 * W1 阶段：所有 Step_* 都是空体；Generate() 仅写 CellId + bIsPentagon，并打印诊断日志。
 * 详见 Docs/WorldGenDesign.md §13.1 / Docs/W1_ModuleSkeleton.md §2.4。
 */
class WORLDGEN_API FWorldGenerator
{
public:
    explicit FWorldGenerator(const FSphereTopology* InTopology, const FWorldGenSettings& InSettings);

    /** 跑完整 8 步流水线；W1 阶段所有 step 都是空函数；执行后 GetCellData() 仅有 CellId + bIsPentagon。 */
    void Generate();

    const TArray<FCellGeoData>& GetCellData()    const { return CellData; }
    const TArray<FPlateInfo>&   GetPlates()      const { return Plates; }
    const TArray<int32>&        GetBaseCellIds() const { return BaseCellIds; }

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
    const FSphereTopology* Topology = nullptr;
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
};
