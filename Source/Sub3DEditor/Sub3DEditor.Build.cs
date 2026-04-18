using UnrealBuildTool;

public class Sub3DEditor : ModuleRules
{
    public Sub3DEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "EditorSubsystem",
            "UnrealEd",
            "ToolMenus",
            "InputCore",
            "Sub3DCore",
            "Sub3DBake"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            "AssetTools",
            "PropertyEditor",
            "LevelEditor",
            "ApplicationCore"
        });
    }
}

