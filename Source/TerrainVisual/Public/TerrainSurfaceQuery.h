#pragma once

#include "TerrainVisualTypes.h"

class TERRAINVISUAL_API ITerrainSurfaceQuery
{
public:
    virtual ~ITerrainSurfaceQuery() = default;

    virtual FTerrainSurfaceQueryResult QueryBaseSurface(const FVector& UnitDirection) const = 0;
};
