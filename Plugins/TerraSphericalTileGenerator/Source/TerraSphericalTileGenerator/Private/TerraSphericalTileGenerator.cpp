#include "TerraSphericalTileGenerator.h"

#include "Framework/Docking/TabManager.h"
#include "STerraSphericalTileGeneratorPanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FTerraSphericalTileGeneratorModule"

namespace TerraSphericalTileGeneratorModule
{
	const FName GeneratorTabName(TEXT("TerraSphericalTileGenerator"));
}

void FTerraSphericalTileGeneratorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TerraSphericalTileGeneratorModule::GeneratorTabName,
		FOnSpawnTab::CreateRaw(this, &FTerraSphericalTileGeneratorModule::SpawnGeneratorTab))
		.SetDisplayName(LOCTEXT("GeneratorTabTitle", "Terra Spherical Terrain Asset Generator"))
		.SetTooltipText(LOCTEXT("GeneratorTabTooltip", "Generate spherical Tile, Ridge, and Peak StaticMesh assets."))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTerraSphericalTileGeneratorModule::RegisterMenus));
}

void FTerraSphericalTileGeneratorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TerraSphericalTileGeneratorModule::GeneratorTabName);
}

void FTerraSphericalTileGeneratorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("TerraCivilization"));
	Section.AddMenuEntry(
		TEXT("OpenTerraSphericalTileGenerator"),
		LOCTEXT("OpenGeneratorLabel", "Terra Spherical Terrain Asset Generator"),
		LOCTEXT("OpenGeneratorTooltip", "Open the spherical Tile, Ridge, and Peak asset generation panel."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]
		{
			FGlobalTabmanager::Get()->TryInvokeTab(TerraSphericalTileGeneratorModule::GeneratorTabName);
		})));
}

TSharedRef<SDockTab> FTerraSphericalTileGeneratorModule::SpawnGeneratorTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STerraSphericalTileGeneratorPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTerraSphericalTileGeneratorModule, TerraSphericalTileGenerator)
