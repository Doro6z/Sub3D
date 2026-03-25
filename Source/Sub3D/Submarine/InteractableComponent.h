#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractableComponent.generated.h"

class ASubCrewCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteract, ASubCrewCharacter*, Interactor);

/**
 * Drop this component on any Actor to make it interactable.
 * Blueprint binds OnInteract to react (take helm, open door, etc.)
 */
UCLASS(ClassGroup="Sub3D", meta=(BlueprintSpawnableComponent))
class SUB3D_API UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractableComponent();

	// Whether this interactable is currently usable
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	bool bIsInteractable = true;

	// Max distance in cm for the line trace to reach this interactable
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	float InteractRange = 200.f;

	// Fired on the owning Actor when a crew member interacts
	UPROPERTY(BlueprintAssignable, Category = "Interact")
	FOnInteract OnInteract;

	// Called by ASubCrewCharacter::Interact() when the trace hits this actor
	UFUNCTION(BlueprintCallable, Category = "Interact")
	void TriggerInteract(ASubCrewCharacter* Interactor);
};
