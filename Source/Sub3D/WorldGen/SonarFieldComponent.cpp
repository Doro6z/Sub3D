#include "SonarFieldComponent.h"

USonarFieldComponent::USonarFieldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USonarFieldComponent::InitializeFromField(const FRouteFieldModel& Field)
{
	CachedSonarField = Field.SonarField;
}

bool USonarFieldComponent::SampleOcclusionAlongRay(const FVector& Start, const FVector& End, float& OutBlockage) const
{
	// Proto 04 STUB. Implement in Proto 05 using CachedSonarField.
	OutBlockage = 0.f;
	return false;
}

int32 USonarFieldComponent::GetSonarVoxelCount() const
{
	int32 Total = 0;
	for (const auto& Pair : CachedSonarField)
		Total += Pair.Value.OccupancySamples.Num();
	return Total;
}
