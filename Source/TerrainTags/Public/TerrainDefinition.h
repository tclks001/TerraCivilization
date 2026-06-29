// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TerrainClimateRule.h"
#include "TerrainDefinition.generated.h"

/**
 * FClimateSample
 *
 * UTerrainDefinition::ScoreFor 的输入。由 FWorldGenerator::Step_ClassifyBiomes
 * 在每个 cell 上拼装：FCellGeoData 的标量场 + 几何标志位。
 */
USTRUCT()
struct TERRAINTAGS_API FClimateSample
{
    GENERATED_BODY()

    float Elevation   = 0.0f;
    float Moisture    = 0.5f;
    float Temperature = 0.0f;
    bool  bIsLand     = true;
    bool  bIsCoast    = false;
    bool  bIsMountain = false;
};

/**
 * UTerrainDefinition
 *
 * 单个地形（Terrain.*）的完整定义：身份（Tag + LayerIndex）、适用条件（ClimateRules[]）、
 * 玩法属性（MoveCost/Defense/Resources）。
 *
 * W4 关键设计：分类规则属于地形定义本身——每个 Def 是"自描述的"，它知道自己适用于哪些
 * (T,M,E,Placement) 组合，无需外部 BiomeTable。WorldGen 仅作为评分器，不知道任何具体生物群系语义。
 *
 * 详见 Docs/W4_BiomeClassification.md §2.1。
 */
UCLASS(BlueprintType)
class TERRAINTAGS_API UTerrainDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * 关联的 Terrain.* GameplayTag。
     * 与 Config/Tags/Terrain.ini 中注册的 Tag 名严格一致；编辑器内通过下拉框选择。
     */
    UPROPERTY(EditAnywhere, Category = "Identity")
    FGameplayTag TerrainTag;

    /**
     * 渲染层 LayerIndex（CellAttrLUT.R 写入值，对应 Texture2DArray slice）。
     * 范围 [0, 19]；预留 [20, 255] 给 R9 Decor/Owner/Fog。
     * 详见 Docs/WorldGenDesign.md §6 19-Layer 表。
     */
    UPROPERTY(EditAnywhere, Category = "Identity", meta = (ClampMin = "0", ClampMax = "255"))
    int32 LayerIndex = 0;

    /**
     * 适用条件；OR 语义——任一条规则命中即"此地形适用于该 cell"。
     * Score = max_{Rule ∈ ClimateRules} (Rule 命中 ? Rule.Priority : 0)。
     * 详见 Docs/W4_BiomeClassification.md §2.2。
     */
    UPROPERTY(EditAnywhere, Category = "Placement")
    TArray<FTerrainClimateRule> ClimateRules;

    /** 命中此地形时拷贝到 FCellGeoData::Resources（W4 仅拷贝、不读取）。 */
    UPROPERTY(EditAnywhere, Category = "Gameplay")
    FGameplayTagContainer DefaultResources;

    // ───── W4 不消费、为 Gameplay/GridRender 预留的字段 ─────

    /** 单位移动该地形的基础成本（W4 不读，Gameplay 模块未来读）。 */
    UPROPERTY(EditAnywhere, Category = "Gameplay")
    float MoveCostBase = 1.0f;

    /** 防御加成（W4 不读，Gameplay 模块未来读）。 */
    UPROPERTY(EditAnywhere, Category = "Gameplay")
    float DefenseBonus = 0.0f;

    /** 是否允许建造（W4 不读，Gameplay 模块未来读）。 */
    UPROPERTY(EditAnywhere, Category = "Gameplay")
    bool bAllowBuilding = true;

    /** 不可通行的 Unit.Movement.* 类别（W4 不读，Gameplay 模块未来读）。 */
    UPROPERTY(EditAnywhere, Category = "Gameplay")
    FGameplayTagContainer ImpassableFor;

    /**
     * 评估匹配分数。0 = 不匹配；越大越优先。
     * WorldGen 端 argmax 跨 Def 取分数最高者作为该 cell 的地形。
     * 详见 Docs/W4_BiomeClassification.md §2.2。
     */
    float ScoreFor(const FClimateSample& Sample) const;
};
