#pragma once

#include "CoreMinimal.h"
#include "AssetActionUtility.h"
#include "SubmarineWaterBakerLibrary.h"
#include "SubmarineDefinitionAssetAction.generated.h"

/**
 * Right-click menu action on USubmarineDefinition assets in the Content Browser:
 *   "Scripted Asset Actions > Bake Water For Selected"
 *
 * Replaces the EUW_BakeSubmarineWater editor utility widget proposed by the plan (P2.4).
 * Same workflow, simpler: select one or more DAs, right-click, hit the action, get the
 * baked CWB_* assets in Params.PackagePathRoot.
 *
 * Hull source: searches the active editor world for an actor whose UClass starts with
 * "BP_Submarine_" — covers Craniata + future submarines that follow the convention.
 * Open the gameplay map (with the submarine placed) before invoking the action; without
 * a placed instance the bake has no collision to query against.
 */
UCLASS()
class SUB3DWATERBAKE_API USubmarineDefinitionAssetAction : public UAssetActionUtility
{
	GENERATED_BODY()

public:
	USubmarineDefinitionAssetAction();

	UFUNCTION(CallInEditor, Category = "Sub3D|Water Bake")
	void BakeWaterForSelected(FSubmarineWaterBakeParams Params);
};
