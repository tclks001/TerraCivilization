// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TerraCivilization : ModuleRules
{
	public TerraCivilization(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Grid", "ProceduralTerrainGenerator", "ProceduralMeshComponent", "WorldGen", "GameplayTags", "TerrainTags", "Gameplay", "PiecePresentation", "NpcMcp", "UMG", "DeveloperSettings" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
