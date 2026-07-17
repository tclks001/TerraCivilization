#include "TerraTerrainDecorGenerator.h"

#include "Framework/Docking/TabManager.h"
#include "STerraTerrainDecorGeneratorPanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FTerraTerrainDecorGeneratorModule"

namespace TerraTerrainDecorGeneratorModule
{
	const FName GeneratorTabName(TEXT("TerraTerrainDecorGenerator"));
}

void FTerraTerrainDecorGeneratorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TerraTerrainDecorGeneratorModule::GeneratorTabName,
		FOnSpawnTab::CreateRaw(this, &FTerraTerrainDecorGeneratorModule::SpawnGeneratorTab))
		.SetDisplayName(LOCTEXT("GeneratorTabTitle", "Terra Terrain Decor Generator"))
		.SetTooltipText(LOCTEXT("GeneratorTabTooltip", "Generate Nanite-ready terrain decor StaticMesh assets."))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTerraTerrainDecorGeneratorModule::RegisterMenus));
}

void FTerraTerrainDecorGeneratorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TerraTerrainDecorGeneratorModule::GeneratorTabName);
}

void FTerraTerrainDecorGeneratorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("TerraCivilization"));
	Section.AddMenuEntry(
		TEXT("OpenTerraTerrainDecorGenerator"),
		LOCTEXT("OpenGeneratorLabel", "Terra Terrain Decor Generator"),
		LOCTEXT("OpenGeneratorTooltip", "Open the terrain decor asset generation panel."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]
		{
			FGlobalTabmanager::Get()->TryInvokeTab(TerraTerrainDecorGeneratorModule::GeneratorTabName);
		})));
}

TSharedRef<SDockTab> FTerraTerrainDecorGeneratorModule::SpawnGeneratorTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STerraTerrainDecorGeneratorPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTerraTerrainDecorGeneratorModule, TerraTerrainDecorGenerator)
