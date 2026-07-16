#include "TerrainVisualField.h"

#include "CellGeoData.h"
#include "FSphereTopology.h"
#include "FSphereTopologyQuery.h"

bool FTerrainVisualField::Initialize(
    const FSphereTopology& InCellTopology,
    const TArray<FCellGeoData>& InGeoCells,
    const FTerrainVisualConfig& InConfig,
    FString& OutError)
{
    Reset();
    OutError.Reset();

    const int32 CellCount = InCellTopology.Cells.Num();
    if (CellCount <= 0)
    {
        OutError = TEXT("TerrainVisual requires a built CellTopology with at least one Cell.");
        return false;
    }
    if (InGeoCells.Num() != CellCount)
    {
        OutError = FString::Printf(
            TEXT("TerrainVisual GeoCells count mismatch. GeoCells=%d CellTopology=%d."),
            InGeoCells.Num(),
            CellCount);
        return false;
    }
    if (InConfig.GlobeRadiusCM <= KINDA_SMALL_NUMBER)
    {
        OutError = FString::Printf(
            TEXT("TerrainVisual GlobeRadiusCM must be positive. Got=%.3f."),
            InConfig.GlobeRadiusCM);
        return false;
    }

    CellTopology = &InCellTopology;
    GeoCells = &InGeoCells;
    Config = InConfig;
    return true;
}

void FTerrainVisualField::Reset()
{
    CellTopology = nullptr;
    GeoCells = nullptr;
    Config = FTerrainVisualConfig();
}

FTerrainSurfaceQueryResult FTerrainVisualField::QueryBaseSurface(const FVector& UnitDirection) const
{
    FTerrainSurfaceQueryResult Result;
    if (!IsInitialized())
    {
        return Result;
    }

    const FVector SafeDirection = UnitDirection.GetSafeNormal();
    if (SafeDirection.IsNearlyZero())
    {
        return Result;
    }

    Result.WorldPosition = Config.PlanetCenterWorld + SafeDirection * Config.GlobeRadiusCM;
    Result.WorldNormal = SafeDirection;
    Result.SurfaceRadiusCM = Config.GlobeRadiusCM;
    Result.bIsValid = true;
    Result.bHasContinuousSurface = false;
    return Result;
}

int32 FTerrainVisualField::ResolveCellId(const FVector& UnitDirection) const
{
    const FVector SafeDirection = UnitDirection.GetSafeNormal();
    if (!CellTopology || SafeDirection.IsNearlyZero())
    {
        return INDEX_NONE;
    }

    const FSphereTopologyQuery Query(CellTopology);
    return Query.FindNearestCell(SafeDirection).CellId;
}
