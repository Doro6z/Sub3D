#include "SubHelmStation.h"

#include "SubmarineBase.h"

ASubHelmStation::ASubHelmStation()
{
	StationType = ESubStationType::Helm;
	bExclusiveOccupancy = true;
}

bool ASubHelmStation::CanEnterStation_Implementation(AController* Controller) const
{
	return Super::CanEnterStation_Implementation(Controller) && OwningSubmarine != nullptr;
}
