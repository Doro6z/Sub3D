#include "SubCrewMovementComponent.h"
#include "Sub3DDebugSettings.h"

#include "SubCrewCharacter.h"
#include "SubInteriorFrameComponent.h"
#include "SubmarineBase.h"
#include "SubMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"

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
	SetIsReplicatedByDefault(true);
}

void USubCrewMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Grid-space authority owns yaw while embarked; CMC's base-rotation carry is bypassed.
	bIgnoreBaseRotation = bIsGridSpaceAuthority;

	// ─── REBASE (pre-CMC) ───
	// Teleport the capsule to the expected world pose so CMC sees a static world
	// around the character. Must use UpdatedComponent (not SetActorLocation) to
	// avoid triggering overlap/move events before the real CMC tick.
	if (bIsGridSpaceAuthority && IsEmbarked() && UpdatedComponent && CharacterOwner)
	{
		if (USubInteriorFrameComponent* Frame = GetInteriorFrame())
		{
			const FTransform SubTransform = Frame->GetSubTransform();

			const FVector RebasedWorldPos = SubTransform.TransformPosition(GridSpaceTransform.GetLocation());
			const FRotator LocalRot = GridSpaceTransform.Rotator();
			const FRotator SubRot = SubTransform.Rotator();
			// Yaw-only capsule rotation: the capsule must stay aligned with world gravity
			// so CMC's collision resolution behaves. Pitch/roll of the sub are cosmetic only.
			const FRotator RebasedWorldRot(0.f, FRotator::NormalizeAxis(SubRot.Yaw + LocalRot.Yaw), 0.f);

			UpdatedComponent->SetWorldLocationAndRotation(
				RebasedWorldPos, RebasedWorldRot.Quaternion(),
				/*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);

			// Carry the controller yaw by the sub's yaw delta so the locally-controlled
			// view stays anchored relative to the sub.
			if (CharacterOwner->IsLocallyControlled())
			{
				if (AController* C = CharacterOwner->GetController())
				{
					const FRotator PrevSubRot = LastSubWorldTransform.Rotator();
					const float DeltaYaw = FRotator::NormalizeAxis(SubRot.Yaw - PrevSubRot.Yaw);
					if (!FMath::IsNearlyZero(DeltaYaw, KINDA_SMALL_NUMBER))
					{
						FRotator CtrlRot = C->GetControlRotation();
						CtrlRot.Yaw = FRotator::NormalizeAxis(CtrlRot.Yaw + DeltaYaw);
						C->SetControlRotation(CtrlRot);
					}
				}
			}

			LastSubWorldTransform = SubTransform;
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickPosture(DeltaTime);

	if (IsEmbarked())
	{
		// ─── EXTRACT (post-CMC) ───
		// The CMC has applied input, gravity, and collision resolution in world space.
		// Project the new world pose back into sub-local space; that becomes the
		// authoritative GridSpaceTransform for next frame's rebase.
		if (bIsGridSpaceAuthority && CharacterOwner)
		{
			if (USubInteriorFrameComponent* Frame = GetInteriorFrame())
			{
				const FTransform SubTransform = Frame->GetSubTransform();
				const FVector NewLocalPos = SubTransform.InverseTransformPosition(CharacterOwner->GetActorLocation());
				const FRotator WorldRot = CharacterOwner->GetActorRotation();
				const FRotator SubRot = SubTransform.Rotator();
				const float LocalYaw = FRotator::NormalizeAxis(WorldRot.Yaw - SubRot.Yaw);
				GridSpaceTransform = FTransform(FRotator(0.f, LocalYaw, 0.f).Quaternion(), NewLocalPos);
			}
		}

		UpdateRelativeState(DeltaTime);
		UpdateInertialState();
		UpdateSupportState();
		AttemptEmbarkedFloorRecovery(DeltaTime);
		UpdateBraceState();
		UpdateHandIKProbes();
		UpdateFootIKTraces();
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
		RelativeLinearVelocity = FVector::ZeroVector;
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
		bHasPreviousRelativeLocation = false;
		PreviousRelativeLocation = FVector::ZeroVector;
		FloorRecoveryTimer = 0.f;
		DebugLogTimer = 0.f;
		LastSubWorldTransform = FTransform::Identity;
		GridSpaceTransform = FTransform::Identity;
	}
}

void USubCrewMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
	// In grid-space authority mode the rebase transports the crew; CMC's
	// base-carry must not also apply the base delta or we double-advance.
	if (bIsGridSpaceAuthority)
	{
		return;
	}
	Super::UpdateBasedMovement(DeltaSeconds);
}

void USubCrewMovementComponent::UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation)
{
	// Same rule for rotation: the rebase owns the capsule yaw relative to the sub.
	if (bIsGridSpaceAuthority)
	{
		return;
	}
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
	const ASubmarineBase* Submarine = GetCurrentSubmarine();
	return Submarine && Submarine->IsInteriorWalkableComponent(CandidateBase);
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

void USubCrewMovementComponent::UpdateRelativeState(float DeltaTime)
{
	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	if (!Frame || !CharacterOwner)
	{
		RelativeLinearVelocity = FVector::ZeroVector;
		bHasPreviousRelativeLocation = false;
		return;
	}

	// In grid-space authority mode GridSpaceTransform is the post-extract pose;
	// derive Rel* directly from it rather than re-projecting world state.
	// Legacy path (not authority) keeps the previous WorldToLocal derivation
	// so off-sub moving bases continue to report a Rel pose.
	const FVector NewRelativeLocation = bIsGridSpaceAuthority
		? GridSpaceTransform.GetLocation()
		: Frame->WorldToLocal(CharacterOwner->GetActorLocation());
	const FRotator NewRelativeRotation = bIsGridSpaceAuthority
		? GridSpaceTransform.Rotator()
		: Frame->WorldToLocalRotation(CharacterOwner->GetActorRotation());

	const float RelFrameDeltaCm = bHasPreviousRelativeLocation
		? static_cast<float>((NewRelativeLocation - PreviousRelativeLocation).Size())
		: 0.f;

	if (bHasPreviousRelativeLocation && DeltaTime > KINDA_SMALL_NUMBER)
	{
		RelativeLinearVelocity = (NewRelativeLocation - PreviousRelativeLocation) / DeltaTime;
	}
	else
	{
		RelativeLinearVelocity = FVector::ZeroVector;
	}

	RelativeLocation = NewRelativeLocation;
	RelativeRotation = NewRelativeRotation;

	const USub3DDebugSettings* DebugSettingsRef = GetDefault<USub3DDebugSettings>();
	if (DebugSettingsRef->bLogCrewJitter)
	{
		const UPrimitiveComponent* Base = CharacterOwner->GetMovementBase();
		const ENetRole LocalRole = CharacterOwner->GetLocalRole();
		const FString MovementModeStr = GetMovementName();
		const FVector SubWorldLoc = Frame->GetOwner() ? Frame->GetOwner()->GetActorLocation() : FVector::ZeroVector;
		const float RelSpeedCmPerSec = (bHasPreviousRelativeLocation && DeltaTime > KINDA_SMALL_NUMBER)
			? RelFrameDeltaCm / DeltaTime
			: 0.f;
		const bool bJitterSpike = bHasPreviousRelativeLocation && RelSpeedCmPerSec > DebugSettingsRef->CrewJitterWarnVelocityCmPerSec;
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("Jitter | Role=%d | dt=%.4f | World=%s | Sub=%s | Rel=%s | RelDeltaCm=%.2f | RelSpeed=%.0f cm/s%s%s | Mode=%s | Falling=%d | Base=%s | FrameValid=%d | GridAuth=%d"),
			static_cast<int32>(LocalRole),
			DeltaTime,
			*CharacterOwner->GetActorLocation().ToCompactString(),
			*SubWorldLoc.ToCompactString(),
			*NewRelativeLocation.ToCompactString(),
			RelFrameDeltaCm,
			RelSpeedCmPerSec,
			bJitterSpike ? TEXT(" [SPIKE]") : TEXT(""),
			bIsGridSpaceAuthority ? TEXT("") : TEXT(" [LegacyBaseCarry]"),
			*MovementModeStr,
			IsFalling() ? 1 : 0,
			*GetNameSafe(Base),
			Frame->IsFrameValid() ? 1 : 0,
			bIsGridSpaceAuthority ? 1 : 0);
		if (bJitterSpike)
		{
			UE_LOG(
				LogSubCrewMovement,
				Warning,
				TEXT("Jitter SPIKE | RelSpeed=%.0f cm/s > threshold=%.0f cm/s | dt=%.4f | Mode=%s | Falling=%d | Base=%s | GridAuth=%d"),
				RelSpeedCmPerSec,
				DebugSettingsRef->CrewJitterWarnVelocityCmPerSec,
				DeltaTime,
				*MovementModeStr,
				IsFalling() ? 1 : 0,
				*GetNameSafe(Base),
				bIsGridSpaceAuthority ? 1 : 0);
		}
	}

	PreviousRelativeLocation = RelativeLocation;
	bHasPreviousRelativeLocation = true;

	if (DebugSettingsRef->bLogCrewMovement)
	{
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("RelativeState | WorldLoc=%s | RelLoc=%s | RelVel=%s | RelRot=%s"),
			*CharacterOwner->GetActorLocation().ToCompactString(),
			*RelativeLocation.ToCompactString(),
			*RelativeLinearVelocity.ToCompactString(),
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

	if (GetDefault<USub3DDebugSettings>()->bLogCrewMovement)
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
	if (!GetDefault<USub3DDebugSettings>()->bDrawCrewMovement || !Frame || !Frame->IsFrameValid() || !CharacterOwner || !GetWorld())
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
	if (!GetDefault<USub3DDebugSettings>()->bLogCrewMovement || !CharacterOwner)
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
		UpdateRelativeState(0.f);
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
	RelativeLinearVelocity = FVector::ZeroVector;
	PreviousRelativeLocation = RelativeLocation;
	bHasPreviousRelativeLocation = bFrameValid;
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


// ── Posture System ──────────────────────────────────────────────────

void USubCrewMovementComponent::SetPostureTarget(float Alpha)
{
	PostureTarget = FMath::Clamp(Alpha, 0.f, 1.f);

	if (PostureTarget < 0.75f)
	{
		SetRunningState(false);
	}

	if (ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner))
	{
		if (!Crew->HasAuthority())
		{
			Crew->ServerSetPostureTarget(PostureTarget);
		}
	}
}

void USubCrewMovementComponent::AddPostureDelta(float Delta)
{
	SetPostureTarget(PostureTarget + Delta);
}

void USubCrewMovementComponent::TickPosture(float DeltaTime)
{
	if (PostureAlpha < 0.75f && bIsRunning)
	{
		SetRunningState(false);
	}

	if (FMath::IsNearlyEqual(PostureAlpha, PostureTarget, 0.001f))
	{
		PostureAlpha = PostureTarget;
	}
	else
	{
		PostureAlpha = FMath::FInterpTo(PostureAlpha, PostureTarget, DeltaTime, PostureInterpSpeed);
	}

	// Update capsule half-height
	if (ACharacter* Char = GetCharacterOwner())
	{
		const float CurrentHalfHeight = Char->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		const float NewHalfHeight = FMath::Lerp(ProneHalfHeight, StandingHalfHeight, PostureAlpha);
		const float HalfHeightDelta = NewHalfHeight - CurrentHalfHeight;
		Char->GetCapsuleComponent()->SetCapsuleHalfHeight(NewHalfHeight, true);
		if (!FMath::IsNearlyZero(HalfHeightDelta, KINDA_SMALL_NUMBER))
		{
			Char->AddActorWorldOffset(FVector(0.f, 0.f, HalfHeightDelta), false, nullptr, ETeleportType::TeleportPhysics);
			bForceNextFloorCheck = true;
		}

		// Update camera Z if FPS camera exists
		if (ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(Char))
		{
			if (Crew->FPSCamera)
			{
				const float NewCameraZ = FMath::Lerp(ProneCameraZ, StandingCameraZ, PostureAlpha);
				FVector CamLoc = Crew->FPSCamera->GetRelativeLocation();
				CamLoc.Z = NewCameraZ;
				Crew->FPSCamera->SetRelativeLocation(CamLoc);
			}
		}
	}
}

// ── Run System ──────────────────────────────────────────────────

void USubCrewMovementComponent::RequestRunStart()
{
	SetRunningState(true);
}

void USubCrewMovementComponent::RequestRunStop()
{
	SetRunningState(false);
}

ECrewPostureState USubCrewMovementComponent::GetPostureState() const
{
	if (PostureAlpha <= 0.25f)
	{
		return ECrewPostureState::Prone;
	}

	if (PostureAlpha < 0.75f)
	{
		return ECrewPostureState::Crouched;
	}

	return ECrewPostureState::Standing;
}

float USubCrewMovementComponent::GetPostureSpeedScale() const
{
	return FMath::Lerp(0.3f, 1.f, PostureAlpha);
}

float USubCrewMovementComponent::GetDesiredWalkSpeedMultiplier() const
{
	const float RunScale = (bIsRunning && GetPostureState() == ECrewPostureState::Standing) ? RunSpeedMultiplier : 1.f;
	return GetPostureSpeedScale() * RunScale;
}

void USubCrewMovementComponent::SetRunningState(bool bNewRunning)
{
	ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner);
	const bool bResolvedRunning =
		bNewRunning
		&& PostureTarget >= 0.75f
		&& (!Crew || !Crew->bIsSwimmingByFlood);

	if (bIsRunning == bResolvedRunning)
	{
		return;
	}

	bIsRunning = bResolvedRunning;

	if (Crew && !Crew->HasAuthority())
	{
		Crew->ServerSetRunning(bIsRunning);
	}
}


// ── Hand IK Probes ──────────────────────────────────────────────

void USubCrewMovementComponent::UpdateHandIKProbes()
{
	// Reset all probes
	for (int32 i = 0; i < 6; ++i)
	{
		HandProbes[i].bHit = false;
		HandProbes[i].Distance = 0.f;
		HandProbes[i].WorldLocation = FVector::ZeroVector;
		HandProbes[i].WorldNormal = FVector::ZeroVector;
	}

	if (!CharacterOwner || !ShouldEvaluateHandIK())
	{
		return;
	}

	const FVector ActorLoc = CharacterOwner->GetActorLocation();
	const FRotator ActorRot = CharacterOwner->GetActorRotation();
	const float CurrentHalfHeight = CharacterOwner->GetCapsuleComponent()
		? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: FMath::Lerp(ProneHalfHeight, StandingHalfHeight, PostureAlpha);

	// Probe directions in actor local space: Forward, Right, Left, BackRight, BackLeft, Back
	struct FProbeSetup
	{
		FVector LocalDir;
		float HeightFraction;
	};

	const FProbeSetup Setups[6] = {
		{ FVector(0, -1, 0), 0.8f },   // 0: Left hand — left
		{ FVector(0,  1, 0), 0.8f },   // 1: Right hand — right
		{ FVector(0, -1, 0), 0.5f },   // 2: Left hip — left, lower
		{ FVector(0,  1, 0), 0.5f },   // 3: Right hip — right, lower
		{ FVector(1, -0.5f, 0), 0.8f },// 4: Left shoulder — forward-left
		{ FVector(1,  0.5f, 0), 0.8f },// 5: Right shoulder — forward-right
	};

	const float ProbeDistance = 60.f; // cm, arm's reach

	for (int32 i = 0; i < 6; ++i)
	{
		const FVector WorldDir = ActorRot.RotateVector(Setups[i].LocalDir.GetSafeNormal());
		const FVector Start = ActorLoc + FVector(0, 0, CurrentHalfHeight * Setups[i].HeightFraction);
		const FVector End = Start + WorldDir * ProbeDistance;

		FHitResult Hit;
		if (QueryBraceSupportHit(Start, End, Hit))
		{
			HandProbes[i].bHit = true;
			HandProbes[i].WorldLocation = Hit.ImpactPoint;
			HandProbes[i].WorldNormal = Hit.ImpactNormal;
			HandProbes[i].Distance = Hit.Distance;
		}
	}
}

bool USubCrewMovementComponent::ShouldEvaluateHandIK() const
{
	const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner);
	if (!CharacterOwner || !IsEmbarked() || (Crew && Crew->bIsSwimmingByFlood))
	{
		return false;
	}

	return SupportQuality01 < 0.8f || LocalSubAngularVelocityDegrees.GetAbsMax() > 2.f;
}


// ── Foot IK Traces ──────────────────────────────────────────────

void USubCrewMovementComponent::UpdateFootIKTraces()
{
	FootIK_R = FVector::ZeroVector;
	FootIK_L = FVector::ZeroVector;

	const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner);
	if (!CharacterOwner || !CharacterOwner->GetCapsuleComponent() || (Crew && Crew->bIsSwimmingByFlood))
	{
		return;
	}

	const FVector ActorLoc = CharacterOwner->GetActorLocation();
	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// Trace down from each foot position
	const FVector RFootBase = ActorLoc + FVector(0, 8, -HalfHeight);
	const FVector LFootBase = ActorLoc + FVector(0, -8, -HalfHeight);
	const float TraceDepth = 30.f;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);

	FHitResult HitR, HitL;
	const UWorld* World = GetWorld();

	if (World->LineTraceSingleByChannel(HitR, RFootBase + FVector(0,0,10), RFootBase - FVector(0,0,TraceDepth), ECC_GameTraceChannel2, Params))
	{
		FootIK_R.Z = HitR.ImpactPoint.Z - (ActorLoc.Z - HalfHeight);
	}

	if (World->LineTraceSingleByChannel(HitL, LFootBase + FVector(0,0,10), LFootBase - FVector(0,0,TraceDepth), ECC_GameTraceChannel2, Params))
	{
		FootIK_L.Z = HitL.ImpactPoint.Z - (ActorLoc.Z - HalfHeight);
	}
}


void USubCrewMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USubCrewMovementComponent, PostureAlpha);
	DOREPLIFETIME(USubCrewMovementComponent, bIsRunning);
}
