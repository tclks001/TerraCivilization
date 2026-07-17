#pragma once

#include "Modules/ModuleManager.h"

class SDockTab;
class SWidget;
class FSpawnTabArgs;

class FTerraTerrainDecorGeneratorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	TSharedRef<SDockTab> SpawnGeneratorTab(const FSpawnTabArgs& Args);
};
