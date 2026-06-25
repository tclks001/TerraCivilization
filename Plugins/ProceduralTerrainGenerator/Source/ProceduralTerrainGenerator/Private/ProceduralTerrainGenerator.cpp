// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All rights reserved.

#include "ProceduralTerrainGenerator.h"

DEFINE_LOG_CATEGORY(LogProceduralTerrainGenerator);

#define LOCTEXT_NAMESPACE "FProceduralTerrainGeneratorModule"

void FProceduralTerrainGeneratorModule::StartupModule(){}

void FProceduralTerrainGeneratorModule::ShutdownModule(){}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FProceduralTerrainGeneratorModule, ProceduralTerrainGenerator)