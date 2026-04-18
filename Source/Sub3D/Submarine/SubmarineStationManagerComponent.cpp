#include "SubmarineStationManagerComponent.h"

#include "Generator/SubmarineDefinition.h"
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

void USubmarineStationManagerComponent::DiscoverAttachedStations()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<AActor*> AttachedActors;
	Owner->GetAttachedActors(AttachedActors, true);

	RegisteredStations.Empty();

	for (AActor* Actor : AttachedActors)
	{
		if (ASubStationBase* Station = Cast<ASubStationBase>(Actor))
		{
			RegisterStation(Station);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[%s] DiscoverAttachedStations: Registered %d stations."), *Owner->GetName(), RegisteredStations.Num());
}

void USubmarineStationManagerComponent::SpawnStationsFromDefinition(const USubmarineDefinition* Definition)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !Definition || !Owner->HasAuthority())
	{
		return;
	}

	for (const FGeneratedStationSlotDef& Slot : Definition->StationSlots)
	{
		const TSubclassOf<ASubStationBase>* ClassPtr = StationClassMap.Find(Slot.StationType);
		if (!ClassPtr || !*ClassPtr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] SpawnStationsFromDefinition: no class mapped for StationType %d (Station=%s)"),
				*Owner->GetName(), static_cast<int32>(Slot.StationType), *Slot.StationId.ToString());
			continue;
		}

		const FTransform WorldTransform = Slot.LocalTransform * Owner->GetActorTransform();
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Owner;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ASubStationBase* Station = World->SpawnActor<ASubStationBase>(*ClassPtr, WorldTransform, SpawnParams);
		if (!Station)
		{
			continue;
		}

		Station->AttachToActor(Owner, FAttachmentTransformRules::KeepWorldTransform);
		Station->StationType = Slot.StationType;
		RegisterStation(Station);
	}

	UE_LOG(LogTemp, Log, TEXT("[%s] SpawnStationsFromDefinition: spawned and registered %d stations from definition."),
		*Owner->GetName(), RegisteredStations.Num());
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
