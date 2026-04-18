using UnrealBuildTool;

public class Sub3DTests : ModuleRules
{
    public Sub3DTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "ProceduralMeshComponent",
            "Sub3DCore",
            "Sub3DBake",
            "Sub3DRuntime",
            "Sub3D"
        });
    }
}
