#include "SubCrewMovementComponent.h"

#include "SubCrewCharacter.h"
#include "SubInteriorFrameComponent.h"
#include "SubmarineBase.h"
#include "SubMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubCrewMovement, Log, All);

namespace
{
bool IsComponentOnSubmarine(const ASubmarineBase* Submarine, const UPrimitiveComponent* Component)
{
	if (!Submarine || !Component)
	{
		return false;
	}

	const AActor* ComponentOwner = Component->GetOwner();
	return ComponentOwner == Submarine
		|| (ComponentOwner && ComponentOwner->GetOwner() == Submarine)
		|| (ComponentOwner && ComponentOwner->GetAttachParentActor() == Submarine)
		|| (ComponentOwner && ComponentOwner->IsAttachedTo(Submarine));
}
}

USubCrewMovementComponent::USubCrewMovementComponent()
{
}

void USubCrewMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsEmbarked())
	{
		UpdateRelativeState();
		UpdateInertialState();
		UpdateSupportState();
		AttemptEmbarkedFloorRecovery(DeltaTime);
		UpdateBraceState();
		ApplyYawCompensation();
		CheckAndLogBaseChange();
		LogPeriodicState(DeltaTime);
		DebugDrawState();
	}
	else
	{
		if (LastKnownBase.IsValid())
		{
			const UPrimitiveComponent* PreviousBase = LastKnownBase.Get();
			UE_LOG(
				LogSubCrewMovement,
				Log,
				TEXT("Embark ended | clearing tracked base: %s on %s"),
				*GetNameSafe(PreviousBase),
				*GetNameSafe(PreviousBase ? PreviousBase->GetOwner() : nullptr));
		}

		LastKnownBase.Reset();
		LastEmbarkedFloorComponent = nullptr;
		LocalSubLinearVelocity = FVector::ZeroVector;
		LocalSubLinearAcceleration = FVector::ZeroVector;
		LocalSubAngularVelocityDegrees = FVector::ZeroVector;
		LocalSubAngularAccelerationDegrees = FVector::ZeroVector;
		bHasValidEmbarkedFloor = false;
		bHasAcceptedEmbarkedBase = false;
		bNeedsEmbarkedFloorRecovery = false;
		SupportQuality01 = 0.f;
		bHasNearbyBraceSupport = false;
		NearbyBraceDistanceCm = 0.f;
		NearbyBraceWorldLocation = FVector::ZeroVector;
		NearbyBraceWorldNormal = FVector::ZeroVector;
		BraceQueryOrigin = FVector::ZeroVector;
		FloorRecoveryTimer = 0.f;
		DebugLogTimer = 0.f;
	}
}

void USubCrewMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
	// D4: Stock based-movement re-enabled as the stabilization experiment.
	Super::UpdateBasedMovement(DeltaSeconds);
}

void USubCrewMovementComponent::UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation)
{
	// D4: Stock based-rotation re-enabled.
	// Controller yaw follow is handled separately in ApplyYawCompensation().
	Super::UpdateBasedRotation(FinalRotation, ReducedRotation);
}

ASubmarineBase* USubCrewMovementComponent::GetCurrentSubmarine() const
{
	if (const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner))
	{
		return Crew->CurrentSubmarine;
	}

	return nullptr;
}

USubInteriorFrameComponent* USubCrewMovementComponent::GetInteriorFrame() const
{
	if (ASubmarineBase* Sub = GetCurrentSubmarine())
	{
		return Sub->InteriorFrame;
	}

	return nullptr;
}

bool USubCrewMovementComponent::IsEmbarked() const
{
	return GetInteriorFrame() != nullptr;
}

bool USubCrewMovementComponent::IsAcceptedEmbarkedBase(const UPrimitiveComponent* CandidateBase) const
{
	if (!CandidateBase)
	{
		return false;
	}

	const ASubmarineBase* Submarine = GetCurrentSubmarine();
	if (!Submarine)
	{
		return false;
	}

	const TArray<UPrimitiveComponent*> WalkableComponents = Submarine->GetInteriorWalkableComponents();
	return WalkableComponents.Num() == 0 || WalkableComponents.Contains(const_cast<UPrimitiveComponent*>(CandidateBase));
}

void USubCrewMovementComponent::UpdateInertialState()
{
	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	if (!Frame || !Frame->IsFrameValid())
	{
		LocalSubLinearVelocity = FVector::ZeroVector;
		LocalSubLinearAcceleration = FVector::ZeroVector;
		LocalSubAngularVelocityDegrees = FVector::ZeroVector;
		LocalSubAngularAccelerationDegrees = FVector::ZeroVector;
		return;
	}

	LocalSubLinearVelocity = Frame->GetLocalLinearVelocity();
	LocalSubLinearAcceleration = Frame->GetLocalLinearAcceleration();
	LocalSubAngularVelocityDegrees = Frame->GetLocalAngularVelocityDegrees();
	LocalSubAngularAccelerationDegrees = Frame->GetLocalAngularAccelerationDegrees();
}

void USubCrewMovementComponent::UpdateRelativeState()
{
	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	if (!Frame || !CharacterOwner)
	{
		return;
	}

	RelativeLocation = Frame->WorldToLocal(CharacterOwner->GetActorLocation());
	RelativeRotation = Frame->WorldToLocalRotation(CharacterOwner->GetActorRotation());

	if (bDebugLogCrewMovement)
	{
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("RelativeState | WorldLoc=%s | RelLoc=%s | RelRot=%s"),
			*CharacterOwner->GetActorLocation().ToCompactString(),
			*RelativeLocation.ToCompactString(),
			*RelativeRotation.ToCompactString());
	}
}

void USubCrewMovementComponent::UpdateSupportState()
{
	if (!CharacterOwner)
	{
		bHasValidEmbarkedFloor = false;
		bHasAcceptedEmbarkedBase = false;
		bNeedsEmbarkedFloorRecovery = false;
		SupportQuality01 = 0.f;
		LastEmbarkedFloorComponent = nullptr;
		return;
	}

	const UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
	LastEmbarkedFloorComponent = CurrentFloor.HitResult.GetComponent();
	bHasValidEmbarkedFloor = CurrentFloor.IsWalkableFloor();
	bHasAcceptedEmbarkedBase = IsAcceptedEmbarkedBase(MovementBase);
	bNeedsEmbarkedFloorRecovery = IsEmbarked() && (!bHasValidEmbarkedFloor || !bHasAcceptedEmbarkedBase || !MovementBase);

	if (!IsEmbarked())
	{
		SupportQuality01 = 0.f;
	}
	else if (bHasValidEmbarkedFloor && bHasAcceptedEmbarkedBase)
	{
		SupportQuality01 = 1.f;
	}
	else if (bHasValidEmbarkedFloor)
	{
		SupportQuality01 = 0.5f;
	}
	else
	{
		SupportQuality01 = 0.f;
	}
}

void USubCrewMovementComponent::AttemptEmbarkedFloorRecovery(float DeltaTime)
{
	if (!bNeedsEmbarkedFloorRecovery || !CharacterOwner || MovementMode != MOVE_Walking)
	{
		FloorRecoveryTimer = 0.f;
		return;
	}

	FloorRecoveryTimer += DeltaTime;
	if (FloorRecoveryTimer < FMath::Max(0.01f, FloorRecoveryIntervalSeconds))
	{
		return;
	}

	FloorRecoveryTimer = 0.f;
	RefreshEmbarkedFlooring();

	if (bDebugLogCrewMovement)
	{
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("FloorRecovery | Base=%s | FloorWalkable=%d | AcceptedBase=%d | SupportQuality=%.2f"),
			*GetNameSafe(CharacterOwner->GetMovementBase()),
			CurrentFloor.IsWalkableFloor() ? 1 : 0,
			IsAcceptedEmbarkedBase(CharacterOwner->GetMovementBase()) ? 1 : 0,
			SupportQuality01);
	}
}

bool USubCrewMovementComponent::QueryBraceSupportHit(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	OutHit = FHitResult();

	const ASubmarineBase* Submarine = GetCurrentSubmarine();
	if (!Submarine || !CharacterOwner || !GetWorld())
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel2);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CrewBraceQuery), false, CharacterOwner);
	TArray<FHitResult> Hits;
	if (!GetWorld()->LineTraceMultiByObjectType(Hits, Start, End, ObjectQueryParams, QueryParams))
	{
		return false;
	}

	float BestHitTime = TNumericLimits<float>::Max();
	bool bFoundHit = false;
	for (const FHitResult& Hit : Hits)
	{
		if (!Hit.bBlockingHit)
		{
			continue;
		}

		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (!IsComponentOnSubmarine(Submarine, HitComponent) || IsAcceptedEmbarkedBase(HitComponent))
		{
			continue;
		}

		if (FMath::Abs(Hit.ImpactNormal.Z) > 0.6f)
		{
			continue;
		}

		if (!bFoundHit || Hit.Time < BestHitTime)
		{
			OutHit = Hit;
			BestHitTime = Hit.Time;
			bFoundHit = true;
		}
	}

	return bFoundHit;
}

void USubCrewMovementComponent::UpdateBraceState()
{
	bHasNearbyBraceSupport = false;
	NearbyBraceDistanceCm = 0.f;
	NearbyBraceWorldLocation = FVector::ZeroVector;
	NearbyBraceWorldNormal = FVector::ZeroVector;
	BraceQueryOrigin = FVector::ZeroVector;

	if (!CharacterOwner || !IsEmbarked() || !GetWorld())
	{
		return;
	}

	BraceQueryOrigin = CharacterOwner->GetActorLocation() + FVector(0.f, 0.f, BraceProbeHeightOffsetCm);
	const FVector Directions[] =
	{
		CharacterOwner->GetActorForwardVector(),
		CharacterOwner->GetActorRightVector(),
		-CharacterOwner->GetActorRightVector()
	};

	float BestDistance = TNumericLimits<float>::Max();
	FHitResult BestHit;
	bool bFoundHit = false;
	for (const FVector& Direction : Directions)
	{
		FHitResult Hit;
		if (!QueryBraceSupportHit(BraceQueryOrigin, BraceQueryOrigin + (Direction * BraceProbeDistanceCm), Hit))
		{
			continue;
		}

		const float HitDistance = FVector::Distance(BraceQueryOrigin, Hit.ImpactPoint);
		if (!bFoundHit || HitDistance < BestDistance)
		{
			BestDistance = HitDistance;
			BestHit = Hit;
			bFoundHit = true;
		}
	}

	if (!bFoundHit)
	{
		return;
	}

	bHasNearbyBraceSupport = true;
	NearbyBraceDistanceCm = BestDistance;
	NearbyBraceWorldLocation = BestHit.ImpactPoint;
	NearbyBraceWorldNormal = BestHit.ImpactNormal;
}

void USubCrewMovementComponent::ApplyYawCompensation()
{
	if (!CharacterOwner || !CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	if (!Frame || !Frame->IsFrameValid())
	{
		return;
	}

	const float YawDelta = Frame->GetFrameRotationDelta().Yaw;
	if (FMath::Abs(YawDelta) <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (AController* Controller = CharacterOwner->GetController())
	{
		FRotator ControlRotation = Controller->GetControlRotation();
		ControlRotation.Yaw = FRotator::NormalizeAxis(ControlRotation.Yaw + YawDelta);
		Controller->SetControlRotation(ControlRotation);

		if (bDebugLogCrewMovement)
		{
			UE_LOG(
				LogSubCrewMovement,
				Log,
				TEXT("YawCompensation | DeltaYaw=%.3f | NewControlYaw=%.3f"),
				YawDelta,
				ControlRotation.Yaw);
		}
	}
}

void USubCrewMovementComponent::CheckAndLogBaseChange()
{
	if (!CharacterOwner)
	{
		return;
	}

	UPrimitiveComponent* CurrentBase = CharacterOwner->GetMovementBase();
	UPrimitiveComponent* PreviousBase = LastKnownBase.Get();

	if (CurrentBase == PreviousBase)
	{
		return;
	}

	const int32 bAcceptedBase = IsAcceptedEmbarkedBase(CurrentBase) ? 1 : 0;
	if (!PreviousBase && CurrentBase)
	{
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("Base acquired: %s on %s | Accepted=%d"),
			*GetNameSafe(CurrentBase),
			*GetNameSafe(CurrentBase->GetOwner()),
			bAcceptedBase);
	}
	else if (PreviousBase && !CurrentBase)
	{
		UE_LOG(
			LogSubCrewMovement,
			Warning,
			TEXT("Base lost! Was: %s on %s"),
			*GetNameSafe(PreviousBase),
			*GetNameSafe(PreviousBase->GetOwner()));
	}
	else if (PreviousBase && CurrentBase)
	{
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("Base changed: %s on %s -> %s on %s | Accepted=%d"),
			*GetNameSafe(PreviousBase),
			*GetNameSafe(PreviousBase->GetOwner()),
			*GetNameSafe(CurrentBase),
			*GetNameSafe(CurrentBase->GetOwner()),
			bAcceptedBase);
	}

	LastKnownBase = CurrentBase;
}

void USubCrewMovementComponent::DebugDrawState()
{
	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	if (!bDebugDrawCrewMovement || !Frame || !Frame->IsFrameValid() || !CharacterOwner || !GetWorld())
	{
		return;
	}

	const FTransform SubTransform = Frame->GetSubTransform();
	const FVector FrameOrigin = SubTransform.GetLocation();
	const FVector ExpectedWorldPosition = Frame->LocalToWorld(RelativeLocation);
	const FVector ActualWorldPosition = CharacterOwner->GetActorLocation();
	const FColor SupportColor = bHasAcceptedEmbarkedBase ? FColor::Green : (bHasValidEmbarkedFloor ? FColor::Yellow : FColor::Red);

	DrawDebugSphere(GetWorld(), FrameOrigin, 24.f, 12, FColor::Green, false, 0.f, 0, 1.5f);
	DrawDebugSphere(GetWorld(), ExpectedWorldPosition, 16.f, 12, FColor::Cyan, false, 0.f, 0, 1.25f);
	DrawDebugSphere(GetWorld(), ActualWorldPosition, 16.f, 12, FColor::Yellow, false, 0.f, 0, 1.25f);
	DrawDebugSphere(GetWorld(), ActualWorldPosition + FVector(0.f, 0.f, 18.f), 10.f, 10, SupportColor, false, 0.f, 0, 1.5f);
	DrawDebugLine(GetWorld(), ExpectedWorldPosition, ActualWorldPosition, FColor::Red, false, 0.f, 0, 1.25f);
	DrawDebugDirectionalArrow(
		GetWorld(),
		FrameOrigin,
		FrameOrigin + (SubTransform.GetUnitAxis(EAxis::X) * 100.f),
		20.f,
		FColor::Blue,
		false,
		0.f,
		0,
		2.f);

	if (!BraceQueryOrigin.IsNearlyZero())
	{
		const FVector ForwardEnd = BraceQueryOrigin + (CharacterOwner->GetActorForwardVector() * BraceProbeDistanceCm);
		const FVector RightEnd = BraceQueryOrigin + (CharacterOwner->GetActorRightVector() * BraceProbeDistanceCm);
		const FVector LeftEnd = BraceQueryOrigin - (CharacterOwner->GetActorRightVector() * BraceProbeDistanceCm);
		DrawDebugSphere(GetWorld(), BraceQueryOrigin, 8.f, 8, FColor::Silver, false, 0.f, 0, 1.25f);
		DrawDebugLine(GetWorld(), BraceQueryOrigin, ForwardEnd, FColor::White, false, 0.f, 0, 1.f);
		DrawDebugLine(GetWorld(), BraceQueryOrigin, RightEnd, FColor::White, false, 0.f, 0, 1.f);
		DrawDebugLine(GetWorld(), BraceQueryOrigin, LeftEnd, FColor::White, false, 0.f, 0, 1.f);
	}

	if (bHasNearbyBraceSupport)
	{
		DrawDebugSphere(GetWorld(), NearbyBraceWorldLocation, 10.f, 10, FColor::Orange, false, 0.f, 0, 1.5f);
		DrawDebugDirectionalArrow(
			GetWorld(),
			NearbyBraceWorldLocation,
			NearbyBraceWorldLocation + (NearbyBraceWorldNormal * 40.f),
			10.f,
			FColor::Orange,
			false,
			0.f,
			0,
			1.5f);
		DrawDebugLine(GetWorld(), BraceQueryOrigin, NearbyBraceWorldLocation, FColor::Orange, false, 0.f, 0, 1.5f);
	}
}

void USubCrewMovementComponent::LogPeriodicState(float DeltaTime)
{
	if (!bDebugLogCrewMovement || !CharacterOwner)
	{
		DebugLogTimer = 0.f;
		return;
	}

	DebugLogTimer += DeltaTime;
	if (DebugLogTimer < 1.f)
	{
		return;
	}

	DebugLogTimer = 0.f;

	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	const UPrimitiveComponent* CurrentBase = CharacterOwner->GetMovementBase();
	const FTransform SubTransform = Frame ? Frame->GetSubTransform() : FTransform::Identity;
	const FVector FrameDeltaLocation = Frame ? Frame->GetFrameLocationDelta() : FVector::ZeroVector;
	const FRotator FrameDeltaRotation = Frame ? Frame->GetFrameRotationDelta() : FRotator::ZeroRotator;

	UE_LOG(
		LogSubCrewMovement,
		Log,
		TEXT("CrewState | Embarked=%d | Mode=%s | Base=%s on %s | WorldLoc=%s | RelLoc=%s | RelRot=%s | SubLoc=%s | FrameDeltaLoc=%s | FrameDeltaRot=%s | SupportFloor=%d | AcceptedBase=%d | Recover=%d | SupportQ=%.2f | Brace=%d | BraceDist=%.1f | LocalVel=%s | LocalAccel=%s | LocalAngVel=%s | LocalAngAccel=%s"),
		IsEmbarked() ? 1 : 0,
		*GetMovementName(),
		*GetNameSafe(CurrentBase),
		*GetNameSafe(CurrentBase ? CurrentBase->GetOwner() : nullptr),
		*CharacterOwner->GetActorLocation().ToCompactString(),
		*RelativeLocation.ToCompactString(),
		*RelativeRotation.ToCompactString(),
		*SubTransform.GetLocation().ToCompactString(),
		*FrameDeltaLocation.ToCompactString(),
		*FrameDeltaRotation.ToCompactString(),
		bHasValidEmbarkedFloor ? 1 : 0,
		bHasAcceptedEmbarkedBase ? 1 : 0,
		bNeedsEmbarkedFloorRecovery ? 1 : 0,
		SupportQuality01,
		bHasNearbyBraceSupport ? 1 : 0,
		NearbyBraceDistanceCm,
		*LocalSubLinearVelocity.ToCompactString(),
		*LocalSubLinearAcceleration.ToCompactString(),
		*LocalSubAngularVelocityDegrees.ToCompactString(),
		*LocalSubAngularAccelerationDegrees.ToCompactString());
}

void USubCrewMovementComponent::InitializeForSubmarine()
{
	USubInteriorFrameComponent* Frame = GetInteriorFrame();
	const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner);
	const bool bFrameValid = Frame && Frame->IsFrameValid() && CharacterOwner;

	if (bFrameValid)
	{
		UpdateRelativeState();
		UpdateInertialState();
	}

	// Tick ordering fix: ensure this CMC ticks AFTER the submarine has moved AND after the frame delta is computed.
	// Without this, UpdateBasedMovement sees zero base delta because the sub
	// hasn't simulated yet this frame, causing one-frame-lag jitter.
	// We also depend on the InteriorFrame to ensure yaw compensation uses fresh deltas.
	bool bSubTickSet = false;
	bool bFrameTickSet = false;

	if (Crew && Crew->CurrentSubmarine)
	{
		if (USubMovementComponent* SubMov = Crew->CurrentSubmarine->SubMovement)
		{
			AddTickPrerequisiteComponent(SubMov);
			bSubTickSet = true;
		}

		if (USubInteriorFrameComponent* FrameComp = Crew->CurrentSubmarine->InteriorFrame)
		{
			AddTickPrerequisiteComponent(FrameComp);
			bFrameTickSet = true;
		}
	}

	LastKnownBase.Reset();
	LastEmbarkedFloorComponent = nullptr;
	bHasValidEmbarkedFloor = false;
	bHasAcceptedEmbarkedBase = false;
	bNeedsEmbarkedFloorRecovery = false;
	SupportQuality01 = 0.f;
	bHasNearbyBraceSupport = false;
	NearbyBraceDistanceCm = 0.f;
	NearbyBraceWorldLocation = FVector::ZeroVector;
	NearbyBraceWorldNormal = FVector::ZeroVector;
	BraceQueryOrigin = FVector::ZeroVector;
	FloorRecoveryTimer = 0.f;
	DebugLogTimer = 0.f;

	UE_LOG(
		LogSubCrewMovement,
		Log,
		TEXT("InitializeForSubmarine | Sub=%s | CharacterLoc=%s | RelLoc=%s | FrameValid=%d | SubPrereq=%d | FramePrereq=%d"),
		*GetNameSafe(Crew ? Crew->CurrentSubmarine : nullptr),
		CharacterOwner ? *CharacterOwner->GetActorLocation().ToCompactString() : TEXT("None"),
		*RelativeLocation.ToCompactString(),
		bFrameValid ? 1 : 0,
		bSubTickSet ? 1 : 0,
		bFrameTickSet ? 1 : 0);
}

void USubCrewMovementComponent::RefreshEmbarkedFlooring()
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	SetMovementMode(MOVE_Walking);
	Velocity = FVector::ZeroVector;
	bForceNextFloorCheck = true;
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	SetBaseFromFloor(CurrentFloor);
	UpdateFloorFromAdjustment();
	CheckAndLogBaseChange();
	UpdateSupportState();
	UpdateBraceState();

	UE_LOG(
		LogSubCrewMovement,
		Log,
		TEXT("RefreshEmbarkedFlooring | Base=%s on %s | FloorWalkable=%d | AcceptedBase=%d | Recover=%d | SupportQ=%.2f | FloorDist=%.2f | LineDist=%.2f | Loc=%s"),
		*GetNameSafe(CharacterOwner->GetMovementBase()),
		*GetNameSafe(CharacterOwner->GetMovementBase() ? CharacterOwner->GetMovementBase()->GetOwner() : nullptr),
		CurrentFloor.IsWalkableFloor() ? 1 : 0,
		IsAcceptedEmbarkedBase(CharacterOwner->GetMovementBase()) ? 1 : 0,
		bNeedsEmbarkedFloorRecovery ? 1 : 0,
		SupportQuality01,
		CurrentFloor.FloorDist,
		CurrentFloor.LineDist,
		*CharacterOwner->GetActorLocation().ToCompactString());
}
