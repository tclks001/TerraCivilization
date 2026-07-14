#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerraSphericalTileGeneratorLibrary.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
	FString StaticMeshAssetPathAndName = TEXT("/Game/Generated/SphericalTiles/SM_SphericalTile");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	bool bCreateMaterialInstance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (EditCondition = "bCreateMaterialInstance"))
	TObjectPtr<UMaterialInterface> ParentMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (EditCondition = "bCreateMaterialInstance"))
	FString MaterialInstanceAssetPathAndName = TEXT("/Game/Generated/SphericalTiles/MI_SphericalTile");

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
	static UStaticMesh* GenerateSphericalTileStaticMeshAsset(
		const FTSTGSphericalTileAssetBuildSettings& Settings,
		FString& OutErrorMessage);
};
