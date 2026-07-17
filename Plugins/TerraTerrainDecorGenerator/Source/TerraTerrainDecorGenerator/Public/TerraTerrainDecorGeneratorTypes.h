#pragma once

#include "CoreMinimal.h"
#include "TerraTerrainDecorGeneratorTypes.generated.h"

class UMaterialInterface;
class UTexture2D;

UENUM(BlueprintType)
enum class ETTDGDecorAssetType : uint8
{
	Ridge UMETA(DisplayName = "Ridge"),
	Peak UMETA(DisplayName = "Peak (Reserved)"),
	Cliff UMETA(DisplayName = "Cliff (Reserved)"),
	Boulder UMETA(DisplayName = "Boulder (Reserved)")
};

USTRUCT(BlueprintType)
struct TERRATERRAINDECORGENERATOR_API FTTDGTextureParameterNames
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FName BaseColor = TEXT("BaseColorTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FName Normal = TEXT("NormalTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FName Roughness = TEXT("RoughnessTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FName Height = TEXT("HeightTexture");
};

USTRUCT(BlueprintType)
struct TERRATERRAINDECORGENERATOR_API FTTDGMaterialSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TObjectPtr<UTexture2D> BaseColorTexture = nullptr;

	/** Only tangent-space NormalDX textures are supported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TObjectPtr<UTexture2D> NormalTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TObjectPtr<UTexture2D> RoughnessTexture = nullptr;

	/** Optional source height map used both by the material and low-amplitude vertex displacement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TObjectPtr<UTexture2D> HeightTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	bool bCreateMaterialInstance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (EditCondition = "bCreateMaterialInstance"))
	TObjectPtr<UMaterialInterface> ParentMaterial = nullptr;

	/** One shared material instance is assigned to every generated variant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (EditCondition = "bCreateMaterialInstance"))
	FString MaterialInstanceAssetPathAndName = TEXT("/Game/Generated/TerrainDecor/Ridges/MI_Ridge_Rock");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (EditCondition = "bCreateMaterialInstance"))
	FTTDGTextureParameterNames TextureParameterNames;
};

USTRUCT(BlueprintType)
struct TERRATERRAINDECORGENERATOR_API FTTDGRidgeShapeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Dimensions", meta = (ClampMin = "10.0"))
	float LengthCM = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Dimensions", meta = (ClampMin = "10.0"))
	float WidthCM = 480.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Dimensions", meta = (ClampMin = "1.0"))
	float HeightCM = 520.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Dimensions", meta = (ClampMin = "0.0"))
	float SkirtDepthCM = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Profile", meta = (ClampMin = "0.25", ClampMax = "8.0"))
	float CrossSectionSharpness = 2.4f;

	/** Crest displacement relative to half width. Positive values move it toward local +Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Profile", meta = (ClampMin = "-0.65", ClampMax = "0.65"))
	float CrestOffsetNormalized = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Profile", meta = (ClampMin = "0.0", ClampMax = "0.45"))
	float CrestOffsetVariation = 0.14f;

	/** Fraction of the ridge length used to blend each end down into the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Profile", meta = (ClampMin = "0.01", ClampMax = "0.49"))
	float EndTaperFraction = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Variation", meta = (ClampMin = "0.0"))
	float CenterlineWavinessCM = 130.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Variation", meta = (ClampMin = "1.0"))
	float CenterlineWavelengthCM = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Variation", meta = (ClampMin = "0.0", ClampMax = "0.8"))
	float WidthVariation = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Variation", meta = (ClampMin = "0.0", ClampMax = "0.8"))
	float HeightVariation = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Erosion", meta = (ClampMin = "0.0"))
	float ErosionStrengthCM = 42.0f;

	/** Number of broad erosion cycles over the full ridge length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Erosion", meta = (ClampMin = "0.1", ClampMax = "32.0"))
	float ErosionFrequency = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Erosion", meta = (ClampMin = "1", ClampMax = "8"))
	int32 ErosionOctaves = 4;

	/** Optional mesh displacement from HeightTexture, centered around texture value 0.5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Erosion", meta = (ClampMin = "0.0"))
	float HeightTextureDisplacementCM = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Mesh", meta = (ClampMin = "8", ClampMax = "512"))
	int32 LengthSegments = 192;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Mesh", meta = (ClampMin = "4", ClampMax = "256"))
	int32 WidthSegments = 48;

	/** UV repetitions over the ridge length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge|Mesh", meta = (ClampMin = "0.1", ClampMax = "64.0"))
	float TextureTiling = 6.0f;
};

USTRUCT(BlueprintType)
struct TERRATERRAINDECORGENERATOR_API FTTDGAssetOutputSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	FString OutputDirectory = TEXT("/Game/Generated/TerrainDecor/Ridges");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	FString StaticMeshNamePrefix = TEXT("SM_Ridge");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (ClampMin = "0"))
	int32 FirstVariantIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (ClampMin = "1", ClampMax = "128"))
	int32 VariantCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	int32 BaseSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (ClampMin = "1"))
	int32 SeedStride = 977;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bEnableNanite = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bEnableCollision = false;
};

USTRUCT(BlueprintType)
struct TERRATERRAINDECORGENERATOR_API FTTDGRidgeAssetBuildSettings
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generator")
	ETTDGDecorAssetType AssetType = ETTDGDecorAssetType::Ridge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
	FTTDGRidgeShapeSettings Ridge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
	FTTDGMaterialSettings Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
	FTTDGAssetOutputSettings Output;
};
