#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerraSphericalTileGeneratorLibrary.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class ETSTGAssetShape : uint8
{
	Tile UMETA(DisplayName = "Spherical Tile"),
	Ridge UMETA(DisplayName = "Ridge"),
	Peak UMETA(DisplayName = "Peak")
};

USTRUCT(BlueprintType, meta = (DisplayName = "FTSTGTextureParameterNames"))
struct TERRASPHERICALTILEGENERATOR_API FTSTGTextureParameterNames
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Spherical Tile Generator")
	FName BaseColor = TEXT("BaseColorTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Spherical Tile Generator")
	FName Normal = TEXT("NormalTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Spherical Tile Generator")
	FName Roughness = TEXT("RoughnessTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terra|Spherical Tile Generator")
	FName Height = TEXT("HeightTexture");
};

USTRUCT(BlueprintType, meta = (DisplayName = "FTSTGSphericalTileAssetBuildSettings"))
struct TERRASPHERICALTILEGENERATOR_API FTSTGSphericalTileAssetBuildSettings
{
	GENERATED_BODY()

	/** Selects the generated spherical terrain asset shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
	ETSTGAssetShape AssetShape = ETSTGAssetShape::Tile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBR")
	TObjectPtr<UTexture2D> BaseColorTexture = nullptr;

	/** Tangent-space normal map. Only NormalDX assets are supported; import as Normalmap with sRGB disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBR", meta = (ToolTip = "Only NormalDX tangent-space normal maps are supported. Import as Normalmap with sRGB disabled."))
	TObjectPtr<UTexture2D> NormalTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBR")
	TObjectPtr<UTexture2D> HeightTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PBR")
	TObjectPtr<UTexture2D> RoughnessTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh", meta = (ClampMin = "1", ClampMax = "512"))
	int32 SubdivisionsPerSide = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (ClampMin = "0.001"))
	float BaseRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	float SphereExtensionAmplitude = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	float HeightmapExtensionAmplitude = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (ClampMin = "0.1", ClampMax = "179.0"))
	float PatchAngularSizeDegrees = 18.0f;

	/** Stable seed reserved for reproducible Ridge/Peak shape variants. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape|Variation")
	int32 ShapeVariationSeed = 1337;

	/** Multiplicative analytic shape noise. Keep zero for the single-asset SV9 baseline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape|Variation", meta = (ClampMin = "0.0", ClampMax = "0.8"))
	float ShapeNoiseStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape|Variation", meta = (ClampMin = "0.25", ClampMax = "32.0"))
	float ShapeNoiseFrequency = 3.5f;

	/** Ridge height above the spherical base. Ridge local +X is the crest direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Shape", meta = (EditCondition = "AssetShape == ETSTGAssetShape::Ridge", EditConditionHides, ClampMin = "0.0", ClampMax = "10000.0"))
	float RidgeHeightCM = 42.0f;

	/** Higher values make the ridge crest narrower and sharper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Shape", meta = (EditCondition = "AssetShape == ETSTGAssetShape::Ridge", EditConditionHides, ClampMin = "0.25", ClampMax = "16.0"))
	float RidgeProfileExponent = 2.5f;

	/** Optional taper at the local X ends. Zero keeps the ends elevated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Shape", meta = (EditCondition = "AssetShape == ETSTGAssetShape::Ridge", EditConditionHides, ClampMin = "0.0", ClampMax = "0.49"))
	float RidgeEndTaperFraction = 0.0f;

	/** Low-frequency sideways bend of the ridge crest in local Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Shape", meta = (EditCondition = "AssetShape == ETSTGAssetShape::Ridge", EditConditionHides, ClampMin = "0.0", ClampMax = "1.0"))
	float RidgeCenterlineWaviness = 0.08f;

	/** Height texture displacement is multiplied by the ridge profile, so Y edges remain grounded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Shape", meta = (EditCondition = "AssetShape == ETSTGAssetShape::Ridge", EditConditionHides, ClampMin = "0.0", ClampMax = "5000.0"))
	float RidgeHeightmapAmplitudeCM = 10.0f;

	/** Peak height above the spherical base. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peak|Shape", meta = (EditCondition = "AssetShape == ETSTGAssetShape::Peak", EditConditionHides, ClampMin = "0.0", ClampMax = "10000.0"))
	float PeakHeightCM = 55.0f;

	/** Higher values make the circular cone narrower. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peak|Shape", meta = (EditCondition = "AssetShape == ETSTGAssetShape::Peak", EditConditionHides, ClampMin = "0.25", ClampMax = "16.0"))
	float PeakProfileExponent = 2.0f;

	/** Centered height texture displacement, masked to the grounded circular footprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peak|Shape", meta = (EditCondition = "AssetShape == ETSTGAssetShape::Peak", EditConditionHides, ClampMin = "0.0", ClampMax = "5000.0"))
	float PeakHeightmapAmplitudeCM = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
	FString StaticMeshAssetPathAndName = TEXT("/Game/Generated/SphericalTiles/SM_SphericalTile");

	/** Convenience output used by the generator panel for Ridge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset|SV9 Ridge", meta = (EditCondition = "AssetShape == ETSTGAssetShape::Ridge", EditConditionHides))
	FString RidgeStaticMeshAssetPathAndName = TEXT("/Game/Generated/SphericalTiles/SV9/SM_SV9_Ridge");

	/** Convenience output used by the generator panel for Peak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset|SV9 Peak", meta = (EditCondition = "AssetShape == ETSTGAssetShape::Peak", EditConditionHides))
	FString PeakStaticMeshAssetPathAndName = TEXT("/Game/Generated/SphericalTiles/SV9/SM_SV9_Peak");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	bool bCreateMaterialInstance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (EditCondition = "bCreateMaterialInstance"))
	TObjectPtr<UMaterialInterface> ParentMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (EditCondition = "bCreateMaterialInstance"))
	FString MaterialInstanceAssetPathAndName = TEXT("/Game/Generated/SphericalTiles/MI_SphericalTile");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material|SV9 Ridge", meta = (EditCondition = "bCreateMaterialInstance && AssetShape == ETSTGAssetShape::Ridge", EditConditionHides))
	FString RidgeMaterialInstanceAssetPathAndName = TEXT("/Game/Generated/SphericalTiles/SV9/MI_SV9_Ridge");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material|SV9 Peak", meta = (EditCondition = "bCreateMaterialInstance && AssetShape == ETSTGAssetShape::Peak", EditConditionHides))
	FString PeakMaterialInstanceAssetPathAndName = TEXT("/Game/Generated/SphericalTiles/SV9/MI_SV9_Peak");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (EditCondition = "bCreateMaterialInstance"))
	FTSTGTextureParameterNames TextureParameterNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Mesh")
	bool bEnableCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Mesh")
	bool bEnableNanite = false;
};

UCLASS()
class TERRASPHERICALTILEGENERATOR_API UTerraSphericalTileGeneratorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Terra|Spherical Tile Generator")
	static UStaticMesh* GenerateSphericalTerrainStaticMeshAsset(
		const FTSTGSphericalTileAssetBuildSettings& Settings,
		FString& OutErrorMessage);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Terra|Spherical Tile Generator")
	static UStaticMesh* GenerateSphericalTileStaticMeshAsset(
		const FTSTGSphericalTileAssetBuildSettings& Settings,
		FString& OutErrorMessage);
};
