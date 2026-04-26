#include "SubCrewMovementComponent.h"
#include "Sub3DDebugSettings.h"

#include "CompartmentVolumeComponent.h"
#include "SubCrewCharacter.h"
#include "SubCrewNetTypes.h"
#include "SubInteriorFrameComponent.h"
#include "SubmarineBase.h"
#include "SubMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
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

	// FP EVA swim stub: ocean is state-driven (ECrewEmbarkState::Outside), not PhysicsVolume-driven.
	// We piggy-back on MOVE_Flying with water-tuned params so it reads as "swim" in game terms.
	// Swap for MOVE_Swimming + buoyancy when a proper ocean sim lands.
	MaxFlySpeed = 250.f;                  // below walking (300) → heavy-water feel
	BrakingDecelerationFlying = 1200.f;   // quick decel on input release → water drag

	// The rebase is the sole transport path. CMC must never impart the MovementBase's
	// velocity into the crew's Velocity on base change (ladder -> deck, deck -> deck, etc.)
	// — that would inject V_sub world-space velocity and fight the rebase each tick,
	// producing persistent jitter scaling with sub speed.
	bImpartBaseVelocityX = false;
	bImpartBaseVelocityY = false;
	bImpartBaseVelocityZ = false;
	bImpartBaseAngularVelocity = false;

	// Custom network move data container: packs GridSpaceTransform + EmbarkState + handoff
	// event bits into the ServerMove RPC payload. Server reads these in MoveAutonomous.
	SubCrewMoveDataContainer = MakeUnique<FCharacterNetworkMoveDataContainer_SubCrew>();
	SetNetworkMoveDataContainer(*SubCrewMoveDataContainer);
}

void USubCrewMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Lazy authority latch — RISING-EDGE ONLY.
	// Fires only when submarine binding transitions false->true (spawn into sub, replication).
	// After that, EmbarkState is explicit (SetEmbarkState via EnterOnFoot, Board, Disembark,
	// HandleHullCrossing) and the latch must NOT re-set Embarked when the crew is legitimately
	// Outside (EVA) while still holding a sub pointer.
	const bool bHasSubmarineBindingNow = HasSubmarineBinding();
	const bool bJustGainedSubBinding = bHasSubmarineBindingNow && !bHadSubmarineBindingLastTick;

	if (bJustGainedSubBinding && !IsGridAuthoritative() && CharacterOwner)
	{
		if (USubInteriorFrameComponent* Frame = GetInteriorFrame())
		{
			const FTransform SubTransform = Frame->GetSubTransform();
			const FVector LocalPos = SubTransform.InverseTransformPosition(CharacterOwner->GetActorLocation());
			const FRotator WorldRot = CharacterOwner->GetActorRotation();
			const float LocalYaw = FRotator::NormalizeAxis(WorldRot.Yaw - SubTransform.Rotator().Yaw);
			GridSpaceTransform = FTransform(FRotator(0.f, LocalYaw, 0.f).Quaternion(), LocalPos);
			LastSubWorldTransform = SubTransform;
			SetEmbarkState(ECrewEmbarkState::Embarked);

			UE_LOG(
				LogSubCrewMovement,
				Log,
				TEXT("GridAuthority ON (lazy edge) | Sub=%s | LocalPos=%s | LocalYaw=%.2f"),
				*GetNameSafe(Frame->GetOwner()),
				*LocalPos.ToCompactString(),
				LocalYaw);
		}
	}
	else if (!bHasSubmarineBindingNow && IsGridAuthoritative())
	{
		// Defensive: lost the sub pointer while still flagged grid-authoritative. Reset to Outside.
		SetEmbarkState(ECrewEmbarkState::Outside);
		UE_LOG(LogSubCrewMovement, Log, TEXT("GridAuthority OFF (sub lost) | submarine context ended"));
	}

	bHadSubmarineBindingLastTick = bHasSubmarineBindingNow;

	// Grid-space authority owns yaw while embarked; CMC's base-rotation carry is bypassed.
	bIgnoreBaseRotation = IsGridAuthoritative();

	// ─── REBASE (pre-CMC) ───
	// Teleport the capsule to the expected world pose so CMC sees a static world
	// around the character. Must use UpdatedComponent (not SetActorLocation) to
	// avoid triggering overlap/move events before the real CMC tick.
	if (IsGridAuthoritative() && HasSubmarineBinding() && UpdatedComponent && CharacterOwner)
	{
		if (USubInteriorFrameComponent* Frame = GetInteriorFrame())
		{
			const FTransform SubTransform = Frame->GetSubTransform();
			// On SimProxy peers, GetEffectiveGridSpaceTransform returns a time-lerped value
			// between the last two replicated GridSpaceTransform updates. Owner/Authority
			// returns the raw GridSpaceTransform (locally computed each tick).
			const FTransform EffectiveGS = GetEffectiveGridSpaceTransform();

			const FVector RebasedWorldPos = SubTransform.TransformPosition(EffectiveGS.GetLocation());
			const FRotator LocalRot = EffectiveGS.Rotator();
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

	if (HasSubmarineBinding())
	{
		// ─── EXTRACT (post-CMC) ───
		// The CMC has applied input, gravity, and collision resolution in world space.
		// Project the new world pose back into sub-local space; that becomes the
		// authoritative GridSpaceTransform for next frame's rebase.
		// Simulated Proxies must NEVER extract; they rely purely on the replicated GridSpaceTransform
		// from the server, otherwise they create a destructive feedback loop against SimulatedTick.
		if (IsGridAuthoritative() && CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
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
	if (IsGridAuthoritative())
	{
		return;
	}
	Super::UpdateBasedMovement(DeltaSeconds);
}

void USubCrewMovementComponent::UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation)
{
	// Same rule for rotation: the rebase owns the capsule yaw relative to the sub.
	if (IsGridAuthoritative())
	{
		return;
	}
	Super::UpdateBasedRotation(FinalRotation, ReducedRotation);
}

void USubCrewMovementComponent::SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation)
{
	// Don't seed a MeshTranslationOffset while grid-authoritative. The rebase places the
	// actor at SubTransform × GridSpaceTransform each tick, which intentionally diverges
	// from ReplicatedMovement.Location (= server's world-space pose). If we let CMC build
	// an offset from that delta, SmoothClientPosition decays it toward zero each frame and
	// the mesh oscillates against the rebase → jitter on simulated-proxy peers. No-op in
	// grid mode prevents the offset from ever being created; mesh tracks actor exactly.
	if (IsGridAuthoritative())
	{
		return;
	}
	Super::SmoothCorrection(OldLocation, OldRotation, NewLocation, NewRotation);
}

bool USubCrewMovementComponent::ServerCheckClientError(
	float ClientTimeStamp,
	float DeltaTime,
	const FVector& Accel,
	const FVector& ClientWorldLocation,
	const FVector& RelativeClientLocation,
	UPrimitiveComponent* ClientMovementBase,
	FName ClientBaseBoneName,
	uint8 ClientMovementMode)
{
	// World-space error check is meaningless when the crew is embarked: client reports
	// SubXf_client_interp * GridSpaceTransform_client, server computes SubXf_server_sim *
	// GridSpaceTransform_server, these ALWAYS differ by the sub interp lag. Bypassing the
	// check trusts the client's reported pose. Safe in cooperative FP; Phase 3.2 replaces
	// this with FSavedMove_Character + local-space validation.
	if (IsGridAuthoritative())
	{
		return false;
	}
	return Super::ServerCheckClientError(
		ClientTimeStamp, DeltaTime, Accel,
		ClientWorldLocation, RelativeClientLocation,
		ClientMovementBase, ClientBaseBoneName, ClientMovementMode);
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

bool USubCrewMovementComponent::HasSubmarineBinding() const
{
	return GetInteriorFrame() != nullptr;
}

void USubCrewMovementComponent::OnRep_GridSpaceTransform()
{
	// SimulatedProxy peer just received a new GridSpaceTransform value. Capture the previous
	// rendered transform as the new "prev" anchor for the lerp, and record receive times so
	// the rebase can compute interpolation alpha each tick.
	const double RealNow = FPlatformTime::Seconds();
	if (!bHasReceivedGridSpaceReplication)
	{
		// First snapshot: no prev to lerp from. Snap the rendered state to the received value.
		GridSpacePrevRendered = GridSpaceTransform;
		GridSpacePrevReceiveRealTime = RealNow;
		GridSpaceTargetReceiveRealTime = RealNow;
		bHasReceivedGridSpaceReplication = true;
	}
	else
	{
		// Anchor new prev at the currently-rendered (interpolated) value, not the previous
		// target — handles the case where alpha hadn't reached 1 when the new value arrived.
		GridSpacePrevRendered = GetEffectiveGridSpaceTransform();
		GridSpacePrevReceiveRealTime = GridSpaceTargetReceiveRealTime;
		GridSpaceTargetReceiveRealTime = RealNow;
	}
}

FTransform USubCrewMovementComponent::GetEffectiveGridSpaceTransform() const
{
	// Owning client (AutonomousProxy) and Authority compute GridSpaceTransform locally
	// each tick — no smoothing needed, the value is fresh.
	if (!CharacterOwner || CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		return GridSpaceTransform;
	}

	// SimulatedProxy peer: lerp between the previous rendered value and the latest received
	// target over the inter-replication interval. Smooths stepwise replication into continuous
	// motion when a peer is walking.
	if (!bHasReceivedGridSpaceReplication
		|| GridSpaceTargetReceiveRealTime <= GridSpacePrevReceiveRealTime)
	{
		return GridSpaceTransform;
	}

	const double RealNow = FPlatformTime::Seconds();
	const double InterpDuration = GridSpaceTargetReceiveRealTime - GridSpacePrevReceiveRealTime;
	const float Alpha = FMath::Clamp(
		static_cast<float>((RealNow - GridSpaceTargetReceiveRealTime) / InterpDuration), 0.f, 1.f);

	const FVector LerpedLoc = FMath::Lerp(
		GridSpacePrevRendered.GetLocation(),
		GridSpaceTransform.GetLocation(),
		Alpha);
	const FRotator LerpedRot = FMath::Lerp(
		GridSpacePrevRendered.Rotator(),
		GridSpaceTransform.Rotator(),
		Alpha);
	return FTransform(LerpedRot.Quaternion(), LerpedLoc);
}

void USubCrewMovementComponent::SetEmbarkState(ECrewEmbarkState NewState)
{
	if (EmbarkState == NewState)
	{
		return;
	}

	const ECrewEmbarkState OldState = EmbarkState;
	EmbarkState = NewState;

	UE_LOG(
		LogSubCrewMovement,
		Log,
		TEXT("EmbarkState: %d -> %d"),
		static_cast<int32>(OldState),
		static_cast<int32>(NewState));
}

void USubCrewMovementComponent::SetPendingHandoff(ECrewHandoffKind Kind)
{
	PendingHandoff = Kind;
}

ECrewHandoffKind USubCrewMovementComponent::ConsumePendingHandoff()
{
	const ECrewHandoffKind Result = PendingHandoff;
	PendingHandoff = ECrewHandoffKind::None;
	return Result;
}

FNetworkPredictionData_Client* USubCrewMovementComponent::GetPredictionData_Client() const
{
	check(CharacterOwner != nullptr);
	if (ClientPredictionData == nullptr)
	{
		USubCrewMovementComponent* MutableThis = const_cast<USubCrewMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_SubCrew(*this);
	}
	return ClientPredictionData;
}

void USubCrewMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);

	// Server-side: after CMC processes the client's move, apply the client's reported
	// grid-space state directly (trust model for FP co-op — production would bound the
	// per-tick delta). The replicated UPROPERTY(COND_SkipOwner) then broadcasts to peers.
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_Authority && !CharacterOwner->IsLocallyControlled())
	{
		if (const FCharacterNetworkMoveData* CurrentMoveData = GetCurrentNetworkMoveData())
		{
			const FCharacterNetworkMoveData_SubCrew* SubMoveData = static_cast<const FCharacterNetworkMoveData_SubCrew*>(CurrentMoveData);
			GridSpaceTransform = SubMoveData->GridSpaceTransform;

			const ECrewEmbarkState ReportedState = static_cast<ECrewEmbarkState>(SubMoveData->EmbarkStateByte);
			if (ReportedState != EmbarkState)
			{
				SetEmbarkState(ReportedState);
			}

			// Handoff event bit: server mirrors the state flip already fired by the client.
			// Velocity blending was applied client-side; we just ensure the server state converges.
			if (SubMoveData->Handoff != ECrewHandoffKind::None)
			{
				UE_LOG(
					LogSubCrewMovement,
					Log,
					TEXT("ServerMove received handoff event | Kind=%d | Crew=%s"),
					static_cast<int32>(SubMoveData->Handoff),
					*GetNameSafe(CharacterOwner));
			}
		}
	}
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
	const FVector NewRelativeLocation = IsGridAuthoritative()
		? GridSpaceTransform.GetLocation()
		: Frame->WorldToLocal(CharacterOwner->GetActorLocation());
	const FRotator NewRelativeRotation = IsGridAuthoritative()
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
			IsGridAuthoritative() ? TEXT("") : TEXT(" [LegacyBaseCarry]"),
			*MovementModeStr,
			IsFalling() ? 1 : 0,
			*GetNameSafe(Base),
			Frame->IsFrameValid() ? 1 : 0,
			IsGridAuthoritative() ? 1 : 0);
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
				IsGridAuthoritative() ? 1 : 0);
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
	bNeedsEmbarkedFloorRecovery = HasSubmarineBinding() && (!bHasValidEmbarkedFloor || !bHasAcceptedEmbarkedBase || !MovementBase);

	if (!HasSubmarineBinding())
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
	// In grid-space authority mode the rebase guarantees the player is at the
	// expected pose each tick, so the recovery sweep (which also wipes Velocity
	// via RefreshEmbarkedFlooring) would fight the CMC's legitimate motion.
	if (IsGridAuthoritative())
	{
		FloorRecoveryTimer = 0.f;
		return;
	}

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

	if (!CharacterOwner || !HasSubmarineBinding() || !GetWorld())
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
	const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>();
	if (!CharacterOwner || !GetWorld())
	{
		return;
	}

	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	const bool bHasValidFrame = Frame && Frame->IsFrameValid();
	const FTransform SubTransform = bHasValidFrame ? Frame->GetSubTransform() : FTransform::Identity;

	// Sphere / line debug gizmos need a valid Frame to have a meaningful reference frame.
	// HUD on-screen messages work regardless (useful for EVA validation).
	if (bHasValidFrame && Settings->bDrawCrewMovement)
	{
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

	// ─── On-screen validation HUD — (MovementState, EnvironmentContext) ───
	// Runs independently of submarine binding — the HUD must stay visible during EVA so
	// we can verify that the state machine actually transitioned to Outside after a hull
	// boundary crossing. When there's no sub binding, Grid/Sub lines show <n/a>.
	if (Settings->bDrawCrewGridAuthority && GEngine && CharacterOwner->IsLocallyControlled())
	{
		const FVector WorldPos = CharacterOwner->GetActorLocation();
		const float WorldYaw = CharacterOwner->GetActorRotation().Yaw;

		const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner);
		const UCompartmentVolumeComponent* Comp = Crew ? Crew->CurrentCompartment.Get() : nullptr;

		const FString StateStr = UEnum::GetValueAsString(EmbarkState);
		const FString CompStr = Comp ? Comp->CompartmentId.ToString() : FString(TEXT("<ocean>"));
		const FString ModeStr = GetMovementName();
		const FString SubBindStr = bHasValidFrame ? FString(TEXT("bound")) : FString(TEXT("unbound"));

		const float DisplayTime = 0.f;  // Refreshed every tick via stable keys.
		const uint64 KeyBase = static_cast<uint64>(CharacterOwner->GetUniqueID()) * 1000;
		GEngine->AddOnScreenDebugMessage(KeyBase + 1, DisplayTime, FColor::Cyan,
			FString::Printf(TEXT("EmbarkState : %s   |   Mode : %s   |   Compartment : %s   |   Sub : %s"),
				*StateStr, *ModeStr, *CompStr, *SubBindStr));

		if (bHasValidFrame)
		{
			const FVector LocalPos = GridSpaceTransform.GetLocation();
			const float LocalYaw = GridSpaceTransform.Rotator().Yaw;
			const FVector SubPos = SubTransform.GetLocation();
			const float SubYaw = SubTransform.Rotator().Yaw;
			GEngine->AddOnScreenDebugMessage(KeyBase + 2, DisplayTime, FColor::Cyan,
				FString::Printf(TEXT("Grid  : X=%8.1f  Y=%8.1f  Z=%8.1f  Yaw=%7.2f"), LocalPos.X, LocalPos.Y, LocalPos.Z, LocalYaw));
			GEngine->AddOnScreenDebugMessage(KeyBase + 4, DisplayTime, FColor::Cyan,
				FString::Printf(TEXT("Sub   : X=%8.1f  Y=%8.1f  Z=%8.1f  Yaw=%7.2f"), SubPos.X, SubPos.Y, SubPos.Z, SubYaw));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(KeyBase + 2, DisplayTime, FColor::Yellow, TEXT("Grid  : <n/a — no sub binding>"));
			GEngine->AddOnScreenDebugMessage(KeyBase + 4, DisplayTime, FColor::Yellow, TEXT("Sub   : <n/a — no sub binding>"));
		}
		GEngine->AddOnScreenDebugMessage(KeyBase + 3, DisplayTime, FColor::Cyan,
			FString::Printf(TEXT("World : X=%8.1f  Y=%8.1f  Z=%8.1f  Yaw=%7.2f"), WorldPos.X, WorldPos.Y, WorldPos.Z, WorldYaw));
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
		TEXT("CrewState | SubBound=%d | EmbarkState=%d | Mode=%s | Base=%s on %s | WorldLoc=%s | RelLoc=%s | RelRot=%s | SubLoc=%s | FrameDeltaLoc=%s | FrameDeltaRot=%s | SupportFloor=%d | AcceptedBase=%d | Recover=%d | SupportQ=%.2f | Brace=%d | BraceDist=%.1f | LocalVel=%s | LocalAccel=%s | LocalAngVel=%s | LocalAngAccel=%s"),
		HasSubmarineBinding() ? 1 : 0,
		static_cast<int32>(EmbarkState),
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
	if (!CharacterOwner || !HasSubmarineBinding() || (Crew && Crew->bIsSwimmingByFlood))
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
	// Owner computes GridSpaceTransform / EmbarkState locally via its own rebase + hull boundary;
	// non-owning clients get the server-authoritative values for their peer-crew rendering.
	DOREPLIFETIME_CONDITION(USubCrewMovementComponent, GridSpaceTransform, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(USubCrewMovementComponent, EmbarkState, COND_SkipOwner);
}
