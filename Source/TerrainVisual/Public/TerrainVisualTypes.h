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

    // SV4.5: deterministic medium-frequency erosion detail.
    bool bEnableMediumFrequencyErosion = true;
    float CrestNoiseAmplitudeCM = 420.0f;
    float ErosionAmplitudeCM = 260.0f;
    float LowlandNoiseAmplitudeCM = 55.0f;
    float MediumFrequencyNoiseFrequency = 5.5f;
    float ErosionDomainWarpAmplitude = 0.42f;
    float ErosionValleySharpness = 2.2f;

    // SV5: visual-only river DAG and material SDF parameters.
    bool bEnableDecorativeRivers = true;
    int32 RiverSourceCount = 6;
    int32 RiverMaxPathCells = 48;
    int32 RiverTerminalBasinCount = 2;
    int32 RiverMinPathCells = 6;
    int32 RiverTerminalLakeMinDischarge = 2;
    float RiverMinWidthCM = 70.0f;
    float RiverWidthScaleCM = 45.0f;
    float RiverLengthWidthGrowthCM = 8.0f;
    float RiverLakeLengthMultiplier = 3.2f;
    float RiverLakeWidthMultiplier = 2.4f;
    float RiverMaxLakeRadiusFraction = 0.25f;
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
