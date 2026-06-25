// Copyright 2021 VICTOR HERNANDEZ MOLPECERES (Rockam). All rights reserved.

using UnrealBuildTool;

public class ProceduralTerrainGeneratorEditor : ModuleRules
{
	public ProceduralTerrainGeneratorEditor(ReadOnlyTargetRules Target) : base(Target)
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
                "Core"
            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "CoreUObject",
                "Engine",
                "Slate",
				"SlateCore",
                "UnrealEd",
                "RawMesh",
                "AssetTools",
                "AssetRegistry",
                "RuntimeMeshComponent",
                "ProceduralTerrainGenerator",
                "DesktopPlatform"
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
