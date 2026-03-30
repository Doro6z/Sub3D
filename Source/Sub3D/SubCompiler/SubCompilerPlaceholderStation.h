#pragma once

#include "CoreMinimal.h"
#include "Submarine/SubStationBase.h"
#include "SubCompilerPlaceholderStation.generated.h"

class ASubCrewCharacter;
class ASubmarineBase;
class UBoxComponent;
class UInteractableComponent;
class UTextRenderComponent;
struct FStationSlotDef;

UCLASS(Blueprintable)
class SUB3D_API ASubCompilerPlaceholderStation : public ASubStationBase
{
	GENERATED_BODY()

public:
	ASubCompilerPlaceholderStation();

	void InitializeFromSlot(const FStationSlotDef& SlotDef, ASubmarineBase* InSubmarine);

protected:
	UFUNCTION()
	void HandlePlaceholderInteract(ASubCrewCharacter* Interactor);

	void UpdateLabel();
	FTransform ComputeReadableLocalTransform(const FTransform& SlotTransform) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> LabelComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInteractableComponent> InteractableComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placeholder")
	FName PlaceholderStationId = NAME_None;
};
