using UnrealBuildTool;

public class Sub3DWaterProto : ModuleRules
{
    public Sub3DWaterProto(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "ProceduralMeshComponent",
            "Niagara",
            "RenderCore",
            "RHI"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Sub3D"  // ASubDoorActor (Étape B), shared FName conventions
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "EditorScriptingUtilities",
                "Blutility"
            });
        }
    }
}
