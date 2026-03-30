#include "SonarAcousticVolumeComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

USonarAcousticVolumeComponent::USonarAcousticVolumeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool USonarAcousticVolumeComponent::IsLocationInside(const FVector& WorldLocation) const
{
	if (!bVolumeEnabled)
	{
		return false;
	}

	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	if (const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()))
	{
		return Primitive->Bounds.GetBox().IsInside(WorldLocation);
	}

	return OwnerActor->GetComponentsBoundingBox(true).IsInside(WorldLocation);
}
