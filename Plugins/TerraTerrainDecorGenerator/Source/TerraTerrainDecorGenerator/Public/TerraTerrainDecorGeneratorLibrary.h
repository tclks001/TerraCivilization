#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerraTerrainDecorGeneratorTypes.h"
#include "TerraTerrainDecorGeneratorLibrary.generated.h"

class UStaticMesh;

UCLASS()
class TERRATERRAINDECORGENERATOR_API UTerraTerrainDecorGeneratorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Terra|Terrain Decor Generator")
	static UStaticMesh* GenerateRidgeStaticMeshAsset(
		const FTTDGRidgeAssetBuildSettings& Settings,
		int32 VariantIndex,
		FString& OutMessage);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Terra|Terrain Decor Generator")
	static bool GenerateRidgeStaticMeshAssets(
		const FTTDGRidgeAssetBuildSettings& Settings,
		TArray<UStaticMesh*>& OutAssets,
		FString& OutMessage);
};
