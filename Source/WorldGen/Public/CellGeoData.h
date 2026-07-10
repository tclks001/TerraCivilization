// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CellGeoData.generated.h"

UENUM(BlueprintType)
enum class ETerraSimpleTerrainType : uint8
{
    Plain UMETA(DisplayName = "Plain"),
    Forest UMETA(DisplayName = "Forest"),
    Mountain UMETA(DisplayName = "Mountain"),
};

/**
 * FCellGeoData
 *
 * WorldGen 对每个 Cell 的输出。CellId == 数组下标。
 *
 * SimpleGameplay 阶段只生成三种玩法地形：
 * - Plain：默认地形。
 * - Forest：保护目标免受弓兵远程吃子。
 * - Mountain：骑兵不可进入 / 不可越过，弓兵在其上远程距离 +1。
 *
 */
USTRUCT(BlueprintType)
struct WORLDGEN_API FCellGeoData
{
    GENERATED_BODY()

    UPROPERTY() int32 CellId = INDEX_NONE;
    UPROPERTY() uint8 bIsPentagon : 1;

    UPROPERTY() ETerraSimpleTerrainType SimpleTerrainType = ETerraSimpleTerrainType::Plain;

    UPROPERTY() int32 BaseFactionId = INDEX_NONE;
    UPROPERTY() int32 OwnerId       = INDEX_NONE;  // 运行时 Gameplay 写

    FCellGeoData()
        : bIsPentagon(0)
    {}
};
