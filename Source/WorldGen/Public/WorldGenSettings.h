// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldGenSettings.generated.h"

/**
 * FWorldGenSettings
 *
 * SimpleGameplay 专用 WorldGen 参数。
 *
 * 旧的板块 / 海陆 / 高程 / 湿度 / 温度 / 河流 / 生物群系评分参数已从简易玩法路径移除；
 * 旧生产分支已有留档。当前模块只生成三种玩法地形：平原、森林、山脉。
 *
 * 设计稿：Docs/SimpleGameplay/WorldGenDesign.md。
 */
USTRUCT(BlueprintType)
struct WORLDGEN_API FWorldGenSettings
{
    GENERATED_BODY()

    /** 随机种子；同一种子 + 同一固定 SubdivisionLevel=3 拓扑应产生相同世界。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|SimpleGameplay")
    int32 RandomSeed = 0;

    /** 生成多少条山脉。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|SimpleGameplay|Mountain", meta = (ClampMin = "0"))
    int32 MountainStripCount = 12;

    /** 每条山脉平均包含多少个 Cell；单次实际值按正态分布采样。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|SimpleGameplay|Mountain", meta = (ClampMin = "1.0"))
    float AverageMountainCellCount = 8.0f;

    /** 生成多少片森林。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|SimpleGameplay|Forest", meta = (ClampMin = "0"))
    int32 ForestPatchCount = 12;

    /** 每片森林平均包含多少个 Cell；单次实际值按正态分布采样。 */
    UPROPERTY(EditAnywhere, Category = "WorldGen|SimpleGameplay|Forest", meta = (ClampMin = "1.0"))
    float AverageForestCellCount = 6.0f;
};
