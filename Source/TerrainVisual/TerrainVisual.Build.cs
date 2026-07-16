using UnrealBuildTool;

public class TerrainVisual : ModuleRules
{
    public TerrainVisual(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Grid",
            "WorldGen",
            "ProceduralMeshComponent"
        });
    }
}
