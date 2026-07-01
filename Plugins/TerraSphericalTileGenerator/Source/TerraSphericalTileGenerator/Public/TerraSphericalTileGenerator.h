#pragma once

#include "Modules/ModuleManager.h"

class FTerraSphericalTileGeneratorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
