#include "SubHullBoundaryComponent.h"

#include "CompartmentVolumeComponent.h"
#include "SubCrewCharacter.h"

USubHullBoundaryComponent::USubHullBoundaryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Shares the "CompartmentProbe" collision profile with UCompartmentVolumeComponent:
	// QueryOnly + Pawn=Overlap, all other channels Ignore. Zero interference with gameplay physics.
	SetCollisionProfileName(TEXT("CompartmentProbe"));
	SetGenerateOverlapEvents(true);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	SetVisibility(true);
	ShapeColor = FColor(220, 120, 60, 255);  // Orange — distinguishable from compartment volumes.

	// Direct assignment (see UCompartmentVolumeComponent::UCompartmentVolumeComponent for rationale).
	BoxExtent = FVector(80.f, 80.f, 80.f);
	LineThickness = 2.f;
}

void USubHullBoundaryComponent::BeginPlay()
{
	Super::BeginPlay();
	OnComponentBeginOverlap.AddDynamic(this, &USubHullBoundaryComponent::HandleBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &USubHullBoundaryComponent::HandleEndOverlap);
}

void USubHullBoundaryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (auto It = CrewSideStates.CreateIterator(); It; ++It)
	{
		ASubCrewCharacter* Crew = It.Key().Get();
		if (!Crew)
		{
			It.RemoveCurrent();
			continue;
		}

		FCrewSideState& State = It.Value();
		const int8 CurSide = ComputeSide(Crew);

		// Single-fire gate: only consider flips when armed, and disarm after firing.
		if (State.bArmed && State.LastSide != 0 && CurSide != 0 && CurSide != State.LastSide)
		{
			const bool bOutgoing = (State.LastSide == -1 && CurSide == +1);
			State.bArmed = false;   // Disarm until the crew leaves the box.

			Crew->HandleHullCrossing(this, bOutgoing);
			OnCapsuleCrossedHull.Broadcast(Crew, bOutgoing);
		}

		State.LastSide = CurSide;
	}
}

void USubHullBoundaryComponent::HandleBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(OtherActor))
	{
		FCrewSideState State;
		State.LastSide = ComputeSide(Crew);
		State.bArmed = true;
		CrewSideStates.Add(Crew, State);
	}
}

void USubHullBoundaryComponent::HandleEndOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/)
{
	if (ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(OtherActor))
	{
		CrewSideStates.Remove(Crew);
	}
}

int8 USubHullBoundaryComponent::ComputeSide(const ASubCrewCharacter* Crew) const
{
	if (!Crew)
	{
		return 0;
	}
	const FVector WorldOrigin = GetComponentLocation();
	const FVector WorldNormal = GetForwardVector();   // Component local +X → world space.
	if (WorldNormal.IsNearlyZero())
	{
		return 0;
	}
	const float Dot = FVector::DotProduct(Crew->GetActorLocation() - WorldOrigin, WorldNormal);
	return (Dot >= 0.f) ? +1 : -1;
}
