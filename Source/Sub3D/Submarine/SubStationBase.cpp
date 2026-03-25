#include "SubStationBase.h"

#include "SubmarineBase.h"
#include "SubmarineStationManagerComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubStation, Log, All);

ASubStationBase::ASubStationBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ASubStationBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASubStationBase, OwningSubmarine);
	DOREPLIFETIME(ASubStationBase, CurrentOccupant);
}

void ASubStationBase::BeginPlay()
{
	Super::BeginPlay();
	TryAutoResolveOwningSubmarine();

	UE_LOG(
		LogSubStation,
		Log,
		TEXT("[%s] BeginPlay | HasAuthority=%d | Owner=%s | AttachParentActor=%s | OwningSubmarine=%s"),
		*GetName(),
		HasAuthority() ? 1 : 0,
		*GetNameSafe(GetOwner()),
		*GetNameSafe(GetAttachParentActor()),
		*GetNameSafe(OwningSubmarine.Get())
	);

	if (OwningSubmarine && OwningSubmarine->StationManager)
	{
		OwningSubmarine->StationManager->RegisterStation(this);
	}
}

void ASubStationBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OwningSubmarine && OwningSubmarine->StationManager)
	{
		OwningSubmarine->StationManager->UnregisterStation(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ASubStationBase::TryAutoResolveOwningSubmarine()
{
	if (!bAutoResolveOwningSubmarine || OwningSubmarine)
	{
		return;
	}

	if (ASubmarineBase* OwnerSub = Cast<ASubmarineBase>(GetOwner()))
	{
		OwningSubmarine = OwnerSub;
		UE_LOG(LogSubStation, Log, TEXT("[%s] OwningSubmarine resolved from Owner: %s"), *GetName(), *GetNameSafe(OwningSubmarine.Get()));
		return;
	}

	// Walk attachment chain to support nested components/child actor setups.
	AActor* ParentActor = GetAttachParentActor();
	while (ParentActor)
	{
		if (ASubmarineBase* ParentSub = Cast<ASubmarineBase>(ParentActor))
		{
			OwningSubmarine = ParentSub;
			UE_LOG(LogSubStation, Log, TEXT("[%s] OwningSubmarine resolved from AttachParent chain: %s"), *GetName(), *GetNameSafe(OwningSubmarine.Get()));
			return;
		}
		ParentActor = ParentActor->GetAttachParentActor();
	}

	// Fallback: walk owner chain.
	AActor* OwnerActor = GetOwner();
	while (OwnerActor)
	{
		if (ASubmarineBase* OwnerSubChain = Cast<ASubmarineBase>(OwnerActor))
		{
			OwningSubmarine = OwnerSubChain;
			UE_LOG(LogSubStation, Log, TEXT("[%s] OwningSubmarine resolved from Owner chain: %s"), *GetName(), *GetNameSafe(OwningSubmarine.Get()));
			return;
		}
		OwnerActor = OwnerActor->GetOwner();
	}

	UE_LOG(LogSubStation, Warning, TEXT("[%s] OwningSubmarine unresolved after auto-resolve."), *GetName());
}

void ASubStationBase::SetOwningSubmarine(ASubmarineBase* InSubmarine)
{
	OwningSubmarine = InSubmarine;
	UE_LOG(LogSubStation, Log, TEXT("[%s] SetOwningSubmarine called. New=%s"), *GetName(), *GetNameSafe(OwningSubmarine.Get()));
}

bool ASubStationBase::IsOccupied() const
{
	return CurrentOccupant != nullptr;
}

bool ASubStationBase::CanEnterStation_Implementation(AController* Controller) const
{
	if (!bStationEnabled || !Controller)
	{
		UE_LOG(
			LogSubStation,
			Warning,
			TEXT("[%s] CanEnter=FALSE | bStationEnabled=%d | Controller=%s"),
			*GetName(),
			bStationEnabled ? 1 : 0,
			*GetNameSafe(Controller)
		);
		return false;
	}

	if (bRequireOwningSubmarine && !OwningSubmarine)
	{
		UE_LOG(LogSubStation, Warning, TEXT("[%s] CanEnter=FALSE | OwningSubmarine is null and required."), *GetName());
		return false;
	}

	if (!bExclusiveOccupancy)
	{
		UE_LOG(LogSubStation, Log, TEXT("[%s] CanEnter=TRUE | non-exclusive station."), *GetName());
		return true;
	}

	const bool bAllowed = CurrentOccupant == nullptr || CurrentOccupant == Controller;
	UE_LOG(
		LogSubStation,
		Log,
		TEXT("[%s] CanEnter=%s | CurrentOccupant=%s | RequestController=%s"),
		*GetName(),
		bAllowed ? TEXT("TRUE") : TEXT("FALSE"),
		*GetNameSafe(CurrentOccupant.Get()),
		*GetNameSafe(Controller)
	);
	return bAllowed;
}

void ASubStationBase::RequestEnterStation_Implementation(AController* Controller)
{
	if (!HasAuthority())
	{
		UE_LOG(LogSubStation, Warning, TEXT("[%s] RequestEnter ignored on non-authority."), *GetName());
		return;
	}

	if (CanEnterStation_Implementation(Controller))
	{
		CurrentOccupant = Controller;
		UE_LOG(LogSubStation, Log, TEXT("[%s] Enter accepted. Occupant=%s"), *GetName(), *GetNameSafe(CurrentOccupant.Get()));
		OnStationEntered(Controller);
	}
	else
	{
		UE_LOG(LogSubStation, Warning, TEXT("[%s] Enter rejected for %s"), *GetName(), *GetNameSafe(Controller));
	}
}

void ASubStationBase::RequestExitStation_Implementation(AController* Controller)
{
	if (!HasAuthority())
	{
		UE_LOG(LogSubStation, Warning, TEXT("[%s] RequestExit ignored on non-authority."), *GetName());
		return;
	}

	if (CurrentOccupant == Controller)
	{
		UE_LOG(LogSubStation, Log, TEXT("[%s] Exit accepted. Occupant=%s"), *GetName(), *GetNameSafe(CurrentOccupant.Get()));
		CurrentOccupant = nullptr;
		OnStationExited(Controller);
	}
	else
	{
		UE_LOG(
			LogSubStation,
			Warning,
			TEXT("[%s] Exit rejected. CurrentOccupant=%s | RequestController=%s"),
			*GetName(),
			*GetNameSafe(CurrentOccupant.Get()),
			*GetNameSafe(Controller)
		);
	}
}

ESubStationType ASubStationBase::GetStationType_Implementation() const
{
	return StationType;
}

AController* ASubStationBase::GetCurrentOccupant_Implementation() const
{
	return CurrentOccupant.Get();
}

ASubmarineBase* ASubStationBase::GetOwningSubmarine_Implementation() const
{
	return OwningSubmarine.Get();
}
