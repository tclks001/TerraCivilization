using UnrealBuildTool;

public class PiecePresentation : ModuleRules
{
    public PiecePresentation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Gameplay"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}
