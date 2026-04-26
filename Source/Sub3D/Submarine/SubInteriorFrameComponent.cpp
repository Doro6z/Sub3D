#include "SubInteriorFrameComponent.h"
#include "Sub3DDebugSettings.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "SubmarineBase.h"
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
		FVector WorldLinearVelocity = FrameLocationDelta / DeltaTime;  // fallback: finite diff (no-SubMov case)
		const USubMovementComponent* SubMov = nullptr;
		if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(Owner))
		{
			SubMov = Sub->SubMovement;
			if (SubMov)
			{
				WorldLinearVelocity = SubMov->Velocity;  // sim-side, stepped at FixedSimulationHz
			}
		}
		LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(WorldLinearVelocity);

		LocalAngularVelocityDegrees = FVector(
			FrameRotationDelta.Roll / DeltaTime,
			FrameRotationDelta.Pitch / DeltaTime,
			FrameRotationDelta.Yaw / DeltaTime);

		// Acceleration signals: recompute only when the sub sim step advances. Dividing a
		// stepped-60Hz velocity by render_dt (e.g. 1/144) produces a spike/zero pattern at
		// sim boundaries that breaks Camera Sway and any consumer of this signal. Use the
		// sim dt as divisor so the acceleration reflects the true per-sim-step change;
		// hold it constant between sim steps.
		if (SubMov)
		{
			const int32 CurrentSimFrame = SubMov->GetSimFrameCounter();
			if (CurrentSimFrame != LastSeenSubSimFrame)
			{
				const float SimDt = SubMov->FixedSimulationHz > KINDA_SMALL_NUMBER
					? (1.f / SubMov->FixedSimulationHz)
					: (1.f / 60.f);
				LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / SimDt;
				LocalAngularAccelerationDegrees = (LocalAngularVelocityDegrees - PreviousLocalAngularVelocityDegrees) / SimDt;
				LastSeenSubSimFrame = CurrentSimFrame;
			}
			// else: hold previous LocalLinearAcceleration / LocalAngularAccelerationDegrees.
		}
		else
		{
			// Fallback (no SubMov): use finite diff at render rate.
			LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime;
			LocalAngularAccelerationDegrees = (LocalAngularVelocityDegrees - PreviousLocalAngularVelocityDegrees) / DeltaTime;
		}
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

	if (GetDefault<USub3DDebugSettings>()->bLogInteriorFrame && (FrameLocationDelta.Size() > 1.f || MaxAbsRotationDelta > 0.1f))
	{
		UE_LOG(
			LogSubInteriorFrame,
			Log,
			TEXT("InteriorFrame delta | Loc=%s | Rot=%s"),
			*FrameLocationDelta.ToCompactString(),
			*FrameRotationDelta.ToCompactString());
	}

	if (GetDefault<USub3DDebugSettings>()->bDrawInteriorFrame && GetWorld())
	{
		const FVector Origin = CurrentLocation;
		DrawDebugSphere(GetWorld(), Origin, 20.f, 12, FColor::Green, false, 0.f, 0, 1.5f);
		DrawDebugLine(GetWorld(), Origin, Origin + (Owner->GetActorForwardVector() * 100.f), FColor::Blue, false, 0.f, 0, 2.f);
		DrawDebugLine(GetWorld(), Origin, Origin + (Owner->GetActorRightVector() * 80.f), FColor::Red, false, 0.f, 0, 2.f);
		DrawDebugLine(GetWorld(), Origin, Origin + (Owner->GetActorUpVector() * 80.f), FColor::Yellow, false, 0.f, 0, 2.f);
		DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + FrameLocationDelta, 20.f, FColor::Cyan, false, 0.f, 0, 1.5f);
	}

	if (GetDefault<USub3DDebugSettings>()->bLogInteriorFrame)
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
