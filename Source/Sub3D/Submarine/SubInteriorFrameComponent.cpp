#include "SubInteriorFrameComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "SubMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubInteriorFrame, Log, All);

USubInteriorFrameComponent::USubInteriorFrameComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void USubInteriorFrameComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* Owner = GetOwner();
	if (Owner)
	{
		PreviousLocation = Owner->GetActorLocation();
		PreviousRotation = Owner->GetActorRotation();
		bFrameValid = true;

		UE_LOG(
			LogSubInteriorFrame,
			Log,
			TEXT("InteriorFrame initialized | Owner=%s | Location=%s | Rotation=%s | FrameValid=%d"),
			*GetNameSafe(Owner),
			*PreviousLocation.ToCompactString(),
			*PreviousRotation.ToCompactString(),
			bFrameValid ? 1 : 0);
	}

	bool bTickPrerequisiteSet = false;
	if (Owner)
	{
		if (USubMovementComponent* SubMov = Owner->FindComponentByClass<USubMovementComponent>())
		{
			AddTickPrerequisiteComponent(SubMov);
			bTickPrerequisiteSet = true;
		}
	}

	UE_LOG(
		LogSubInteriorFrame,
		Log,
		TEXT("Tick prerequisite set on SubMovementComponent: %s"),
		bTickPrerequisiteSet ? TEXT("true") : TEXT("false"));
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

	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(FrameLocationDelta / DeltaTime);
		LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;

		LocalAngularVelocityDegrees = FVector(
			FrameRotationDelta.Roll / DeltaTime,
			FrameRotationDelta.Pitch / DeltaTime,
			FrameRotationDelta.Yaw / DeltaTime);
		LocalAngularAccelerationDegrees = (LocalAngularVelocityDegrees - PreviousLocalAngularVelocityDegrees) / DeltaTime;
	}
	else
	{
		LocalLinearVelocity = FVector::ZeroVector;
		LocalLinearAcceleration = FVector::ZeroVector;
		LocalAngularVelocityDegrees = FVector::ZeroVector;
		LocalAngularAccelerationDegrees = FVector::ZeroVector;
	}

	const float MaxAbsRotationDelta = FMath::Max3(
		FMath::Abs(FrameRotationDelta.Pitch),
		FMath::Abs(FrameRotationDelta.Yaw),
		FMath::Abs(FrameRotationDelta.Roll));

	if (bDebugLogFrame && (FrameLocationDelta.Size() > 1.f || MaxAbsRotationDelta > 0.1f))
	{
		UE_LOG(
			LogSubInteriorFrame,
			Log,
			TEXT("InteriorFrame delta | Loc=%s | Rot=%s"),
			*FrameLocationDelta.ToCompactString(),
			*FrameRotationDelta.ToCompactString());
	}

	if (bDebugDrawFrame && GetWorld())
	{
		const FVector Origin = CurrentLocation;
		DrawDebugSphere(GetWorld(), Origin, 20.f, 12, FColor::Green, false, 0.f, 0, 1.5f);
		DrawDebugLine(GetWorld(), Origin, Origin + (Owner->GetActorForwardVector() * 100.f), FColor::Blue, false, 0.f, 0, 2.f);
		DrawDebugLine(GetWorld(), Origin, Origin + (Owner->GetActorRightVector() * 80.f), FColor::Red, false, 0.f, 0, 2.f);
		DrawDebugLine(GetWorld(), Origin, Origin + (Owner->GetActorUpVector() * 80.f), FColor::Yellow, false, 0.f, 0, 2.f);
		DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + FrameLocationDelta, 20.f, FColor::Cyan, false, 0.f, 0, 1.5f);
	}

	if (bDebugLogFrame)
	{
		DebugLogTimer += DeltaTime;
		if (DebugLogTimer >= 1.f)
		{
			DebugLogTimer = 0.f;
			UE_LOG(
				LogSubInteriorFrame,
				Log,
				TEXT("InteriorFrame | Loc=%s | Rot=%s | DeltaLoc=%s | DeltaRot=%s | LocalVel=%s | LocalAccel=%s | LocalAngVel=%s | LocalAngAccel=%s"),
				*CurrentLocation.ToCompactString(),
				*CurrentRotation.ToCompactString(),
				*FrameLocationDelta.ToCompactString(),
				*FrameRotationDelta.ToCompactString(),
				*LocalLinearVelocity.ToCompactString(),
				*LocalLinearAcceleration.ToCompactString(),
				*LocalAngularVelocityDegrees.ToCompactString(),
				*LocalAngularAccelerationDegrees.ToCompactString());
		}
	}
	else
	{
		DebugLogTimer = 0.f;
	}

	PreviousLocalLinearVelocity = LocalLinearVelocity;
	PreviousLocalAngularVelocityDegrees = LocalAngularVelocityDegrees;
	PreviousLocation = CurrentLocation;
	PreviousRotation = CurrentRotation;
}

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
