#pragma once

#include "CoreMinimal.h"
#include "TerrainVisualTypes.h"

class FSphereTopology;
struct FCellGeoData;

class TERRAINVISUAL_API FTerrainVisualField
{
public:
    bool Initialize(
        const FSphereTopology& InCellTopology,
        const TArray<FCellGeoData>& InGeoCells,
        const FTerrainVisualConfig& InConfig,
        FString& OutError);

    void Reset();

    bool IsInitialized() const { return CellTopology != nullptr; }
    const FTerrainVisualConfig& GetConfig() const { return Config; }

    FTerrainSurfaceQueryResult QueryBaseSurface(const FVector& UnitDirection) const;
    int32 ResolveCellId(const FVector& UnitDirection) const;

private:
    const FSphereTopology* CellTopology = nullptr;
    const TArray<FCellGeoData>* GeoCells = nullptr;
    FTerrainVisualConfig Config;
};
