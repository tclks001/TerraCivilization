// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "WorldGenSettings.generated.h"

// W7 阶段会引入具体的 UBiomeTable : public UDataAsset；W1~W6 阶段 BiomeTable 字段类型先用基类 UDataAsset 占位，
// 避免提前创建 UBiomeTable 文件污染骨架。W7 落地时把 TSoftObjectPtr<UDataAsset> 升级为 TSoftObjectPtr<UBiomeTable>。

/**
 * FWorldGenSettings
 *
 * WorldGen 流水线参数面板（W1~W7 全 step 参数集中地）。
 * 字段按 step 分组，每字段标注启用阶段；W1 阶段全部仅持有不消费。
 * 详见 Docs/WorldGenDesign.md §4.1。
 */
USTRUCT(BlueprintType)
struct WORLDGEN_API FWorldGenSettings
{
    GENERATED_BODY()

    // ───────────── General ─────────────

    /** 随机种子；同一种子 + 同 SubdivisionLevel 永远产生相同世界。W1 持有，W2 起消费。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|General")
    int32 RandomSeed = 0;

    // ───────────── Plates (W2) ─────────────

    /** 板块数量；推荐 8~16，与 12 五边形数量协调。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Plates", meta = (ClampMin = "3", ClampMax = "32"))
    int32 PlateCount = 12;

    /** 海洋板块比例。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Plates", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float OceanicPlateRatio = 0.6f;

    /** 板块边界类型判别阈值（汇聚/张裂/平移）。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Plates", meta = (ClampMin = "0.0"))
    float PlateBoundaryThreshold = 0.1f;

    // ───────────── Elevation (W2 / W3) ─────────────

    /** 海平面阈值（Elevation 低于此值视为海洋）。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Elevation", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float SeaLevel = 0.0f;

    /** 山脉阈值（Elevation 高于此值视为山脉）。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Elevation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MountainThreshold = 0.5f;

    /** 板块汇聚边界的山脉抬升强度。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Elevation", meta = (ClampMin = "0.0"))
    float MountainBoundaryStrength = 1.0f;

    /** 高程噪声振幅（fbm）。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Elevation", meta = (ClampMin = "0.0"))
    float ElevationNoiseAmplitude = 0.3f;

    /** 高程噪声频率。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Elevation", meta = (ClampMin = "0.0"))
    float ElevationNoiseFrequency = 2.0f;

    // ───────────── Climate (W3) ─────────────

    /** 湿度场最大值（影响整体雨量）。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MoistureScale = 1.0f;

    /** 海距湿度衰减系数（每 cell 衰减）。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Climate", meta = (ClampMin = "0.0"))
    float MoistureCoastalFalloff = 0.05f;

    /** 雨影衰减系数（越过山脉后湿度乘以此值）。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RainShadowFactor = 0.3f;

    /** 全局温度偏移（暖期 = +0.2，冰期 = -0.3）。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Climate", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float TemperatureBias = 0.0f;

    /** 海拔每升高 0.1 单位，温度下降的量。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Climate", meta = (ClampMin = "0.0"))
    float TemperatureLapseRate = 0.3f;

    // ───────────── Rivers (W5) ─────────────

    /** 河流流量阈值；汇流量大于此值的 cell 标为河流。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Rivers", meta = (ClampMin = "0.0"))
    float RiverDischargeThreshold = 5.0f;

    /** 湖泊流量阈值。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Rivers", meta = (ClampMin = "0.0"))
    float LakeDischargeThreshold = 20.0f;

    // ───────────── Bases (W6) ─────────────

    /** 12 个势力基地的 Trait（数量必须 == 5/12 五边形数）。W6 启用。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Bases")
    TArray<FGameplayTagContainer> ForcedBaseTraits;

    // ───────────── Biomes (W4 / W7) ─────────────

    /**
     * 生物群系查表（W7 启用 DataAsset 化）；W4 阶段先用 cpp 硬编码默认 5x5 表。
     * W1~W6 类型为基类 UDataAsset；W7 阶段定义 UBiomeTable : public UDataAsset 后升级为 TSoftObjectPtr<UBiomeTable>。
     */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Biomes")
    TSoftObjectPtr<UDataAsset> BiomeTable;
};
