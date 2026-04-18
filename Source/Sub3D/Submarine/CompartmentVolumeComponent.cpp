#include "CompartmentVolumeComponent.h"

UCompartmentVolumeComponent::UCompartmentVolumeComponent()
{
	// Visible in editor, hidden at runtime.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	SetVisibility(true);
	ShapeColor = FColor(50, 180, 220, 255);

	// Default box extent: 100cm each axis = 200x200x200cm volume.
	// Direct assignment instead of SetBoxExtent: the setter triggers
	// UBoxComponent::UpdateBodySetup, which calls NewObject<UBodySetup> with
	// no name. That is illegal inside a UObject constructor and crashes CDO
	// construction at editor launch.
	BoxExtent = FVector(100.f, 100.f, 100.f);
	LineThickness = 2.f;
}

void UCompartmentVolumeComponent::OnRegister()
{
	Super::OnRegister();
	ShapeColor = VolumeColor;
}

#if WITH_EDITOR
void UCompartmentVolumeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCompartmentVolumeComponent, VolumeColor))
	{
		ShapeColor = VolumeColor;
		MarkRenderStateDirty();
	}
}
#endif
