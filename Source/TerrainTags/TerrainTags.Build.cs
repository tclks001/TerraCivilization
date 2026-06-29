// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TerrainTags : ModuleRules
{
    public TerrainTags(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags"
        });
    }
}
