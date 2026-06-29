// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Interval.h"
#include "TerrainPlacementMask.h"
#include "TerrainClimateRule.generated.h"

/**
 * FTerrainClimateRule
 *
 * UTerrainDefinition 的"适用条件"单条规则；OR 语义——同 Def 多条规则取最大 Priority。
 * 命中条件：Placement 几何匹配 && Temperature/Moisture/Elevation 三轴均落在区间内。
 *
 * W4 起步采用硬区间命中（1.0/0.0 不平滑）；W4.5+ 可升级为 smoothstep 软匹配。
 * 详见 Docs/W4_BiomeClassification.md §2.1 / §2.2。
 */
USTRUCT(BlueprintType)
struct TERRAINTAGS_API FTerrainClimateRule
{
    GENERATED_BODY()

    /** 几何 / Placement 要求；不匹配直接得 0 分。 */
    UPROPERTY(EditAnywhere, Category = "Placement")
    ETerrainPlacementMask Placement = ETerrainPlacementMask::Land;

    /** 温度区间 [Min, Max]（FCellGeoData::Temperature ∈ [-1, 1]）。 */
    UPROPERTY(EditAnywhere, Category = "Climate")
    FFloatInterval Temperature = FFloatInterval(-1.0f, 1.0f);

    /** 湿度区间 [Min, Max]（FCellGeoData::Moisture ∈ [0, 1]）。 */
    UPROPERTY(EditAnywhere, Category = "Climate")
    FFloatInterval Moisture = FFloatInterval(0.0f, 1.0f);

    /** 高程区间 [Min, Max]（FCellGeoData::Elevation ∈ [-1, 1]）。 */
    UPROPERTY(EditAnywhere, Category = "Climate")
    FFloatInterval Elevation = FFloatInterval(-1.0f, 1.0f);

    /**
     * 命中后的得分；用于消歧（多 Def 同时命中时取最高 Priority）。
     * 例：Wetland 与 Forest.Tropical 都能匹配 (T=0.9, M=0.95) 时，把 Wetland 的 Priority
     * 设为 1.5 > Forest.Tropical 的 1.0，则 Wetland 胜。
     */
    UPROPERTY(EditAnywhere, Category = "Score", meta = (ClampMin = "0.01"))
    float Priority = 1.0f;
};
