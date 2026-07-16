#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "TerrainVisualSurfaceComponent.generated.h"

class FSphereTopology;
struct FCellGeoData;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;

UCLASS(ClassGroup=(TerrainVisual), meta=(BlueprintSpawnableComponent))
class TERRAINVISUAL_API UTerrainVisualSurfaceComponent : public UProceduralMeshComponent
{
    GENERATED_BODY()

public:
    UTerrainVisualSurfaceComponent(const FObjectInitializer& ObjectInitializer);

    bool RebuildBaseSphere(
        const FSphereTopology& SurfaceTopology,
        const FSphereTopology& CellTopology,
        float RadiusCM);
    void ClearSurface();

    bool InitializeHighlightResources(const FSphereTopology& CellTopology);
    bool InitializeTerrainResources(const TArray<FCellGeoData>& GeoCells, int32 VisualSeed);
    void SetHighlightMaterial(UMaterialInterface* InMaterial);
    void SetHighlightParameters(
        const FVector& PlanetCenter,
        const FLinearColor& BaseGroundColor,
        float HighlightPaddingRad,
        float HighlightStrength);
    bool WriteHighlightCell(int32 CellId, const FLinearColor& Color, float Intensity);
    void ClearHighlightCells();

    bool HasBuiltSurface() const { return bHasBuiltSurface; }
    int32 GetBuiltSubdivisionLevel() const { return BuiltSubdivisionLevel; }
    bool HasHighlightResources() const { return SurfaceCellDirectionLUT != nullptr && SurfaceHighlightLUT != nullptr; }

private:
    bool bHasBuiltSurface = false;
    int32 BuiltSubdivisionLevel = INDEX_NONE;
    int32 HighlightCellCount = 0;
    TArray<FColor> HighlightPixels;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> SurfaceCellDirectionLUT;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> SurfaceHighlightLUT;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> SurfaceTerrainLUT;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> HighlightMID;

    void UploadHighlightTexture_(int32 CellId = INDEX_NONE);
    bool FindLogicalCellsForSurfaceTriangle_(
        const FSphereTopology& SurfaceTopology,
        const FSphereTopology& CellTopology,
        int32 SurfaceTriangleId,
        int32& OutCell0,
        int32& OutCell1,
        int32& OutCell2) const;
};
