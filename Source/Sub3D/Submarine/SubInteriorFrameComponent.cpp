#include "SubInteriorFrameComponent.h"

#include "GameFramework/Actor.h"
#include "SubMovementComponent.h"

USubInteriorFrameComponent::USubInteriorFrameComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void USubInteriorFrameComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		PreviousLocation = Owner->GetActorLocation();
		PreviousRotation = Owner->GetActorRotation();
		bFrameValid = true;
	}

	// Tick after SubMovementComponent so the delta reflects this frame's motion.
	if (const AActor* Owner = GetOwner())
	{
		if (USubMovementComponent* SubMov = Owner->FindComponentByClass<USubMovementComponent>())
		{
			AddTickPrerequisiteComponent(SubMov);
		}
	}
}

void USubInteriorFrameComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FVector CurrentLocation = Owner->GetActorLocation();
	const FRotator CurrentRotation = Owner->GetActorRotation();

	FrameLocationDelta = CurrentLocation - PreviousLocation;
	FrameRotationDelta = (CurrentRotation - PreviousRotation).GetNormalized();

	PreviousLocation = CurrentLocation;
	PreviousRotation = CurrentRotation;
}

// ── Coordinate conversion ────────────────────────────────────────────────────

FVector USubInteriorFrameComponent::WorldToLocal(const FVector& WorldPosition) const
{
	if (const AActor* Owner = GetOwner())
	{
		return Owner->GetActorTransform().InverseTransformPosition(WorldPosition);
	}
	return WorldPosition;
}

FVector USubInteriorFrameComponent::LocalToWorld(const FVector& LocalPosition) const
{
	if (const AActor* Owner = GetOwner())
	{
		return Owner->GetActorTransform().TransformPosition(LocalPosition);
	}
	return LocalPosition;
}

FRotator USubInteriorFrameComponent::WorldToLocalRotation(const FRotator& WorldRotation) const
{
	if (const AActor* Owner = GetOwner())
	{
		const FQuat OwnerQuat = Owner->GetActorQuat();
		const FQuat WorldQuat = WorldRotation.Quaternion();
		return (OwnerQuat.Inverse() * WorldQuat).Rotator();
	}
	return WorldRotation;
}

FRotator USubInteriorFrameComponent::LocalToWorldRotation(const FRotator& LocalRotation) const
{
	if (const AActor* Owner = GetOwner())
	{
		const FQuat OwnerQuat = Owner->GetActorQuat();
		const FQuat LocalQuat = LocalRotation.Quaternion();
		return (OwnerQuat * LocalQuat).Rotator();
	}
	return LocalRotation;
}

FTransform USubInteriorFrameComponent::GetSubTransform() const
{
	if (const AActor* Owner = GetOwner())
	{
		return Owner->GetActorTransform();
	}
	return FTransform::Identity;
}
