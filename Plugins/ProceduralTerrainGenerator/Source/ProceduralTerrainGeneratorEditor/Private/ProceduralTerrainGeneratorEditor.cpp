// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All rights reserved.

#include "ProceduralTerrainGeneratorEditor.h"
#include "PropertyEditorModule.h"
#include "PtgManager.h"
#include "PTG_EditorDetails.h"
#include "PtgUtils.h"

void FProceduralTerrainGeneratorEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(APtgManager::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FPTG_EditorDetails::MakeInstance));
	UPtgUtils::PrintDebugMessage(nullptr, TEXT("ProceduralTerrainGeneratorEditor module started and custom layout registered."), EPtgDebugMessageTypes::Info, 5.0f);
}

void FProceduralTerrainGeneratorEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(APtgManager::StaticClass()->GetFName());
		UPtgUtils::PrintDebugMessage(nullptr, TEXT("ProceduralTerrainGeneratorEditor module shutdown and custom layout unregistered."), EPtgDebugMessageTypes::Info, 5.0f);
	}
}

IMPLEMENT_MODULE(FProceduralTerrainGeneratorEditorModule, ProceduralTerrainGeneratorEditor)
