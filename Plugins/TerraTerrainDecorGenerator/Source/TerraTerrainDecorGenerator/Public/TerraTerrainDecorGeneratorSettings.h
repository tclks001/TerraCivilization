#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TerraTerrainDecorGeneratorTypes.h"
#include "TerraTerrainDecorGeneratorSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings)
class TERRATERRAINDECORGENERATOR_API UTerraTerrainDecorGeneratorSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Terrain Decor Generator", meta = (ShowOnlyInnerProperties))
	FTTDGRidgeAssetBuildSettings RidgeBuildSettings;
};
