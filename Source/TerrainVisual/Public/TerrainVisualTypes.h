#pragma once

#include "CoreMinimal.h"
#include "TerrainVisualTypes.generated.h"

UENUM(BlueprintType)
enum class ETerrainVisualMode : uint8
{
    LegacyHISMDebug,
    ContinuousSurface,
};

struct TERRAINVISUAL_API FTerrainVisualConfig
{
    ETerrainVisualMode VisualMode = ETerrainVisualMode::LegacyHISMDebug;
    int32 SurfaceSubdivisionLevel = 7;
    float GlobeRadiusCM = 15000.0f;
    int32 GlobalVisualSeed = 0;
    bool bEnableDiagnostics = false;
    FVector PlanetCenterWorld = FVector::ZeroVector;

    // SV4: deterministic, visual-only macro landform parameters.
    float MountainHeightCM = 2400.0f;
    float MountainFalloffExponent = 3.0f;
    float ForestHeightCM = 500.0f;
    float ForestSigmoidSteepness = 8.0f;
    float LandformSupportScale = 1.25f;
};

struct TERRAINVISUAL_API FTerrainSurfaceQueryResult
{
    FVector WorldPosition = FVector::ZeroVector;
    FVector WorldNormal = FVector::UpVector;
    float SurfaceRadiusCM = 0.0f;
    int32 CellId = INDEX_NONE;
    bool bIsValid = false;
    bool bHasContinuousSurface = false;
};

struct TERRAINVISUAL_API FTerrainVisualDiagnostics
{
    bool bInitialized = false;
    ETerrainVisualMode RequestedMode = ETerrainVisualMode::LegacyHISMDebug;
    bool bCanActivateContinuousSurface = false;
    int32 CellCount = 0;
    int32 GeoCellCount = 0;
    FString ContinuousSurfaceStatus;
};
