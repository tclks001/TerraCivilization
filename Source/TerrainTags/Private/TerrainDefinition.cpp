// Copyright Epic Games, Inc. All Rights Reserved.

#include "TerrainDefinition.h"

float UTerrainDefinition::ScoreFor(const FClimateSample& S) const
{
    float Best = 0.0f;
    for (const FTerrainClimateRule& Rule : ClimateRules)
    {
        // 1) Placement 几何要求
        bool bPlacementOK = false;
        switch (Rule.Placement)
        {
            case ETerrainPlacementMask::Any:
                bPlacementOK = true;
                break;
            case ETerrainPlacementMask::Land:
                // 陆地（非海岸非山脉）：cell 是陆地 + 不是海岸 + 不是山脉
                bPlacementOK = S.bIsLand && !S.bIsCoast && !S.bIsMountain;
                break;
            case ETerrainPlacementMask::Ocean:
                bPlacementOK = !S.bIsLand;
                break;
            case ETerrainPlacementMask::Coast:
                // 海岸：陆地侧的海岸（与海洋 1-ring 邻居）
                bPlacementOK = S.bIsLand && S.bIsCoast;
                break;
            case ETerrainPlacementMask::Mountain:
                // 山脉：陆地高海拔区
                bPlacementOK = S.bIsLand && S.bIsMountain;
                break;
            default:
                bPlacementOK = false;
                break;
        }
        if (!bPlacementOK) continue;

        // 2) 三轴硬区间命中（W4 起步：1.0/0.0 不平滑；W4.5+ 可升 smoothstep）
        const bool bTOk = (S.Temperature >= Rule.Temperature.Min) && (S.Temperature <= Rule.Temperature.Max);
        const bool bMOk = (S.Moisture    >= Rule.Moisture.Min   ) && (S.Moisture    <= Rule.Moisture.Max);
        const bool bEOk = (S.Elevation   >= Rule.Elevation.Min  ) && (S.Elevation   <= Rule.Elevation.Max);
        if (!(bTOk && bMOk && bEOk)) continue;

        // 3) 命中：取该规则的 Priority；同 Def 多条规则 OR 语义取最大
        Best = FMath::Max(Best, Rule.Priority);
    }
    return Best;
}
