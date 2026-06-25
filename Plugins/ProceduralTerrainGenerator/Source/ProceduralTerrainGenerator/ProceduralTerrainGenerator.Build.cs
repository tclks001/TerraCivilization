// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All rights reserved.

using UnrealBuildTool;

public class ProceduralTerrainGenerator : ModuleRules
{
	public ProceduralTerrainGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// PtgProcMeshDataHelper.h 这种 Public 头使用了 RuntimeMeshCore.h / FRuntimeMeshTangent，
				// 必须以 Public 依赖的方式向下游模块（如 TerraCivilization）传递 include path，
				// 否则下游 #include "PtgProcMeshDataHelper.h" 时会报找不到 RuntimeMeshCore.h。
				"RuntimeMeshComponent"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"FastNoiseLite"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
