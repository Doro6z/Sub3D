using UnrealBuildTool;

public class Sub3DWaterBake : ModuleRules
{
    public Sub3DWaterBake(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Sub3DCore",  // UCompartmentWaterBake type
            "Sub3D"       // USubmarineDefinition + UCompartmentVolumeComponent (read-only access)
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "AssetRegistry",            // FAssetRegistryModule for asset save notification
                "UnrealEd",                 // FSavePackageArgs / UPackage::Save / CreatePackage
                "Blutility",                // UAssetActionUtility (right-click DA action)
                "EditorScriptingUtilities"  // UEditorAssetLibrary, UEditorUtilityLibrary
            });
        }
    }
}
