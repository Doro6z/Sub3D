#include "InteractableComponent.h"

UInteractableComponent::UInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteractableComponent::TriggerInteract(ASubCrewCharacter* Interactor)
{
	if (!bIsInteractable) return;
	OnInteract.Broadcast(Interactor);
}
