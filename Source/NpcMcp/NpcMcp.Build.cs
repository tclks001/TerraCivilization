using UnrealBuildTool;

public class NpcMcp : ModuleRules
{
    public NpcMcp(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Gameplay",
            "Json",
            "JsonUtilities",
            "ModelContextProtocol"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
        });
    }
}
