#include "SubPlayerHUDWidget.h"
#include "SubCrewCharacter.h"
#include "SubInteractionComponent.h"
#include "InteractableComponent.h"

ASubCrewCharacter* USubPlayerHUDWidget::GetOwningCrewCharacter() const
{
	return Cast<ASubCrewCharacter>(GetOwningPlayerPawn());
}

USubInteractionComponent* USubPlayerHUDWidget::GetInteractionComponent() const
{
	if (const ASubCrewCharacter* Character = GetOwningCrewCharacter())
	{
		return Character->InteractionComponent;
	}
	return nullptr;
}

float USubPlayerHUDWidget::GetHealthPercent() const
{
	if (const ASubCrewCharacter* Character = GetOwningCrewCharacter())
	{
		return Character->GetHealthNormalized();
	}
	return 0.f;
}

bool USubPlayerHUDWidget::HasFocusedInteractable() const
{
	if (const USubInteractionComponent* InteractionComp = GetInteractionComponent())
	{
		return InteractionComp->FocusedInteractable != nullptr;
	}
	return false;
}

FText USubPlayerHUDWidget::GetInteractionActionText() const
{
	if (const USubInteractionComponent* InteractionComp = GetInteractionComponent())
	{
		if (InteractionComp->FocusedInteractable)
		{
			return InteractionComp->FocusedInteractable->InteractionActionText;
		}
	}
	return FText::GetEmpty();
}
