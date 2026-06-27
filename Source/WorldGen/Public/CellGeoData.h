// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CellGeoData.generated.h"

/**
 * FCellGeoData
 *
 * WorldGen 流水线对每个 Cell 的输出。CellId == 数组下标。
 * 详见 Docs/WorldGenDesign.md §4.2。
 *
 * 字段写入阶段表：
 *   W1: CellId, bIsPentagon
 *   W2: PlateId, bIsLand, bIsCoast
 *   W3: Elevation, Moisture, Temperature, bIsMountain
 *   W4: TerrainTag, Resources
 *   W5: bIsRiver, bIsLake, FlowTo
 *   W6: BaseFactionId
 *   运行时（Gameplay）: OwnerId
 */
USTRUCT(BlueprintType)
struct WORLDGEN_API FCellGeoData
{
    GENERATED_BODY()

    UPROPERTY() int32 CellId          = INDEX_NONE;
    UPROPERTY() int32 PlateId         = INDEX_NONE;

    UPROPERTY() float Elevation       = 0.f;     // [-1, 1]
    UPROPERTY() float Moisture        = 0.5f;    // [ 0, 1]
    UPROPERTY() float Temperature     = 0.f;     // [-1, 1]

    UPROPERTY() uint8 bIsLand     : 1;
    UPROPERTY() uint8 bIsCoast    : 1;
    UPROPERTY() uint8 bIsMountain : 1;
    UPROPERTY() uint8 bIsRiver    : 1;
    UPROPERTY() uint8 bIsLake     : 1;
    UPROPERTY() uint8 bIsPentagon : 1;

    UPROPERTY() FGameplayTag           TerrainTag;
    UPROPERTY() FGameplayTagContainer  Resources;

    UPROPERTY() int32 BaseFactionId = INDEX_NONE;
    UPROPERTY() int32 OwnerId       = INDEX_NONE;  // 运行时 Gameplay 写
    UPROPERTY() int32 FlowTo        = INDEX_NONE;  // W5 河流下游 cell（局部最低点 = INDEX_NONE）

    FCellGeoData()
        : bIsLand(1)
        , bIsCoast(0)
        , bIsMountain(0)
        , bIsRiver(0)
        , bIsLake(0)
        , bIsPentagon(0)
    {}
};
