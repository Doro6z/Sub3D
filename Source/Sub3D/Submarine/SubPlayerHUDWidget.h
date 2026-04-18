#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SubPlayerHUDWidget.generated.h"

class ASubCrewCharacter;
class USubInteractionComponent;
class UInteractableComponent;

/**
 * Base C++ class for the Player HUD.
 * Provides easy access to character and interaction state for Blueprint.
 */
UCLASS()
class SUB3D_API USubPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Returns the character being controlled by the local player
	UFUNCTION(BlueprintPure, Category = "Submarine|HUD")
	ASubCrewCharacter* GetOwningCrewCharacter() const;

	// Returns the interaction component of the owning character
	UFUNCTION(BlueprintPure, Category = "Submarine|HUD")
	USubInteractionComponent* GetInteractionComponent() const;

	// Returns the current health of the character (0.0 to 1.0)
	UFUNCTION(BlueprintPure, Category = "Submarine|HUD")
	float GetHealthPercent() const;

	// Returns true if the character is looking at an interactable object
	UFUNCTION(BlueprintPure, Category = "Submarine|HUD")
	bool HasFocusedInteractable() const;

	// Returns the text describing the current interaction (e.g. "Take Helm")
	UFUNCTION(BlueprintPure, Category = "Submarine|HUD")
	FText GetInteractionActionText() const;
};
