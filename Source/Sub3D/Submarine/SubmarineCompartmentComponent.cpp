#include "SubmarineCompartmentComponent.h"

#include "Net/UnrealNetwork.h"
#include "StructuralHullTypes.h"
#include "SubHullComponent.h"
#include "SubmarineBase.h"

USubmarineCompartmentComponent::USubmarineCompartmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void USubmarineCompartmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
	{
		if (Sub->SubHull)
		{
			SyncFromHullComponent(Sub->SubHull);
		}
	}
}

void USubmarineCompartmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USubmarineCompartmentComponent, Compartments);
	DOREPLIFETIME(USubmarineCompartmentComponent, Doors);
	DOREPLIFETIME(USubmarineCompartmentComponent, TotalWaterMassLiters);
}

void USubmarineCompartmentComponent::SyncFromHullComponent(const USubHullComponent* HullComponent)
{
	if (!HullComponent)
	{
		return;
	}

	TArray<FCompartmentState> NewCompartments;
	HullComponent->ExportCompartmentStates(NewCompartments);
	Compartments = MoveTemp(NewCompartments);
	TotalWaterMassLiters = 0.f;

	for (const FCompartmentState& State : Compartments)
	{
		TotalWaterMassLiters += FMath::Max(0.f, State.WaterMassLiters);
	}
}

void USubmarineCompartmentComponent::RegisterDoor(const FDoorState& DoorState)
{
	const int32 ExistingIndex = Doors.IndexOfByPredicate([&DoorState](const FDoorState& Existing)
	{
		return Existing.DoorId == DoorState.DoorId;
	});

	if (ExistingIndex != INDEX_NONE)
	{
		Doors[ExistingIndex] = DoorState;
		return;
	}

	Doors.Add(DoorState);
}

void USubmarineCompartmentComponent::SetDoorClosed(FName DoorId, bool bClosed)
{
	if (FDoorState* Door = Doors.FindByPredicate([DoorId](const FDoorState& Existing)
	{
		return Existing.DoorId == DoorId;
	}))
	{
		Door->bClosed = bClosed;
	}
}

bool USubmarineCompartmentComponent::IsDoorClosed(FName DoorId) const
{
	if (const FDoorState* Door = Doors.FindByPredicate([DoorId](const FDoorState& Existing)
	{
		return Existing.DoorId == DoorId;
	}))
	{
		return Door->bClosed;
	}

	return false;
}

bool USubmarineCompartmentComponent::TryGetDoorState(FName DoorId, FDoorState& OutDoorState) const
{
	if (const FDoorState* Door = Doors.FindByPredicate([DoorId](const FDoorState& Existing)
	{
		return Existing.DoorId == DoorId;
	}))
	{
		OutDoorState = *Door;
		return true;
	}

	return false;
}

float USubmarineCompartmentComponent::GetCompartmentFlood01(FName CompartmentId) const
{
	if (const FCompartmentState* Comp = Compartments.FindByPredicate([CompartmentId](const FCompartmentState& Existing)
	{
		return Existing.CompartmentId == CompartmentId;
	}))
	{
		return Comp->FloodLevel01;
	}

	return 0.f;
}
