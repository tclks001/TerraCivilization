#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "TerrainVisualSurfaceComponent.generated.h"

class FSphereTopology;
struct FCellGeoData;
class ITerrainSurfaceQuery;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class FTerrainVisualRiverSystem;

UCLASS(ClassGroup=(TerrainVisual), meta=(BlueprintSpawnableComponent))
class TERRAINVISUAL_API UTerrainVisualSurfaceComponent : public UProceduralMeshComponent
{
    GENERATED_BODY()

public:
    UTerrainVisualSurfaceComponent(const FObjectInitializer& ObjectInitializer);

    bool RebuildBaseSphere(
        const FSphereTopology& SurfaceTopology,
        const FSphereTopology& CellTopology,
        const ITerrainSurfaceQuery& SurfaceQuery,
        float RadiusCM);
    void ClearSurface();

    bool InitializeHighlightResources(const FSphereTopology& CellTopology);
    bool InitializeTerrainResources(const TArray<FCellGeoData>& GeoCells, int32 VisualSeed);
    bool InitializeRiverResources(const FTerrainVisualRiverSystem& RiverSystem);
    void SetHighlightMaterial(UMaterialInterface* InMaterial);
    void SetSurfaceEnhancementParameters(
        UTexture2D* GravelColor, UTexture2D* GravelNormal, UTexture2D* GravelRoughness,
        UTexture2D* MossColor, UTexture2D* MossNormal, UTexture2D* MossRoughness,
        UTexture2D* RockColor, UTexture2D* RockNormal, UTexture2D* RockRoughness,
        const FLinearColor& PlainTint, const FLinearColor& ForestTint, const FLinearColor& MountainTint,
        float TileScaleCM, float TriplanarSharpness, float NormalStrength);
    void SetHighlightParameters(
        const FVector& PlanetCenter,
        const FLinearColor& BaseGroundColor,
        float HighlightPaddingRad,
        float HighlightStrength);
    void ApplySharedMaterialParameters(
        UMaterialInstanceDynamic* MaterialInstance,
        const FVector& PlanetCenter,
        const FLinearColor& BaseGroundColor,
        float HighlightPaddingRad,
        float HighlightStrength) const;
    bool WriteHighlightCell(int32 CellId, const FLinearColor& Color, float Intensity);
    void ClearHighlightCells();

    bool HasBuiltSurface() const { return bHasBuiltSurface; }
    int32 GetBuiltSubdivisionLevel() const { return BuiltSubdivisionLevel; }
    bool HasHighlightResources() const { return SurfaceCellDirectionLUT != nullptr && SurfaceHighlightLUT != nullptr; }

private:
    bool bHasBuiltSurface = false;
    int32 BuiltSubdivisionLevel = INDEX_NONE;
    int32 HighlightCellCount = 0;
    int32 RiverSegmentCount = 0;
    int32 RiverLakeCount = 0;
    TArray<FColor> HighlightPixels;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> SurfaceCellDirectionLUT;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> SurfaceHighlightLUT;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> SurfaceTerrainLUT;
    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> SurfaceRiverSegmentLUT;
    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> SurfaceRiverLakeLUT;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> HighlightMID;

    UPROPERTY(Transient) TObjectPtr<UTexture2D> GravelColorTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> GravelNormalTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> GravelRoughnessTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> MossColorTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> MossNormalTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> MossRoughnessTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> RockColorTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> RockNormalTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> RockRoughnessTexture;

    FLinearColor SurfacePlainTint = FLinearColor::White;
    FLinearColor SurfaceForestTint = FLinearColor::White;
    FLinearColor SurfaceMountainTint = FLinearColor::White;
    float SurfaceTileScaleCM = 450.0f;
    float SurfaceTriplanarSharpness = 4.0f;
    float SurfaceNormalStrength = 0.55f;

    void UploadHighlightTexture_(int32 CellId = INDEX_NONE);
    void ApplySurfaceEnhancementParameters_();
    bool FindLogicalCellsForSurfaceTriangle_(
        const FSphereTopology& SurfaceTopology,
        const FSphereTopology& CellTopology,
        int32 SurfaceTriangleId,
        int32& OutCell0,
        int32& OutCell1,
        int32& OutCell2) const;
};
