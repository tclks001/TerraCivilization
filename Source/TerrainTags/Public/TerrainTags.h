// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * TerrainTags 模块入口（W4 引入）。
 *
 * 该模块提供：
 *   - UTerrainDefinition / UTerrainSet（DataAsset）
 *   - FTerrainClimateRule / ETerrainPlacementMask（数据结构）
 *   - 17 个 Terrain.* GameplayTag 通过 Config/Tags/Terrain.ini 注册（不在本模块 cpp 里静态绑定）
 *
 * WorldGen 模块依赖 TerrainTags，仅作为评分器：
 *   for cell in Cells:
 *       Best = argmax_{Def in TerrainSet->TerrainDefs} Def->ScoreFor(Sample(cell))
 *       cell.TerrainTag = Best->TerrainTag
 *
 * 详见 Docs/W4_BiomeClassification.md。
 */
class FTerrainTagsModule : public IModuleInterface
{
};
