// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TerrainPlacementMask.generated.h"

/**
 * ETerrainPlacementMask
 *
 * UTerrainDefinition::ClimateRules[].Placement 取值。表达"该规则适用于哪种几何位置"。
 * 该枚举保留给旧地理生物群系评分器或未来扩展使用；当前 SimpleGameplay WorldGen 不再执行
 * Step_ClassifyBiomes，只直接写入平原 / 森林 / 山脉三种 TerrainTag。
 *
 * 详见 Docs/W4_BiomeClassification.md §2.1 / §2.2 与 Docs/SimpleGameplay/WorldGenDesign.md。
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
