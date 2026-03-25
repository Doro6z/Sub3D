// Copyright (c) 2026 DXV Systems.
using UnrealBuildTool;

public class LevelSwitcher : ModuleRules
{
	public LevelSwitcher(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
		new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"EditorStyle",
			"ToolMenus",
			"Projects",
			"AssetRegistry",
			"ContentBrowser",
			"AssetTools",
			"DeveloperSettings",
			"DeveloperSettings",
			"EditorWidgets",
			"Documentation",
			"WorkspaceMenuStructure",
			"ApplicationCore"
		}
		);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
