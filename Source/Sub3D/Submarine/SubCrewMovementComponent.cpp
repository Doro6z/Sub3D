#include "SubCrewMovementComponent.h"
#include "Sub3DDebugSettings.h"

#include "CompartmentVolumeComponent.h"
#include "LadderClimbComponent.h"
#include "SubCrewCharacter.h"
#include "SubCrewNetTypes.h"
#include "SubmarineBase.h"
#include "SubMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PhysicsVolume.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubCrewMovement, Log, All);

namespace
{
static TAutoConsoleVariable<int32> CVarCrewMotionChainTrace(
	TEXT("Sub3D.Crew.MotionChainTrace"),
	0,
	TEXT("Diagnostic: 1 logs per-tick crew motion-chain snapshots at pre-rebase, post-rebase, post-CMC, and post-extract."));

bool IsCrewMotionChainTraceEnabled()
{
	const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>();
	return CVarCrewMotionChainTrace.GetValueOnGameThread() != 0
		|| (Settings && Settings->bLogMotionChainTrace);
}

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

float StepAngleConstant(float CurrentDeg, float TargetDeg, float MaxStepDeg)
{
	const float DeltaDeg = FMath::FindDeltaAngleDegrees(CurrentDeg, TargetDeg);
	const float StepDeg = FMath::Clamp(DeltaDeg, -MaxStepDeg, MaxStepDeg);
	return FRotator::NormalizeAxis(CurrentDeg + StepDeg);
}

FVector2D ClampMoveAxis(FVector2D MoveAxis)
{
	MoveAxis.X = FMath::Clamp(MoveAxis.X, -1.f, 1.f);
	MoveAxis.Y = FMath::Clamp(MoveAxis.Y, -1.f, 1.f);

	if (MoveAxis.SizeSquared() > 1.f)
	{
		MoveAxis.Normalize();
	}

	return MoveAxis;
}

float ClampInputAxis(float Axis)
{
	return FMath::Clamp(Axis, -1.f, 1.f);
}

ECrewLocomotionStance ResolveLocomotionStance(float PostureAlpha, bool bIsSwimming)
{
	if (bIsSwimming)
	{
		return ECrewLocomotionStance::Swimming;
	}

	if (PostureAlpha <= 0.25f)
	{
		return ECrewLocomotionStance::Prone;
	}

	if (PostureAlpha < 0.75f)
	{
		return ECrewLocomotionStance::Crouched;
	}

	return ECrewLocomotionStance::Standing;
}

ECrewLocomotionGait ResolveLocomotionGait(ECrewLocomotionStance Stance, bool bIsMoving, bool bIsRunning)
{
	if (Stance == ECrewLocomotionStance::Swimming)
	{
		return ECrewLocomotionGait::Swim;
	}

	if (!bIsMoving)
	{
		return ECrewLocomotionGait::Idle;
	}

	if (Stance == ECrewLocomotionStance::Prone)
	{
		return ECrewLocomotionGait::ProneCrawl;
	}

	if (Stance == ECrewLocomotionStance::Crouched)
	{
		return ECrewLocomotionGait::CrouchWalk;
	}

	return bIsRunning ? ECrewLocomotionGait::Sprint : ECrewLocomotionGait::Walk;
}

const TCHAR* MovementModeToDebugName(EMovementMode Mode)
{
	switch (Mode)
	{
	case MOVE_None:       return TEXT("None");
	case MOVE_Walking:    return TEXT("Walking");
	case MOVE_NavWalking: return TEXT("NavWalking");
	case MOVE_Falling:    return TEXT("Falling");
	case MOVE_Swimming:   return TEXT("Swimming");
	case MOVE_Flying:     return TEXT("Flying");
	case MOVE_Custom:     return TEXT("Custom");
	default:              return TEXT("Unknown");
	}
}
}

USubCrewMovementComponent::USubCrewMovementComponent()
{
	SetIsReplicatedByDefault(true);

	// EVA swim is state-driven from hull-boundary crossings, not PhysicsVolume-driven.
	MaxSwimSpeed = 240.f;
	BrakingDecelerationSwimming = 900.f;
	Buoyancy = 1.f;
	NavAgentProps.bCanSwim = true;

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

void USubCrewMovementComponent::SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode)
{
	if (EmbarkState == ECrewEmbarkState::Outside && NewMovementMode == MOVE_Falling)
	{
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("Exterior swim blocked native Falling fallback | Crew=%s | CurrentMode=%s | PhysicsVolume=%s water=%d"),
			*GetNameSafe(CharacterOwner),
			MovementModeToDebugName(MovementMode),
			*GetNameSafe(GetPhysicsVolume()),
			(GetPhysicsVolume() && GetPhysicsVolume()->bWaterVolume) ? 1 : 0);
		NewMovementMode = MOVE_Swimming;
		NewCustomMode = 0;
	}

	Super::SetMovementMode(NewMovementMode, NewCustomMode);
}

bool USubCrewMovementComponent::IsInWater() const
{
	return EmbarkState == ECrewEmbarkState::Outside || Super::IsInWater();
}

void USubCrewMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (!bReceivedMoveInputThisFrame)
	{
		PendingPlanarMoveAxis = FVector2D::ZeroVector;
	}
	if (!bReceivedVerticalMoveInputThisFrame)
	{
		PendingVerticalMoveAxis = 0.f;
	}

	LastMoveIntent = BuildMoveIntent(PendingPlanarMoveAxis, PendingVerticalMoveAxis);
	if (CharacterOwner && LastMoveIntent.bHasMoveInput)
	{
		AddInputVector(LastMoveIntent.WorldMoveDirection * LastMoveIntent.MoveInputStrength, false);
	}

	bReceivedMoveInputThisFrame = false;
	bReceivedVerticalMoveInputThisFrame = false;

	// Lazy authority latch — RISING-EDGE ONLY.
	// Fires only when submarine binding transitions false->true (spawn into sub, replication).
	// After that, EmbarkState is explicit (SetEmbarkState via EnterOnFoot, Board, Disembark,
	// HandleHullCrossing) and the latch must NOT re-set Embarked when the crew is legitimately
	// Outside (EVA) while still holding a sub pointer.
	const bool bHasSubmarineBindingNow = HasSubmarineBinding();
	const bool bJustGainedSubBinding = bHasSubmarineBindingNow && !bHadSubmarineBindingLastTick;

	if (bJustGainedSubBinding && !IsGridAuthoritative() && CharacterOwner)
	{
		if (const ASubmarineBase* Sub = GetCurrentSubmarine())
		{
			const FTransform SubTransform = Sub->GetActorTransform();
			const FVector LocalPos = SubTransform.InverseTransformPosition(CharacterOwner->GetActorLocation());
			const FRotator WorldRot = CharacterOwner->GetActorRotation();
			const float LocalYaw = FRotator::NormalizeAxis(WorldRot.Yaw - SubTransform.Rotator().Yaw);
			GridSpaceTransform = FTransform(FRotator(0.f, LocalYaw, 0.f).Quaternion(), LocalPos);
			GridFacingYawDeg = LocalYaw;
			DesiredGridFacingYawDeg = LocalYaw;
			GridFacingYawRateDegPerSec = 0.f;
			bHasGridFacingYaw = true;
			LastSubWorldTransform = SubTransform;
			SetEmbarkState(ECrewEmbarkState::Embarked);

			UE_LOG(
				LogSubCrewMovement,
				Log,
				TEXT("GridAuthority ON (lazy edge) | Sub=%s | LocalPos=%s | LocalYaw=%.2f"),
				*GetNameSafe(Sub),
				*LocalPos.ToCompactString(),
				LocalYaw);
		}
	}
	else if (!bHasSubmarineBindingNow && IsGridAuthoritative())
	{
		// Defensive: lost the sub pointer while still flagged grid-authoritative. Reset to Outside.
		SetEmbarkState(ECrewEmbarkState::Outside);
		ResetGridFacingYaw();
		UE_LOG(LogSubCrewMovement, Log, TEXT("GridAuthority OFF (sub lost) | submarine context ended"));
	}

	bHadSubmarineBindingLastTick = bHasSubmarineBindingNow;

	// Grid-space authority owns yaw while embarked; CMC's base-rotation carry is bypassed.
	bIgnoreBaseRotation = IsGridAuthoritative();
	if (CharacterOwner)
	{
		// While grid-authoritative, actor yaw is SubYaw + GridYaw. The local camera
		// still follows ControlRotation through FPSCamera/TPSCameraBoom, but the
		// Character must not copy ControlRotation back onto ActorYaw between rebases.
		CharacterOwner->bUseControllerRotationYaw = !IsGridAuthoritative();
	}

	// ─── TRACE capture: PRE-REBASE ───
	const bool bTraceMotionChain = IsCrewMotionChainTraceEnabled()
		&& CharacterOwner && CharacterOwner->IsLocallyControlled();
	FMotionChainSnapshot TracePreReb;
	FMotionChainSnapshot TracePostReb;
	FMotionChainSnapshot TracePostCMC;
	FMotionChainSnapshot TracePostExtract;
	if (bTraceMotionChain)
	{
		CaptureTraceSnapshot(TracePreReb);
	}

	// ─── LADDER CLIMB (pre-rebase) ───
	// Drives GridSpaceTransform along the active ladder line; the rebase below then
	// sets the capsule to the resulting sub-relative pose. Skipped when not climbing.
	if (CurrentLadder && CharacterOwner && CharacterOwner->IsLocallyControlled())
	{
		TickLadderClimb(DeltaTime);
	}

	// ─── REBASE (pre-CMC) ───
	// Teleport the capsule to the expected world pose so CMC sees a static world
	// around the character. Must use UpdatedComponent (not SetActorLocation) to
	// avoid triggering overlap/move events before the real CMC tick.
	if (IsGridAuthoritative() && HasSubmarineBinding() && UpdatedComponent && CharacterOwner)
	{
		if (const ASubmarineBase* Sub = GetCurrentSubmarine())
		{
			const FTransform SubTransform = Sub->GetActorTransform();
			UpdateGridFacingYaw(DeltaTime, SubTransform);

			const FVector RebasedWorldPos = SubTransform.TransformPosition(GridSpaceTransform.GetLocation());
			const FRotator LocalRot = GridSpaceTransform.Rotator();
			const FRotator SubRot = SubTransform.Rotator();
			// Yaw-only capsule rotation: the capsule must stay aligned with world gravity
			// so CMC's collision resolution behaves. Pitch/roll of the sub are cosmetic only.
			const FRotator RebasedWorldRot(0.f, FRotator::NormalizeAxis(SubRot.Yaw + LocalRot.Yaw), 0.f);

			UpdatedComponent->SetWorldLocationAndRotation(
				RebasedWorldPos, RebasedWorldRot.Quaternion(),
				/*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);

			// This rebase is our authoritative moving-frame transport, not an
			// external displacement that CMC should diagnose on the next
			// PerformMovement call. Without this, CMC sees the sub-carried
			// position delta as bTeleportedSinceLastUpdate every frame while the
			// submarine moves. That forces floor validation/adjustment on moving
			// stair geometry and can create persistent correction jitter.
			LastUpdateLocation = UpdatedComponent->GetComponentLocation();
			LastUpdateRotation = UpdatedComponent->GetComponentQuat();
			bTeleportedSinceLastUpdate = false;

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

	if (bTraceMotionChain)
	{
		CaptureTraceSnapshot(TracePostReb);
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bTraceMotionChain)
	{
		CaptureTraceSnapshot(TracePostCMC);
	}

	TickPosture(DeltaTime);

	if (HasSubmarineBinding())
	{
		// ─── EXTRACT (post-CMC) ───
		// The CMC has applied input, gravity, and collision resolution in world space.
		// Project the new world pose back into sub-local space; that becomes the
		// authoritative GridSpaceTransform for next frame's rebase.
		if (IsGridAuthoritative() && CharacterOwner)
		{
			if (const ASubmarineBase* Sub = GetCurrentSubmarine())
			{
				const FTransform SubTransform = Sub->GetActorTransform();
				const FVector NewLocalPos = SubTransform.InverseTransformPosition(CharacterOwner->GetActorLocation());
				const float LocalYaw = bHasGridFacingYaw
					? GridFacingYawDeg
					: GridSpaceTransform.Rotator().Yaw;
				GridSpaceTransform = FTransform(FRotator(0.f, LocalYaw, 0.f).Quaternion(), NewLocalPos);
			}
		}

		UpdateRelativeState(DeltaTime);
		UpdateInertialState();
		UpdateSupportState();
		UpdateLocomotionFrame();
		AttemptEmbarkedFloorRecovery(DeltaTime);
		UpdateBraceState();
		UpdateHandIKProbes();
		UpdateFootIKTraces();
		CheckAndLogBaseChange();
		LogPeriodicState(DeltaTime);
		DebugDrawState();

		if (bTraceMotionChain)
		{
			CaptureTraceSnapshot(TracePostExtract);
			EmitMotionChainTrace(TracePreReb, TracePostReb, TracePostCMC, TracePostExtract, DeltaTime);
			TracePrevPostExtract = TracePostExtract;
			bHasTracePrev = true;
		}
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
		ResetGridFacingYaw();
	}

	UpdateLocomotionFrame();
	LogMotionChainTick(DeltaTime);
}

void USubCrewMovementComponent::LogMotionChainTick(float DeltaTime) const
{
	const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>();
	if (!Settings || !Settings->bLogPresentationChain || !CharacterOwner)
	{
		return;
	}
	// One emit per render frame per machine — gate on locally-controlled so neither
	// peer SimProxies nor server-Authority duplicates fire the line.
	if (!CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	const ENetRole LocalRole = CharacterOwner->GetLocalRole();
	const FVector CrewWorld = CharacterOwner->GetActorLocation();
	const FRotator CrewWorldRot = CharacterOwner->GetActorRotation();
	const FVector GridLocal = GridSpaceTransform.GetLocation();
	const float GridYaw = GridSpaceTransform.Rotator().Yaw;
	const FString ModeStr = GetMovementName();
	const UPrimitiveComponent* Base = CharacterOwner->GetMovementBase();
	const ASubmarineBase* Sub = GetCurrentSubmarine();
	const USubMovementComponent* SubMov = Sub ? Sub->SubMovement : nullptr;

	const FVector SubWorld = Sub ? Sub->GetActorLocation() : FVector::ZeroVector;
	const FVector SubVel = SubMov ? SubMov->Velocity : FVector::ZeroVector;
	const FVector SubAcc = SubMov ? SubMov->LinearAcceleration : FVector::ZeroVector;
	const float SubRudder = SubMov ? SubMov->GetRudderInput() : 0.f;
	const float SubDive = SubMov ? SubMov->GetDivePlaneInput() : 0.f;
	const float SubThrust = SubMov ? SubMov->GetThrustInput() : 0.f;
	const int32 SubSimFrame = SubMov ? SubMov->GetSimFrameCounter() : 0;

	UE_LOG(
		LogSubCrewMovement,
		Log,
		TEXT("Motion chain tick | Role=%d dt=%.4f | Sub.W=%s SimFrame=%d Vel=%s Acc=%s Rud=%.2f Dive=%.2f Thr=%.2f | Crew.W=%s Yaw=%.1f Grid.L=%s GridYaw=%.1f | Embark=%d Mode=%s Falling=%d Base=%s | RelVel=%s SupportQ=%.2f"),
		static_cast<int32>(LocalRole),
		DeltaTime,
		*SubWorld.ToCompactString(),
		SubSimFrame,
		*SubVel.ToCompactString(),
		*SubAcc.ToCompactString(),
		SubRudder, SubDive, SubThrust,
		*CrewWorld.ToCompactString(),
		CrewWorldRot.Yaw,
		*GridLocal.ToCompactString(),
		GridYaw,
		static_cast<int32>(EmbarkState),
		*ModeStr,
		IsFalling() ? 1 : 0,
		*GetNameSafe(Base),
		*RelativeLinearVelocity.ToCompactString(),
		SupportQuality01);
}

void USubCrewMovementComponent::CaptureTraceSnapshot(FMotionChainSnapshot& Out) const
{
	if (!CharacterOwner)
	{
		return;
	}
	const ASubmarineBase* Sub = GetCurrentSubmarine();
	const USubMovementComponent* SubMov = Sub ? Sub->SubMovement : nullptr;

	Out.SubLoc = Sub ? Sub->GetActorLocation() : FVector::ZeroVector;
	Out.SubRot = Sub ? Sub->GetActorRotation() : FRotator::ZeroRotator;
	Out.SubVel = SubMov ? SubMov->Velocity : FVector::ZeroVector;
	Out.CrewWorld = CharacterOwner->GetActorLocation();
	Out.CrewWorldRot = CharacterOwner->GetActorRotation();
	Out.CMCVelocity = Velocity;
	Out.GridLocal = GridSpaceTransform.GetLocation();
	Out.GridYaw = GridSpaceTransform.Rotator().Yaw;
	Out.MovementMode = static_cast<uint8>(MovementMode);
	Out.bFalling = IsFalling();
	const UPrimitiveComponent* Base = CharacterOwner->GetMovementBase();
	Out.BaseName = Base ? Base->GetFName() : NAME_None;
	Out.BaseOwnerName = (Base && Base->GetOwner()) ? Base->GetOwner()->GetFName() : NAME_None;
	Out.FloorImpact = CurrentFloor.HitResult.ImpactPoint;
	Out.FloorDist = CurrentFloor.FloorDist;
	Out.bWalkable = CurrentFloor.IsWalkableFloor();
}

void USubCrewMovementComponent::EmitMotionChainTrace(
	const FMotionChainSnapshot& PreReb,
	const FMotionChainSnapshot& PostReb,
	const FMotionChainSnapshot& PostCMC,
	const FMotionChainSnapshot& PostExtract,
	float DeltaTime) const
{
	// Compute deltas at each handoff.
	const FVector SubD = PostExtract.SubLoc - PreReb.SubLoc;
	const float SubDYaw = FRotator::NormalizeAxis(PostExtract.SubRot.Yaw - PreReb.SubRot.Yaw);

	const FVector RebaseD = PostReb.CrewWorld - PreReb.CrewWorld;
	const float RebaseDYaw = FRotator::NormalizeAxis(PostReb.CrewWorldRot.Yaw - PreReb.CrewWorldRot.Yaw);

	const FVector CMCD = PostCMC.CrewWorld - PostReb.CrewWorld;
	const float CMCDYaw = FRotator::NormalizeAxis(PostCMC.CrewWorldRot.Yaw - PostReb.CrewWorldRot.Yaw);

	const FVector GridD = PostExtract.GridLocal - PreReb.GridLocal;
	const float GridDYaw = FRotator::NormalizeAxis(PostExtract.GridYaw - PreReb.GridYaw);

	const FVector CMCVelD = PostCMC.CMCVelocity - PreReb.CMCVelocity;
	const FVector FloorD = PostExtract.FloorImpact - PreReb.FloorImpact;

	// Detect transactional events by comparing PRE and POST-CMC.
	TStringBuilder<256> Events;
	if (PreReb.MovementMode != PostCMC.MovementMode)
	{
		Events.Appendf(TEXT("[ModeChange %u→%u] "), PreReb.MovementMode, PostCMC.MovementMode);
	}
	if (PreReb.bFalling != PostCMC.bFalling)
	{
		Events.Appendf(TEXT("[FallingFlip %d→%d] "), PreReb.bFalling ? 1 : 0, PostCMC.bFalling ? 1 : 0);
	}
	if (PreReb.BaseName != PostCMC.BaseName)
	{
		Events.Appendf(TEXT("[BaseChange %s→%s] "), *PreReb.BaseName.ToString(), *PostCMC.BaseName.ToString());
	}
	if (PreReb.bWalkable != PostCMC.bWalkable)
	{
		Events.Appendf(TEXT("[WalkableFlip %d→%d] "), PreReb.bWalkable ? 1 : 0, PostCMC.bWalkable ? 1 : 0);
	}
	const float FloorJump = static_cast<float>(FloorD.Size());
	if (FloorJump > 5.f)
	{
		Events.Appendf(TEXT("[FloorJump %.2fcm] "), FloorJump);
	}
	const FString EventsText = Events.Len() > 0 ? FString(Events.ToString()) : FString(TEXT("(none)"));

	UE_LOG(
		LogSubCrewMovement,
		Log,
		TEXT("TRACE | dt=%.4f Mode=%u Falling=%d Base=%s\n")
		TEXT("  Sub: Loc=%s Yaw=%.2f Vel=%s | Δ over tick=%s ΔYaw=%.3f\n")
		TEXT("  PreReb:  Crew=%s Yaw=%.2f CMC.Vel=%s GridLocal=%s GridYaw=%.2f\n")
		TEXT("  PostReb: Crew=%s Yaw=%.2f | Rebase Δ=%s ΔYaw=%.3f (expected ≈ SubΔ since GridUnchanged)\n")
		TEXT("  PostCMC: Crew=%s Yaw=%.2f CMC.Vel=%s | CMC sim Δ=%s ΔYaw=%.3f CMC.VelΔ=%s\n")
		TEXT("  PostExt: GridLocal=%s GridYaw=%.2f | Grid extract Δ=%s ΔYaw=%.3f\n")
		TEXT("  Floor: Impact=%s Dist=%.2f Walkable=%d FloorΔ=%.2fcm\n")
		TEXT("  EVENTS: %s"),
		DeltaTime, static_cast<uint32>(PostExtract.MovementMode), PostExtract.bFalling ? 1 : 0,
		*PostExtract.BaseName.ToString(),
		*PreReb.SubLoc.ToCompactString(), PreReb.SubRot.Yaw, *PreReb.SubVel.ToCompactString(),
		*SubD.ToCompactString(), SubDYaw,
		*PreReb.CrewWorld.ToCompactString(), PreReb.CrewWorldRot.Yaw,
		*PreReb.CMCVelocity.ToCompactString(),
		*PreReb.GridLocal.ToCompactString(), PreReb.GridYaw,
		*PostReb.CrewWorld.ToCompactString(), PostReb.CrewWorldRot.Yaw,
		*RebaseD.ToCompactString(), RebaseDYaw,
		*PostCMC.CrewWorld.ToCompactString(), PostCMC.CrewWorldRot.Yaw,
		*PostCMC.CMCVelocity.ToCompactString(),
		*CMCD.ToCompactString(), CMCDYaw, *CMCVelD.ToCompactString(),
		*PostExtract.GridLocal.ToCompactString(), PostExtract.GridYaw,
		*GridD.ToCompactString(), GridDYaw,
		*PostExtract.FloorImpact.ToCompactString(), PostExtract.FloorDist, PostExtract.bWalkable ? 1 : 0,
		FloorJump,
		*EventsText);
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

void USubCrewMovementComponent::PhysicsVolumeChanged(APhysicsVolume* NewVolume)
{
	if (EmbarkState == ECrewEmbarkState::Outside && (!NewVolume || !NewVolume->bWaterVolume))
	{
		return;
	}

	Super::PhysicsVolumeChanged(NewVolume);
}

void USubCrewMovementComponent::SetDefaultMovementMode()
{
	if (EmbarkState == ECrewEmbarkState::Outside)
	{
		SetMovementMode(MOVE_Swimming);
		return;
	}

	Super::SetDefaultMovementMode();
}

void USubCrewMovementComponent::PhysSwimming(float DeltaTime, int32 Iterations)
{
	const APhysicsVolume* PhysicsVolume = GetPhysicsVolume();
	if (EmbarkState != ECrewEmbarkState::Outside || (PhysicsVolume && PhysicsVolume->bWaterVolume))
	{
		Super::PhysSwimming(DeltaTime, Iterations);
		return;
	}

	if (DeltaTime <= KINDA_SMALL_NUMBER || !CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	RestorePreAdditiveRootMotionVelocity();
	bJustTeleported = false;

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		CalcVelocity(
			DeltaTime,
			FMath::Max(0.f, Sub3DExteriorSwimFluidFriction),
			true,
			GetMaxBrakingDeceleration());
	}

	ApplyRootMotionToVelocity(DeltaTime);

	const FVector Adjusted = Velocity * DeltaTime;
	if (Adjusted.IsNearlyZero())
	{
		return;
	}

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		HandleImpact(Hit, DeltaTime, Adjusted);
		SlideAlongSurface(Adjusted, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && !bJustTeleported)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
}

void USubCrewMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner);
	const APhysicsVolume* PhysicsVolume = GetPhysicsVolume();
	UE_LOG(
		LogSubCrewMovement,
		Log,
		TEXT("MovementModeChanged | %s(%d/%u) -> %s(%d/%u) | Crew=%s | Role=%d | EmbarkState=%d | Sub=%s | Comp=%s | PhysicsVolume=%s water=%d"),
		MovementModeToDebugName(PreviousMovementMode),
		static_cast<int32>(PreviousMovementMode),
		static_cast<uint32>(PreviousCustomMode),
		MovementModeToDebugName(MovementMode),
		static_cast<int32>(MovementMode),
		static_cast<uint32>(CustomMovementMode),
		*GetNameSafe(CharacterOwner),
		CharacterOwner ? static_cast<int32>(CharacterOwner->GetLocalRole()) : -1,
		static_cast<int32>(EmbarkState),
		*GetNameSafe(GetCurrentSubmarine()),
		Crew ? *Crew->CurrentCompartmentId.ToString() : TEXT("<no-crew>"),
		*GetNameSafe(PhysicsVolume),
		(PhysicsVolume && PhysicsVolume->bWaterVolume) ? 1 : 0);
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

bool USubCrewMovementComponent::HasSubmarineBinding() const
{
	return GetCurrentSubmarine() != nullptr;
}

void USubCrewMovementComponent::SetEmbarkState(ECrewEmbarkState NewState)
{
	if (EmbarkState == NewState)
	{
		return;
	}

	const ECrewEmbarkState OldState = EmbarkState;
	EmbarkState = NewState;
	if (NewState == ECrewEmbarkState::Outside)
	{
		ResetGridFacingYaw();
	}

	UE_LOG(
		LogSubCrewMovement,
		Log,
		TEXT("EmbarkState: %d -> %d"),
		static_cast<int32>(OldState),
		static_cast<int32>(NewState));
}

void USubCrewMovementComponent::ResetGridFacingYaw()
{
	GridFacingYawDeg = 0.f;
	DesiredGridFacingYawDeg = 0.f;
	GridFacingYawRateDegPerSec = 0.f;
	bHasGridFacingYaw = false;
}

bool USubCrewMovementComponent::IsCrewSwimming() const
{
	return MovementMode == MOVE_Swimming;
}

FVector USubCrewMovementComponent::BuildWorldMoveInput(FVector2D MoveAxis, float VerticalAxis) const
{
	const FVector2D ClampedMoveAxis = ClampMoveAxis(MoveAxis);
	const float ClampedVerticalAxis = ClampInputAxis(VerticalAxis);

	float ControlYawDeg = CharacterOwner ? CharacterOwner->GetActorRotation().Yaw : 0.f;
	FRotator ControlRotation(0.f, ControlYawDeg, 0.f);
	if (CharacterOwner)
	{
		if (const AController* Controller = CharacterOwner->GetController())
		{
			ControlRotation = Controller->GetControlRotation();
			ControlYawDeg = ControlRotation.Yaw;
		}
	}

	const FRotator ControlYawRot(0.f, FRotator::NormalizeAxis(ControlYawDeg), 0.f);
	const FVector YawForward = ControlYawRot.Vector();
	const FVector YawRight = FRotationMatrix(ControlYawRot).GetScaledAxis(EAxis::Y);

	if (!IsCrewSwimming())
	{
		return (YawForward * ClampedMoveAxis.X + YawRight * ClampedMoveAxis.Y).GetClampedToMaxSize(1.f);
	}

	const FVector CameraForward = ControlRotation.Vector();
	const FVector SwimInput =
		(CameraForward * ClampedMoveAxis.X)
		+ (YawRight * ClampedMoveAxis.Y)
		+ (FVector::UpVector * ClampedVerticalAxis * FMath::Max(0.f, SwimVerticalInputScale));

	return SwimInput.GetClampedToMaxSize(1.f);
}

FCrewMoveIntent USubCrewMovementComponent::BuildMoveIntent(FVector2D MoveAxis, float VerticalAxis) const
{
	FCrewMoveIntent Intent;
	Intent.MoveAxis = ClampMoveAxis(MoveAxis);
	Intent.VerticalAxis = IsCrewSwimming() ? ClampInputAxis(VerticalAxis) : 0.f;
	const FVector RawMoveInput = BuildWorldMoveInput(Intent.MoveAxis, Intent.VerticalAxis);
	Intent.MoveInputStrength = FMath::Clamp(RawMoveInput.Size(), 0.f, 1.f);
	Intent.bHasMoveInput = Intent.MoveInputStrength > KINDA_SMALL_NUMBER;

	float ControlYawDeg = CharacterOwner ? CharacterOwner->GetActorRotation().Yaw : 0.f;
	if (CharacterOwner)
	{
		if (const AController* Controller = CharacterOwner->GetController())
		{
			ControlYawDeg = Controller->GetControlRotation().Yaw;
		}
	}

	Intent.ControlYawDeg = FRotator::NormalizeAxis(ControlYawDeg);

	if (Intent.bHasMoveInput)
	{
		Intent.WorldMoveDirection = RawMoveInput.GetSafeNormal();
	}

	if (Intent.WorldMoveDirection.IsNearlyZero())
	{
		Intent.WorldMoveDirection = FVector::ZeroVector;
		Intent.MoveWorldYawDeg = Intent.ControlYawDeg;
	}
	else
	{
		Intent.MoveWorldYawDeg = Intent.WorldMoveDirection.ToOrientationRotator().Yaw;
	}
	Intent.DesiredWorldYawDeg = Intent.ControlYawDeg;

	if (const ASubmarineBase* Sub = GetCurrentSubmarine())
	{
		const float SubYawDeg = Sub->GetActorRotation().Yaw;
		const FRotator SubYawRot(0.f, SubYawDeg, 0.f);
		Intent.LocalMoveDirection = SubYawRot.UnrotateVector(Intent.WorldMoveDirection).GetSafeNormal();
		Intent.DesiredGridYawDeg = FRotator::NormalizeAxis(Intent.DesiredWorldYawDeg - SubYawDeg);
	}
	else
	{
		Intent.LocalMoveDirection = Intent.WorldMoveDirection;
		Intent.DesiredGridYawDeg = Intent.DesiredWorldYawDeg;
	}

	return Intent;
}

void USubCrewMovementComponent::ApplyCrewPlanarMoveInput(FVector2D MoveAxis)
{
	PendingPlanarMoveAxis = ClampMoveAxis(MoveAxis);
	LastMoveIntent = BuildMoveIntent(PendingPlanarMoveAxis, PendingVerticalMoveAxis);
	bReceivedMoveInputThisFrame = true;
}

void USubCrewMovementComponent::ApplyCrewVerticalMoveInput(float Axis)
{
	PendingVerticalMoveAxis = ClampInputAxis(Axis);
	LastMoveIntent = BuildMoveIntent(PendingPlanarMoveAxis, PendingVerticalMoveAxis);
	bReceivedVerticalMoveInputThisFrame = true;
}

void USubCrewMovementComponent::UpdateLocomotionFrame()
{
	FCrewLocomotionFrame Frame;
	const ASubmarineBase* Sub = GetCurrentSubmarine();
	const bool bSwimming = IsCrewSwimming();
	const bool bGrid = IsGridAuthoritative() && Sub != nullptr;

	Frame.bIsGridAuthoritative = bGrid;
	Frame.bIsSwimming = bSwimming;
	Frame.bIsRunning = bIsRunning;
	Frame.PostureAlpha = PostureAlpha;
	Frame.SupportQuality01 = SupportQuality01;
	Frame.LocalTurnRateDegPerSec = bGrid ? GridFacingYawRateDegPerSec : 0.f;

	if (CharacterOwner)
	{
		Frame.BodyWorldYawDeg = CharacterOwner->GetActorRotation().Yaw;
	}

	if (Sub)
	{
		Frame.BodyLocalYawDeg = FRotator::NormalizeAxis(Frame.BodyWorldYawDeg - Sub->GetActorRotation().Yaw);
	}
	else
	{
		Frame.BodyLocalYawDeg = Frame.BodyWorldYawDeg;
	}

	if (bGrid)
	{
		Frame.LocalVelocity = RelativeLinearVelocity;
		Frame.WorldVelocity = Sub->GetActorTransform().TransformVectorNoScale(RelativeLinearVelocity);
		Frame.Speed2D = bSwimming ? Frame.LocalVelocity.Size() : Frame.LocalVelocity.Size2D();
	}
	else
	{
		Frame.WorldVelocity = Velocity;
		if (Sub)
		{
			const FRotator SubYawRot(0.f, Sub->GetActorRotation().Yaw, 0.f);
			Frame.LocalVelocity = SubYawRot.UnrotateVector(Frame.WorldVelocity);
		}
		else
		{
			Frame.LocalVelocity = Frame.WorldVelocity;
		}
		Frame.Speed2D = bSwimming ? Frame.WorldVelocity.Size() : Frame.WorldVelocity.Size2D();
	}

	Frame.bIsMoving = Frame.Speed2D > 10.f;
	Frame.Stance = ResolveLocomotionStance(PostureAlpha, bSwimming);
	Frame.Gait = ResolveLocomotionGait(Frame.Stance, Frame.bIsMoving, bIsRunning);

	if (Frame.bIsMoving && (!bSwimming || !Frame.LocalVelocity.IsNearlyZero()))
	{
		const FVector DirectionVelocity = bGrid ? Frame.LocalVelocity : Frame.WorldVelocity;
		const float VelocityYawDeg = DirectionVelocity.Size2D() > KINDA_SMALL_NUMBER
			? DirectionVelocity.ToOrientationRotator().Yaw
			: (bGrid ? Frame.BodyLocalYawDeg : Frame.BodyWorldYawDeg);
		const float BodyYawDeg = bGrid ? Frame.BodyLocalYawDeg : Frame.BodyWorldYawDeg;
		Frame.DirectionDeg = FMath::FindDeltaAngleDegrees(BodyYawDeg, VelocityYawDeg);
	}

	LastLocomotionFrame = Frame;
}

void USubCrewMovementComponent::UpdateGridFacingYaw(float DeltaTime, const FTransform& /*SubTransform*/)
{
	const float CurrentGridYaw = FRotator::NormalizeAxis(GridSpaceTransform.Rotator().Yaw);
	if (!bHasGridFacingYaw)
	{
		GridFacingYawDeg = CurrentGridYaw;
		DesiredGridFacingYawDeg = CurrentGridYaw;
		GridFacingYawRateDegPerSec = 0.f;
		bHasGridFacingYaw = true;
	}

	DesiredGridFacingYawDeg = GridFacingYawDeg;
	if (CharacterOwner && CharacterOwner->IsLocallyControlled())
	{
		DesiredGridFacingYawDeg = LastMoveIntent.DesiredGridYawDeg;
	}
	else
	{
		// Non-owners consume the replicated GridSpaceTransform yaw directly.
		DesiredGridFacingYawDeg = CurrentGridYaw;
	}

	const float PreviousYaw = GridFacingYawDeg;
	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		const float MaxStepDeg = FMath::Max(1.f, GridFacingTurnRateDegPerSec) * DeltaTime;
		GridFacingYawDeg = StepAngleConstant(PreviousYaw, DesiredGridFacingYawDeg, MaxStepDeg);
		GridFacingYawRateDegPerSec = FMath::FindDeltaAngleDegrees(PreviousYaw, GridFacingYawDeg) / DeltaTime;
	}
	else
	{
		GridFacingYawDeg = FRotator::NormalizeAxis(DesiredGridFacingYawDeg);
		GridFacingYawRateDegPerSec = 0.f;
	}

	GridSpaceTransform = FTransform(
		FRotator(0.f, GridFacingYawDeg, 0.f).Quaternion(),
		GridSpaceTransform.GetLocation());
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
	const bool bServerRemoteAutonomousProxy =
		CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_Authority && !CharacterOwner->IsLocallyControlled();

	bool bHasReportedSubMove = false;
	FTransform ReportedGridSpaceTransform = FTransform::Identity;
	ECrewEmbarkState ReportedState = ECrewEmbarkState::Outside;
	ECrewHandoffKind ReportedHandoff = ECrewHandoffKind::None;

	auto ApplyReportedSubMoveState = [&]()
	{
		GridSpaceTransform = ReportedGridSpaceTransform;
		if (ReportedState == ECrewEmbarkState::Embarked || ReportedState == ECrewEmbarkState::Transitioning)
		{
			GridFacingYawDeg = FRotator::NormalizeAxis(GridSpaceTransform.Rotator().Yaw);
			DesiredGridFacingYawDeg = GridFacingYawDeg;
			GridFacingYawRateDegPerSec = 0.f;
			bHasGridFacingYaw = true;
		}
		else
		{
			ResetGridFacingYaw();
		}

		if (ReportedState != EmbarkState)
		{
			SetEmbarkState(ReportedState);
		}

		if (ReportedState == ECrewEmbarkState::Outside && MovementMode != MOVE_Swimming)
		{
			if (const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner))
			{
				GravityScale = Crew->SwimGravityScale;
			}
			SetMovementMode(MOVE_Swimming);
		}
	};

	if (bServerRemoteAutonomousProxy)
	{
		if (const FCharacterNetworkMoveData* CurrentMoveData = GetCurrentNetworkMoveData())
		{
			const FCharacterNetworkMoveData_SubCrew* SubMoveData = static_cast<const FCharacterNetworkMoveData_SubCrew*>(CurrentMoveData);
			bHasReportedSubMove = true;
			ReportedGridSpaceTransform = SubMoveData->GridSpaceTransform;
			ReportedState = static_cast<ECrewEmbarkState>(SubMoveData->EmbarkStateByte);
			ReportedHandoff = SubMoveData->Handoff;
			ApplyReportedSubMoveState();
		}
	}

	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);

	// Server-side: after CMC processes the client's move, apply the client's reported
	// grid-space state directly (trust model for FP co-op — production would bound the
	// per-tick delta). The replicated UPROPERTY(COND_SkipOwner) then broadcasts to peers.
	if (bServerRemoteAutonomousProxy && bHasReportedSubMove)
	{
		ApplyReportedSubMoveState();

		if (ReportedHandoff != ECrewHandoffKind::None)
		{
			UE_LOG(
				LogSubCrewMovement,
				Log,
				TEXT("ServerMove received handoff event | Kind=%d | Crew=%s | ReportedState=%d | Mode=%s"),
				static_cast<int32>(ReportedHandoff),
				*GetNameSafe(CharacterOwner),
				static_cast<int32>(ReportedState),
				MovementModeToDebugName(MovementMode));
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
	const ASubmarineBase* Sub = GetCurrentSubmarine();
	if (!Sub)
	{
		LocalSubLinearVelocity = FVector::ZeroVector;
		LocalSubLinearAcceleration = FVector::ZeroVector;
		LocalSubAngularVelocityDegrees = FVector::ZeroVector;
		LocalSubAngularAccelerationDegrees = FVector::ZeroVector;
		return;
	}

	const FTransform SubXf_Inertia = Sub->GetActorTransform();
	const USubMovementComponent* SubMov_Inertia = Sub->SubMovement;
	const FVector WorldLinVel = SubMov_Inertia ? SubMov_Inertia->Velocity : FVector::ZeroVector;
	const FVector WorldLinAcc = SubMov_Inertia ? SubMov_Inertia->LinearAcceleration : FVector::ZeroVector;
	const FVector WorldAngVelDeg = SubMov_Inertia ? FVector(0.f, SubMov_Inertia->GetPitchRateDegPerSec(), SubMov_Inertia->GetYawRateDegPerSec()) : FVector::ZeroVector;
	const FVector WorldAngAccDeg = SubMov_Inertia ? SubMov_Inertia->AngularAccelerationDeg : FVector::ZeroVector;
	LocalSubLinearVelocity = SubXf_Inertia.InverseTransformVectorNoScale(WorldLinVel);
	LocalSubLinearAcceleration = SubXf_Inertia.InverseTransformVectorNoScale(WorldLinAcc);
	LocalSubAngularVelocityDegrees = SubXf_Inertia.InverseTransformVectorNoScale(WorldAngVelDeg);
	LocalSubAngularAccelerationDegrees = SubXf_Inertia.InverseTransformVectorNoScale(WorldAngAccDeg);
}

void USubCrewMovementComponent::UpdateRelativeState(float DeltaTime)
{
	const ASubmarineBase* Sub = GetCurrentSubmarine();
	if (!Sub || !CharacterOwner)
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
		: Sub->GetActorTransform().InverseTransformPosition(CharacterOwner->GetActorLocation());
	const FRotator NewRelativeRotation = IsGridAuthoritative()
		? GridSpaceTransform.Rotator()
		: (Sub->GetActorQuat().Inverse() * CharacterOwner->GetActorQuat()).Rotator();

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
	if (DebugSettingsRef->ShouldLogCrewJitter())
	{
		const UPrimitiveComponent* Base = CharacterOwner->GetMovementBase();
		const ENetRole LocalRole = CharacterOwner->GetLocalRole();
		const FString MovementModeStr = GetMovementName();
		const FVector SubWorldLoc = Sub ? Sub->GetActorLocation() : FVector::ZeroVector;
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
			Sub ? 1 : 0,
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

	if (DebugSettingsRef->ShouldLogCrewMovement())
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

void USubCrewMovementComponent::BeginLadderClimb(ULadderClimbComponent* Ladder, float StartProgress01)
{
	if (!Ladder || !CharacterOwner) { return; }

	// Authority-only entry point. The replicated CurrentLadder + LadderClimbProgress01 push
	// the state to peer SimProxies; the owning client also runs locally via the same call
	// path when it predicts the interact RPC has been accepted.
	const bool bAuthority = CharacterOwner->HasAuthority();
	if (!bAuthority) { return; }

	CurrentLadder = Ladder;
	LadderClimbProgress01 = FMath::Clamp(StartProgress01, 0.f, 1.f);

	// Snap GridSpaceTransform to the ladder pose so the rebase below matches the climb start.
	GridSpaceTransform = Ladder->ComputeClimbLocalPose(LadderClimbProgress01);

	// While climbing, freeze CMC simulation. We drive the pose directly via the rebase.
	SetMovementMode(MOVE_None);

	UE_LOG(LogSubCrewMovement, Log, TEXT("BeginLadderClimb | Ladder=%s | StartProgress=%.2f"),
		*GetNameSafe(Ladder), LadderClimbProgress01);
}

void USubCrewMovementComponent::EndLadderClimb(float StepOffProgress01)
{
	if (!CurrentLadder || !CharacterOwner) { return; }

	const bool bAuthority = CharacterOwner->HasAuthority();
	if (!bAuthority) { return; }

	ULadderClimbComponent* LeavingLadder = CurrentLadder;
	const float ClampedProgress = FMath::Clamp(StepOffProgress01, 0.f, 1.f);

	// Compute step-off local pose: continue past the ladder end by StepOffOffsetCm along the
	// climb direction (so the crew lands on the deck adjacent to the ladder rather than
	// floating at the very top/bottom).
	FTransform StepOffLocal = LeavingLadder->ComputeClimbLocalPose(ClampedProgress);
	const FVector ExitDir = (ClampedProgress >= 0.5f)
		?  LeavingLadder->GetClimbDirectionLocal()
		: -LeavingLadder->GetClimbDirectionLocal();
	StepOffLocal.AddToTranslation(ExitDir * LeavingLadder->StepOffOffsetCm);

	GridSpaceTransform = StepOffLocal;
	CurrentLadder = nullptr;
	LadderClimbProgress01 = 0.f;

	// Restore walking on the deck.
	SetMovementMode(MOVE_Walking);

	LeavingLadder->NotifyClimbFinished(Cast<ASubCrewCharacter>(CharacterOwner));

	UE_LOG(LogSubCrewMovement, Log, TEXT("EndLadderClimb | StepOffProgress=%.2f"), ClampedProgress);
}

void USubCrewMovementComponent::TickLadderClimb(float DeltaTime)
{
	if (!CurrentLadder || !CharacterOwner) { return; }

	// Forward input axis (W=+1, S=-1) drives climb progress. We read the CMC Acceleration —
	// CharacterMovement projects pending input onto Acceleration each tick. Project onto the
	// pawn's forward direction to extract a scalar.
	float InputForwardAxis = 0.f;
	if (CharacterOwner->IsLocallyControlled())
	{
		const FVector InputVec = ConsumeInputVector();  // already in world space
		const FVector ForwardWorld = CharacterOwner->GetActorForwardVector();
		InputForwardAxis = static_cast<float>(FVector::DotProduct(InputVec, ForwardWorld));
		InputForwardAxis = FMath::Clamp(InputForwardAxis, -1.f, 1.f);
	}

	const float Speed = FMath::Max(0.05f, CurrentLadder->ClimbSpeedPerSec);
	LadderClimbProgress01 = FMath::Clamp(LadderClimbProgress01 + InputForwardAxis * Speed * DeltaTime, 0.f, 1.f);

	// Override the GridSpaceTransform so the rebase below places the capsule exactly on
	// the ladder line at this progress.
	GridSpaceTransform = CurrentLadder->ComputeClimbLocalPose(LadderClimbProgress01);

	// Reaching either end exits automatically. Server-side decision only — owning client
	// predicts via the same condition next tick after RPC roundtrip; for now, only emit
	// the EndLadderClimb on authority.
	if (CharacterOwner->HasAuthority())
	{
		if (LadderClimbProgress01 >= 0.999f)
		{
			EndLadderClimb(1.f);
		}
		else if (LadderClimbProgress01 <= 0.001f && InputForwardAxis < 0.f)
		{
			EndLadderClimb(0.f);
		}
	}
}

void USubCrewMovementComponent::OnRep_LadderClimbProgress()
{
	// Peer SimProxy: nothing to do beyond the property update — the rebase in TickComponent
	// will pick up GridSpaceTransform (which the owner+authority drive) on next tick.
}

void USubCrewMovementComponent::OnRep_GridSpaceTransform()
{
	const FTransform PreviousTransform = LastReceivedReplicatedGridSpaceTransform;
	const bool bHadPreviousPacket = bHasReceivedReplicatedGridSpaceTransform;
	const double PacketReceiveRealTime = FPlatformTime::Seconds();
	const double PacketGapRealSeconds = bHadPreviousPacket
		? (PacketReceiveRealTime - LastReceivedGridSpaceTransformRealTime)
		: 0.0;

	LastReceivedReplicatedGridSpaceTransform = GridSpaceTransform;
	LastReceivedGridSpaceTransformRealTime = PacketReceiveRealTime;
	bHasReceivedReplicatedGridSpaceTransform = true;

	const float ReplicatedGridYaw = FRotator::NormalizeAxis(GridSpaceTransform.Rotator().Yaw);
	GridFacingYawDeg = ReplicatedGridYaw;
	DesiredGridFacingYawDeg = ReplicatedGridYaw;
	GridFacingYawRateDegPerSec = (bHadPreviousPacket && PacketGapRealSeconds > KINDA_SMALL_NUMBER)
		? FMath::FindDeltaAngleDegrees(PreviousTransform.Rotator().Yaw, ReplicatedGridYaw) / static_cast<float>(PacketGapRealSeconds)
		: 0.f;
	bHasGridFacingYaw = true;

	const USub3DDebugSettings* DebugSettingsRef = GetDefault<USub3DDebugSettings>();
	if (!DebugSettingsRef || !DebugSettingsRef->ShouldLogCrewJitter())
	{
		return;
	}

	const ASubmarineBase* Sub = GetCurrentSubmarine();
	const FVector NewLocalPos = GridSpaceTransform.GetLocation();
	const FVector PreviousLocalPos = PreviousTransform.GetLocation();
	const FVector LocalDelta = bHadPreviousPacket ? (NewLocalPos - PreviousLocalPos) : FVector::ZeroVector;
	const float LocalDeltaCm = LocalDelta.Size();
	const float LocalYawDeltaDeg = bHadPreviousPacket
		? FMath::FindDeltaAngleDegrees(PreviousTransform.Rotator().Yaw, GridSpaceTransform.Rotator().Yaw)
		: 0.f;
	const FVector RebasedWorldLoc = Sub ? Sub->GetActorTransform().TransformPosition(NewLocalPos) : FVector::ZeroVector;
	const FVector SubWorldLoc = Sub ? Sub->GetActorLocation() : FVector::ZeroVector;
	const UPrimitiveComponent* Base = CharacterOwner ? CharacterOwner->GetMovementBase() : nullptr;

	UE_LOG(
		LogSubCrewMovement,
		Log,
		TEXT("Grid packet | Role=%d | First=%d | GapReal=%.3fs | Local=%s | LocalDelta=%s | LocalDeltaCm=%.2f | YawDelta=%.1f | RebasedWorld=%s | Sub=%s | Mode=%s | Base=%s | GridAuth=%d"),
		CharacterOwner ? static_cast<int32>(CharacterOwner->GetLocalRole()) : -1,
		bHadPreviousPacket ? 0 : 1,
		static_cast<float>(PacketGapRealSeconds),
		*NewLocalPos.ToCompactString(),
		*LocalDelta.ToCompactString(),
		LocalDeltaCm,
		LocalYawDeltaDeg,
		*RebasedWorldLoc.ToCompactString(),
		*SubWorldLoc.ToCompactString(),
		*GetMovementName(),
		*GetNameSafe(Base),
		IsGridAuthoritative() ? 1 : 0);
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

	// Grid authority disables movement-base transport and rotation carry, but walking still
	// needs CMC floor support. MovementBase therefore remains meaningful as "which surface is
	// currently supporting the capsule", not as a transport parent that carries the crew.
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

	if (GetDefault<USub3DDebugSettings>()->ShouldLogCrewMovement())
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

	const USub3DDebugSettings* DebugSettingsRef = GetDefault<USub3DDebugSettings>();
	const bool bShouldLogBaseTransitions = DebugSettingsRef
		&& (DebugSettingsRef->ShouldLogCrewMovement() || DebugSettingsRef->ShouldLogCrewJitter());
	if (!bShouldLogBaseTransitions)
	{
		LastKnownBase = CurrentBase;
		return;
	}

	const ASubmarineBase* Sub = GetCurrentSubmarine();
	const FVector SubWorldLoc = Sub ? Sub->GetActorLocation() : FVector::ZeroVector;
	const FString BaseContext = FString::Printf(
		TEXT(" | Mode=%s | Falling=%d | FloorWalkable=%d | FloorDist=%.2f | Rel=%s | World=%s | Sub=%s | GridAuth=%d"),
		*GetMovementName(),
		IsFalling() ? 1 : 0,
		CurrentFloor.IsWalkableFloor() ? 1 : 0,
		CurrentFloor.FloorDist,
		*RelativeLocation.ToCompactString(),
		*CharacterOwner->GetActorLocation().ToCompactString(),
		*SubWorldLoc.ToCompactString(),
		IsGridAuthoritative() ? 1 : 0);

	const int32 bAcceptedBase = IsAcceptedEmbarkedBase(CurrentBase) ? 1 : 0;
	if (!PreviousBase && CurrentBase)
	{
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("Base acquired: %s on %s | Accepted=%d%s"),
			*GetNameSafe(CurrentBase),
			*GetNameSafe(CurrentBase->GetOwner()),
			bAcceptedBase,
			*BaseContext);
	}
	else if (PreviousBase && !CurrentBase)
	{
		UE_LOG(
			LogSubCrewMovement,
			Warning,
			TEXT("Base lost! Was: %s on %s%s"),
			*GetNameSafe(PreviousBase),
			*GetNameSafe(PreviousBase->GetOwner()),
			*BaseContext);
	}
	else if (PreviousBase && CurrentBase)
	{
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("Base changed: %s on %s -> %s on %s | Accepted=%d%s"),
			*GetNameSafe(PreviousBase),
			*GetNameSafe(PreviousBase->GetOwner()),
			*GetNameSafe(CurrentBase),
			*GetNameSafe(CurrentBase->GetOwner()),
			bAcceptedBase,
			*BaseContext);
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

	const ASubmarineBase* Sub = GetCurrentSubmarine();
	const bool bHasValidFrame = Sub != nullptr;
	const FTransform SubTransform = bHasValidFrame ? Sub->GetActorTransform() : FTransform::Identity;

	// Sphere / line debug gizmos need a valid submarine reference frame.
	// HUD on-screen messages work regardless (useful for EVA validation).
	if (bHasValidFrame && Settings->bDrawCrewMovement)
	{
		const FVector FrameOrigin = SubTransform.GetLocation();
		const FVector ExpectedWorldPosition = Sub->GetActorTransform().TransformPosition(RelativeLocation);
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
		GEngine->AddOnScreenDebugMessage(1001, DisplayTime, FColor::Cyan,
			FString::Printf(TEXT("EmbarkState : %s   |   Mode : %s   |   Compartment : %s   |   Sub : %s"),
				*StateStr, *ModeStr, *CompStr, *SubBindStr));

		if (bHasValidFrame)
		{
			const FVector LocalPos = GridSpaceTransform.GetLocation();
			const float LocalYaw = GridSpaceTransform.Rotator().Yaw;
			const FVector SubPos = SubTransform.GetLocation();
			const float SubYaw = SubTransform.Rotator().Yaw;
			GEngine->AddOnScreenDebugMessage(1002, DisplayTime, FColor::Cyan,
				FString::Printf(TEXT("Grid  : X=%8.1f  Y=%8.1f  Z=%8.1f  Yaw=%7.2f  Desired=%7.2f  Rate=%7.1f"),
					LocalPos.X, LocalPos.Y, LocalPos.Z, LocalYaw, DesiredGridFacingYawDeg, GridFacingYawRateDegPerSec));
			GEngine->AddOnScreenDebugMessage(1004, DisplayTime, FColor::Cyan,
				FString::Printf(TEXT("Sub   : X=%8.1f  Y=%8.1f  Z=%8.1f  Yaw=%7.2f"), SubPos.X, SubPos.Y, SubPos.Z, SubYaw));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(1002, DisplayTime, FColor::Yellow, TEXT("Grid  : <n/a — no sub binding>"));
			GEngine->AddOnScreenDebugMessage(1004, DisplayTime, FColor::Yellow, TEXT("Sub   : <n/a — no sub binding>"));
		}
		GEngine->AddOnScreenDebugMessage(1003, DisplayTime, FColor::Cyan,
			FString::Printf(TEXT("World : X=%8.1f  Y=%8.1f  Z=%8.1f  Yaw=%7.2f"), WorldPos.X, WorldPos.Y, WorldPos.Z, WorldYaw));
	}
}

void USubCrewMovementComponent::LogPeriodicState(float DeltaTime)
{
	if (!GetDefault<USub3DDebugSettings>()->ShouldLogCrewMovement() || !CharacterOwner)
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

	const ASubmarineBase* Sub = GetCurrentSubmarine();
	const UPrimitiveComponent* CurrentBase = CharacterOwner->GetMovementBase();
	const FTransform SubTransform = Sub ? Sub->GetActorTransform() : FTransform::Identity;
	const FVector FrameDeltaLocation = FVector::ZeroVector;
	const FRotator FrameDeltaRotation = FRotator::ZeroRotator;

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
	const ASubmarineBase* Sub = GetCurrentSubmarine();
	const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner);
	const bool bFrameValid = Sub && CharacterOwner;

	if (bFrameValid)
	{
		UpdateRelativeState(0.f);
		UpdateInertialState();
	}

	// Tick ordering fix: ensure this CMC ticks AFTER the submarine has moved.
	// Without this, the rebase reads a stale Sub->GetActorTransform() because the sub
	// hasn't simulated yet this frame, causing one-frame-lag jitter.
	bool bSubTickSet = false;

	if (Crew && Crew->CurrentSubmarine)
	{
		if (USubMovementComponent* SubMov = Crew->CurrentSubmarine->SubMovement)
		{
			AddTickPrerequisiteComponent(SubMov);
			bSubTickSet = true;
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
		TEXT("InitializeForSubmarine | Sub=%s | CharacterLoc=%s | RelLoc=%s | SubBound=%d | SubPrereq=%d"),
		*GetNameSafe(Crew ? Crew->CurrentSubmarine : nullptr),
		CharacterOwner ? *CharacterOwner->GetActorLocation().ToCompactString() : TEXT("None"),
		*RelativeLocation.ToCompactString(),
		bFrameValid ? 1 : 0,
		bSubTickSet ? 1 : 0);
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
		&& !IsCrewSwimming();

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
	if (!CharacterOwner || !HasSubmarineBinding() || IsCrewSwimming())
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

	UWorld* World = GetWorld();
	if (!CharacterOwner || !CharacterOwner->GetCapsuleComponent() || !World || IsCrewSwimming())
	{
		FootIK_R_State = FCrewFootIKState();
		FootIK_L_State = FCrewFootIKState();
		return;
	}

	const FVector ActorLoc = CharacterOwner->GetActorLocation();
	const FVector ActorRight = CharacterOwner->GetActorRightVector();
	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const ASubmarineBase* Submarine = GetCurrentSubmarine();
	const bool bRequireInteriorFloor = IsGridAuthoritative() && Submarine != nullptr;
	const float DeltaSeconds = World->GetDeltaSeconds();

	const USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
	const auto ResolveFootOrigin = [&](FName SocketName, float SideSign)
	{
		if (Mesh && Mesh->DoesSocketExist(SocketName))
		{
			return Mesh->GetSocketLocation(SocketName);
		}

		return ActorLoc + (ActorRight * SideSign * 8.f) - FVector(0.f, 0.f, HalfHeight);
	};

	const auto SolveFoot = [&](const FVector& FootOrigin, FCrewFootIKState& State, FVector& LegacyOffset)
	{
		FCrewFootIKState TargetState;
		const FVector TraceStart = FootOrigin + FVector(0.f, 0.f, FootIKTraceUpCm);
		const FVector TraceEnd = FootOrigin - FVector(0.f, 0.f, FootIKTraceDownCm);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(CrewFootIK), false, CharacterOwner);
		TArray<FHitResult> Hits;
		if (World->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_GameTraceChannel2, Params))
		{
			for (const FHitResult& Hit : Hits)
			{
				if (!Hit.bBlockingHit)
				{
					continue;
				}

				if (bRequireInteriorFloor && !Submarine->IsInteriorWalkableComponent(Hit.GetComponent()))
				{
					continue;
				}

				TargetState.bHasHit = true;
				TargetState.TargetWorldLocation = Hit.ImpactPoint;
				TargetState.TargetWorldNormal = Hit.ImpactNormal;
				TargetState.Offset.Z = FMath::Clamp(Hit.ImpactPoint.Z - FootOrigin.Z, -FootIKMaxOffsetCm, FootIKMaxOffsetCm);
				break;
			}
		}

		const float TargetWeight = TargetState.bHasHit ? 1.f : 0.f;
		State.Weight = FMath::FInterpTo(State.Weight, TargetWeight, DeltaSeconds, FootIKInterpSpeed);
		State.Offset = FMath::VInterpTo(State.Offset, TargetState.Offset, DeltaSeconds, FootIKInterpSpeed);
		State.TargetWorldLocation = TargetState.bHasHit ? TargetState.TargetWorldLocation : State.TargetWorldLocation;
		State.TargetWorldNormal = TargetState.bHasHit ? TargetState.TargetWorldNormal : State.TargetWorldNormal;
		State.bHasHit = TargetState.bHasHit || State.Weight > 0.01f;
		State.bIsPlanted = TargetState.bHasHit && LastLocomotionFrame.Speed2D <= 20.f;
		State.PlantAlpha = FMath::FInterpTo(State.PlantAlpha, State.bIsPlanted ? 1.f : 0.f, DeltaSeconds, FootIKInterpSpeed);

		if (State.Weight <= 0.01f)
		{
			State = FCrewFootIKState();
		}

		LegacyOffset = State.Offset * State.Weight;
	};

	SolveFoot(ResolveFootOrigin(RightFootIKSocketName, 1.f), FootIK_R_State, FootIK_R);
	SolveFoot(ResolveFootOrigin(LeftFootIKSocketName, -1.f), FootIK_L_State, FootIK_L);
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
	// Ladder climb state: owner predicts locally, peer SimProxy gets the replicated values.
	DOREPLIFETIME(USubCrewMovementComponent, CurrentLadder);
	DOREPLIFETIME_CONDITION(USubCrewMovementComponent, LadderClimbProgress01, COND_SkipOwner);
}
