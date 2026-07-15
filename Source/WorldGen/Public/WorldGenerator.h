// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "WorldGenSettings.h"
#include "CellGeoData.h"

class FSphereTopology;

/**
 * FWorldGenerator
 *
 * SimpleGameplay 专用世界生成器。
 *
 * 当前实现不再运行旧的板块 / 海陆 / 高程 / 湿度 / 温度 / 河流 / 生物群系评分流水线，
 * 而是基于固定球面拓扑生成三种玩法地形：平原、森林、山脉。
 *
 * 设计稿：Docs/SimpleGameplay/WorldGenDesign.md。
 */
class WORLDGEN_API FWorldGenerator
{
public:
    explicit FWorldGenerator(FSphereTopology* InTopology, const FWorldGenSettings& InSettings);

    /** 生成 SimpleGameplay 三地形世界。 */
    void Generate();

    const TArray<FCellGeoData>& GetCellData() const { return CellData; }
    const TArray<int32>& GetBaseCellIds() const { return BaseCellIds; }
    const TArray<ETerraSimpleTerrainType>& GetSimpleTerrainField() const { return SimpleTerrainField; }
    void OverrideSimpleTerrainField(const TArray<ETerraSimpleTerrainType>& InTerrainField);

private:
    struct FMountainGrowTip
    {
        int32 PrevCellId = INDEX_NONE;
        int32 CurCellId = INDEX_NONE;
        bool bActive = false;
    };

    void InitSimpleTerrainAsPlain();
    void CollectSimpleBaseCells();
    void BuildSimpleProtectedCells(int32 Radius);
    void GenerateMountainStrips();
    void GenerateForestPatches();
    void WriteSimpleTerrainToCellData(bool bClearProtectedCells = true);
    void LogSimpleTerrainSummary() const;

    int32 SamplePositiveNormalCount(float AverageCount) const;

    bool IsValidCellId(int32 CellId) const;
    bool IsProtectedCell(int32 CellId) const;
    bool IsValidMountainCell(int32 CellId) const;
    bool IsValidForestCell(int32 CellId) const;
    int32 FindNeighborIndex(int32 CurCellId, int32 NeighborCellId) const;

    bool PickRandomValidMountainSeedPair(int32& OutA, int32& OutB);
    void GrowMountainStrip(int32 TargetCount);
    bool TryGrowMountainTip(FMountainGrowTip& Tip, int32& RemainingCount);
    int32 PickMountainNextCell(int32 PrevCellId, int32 CurCellId);

    bool PickRandomValidForestSeed(int32& OutSeed);
    void GrowForestPatch(int32 TargetCount);
    FVector ComputeForestCenter(const TArray<int32>& ForestCells) const;
    void CollectForestFrontier(const TArray<int32>& ForestCells, TSet<int32>& OutFrontier) const;
    int32 PickBestForestFrontierCell(const TSet<int32>& Frontier, const FVector& CenterDir);

private:
    FSphereTopology* Topology = nullptr;
    FWorldGenSettings Settings;
    mutable FRandomStream Rng;

    TArray<FCellGeoData> CellData;
    TArray<int32> BaseCellIds;
    TArray<ETerraSimpleTerrainType> SimpleTerrainField;
    TArray<bool> bProtectedCell;

    int32 LastProtectedCount = 0;
    int32 LastMountainCount = 0;
    int32 LastForestCount = 0;
    int32 LastCompletedMountainStripCount = 0;
    int32 LastCompletedForestPatchCount = 0;
    int32 LastStoppedMountainStripCount = 0;
    int32 LastStoppedForestPatchCount = 0;
};
