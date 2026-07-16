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
        const TArray<FIntPoint>& InMountainRidgeSegments,
        const FTerrainVisualConfig& InConfig,
        FString& OutError);

    void Reset();

    bool IsInitialized() const { return CellTopology != nullptr; }
    const FTerrainVisualConfig& GetConfig() const { return Config; }

    FTerrainSurfaceQueryResult QueryBaseSurface(const FVector& UnitDirection) const;
    int32 ResolveCellId(const FVector& UnitDirection) const;

private:
    struct FLandformSource
    {
        FVector UnitCenter = FVector::ZeroVector;
        float SupportRadians = 0.0f;
    };

    struct FRidgeSegment
    {
        FVector StartUnit = FVector::ZeroVector;
        FVector EndUnit = FVector::ZeroVector;
        float SupportRadians = 0.0f;
    };

    float EvaluateMacroHeightCM_(const FVector& UnitDirection) const;
    float EvaluateMountainRidgeHeightCM_(const FVector& UnitDirection) const;
    float EvaluateForestHeightCM_(const FVector& UnitDirection) const;
    static float DistanceToGreatCircleArcRadians_(const FVector& UnitDirection, const FRidgeSegment& Segment);
    static float EvaluateNormalizedSigmoid_(float Interior, float Steepness);
    float EvaluateSurfaceRadiusCM_(const FVector& UnitDirection) const;
    FVector EvaluateSurfaceNormal_(const FVector& UnitDirection) const;

    const FSphereTopology* CellTopology = nullptr;
    const TArray<FCellGeoData>* GeoCells = nullptr;
    FTerrainVisualConfig Config;
    TArray<FRidgeSegment> MountainRidgeSegments;
    TArray<FLandformSource> ForestSources;
};
