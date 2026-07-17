#pragma once

#include "TerrainSurfaceQuery.h"
#include "TerrainVisualField.h"
#include "TerrainVisualRiverSystem.h"

class FSphereTopology;
struct FCellGeoData;

class TERRAINVISUAL_API FTerrainVisualCoordinator final : public ITerrainSurfaceQuery
{
public:
    bool Initialize(
        const FSphereTopology& InCellTopology,
        const TArray<FCellGeoData>& InGeoCells,
        const TArray<FIntPoint>& InMountainRidgeSegments,
        const FTerrainVisualConfig& InConfig,
        FString& OutError);

    void Reset();

    ETerrainVisualMode GetMode() const { return Diagnostics.RequestedMode; }
    bool CanActivateContinuousSurface(FString& OutReason) const;
    const FTerrainVisualDiagnostics& GetDiagnostics() const { return Diagnostics; }

    virtual FTerrainSurfaceQueryResult QueryBaseSurface(const FVector& UnitDirection) const override;
    int32 ResolveCellId(const FVector& UnitDirection) const;
    void SetContinuousSurfaceAvailable(bool bAvailable);
    const FTerrainVisualRiverSystem& GetRiverSystem() const { return RiverSystem; }

private:
    FTerrainVisualField VisualField;
    FTerrainVisualRiverSystem RiverSystem;
    FTerrainVisualDiagnostics Diagnostics;
};
