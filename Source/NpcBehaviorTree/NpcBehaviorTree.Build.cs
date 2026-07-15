using UnrealBuildTool;

public class NpcBehaviorTree : ModuleRules
{
    public NpcBehaviorTree(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AIModule",
            "GameplayTasks",
            "Gameplay"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "TerraCivilization"
        });
    }
}
