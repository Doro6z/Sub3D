using UnrealBuildTool;

public class Sub3D : ModuleRules
{
	public Sub3D(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"NetCore",
			"ProceduralMeshComponent",
			"GameplayTags",
			"Niagara",
			"PCG",
			"PhysicsCore",
			"RuntimeSyncDiagnostics",
			"AnimGraphRuntime",
			"DeveloperSettings",
			"Sub3DRuntime",
			"Sub3DCore"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AssetRegistry",
				"MeshDescription",
				"StaticMeshDescription",
				"UnrealEd",
				"AnimGraph",
				"BlueprintGraph"
			});
		}

		PublicIncludePaths.AddRange(new string[]
		{
			"Sub3D",
			"Sub3D/Submarine",
			"Sub3D/Submarine/Generator",
			"Sub3D/WorldGen",
			"Sub3D/Diagnostics",
			"Sub3D/Debug"
		});
	}
}
