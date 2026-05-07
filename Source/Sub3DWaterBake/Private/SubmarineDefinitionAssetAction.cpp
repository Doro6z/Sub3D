#include "SubmarineDefinitionAssetAction.h"

#include "Sub3DWaterBake.h"
#include "Submarine/Generator/SubmarineDefinition.h"
#include "Types/CompartmentWaterBake.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "EditorUtilityLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

USubmarineDefinitionAssetAction::USubmarineDefinitionAssetAction()
{
#if WITH_EDITORONLY_DATA
	SupportedClasses.Add(USubmarineDefinition::StaticClass());
#endif
}

void USubmarineDefinitionAssetAction::BakeWaterForSelected(FSubmarineWaterBakeParams Params)
{
#if WITH_EDITOR
	TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();
	if (SelectedAssets.Num() == 0)
	{
		UE_LOG(LogWaterBake, Warning, TEXT("BakeWaterForSelected: no asset selected"));
		return;
	}

	UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
	if (!ActorSubsystem)
	{
		UE_LOG(LogWaterBake, Error, TEXT("BakeWaterForSelected: EditorActorSubsystem unavailable"));
		return;
	}

	TArray<AActor*> AllActors = ActorSubsystem->GetAllLevelActors();
	AActor* HullActor = nullptr;
	for (AActor* Actor : AllActors)
	{
		if (!Actor) continue;
		const FString ClassName = Actor->GetClass()->GetName();
		if (ClassName.StartsWith(TEXT("BP_Submarine_")))
		{
			HullActor = Actor;
			break;
		}
	}
	if (!HullActor)
	{
		UE_LOG(LogWaterBake, Error,
			TEXT("BakeWaterForSelected: no BP_Submarine_* actor in the active level. ")
			TEXT("Open the gameplay map with the submarine placed before running this action."));
		return;
	}

	UE_LOG(LogWaterBake, Display, TEXT("BakeWaterForSelected: hull actor = %s (class %s)"),
		*HullActor->GetName(), *HullActor->GetClass()->GetName());

	int32 TotalCompartments = 0;
	int32 TotalDefinitions = 0;
	for (UObject* Asset : SelectedAssets)
	{
		USubmarineDefinition* Definition = Cast<USubmarineDefinition>(Asset);
		if (!Definition) continue;
		++TotalDefinitions;

		FString Report;
		const int32 Successful = USubmarineWaterBakerLibrary::BakeAllCompartments(Definition, HullActor, Params, Report);
		TotalCompartments += Successful;
		// The asset action surface doesn't display the report — full content is in LogWaterBake.
		UE_LOG(LogWaterBake, Display, TEXT("Report for %s:\n%s"), *Definition->GetName(), *Report);

		// The DA does not yet hold a WaterBakes map (P2.5 will add it). The CWB_* assets are
		// produced and saved to disk by BakeCompartment; the DA itself stays untouched here.
		// Save it anyway to flush any other dirty state the bake may have caused indirectly.
		UEditorAssetLibrary::SaveLoadedAsset(Definition);
	}

	UE_LOG(LogWaterBake, Display,
		TEXT("BakeWaterForSelected: baked %d compartments across %d definition(s)"),
		TotalCompartments, TotalDefinitions);
#endif
}
