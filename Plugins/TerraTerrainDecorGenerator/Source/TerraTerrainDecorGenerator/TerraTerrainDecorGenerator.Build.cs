using UnrealBuildTool;

public class TerraTerrainDecorGenerator : ModuleRules
{
	public TerraTerrainDecorGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"AssetRegistry",
				"GeometryCore",
				"GeometryFramework",
				"GeometryScriptingCore",
				"GeometryScriptingEditor",
				"MaterialEditor",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"ToolMenus"
			});
	}
}
