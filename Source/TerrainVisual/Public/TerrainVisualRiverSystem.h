#pragma once

#include "CoreMinimal.h"

class FSphereTopology;
class ITerrainSurfaceQuery;
struct FTerrainVisualConfig;
struct FCellGeoData;

struct FTerrainVisualRiverSegment
{
    FVector StartUnit = FVector::ZeroVector;
    FVector EndUnit = FVector::ZeroVector;
    float StartWidthRad = 0.0f;
    float EndWidthRad = 0.0f;
};

struct FTerrainVisualRiverLake
{
    FVector CenterUnit = FVector::ZeroVector;
    FVector FlowAxisUnit = FVector::ForwardVector;
    float RadiusAlongRad = 0.0f;
    float RadiusAcrossRad = 0.0f;
};

class TERRAINVISUAL_API FTerrainVisualRiverSystem
{
public:
    bool Build(const FSphereTopology& Topology, const TArray<FCellGeoData>& GeoCells, const ITerrainSurfaceQuery& SurfaceQuery, const FTerrainVisualConfig& Config);
    void Reset();

    const TArray<FTerrainVisualRiverSegment>& GetSegments() const { return Segments; }
    const TArray<FTerrainVisualRiverLake>& GetTerminalLakes() const { return TerminalLakes; }

private:
    TArray<FTerrainVisualRiverSegment> Segments;
    TArray<FTerrainVisualRiverLake> TerminalLakes;
};
