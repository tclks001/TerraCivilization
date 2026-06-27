// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PlateInfo.generated.h"

/**
 * FPlateInfo
 *
 * 板块中间数据。W2 起填充；W1 仅声明。
 * 详见 Docs/WorldGenDesign.md §4.3。
 */
USTRUCT()
struct WORLDGEN_API FPlateInfo
{
    GENERATED_BODY()

    int32   PlateId       = INDEX_NONE;
    int32   SeedCellId    = INDEX_NONE;
    FVector DriftAxis     = FVector::ZeroVector;   // 球面切平面单位向量（垂直于 SeedCellId 的 UnitCenter）
    float   DriftSpeed    = 0.f;                   // [0, 1]
    bool    bIsOceanic    = false;
    float   BaseElevation = 0.f;                   // 海洋板块负、大陆板块正
};
