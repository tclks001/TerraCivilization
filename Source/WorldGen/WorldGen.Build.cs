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

        // W3 引入：fbm 噪声采样（FastNoiseLite C++ 库，已在 Plugins/ProceduralTerrainGenerator/Source/ThirdParty）。
        // 直接依赖最纯净；不引入 PTG 主模块（避免 UObject 包装类污染 WorldGen）。
        PrivateDependencyModuleNames.AddRange(new string[] { "FastNoiseLite" });
    }
}
