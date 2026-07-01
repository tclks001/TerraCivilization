// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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
 * 旧地理字段暂时保留为渲染 / 调试兼容输出，但不再由板块、海陆、气候流水线驱动。
 */
USTRUCT(BlueprintType)
struct WORLDGEN_API FCellGeoData
{
    GENERATED_BODY()

    UPROPERTY() int32 CellId          = INDEX_NONE;
    UPROPERTY() int32 PlateId         = INDEX_NONE;

    UPROPERTY() float Elevation       = 0.f;     // 简易玩法兼容值：山脉=1，森林/平原=0
    UPROPERTY() float Moisture        = 0.5f;    // 简易玩法兼容值：森林=1，其余=0.5
    UPROPERTY() float Temperature     = 0.f;     // 简易玩法固定为 0

    UPROPERTY() uint8 bIsLand     : 1;
    UPROPERTY() uint8 bIsCoast    : 1;
    UPROPERTY() uint8 bIsMountain : 1;
    UPROPERTY() uint8 bIsRiver    : 1;
    UPROPERTY() uint8 bIsLake     : 1;
    UPROPERTY() uint8 bIsPentagon : 1;

    UPROPERTY() ETerraSimpleTerrainType SimpleTerrainType = ETerraSimpleTerrainType::Plain;

    UPROPERTY() FGameplayTag           TerrainTag;
    UPROPERTY() FGameplayTagContainer  Resources;

    UPROPERTY() int32 BaseFactionId = INDEX_NONE;
    UPROPERTY() int32 OwnerId       = INDEX_NONE;  // 运行时 Gameplay 写
    UPROPERTY() int32 FlowTo        = INDEX_NONE;  // 简易玩法不生成河流，固定 INDEX_NONE

    FCellGeoData()
        : bIsLand(1)
        , bIsCoast(0)
        , bIsMountain(0)
        , bIsRiver(0)
        , bIsLake(0)
        , bIsPentagon(0)
    {}
};
