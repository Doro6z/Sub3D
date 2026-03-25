#include "Providers/RelativeFrameExpectedTransformProvider.h"

URelativeFrameExpectedTransformProvider::URelativeFrameExpectedTransformProvider()
{
	RelativeExpectedTransform = FTransform::Identity;
}

bool URelativeFrameExpectedTransformProvider::GetExpectedTransform(const AActor* MonitoredActor, FTransform& OutTransform) const
{
	if (FrameComponent.IsValid())
	{
		OutTransform = RelativeExpectedTransform * FrameComponent->GetComponentTransform();
		return true;
	}

	return false;
}
