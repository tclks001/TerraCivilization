#include "TerrainVisualCoordinator.h"

#include "CellGeoData.h"
#include "FSphereTopology.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerrainVisual, Log, All);

bool FTerrainVisualCoordinator::Initialize(
    const FSphereTopology& InCellTopology,
    const TArray<FCellGeoData>& InGeoCells,
    const TArray<FIntPoint>& InMountainRidgeSegments,
    const FTerrainVisualConfig& InConfig,
    FString& OutError)
{
    Reset();

    Diagnostics.RequestedMode = InConfig.VisualMode;
    Diagnostics.CellCount = InCellTopology.Cells.Num();
    Diagnostics.GeoCellCount = InGeoCells.Num();
    Diagnostics.ContinuousSurfaceStatus = TEXT("SV1 continuous surface component is not implemented.");

    if (!VisualField.Initialize(InCellTopology, InGeoCells, InMountainRidgeSegments, InConfig, OutError))
    {
        Diagnostics.ContinuousSurfaceStatus = FString::Printf(
            TEXT("TerrainVisual initialization failed: %s"),
            *OutError);
        return false;
    }

    Diagnostics.bInitialized = true;
    Diagnostics.bCanActivateContinuousSurface = false;
    if (InConfig.bEnableDiagnostics)
    {
        UE_LOG(LogTerrainVisual, Log,
            TEXT("[TerrainVisual][SV0] Initialized Mode=%d Cells=%d GeoCells=%d SurfaceSub=%d Radius=%.1fcm"),
            static_cast<int32>(Diagnostics.RequestedMode),
            Diagnostics.CellCount,
            Diagnostics.GeoCellCount,
            InConfig.SurfaceSubdivisionLevel,
            InConfig.GlobeRadiusCM);
    }
    return true;
}

void FTerrainVisualCoordinator::Reset()
{
    VisualField.Reset();
    Diagnostics = FTerrainVisualDiagnostics();
}

bool FTerrainVisualCoordinator::CanActivateContinuousSurface(FString& OutReason) const
{
    OutReason = Diagnostics.ContinuousSurfaceStatus;
    return Diagnostics.bCanActivateContinuousSurface;
}

FTerrainSurfaceQueryResult FTerrainVisualCoordinator::QueryBaseSurface(const FVector& UnitDirection) const
{
    FTerrainSurfaceQueryResult Result = VisualField.QueryBaseSurface(UnitDirection);
    Result.bHasContinuousSurface = Diagnostics.bCanActivateContinuousSurface;
    return Result;
}

int32 FTerrainVisualCoordinator::ResolveCellId(const FVector& UnitDirection) const
{
    return VisualField.ResolveCellId(UnitDirection);
}

void FTerrainVisualCoordinator::SetContinuousSurfaceAvailable(bool bAvailable)
{
    Diagnostics.bCanActivateContinuousSurface = bAvailable;
    Diagnostics.ContinuousSurfaceStatus = bAvailable
        ? TEXT("SV1 continuous surface is available.")
        : TEXT("SV1 continuous surface component is not implemented.");
}
