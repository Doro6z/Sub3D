using UnrealBuildTool;

public class Sub3DDebugPanel : ModuleRules
{
	public Sub3DDebugPanel(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"ApplicationCore",  // FPlatformApplicationMisc::ClipboardCopy
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"EditorStyle",
			"ToolMenus",
			"Projects",
			"WorkspaceMenuStructure",
			"PropertyEditor",
			"DeveloperSettings",
			"Sub3D",
			"Sub3DCore",       // UCompartmentWaterBake (return type of bake)
			"Sub3DWaterBake"   // USubmarineWaterBakerLibrary (Authoring section bake button)
		});
	}
}
