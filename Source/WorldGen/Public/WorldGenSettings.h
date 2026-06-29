// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "WorldGenSettings.generated.h"

// W4 起：原 W7 计划取消，改为 UTerrainSet（DataAsset，持一组 UTerrainDefinition*）。
// UTerrainDefinition 自身携带 ClimateRules[]——分类规则属于地形定义本身，
// 设计师可在编辑器内调；详见 Docs/W4_BiomeClassification.md §2.1。
// 17 个 Terrain.* GameplayTag 走 Config/Tags/Terrain.ini 集中注册（不走 cpp FNativeGameplayTag、
// cpp 不消费具体 Tag 名）。

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

    /** 海距湿度衰减系数。上风向 SSSP 中按累积距离衰减；Moist = MoistureScale * exp(-MoistureCoastalFalloff * AccumDist)。
     *  W3.5 默认 0.15；调大→梯度更陶、调小→梯度更平。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Climate", meta = (ClampMin = "0.0"))
    float MoistureCoastalFalloff = 0.15f;

    /** [W3.5 deprecated] 原雨影衰减系数。雨影已内嵌到上风向 SSSP 边权中（AlphaElevation），本字段保留但 W3 不读；W4+ 可能重启用。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RainShadowFactor = 0.3f;

    /** [W3.5 新增] 高程衰减加权系数。上风向 SSSP 中高地 cell 边权 = 1 + AlphaElevation × max(0, Elev - SeaLevel)。
     *  默认 4.0：Elev=1、SeaLevel=0 时山脉 cell 边权 = 5（= 5 个平地 cell）。
     *  调低→雨影减弱；调高→雨影加强、高原也变干。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Climate", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float AlphaElevation = 4.0f;

    /** [W3.5 新增] 海岸边权减免系数。上风向 SSSP 中海岸 cell 为上风邻居时边权 = 1 - AlphaCoast。
     *  默认 0.5：海岸为上风邻居时边权 = 0.5（海岸推进湿气更高效）。上限 1.0 避免边权 ≤ 0。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AlphaCoast = 0.5f;

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

    // ───────────── Biomes (W4) ─────────────

    /**
     * W4 起：UTerrainSet（DataAsset）持有 17~19 个 UTerrainDefinition* 引用；每个 Def
     * 自带 ClimateRules[] 规则、LayerIndex、玩法属性。Step_ClassifyBiomes 不知道
     * Tag/Layer 语义，仅递归调用 Def->ScoreFor(Sample) 取 argmax。
     * 详见 Docs/W4_BiomeClassification.md §2.1。
     */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Biomes")
    TSoftObjectPtr<class UTerrainSet> TerrainSet;

    /**
     * 覆盖盲区 fallback 时使用的 Tag（一般指向 Terrain.Plain.Grass）。
     * 设计师在编辑器面板挂上 Tag；cpp 端不出现具体 Tag 字符串。
     * 详见 Docs/W4_BiomeClassification.md §2.3 / §3.2。
     */
    UPROPERTY(EditAnywhere, Category = "WorldGen|Biomes")
    FGameplayTag SentinelTerrainTag;
};