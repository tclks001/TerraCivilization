#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TerraSphericalTileGeneratorLibrary.h"
#include "TerraSphericalTileGeneratorSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings)
class TERRASPHERICALTILEGENERATOR_API UTerraSphericalTileGeneratorSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Spherical Terrain Asset Generator", meta = (ShowOnlyInnerProperties))
	FTSTGSphericalTileAssetBuildSettings BuildSettings;
};
