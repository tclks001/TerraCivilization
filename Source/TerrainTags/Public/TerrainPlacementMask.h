// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TerrainPlacementMask.generated.h"

/**
 * ETerrainPlacementMask
 *
 * UTerrainDefinition::ClimateRules[].Placement 取值。表达"该规则适用于哪种几何位置"。
 * 由 FWorldGenerator::Step_ClassifyBiomes 与 FCellGeoData 的 bIsLand/bIsCoast/bIsMountain 联合判别。
 *
 * 几何短路（海/陆/山）通过 Placement 表达，不在 cpp 里硬编码——
 * 设计师可在 DataAsset 中调整，与"完全数据驱动"原则一致。
 *
 * 详见 Docs/W4_BiomeClassification.md §2.1 / §2.2。
 */
UENUM(BlueprintType)
enum class ETerrainPlacementMask : uint8
{
    Any       UMETA(DisplayName = "任意（无几何要求）"),
    Land      UMETA(DisplayName = "陆地（bIsLand && !bIsCoast && !bIsMountain）"),
    Ocean     UMETA(DisplayName = "海洋（!bIsLand）"),
    Coast     UMETA(DisplayName = "海岸（bIsLand && bIsCoast）"),
    Mountain  UMETA(DisplayName = "山脉（bIsLand && bIsMountain）"),
};
