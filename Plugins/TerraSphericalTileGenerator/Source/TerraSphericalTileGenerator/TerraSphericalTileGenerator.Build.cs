using UnrealBuildTool;

public class TerraSphericalTileGenerator : ModuleRules
{
	public TerraSphericalTileGenerator(ReadOnlyTargetRules Target) : base(Target)
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
				"AssetTools",
				"GeometryCore",
				"GeometryFramework",
				"GeometryScriptingCore",
				"GeometryScriptingEditor",
				"MaterialEditor",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"LevelEditor"
			});
	}
}
