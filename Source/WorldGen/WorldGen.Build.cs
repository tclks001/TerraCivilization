// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorldGen : ModuleRules
{
    public WorldGen(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Grid",
            "GameplayTags",
            "TerrainTags"
        });
    }
}
