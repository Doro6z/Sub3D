using UnrealBuildTool;

public class Sub3DRuntime : ModuleRules
{
    public Sub3DRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Sub3DCore",
            "ProceduralMeshComponent"
        });
    }
}

