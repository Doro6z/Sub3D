#include "SubmarineStationManagerComponent.h"

#include "SubStationBase.h"

USubmarineStationManagerComponent::USubmarineStationManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USubmarineStationManagerComponent::RegisterStation(ASubStationBase* Station)
{
	if (!Station)
	{
		return;
	}

	RegisteredStations.Remove(nullptr);
	RegisteredStations.AddUnique(Station);
}

void USubmarineStationManagerComponent::UnregisterStation(ASubStationBase* Station)
{
	if (!Station)
	{
		return;
	}

	RegisteredStations.Remove(Station);
}

ASubStationBase* USubmarineStationManagerComponent::GetFirstStationOfType(ESubStationType StationType) const
{
	for (ASubStationBase* Station : RegisteredStations)
	{
		if (Station && Station->StationType == StationType)
		{
			return Station;
		}
	}

	return nullptr;
}

bool USubmarineStationManagerComponent::IsStationTypeOccupied(ESubStationType StationType) const
{
	for (ASubStationBase* Station : RegisteredStations)
	{
		if (Station && Station->StationType == StationType && Station->IsOccupied())
		{
			return true;
		}
	}

	return false;
}
