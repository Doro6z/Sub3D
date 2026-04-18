using UnrealBuildTool;

public class Sub3DBuilder : ModuleRules
{
    public Sub3DBuilder(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Sub3DCore",
            "Sub3DRuntime",
            "ProceduralMeshComponent",
            "UMG"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "Sub3DBake",
                "UnrealEd",
                "EditorSubsystem"
            });
        }
    }
}
