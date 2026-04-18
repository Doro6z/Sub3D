using UnrealBuildTool;

public class Sub3DBake : ModuleRules
{
    public Sub3DBake(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Sub3DCore",
            "EditorSubsystem",
            "UnrealEd",
            "Sub3DRuntime",
            "ProceduralMeshComponent",
            "Sub3D"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry"
        });
    }
}
