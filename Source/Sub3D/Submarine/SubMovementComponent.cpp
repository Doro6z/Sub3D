#include "SubMovementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Generator/SubmarineDefinition.h"
#include "Net/UnrealNetwork.h"
#include "SubFloodComponent.h"
#include "SubmarineBase.h"
#include "SubmarineCompartmentComponent.h"
#include "SubmarineSystemsComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubMovement, Log, All);

static constexpr float G_SI = 9.81f;

// Runtime debug toggles for Point 1 jitter diagnosis. Set from the UE console
// in PIE, e.g. `sub.DisableVisualInterp 1` or `sub.LogVisualInterp 1`.
static TAutoConsoleVariable<bool> CVarSubDisableVisualInterp(
	TEXT("sub.DisableVisualInterp"),
	false,
	TEXT("Authoritative path only. When true, skip the Lerp(PrevSim, CurrSim, alpha) ")
	TEXT("visual write — actor root sits on CurrSim. Use to isolate whether Point 1 ")
	TEXT("interpolation is the source of observed jitter."),
	ECVF_Default);

static TAutoConsoleVariable<bool> CVarSubLogVisualInterp(
	TEXT("sub.LogVisualInterp"),
	false,
	TEXT("Emit SUB_TRACE per-tick log: PreUndo / PostUndo / PostSim / PostInterp poses, ")
	TEXT("alpha, sim-steps-this-tick, external-drift (vs last tick end), tick delta. ")
	TEXT("Flags BACKWARD and EXTERNAL_WRITE anomalies as Error."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarSubTraceExternalDriftThresholdCm(
	TEXT("sub.TraceExternalDriftThresholdCm"),
	0.01f,
	TEXT("Threshold (cm) above which an ExtDrift delta is flagged as EXTERNAL_WRITE. ")
	TEXT("Distance between actor.Location at tick start and the pose we wrote at end of ")
	TEXT("previous tick. Non-zero in standalone means a foreign writer exists."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarSubTraceBackwardThresholdCm(
	TEXT("sub.TraceBackwardThresholdCm"),
	0.1f,
	TEXT("Threshold (cm) below which TickDelta.X is flagged as BACKWARD motion. ")
	TEXT("Negative forward delta during cruise is a hard anomaly."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarSubMaxInterpDurationSec(
	TEXT("sub.MaxInterpDurationSec"),
	0.1f,
	TEXT("Upper bound (seconds) on the InterpDuration computed by HandleReplicatedNetState. ")
	TEXT("Without this clamp, a large server-side frame gap or a client-side framerate dip ")
	TEXT("produces FrameDelta * FixedSimDt values of several hundred ms, which makes the ")
	TEXT("client visually crawl over the true sub distance rather than catch up. Default 100ms. ")
	TEXT("Set <= 0 to disable the clamp (legacy adaptive-only behavior)."),
	ECVF_Default);

namespace
{
bool UsesComplexAsSimpleSweep(const UPrimitiveComponent* PrimitiveComponent)
{
	const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimitiveComponent);
	if (!StaticMeshComponent)
	{
		return false;
	}

	const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
	const UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	return BodySetup && BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple;
}
}

USubMovementComponent::USubMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	FBallastTank Front;
	Front.LocalPosition = FVector(-500.f, 0.f, 0.f);
	Front.Volume = 10.f;
	Front.FillLevel = 0.5f;
	Front.TargetFill = 0.5f;
	Front.PumpFlowRate = 0.05f;
	Front.PumpState = EPumpState::Nominal;
	Ballasts.Add(Front);

	FBallastTank Rear;
	Rear.LocalPosition = FVector(500.f, 0.f, 0.f);
	Rear.Volume = 10.f;
	Rear.FillLevel = 0.5f;
	Rear.TargetFill = 0.5f;
	Rear.PumpFlowRate = 0.05f;
	Rear.PumpState = EPumpState::Nominal;
	Ballasts.Add(Rear);
}

void USubMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeNeutralBuoyancy();
}

void USubMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USubMovementComponent, Ballasts);
	DOREPLIFETIME(USubMovementComponent, GlobalTargetFill);
	DOREPLIFETIME(USubMovementComponent, ThrustInput);
	DOREPLIFETIME(USubMovementComponent, RudderInput);
	DOREPLIFETIME(USubMovementComponent, DivePlaneInput);
}

// -------------------------------------------------------------------------
// Input setters — authoritative-only write sites for the replicated input
// fields. The PlayerController owning the helmsman is responsible for routing
// raw axis values through a ServerRPC and calling these setters on the server.
// Client-side calls are a no-op in practice because UPROPERTY(Replicated) will
// immediately overwrite the field on the next replication tick.
// Matching BlueprintPure getters live inline in the header.
// -------------------------------------------------------------------------

void USubMovementComponent::SetPowerInput(float Value)
{
	ThrustInput = FMath::Clamp(Value, -1.f, 1.f);
}

void USubMovementComponent::SetThrustInput(float Value)
{
	SetPowerInput(Value);
}

void USubMovementComponent::SetRudderInput(float Value)
{
	RudderInput = FMath::Clamp(Value, -1.f, 1.f);
}

void USubMovementComponent::SetDivePlaneInput(float Value)
{
	DivePlaneInput = FMath::Clamp(Value, -1.f, 1.f);
}

void USubMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (Owner->GetLevel())
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}

	const bool bTraceEnabled = CVarSubLogVisualInterp.GetValueOnGameThread();
	const bool bDisableInterp = CVarSubDisableVisualInterp.GetValueOnGameThread();

	// Captured at the very top for EXTERNAL_WRITE detection: any delta
	// between LastPostTickLocation (end of our previous tick) and PreUndoLoc
	// (start of this tick, before we do anything) means a foreign writer
	// modified the actor root between ticks.
	const FVector PreUndoLoc = Owner->GetActorLocation();
	const FRotator PreUndoRot = Owner->GetActorRotation();

	// Undo visual offset from previous frame so the sim operates on the
	// authoritative post-step pose, not on the frame-time interpolated pose.
	if (bHasVisualOffset && Owner->HasAuthority())
	{
		Owner->SetActorLocationAndRotation(AuthoritativeLocation, AuthoritativeRotation, false, nullptr, ETeleportType::TeleportPhysics);
		bHasVisualOffset = false;
	}

	const FVector PostUndoLoc = Owner->GetActorLocation();

	if (const ASubmarineBase* Submarine = Cast<ASubmarineBase>(Owner))
	{
		if (Submarine->bFreezeMovementForTesting)
		{
			Velocity = FVector::ZeroVector;
			YawRateDegPerSec = 0.f;
			PitchRateDegPerSec = 0.f;
			CurrentDepth = FMath::Max(0.f, -Owner->GetActorLocation().Z / 100.f);

			if (Owner->HasAuthority())
			{
				if (ASubmarineBase* MutableSub = Cast<ASubmarineBase>(Owner))
				{
					MutableSub->RefreshRepState();
				}
			}
			else
			{
				InterpAlpha = 1.f;
			}

			return;
		}
	}

	if (!Owner->HasAuthority())
	{
		InterpolateClient(DeltaTime);
		return;
	}

	SimAccumulator += DeltaTime;
	const float FixedSimDt = FixedSimulationHz > KINDA_SMALL_NUMBER ? (1.f / FixedSimulationHz) : (1.f / 30.f);
	bool bSimulated = false;
	int32 StepsThisTick = 0;

	while (SimAccumulator >= FixedSimDt)
	{
		// Capture the pre-step pose as PrevSim. After the loop exits,
		// PrevSimLocation/Rotation hold the pose at the start of the last
		// executed SimulateStep. When the loop runs multiple steps in one
		// tick (catch-up after a frame spike), only the last two poses are
		// retained — intermediate steps are not visualized.
		PrevSimLocation = Owner->GetActorLocation();
		PrevSimRotation = Owner->GetActorRotation();

		SimulateStep(FixedSimDt);
		SimAccumulator -= FixedSimDt;
		++SimFrameCounter;
		++StepsThisTick;
		bSimulated = true;
		bHasSimBuffer = true;
	}

	if (bSimulated)
	{
		if (ASubmarineBase* Sub = Cast<ASubmarineBase>(Owner))
		{
			// Tick-order invariant: RefreshRepState must run before any
			// visual offset is written to the actor. It samples the actor
			// transform directly (SubmarineBase::RefreshRepState), and the
			// actor currently holds CurrSim (post-last-step pose). Writing
			// an interpolated pose below would poison replication with a
			// non-authoritative transform.
			Sub->RefreshRepState();
		}
	}

	// Capture the latest sim-authoritative pose as CurrSim.
	AuthoritativeLocation = Owner->GetActorLocation();
	AuthoritativeRotation = Owner->GetActorRotation();

	const FVector PostSimLoc = AuthoritativeLocation;

	// Visual interpolation between PrevSim and CurrSim. Alpha = how far
	// we are into the next sim step (SimAccumulator / FixedSimDt). The
	// rendered pose trails CurrSim by at most one sim step (~16.6ms at
	// 60Hz), which is the cost of showing only known poses instead of
	// velocity-based predictions that diverge each step due to drag,
	// damping, pitch/vertical coupling, and idle restore.
	//
	// Suppressed while in contact (Option A): holding CurrSim directly on
	// the wall avoids visual oscillation between pre- and post-depenetration
	// poses.
	// Suppressed when `sub.DisableVisualInterp 1` (debug): actor sits on
	// CurrSim so we can isolate the jitter source in PIE.
	float AlphaUsed = 0.f;
	bool bWroteInterp = false;
	if (!bDisableInterp && bHasSimBuffer && !bLastStepHadBlockingHit)
	{
		AlphaUsed = FMath::Clamp(SimAccumulator / FixedSimDt, 0.f, 1.f);
		const FVector InterpLocation = FMath::Lerp(PrevSimLocation, AuthoritativeLocation, AlphaUsed);
		const FRotator InterpRotation = FMath::Lerp(PrevSimRotation, AuthoritativeRotation, AlphaUsed);

		Owner->SetActorLocationAndRotation(InterpLocation, InterpRotation, false, nullptr, ETeleportType::None);
		bHasVisualOffset = true;
		bWroteInterp = true;
	}

	// Snapshot the final pose we leave on the actor root, for the next
	// tick's EXTERNAL_WRITE check and the BACKWARD anomaly check.
	const FVector PostInterpLoc = Owner->GetActorLocation();
	const FRotator PostInterpRot = Owner->GetActorRotation();

	if (bTraceEnabled)
	{
		const FVector ExtDrift = bHasLastPostTick ? (PreUndoLoc - LastPostTickLocation) : FVector::ZeroVector;
		const FVector TickDelta = bHasLastPostTick ? (PostInterpLoc - LastPostTickLocation) : FVector::ZeroVector;

		UE_LOG(LogSubMovement, Log,
			TEXT("SUB_TRACE | DT=%.4f Accum=%.4f Alpha=%.3f Steps=%d Contact=%d Interp=%d")
			TEXT(" | PreUndo=(%.2f,%.2f,%.2f) PostUndo=(%.2f,%.2f,%.2f) PostSim=(%.2f,%.2f,%.2f) PostInterp=(%.2f,%.2f,%.2f)")
			TEXT(" | PrevSim=(%.2f,%.2f,%.2f) CurrSim=(%.2f,%.2f,%.2f)")
			TEXT(" | ExtDrift=(%.3f,%.3f,%.3f) TickDelta=(%.3f,%.3f,%.3f)"),
			DeltaTime, SimAccumulator, AlphaUsed, StepsThisTick,
			bLastStepHadBlockingHit ? 1 : 0, bWroteInterp ? 1 : 0,
			PreUndoLoc.X, PreUndoLoc.Y, PreUndoLoc.Z,
			PostUndoLoc.X, PostUndoLoc.Y, PostUndoLoc.Z,
			PostSimLoc.X, PostSimLoc.Y, PostSimLoc.Z,
			PostInterpLoc.X, PostInterpLoc.Y, PostInterpLoc.Z,
			PrevSimLocation.X, PrevSimLocation.Y, PrevSimLocation.Z,
			AuthoritativeLocation.X, AuthoritativeLocation.Y, AuthoritativeLocation.Z,
			ExtDrift.X, ExtDrift.Y, ExtDrift.Z,
			TickDelta.X, TickDelta.Y, TickDelta.Z);

		const float ExtDriftThreshold = CVarSubTraceExternalDriftThresholdCm.GetValueOnGameThread();
		if (bHasLastPostTick && ExtDrift.Size() > ExtDriftThreshold)
		{
			UE_LOG(LogSubMovement, Error,
				TEXT("SUB_TRACE EXTERNAL_WRITE | A foreign writer modified the actor between last tick's end and this tick's start.")
				TEXT(" | LastPostTick=(%.3f,%.3f,%.3f) PreUndo=(%.3f,%.3f,%.3f) Drift=%.3f"),
				LastPostTickLocation.X, LastPostTickLocation.Y, LastPostTickLocation.Z,
				PreUndoLoc.X, PreUndoLoc.Y, PreUndoLoc.Z,
				ExtDrift.Size());
		}

		const float BackwardThreshold = CVarSubTraceBackwardThresholdCm.GetValueOnGameThread();
		if (bHasLastPostTick && TickDelta.X < -BackwardThreshold)
		{
			UE_LOG(LogSubMovement, Error,
				TEXT("SUB_TRACE BACKWARD | Actor moved backward on X this tick during forward cruise.")
				TEXT(" | TickDelta.X=%.3f Alpha=%.3f Steps=%d PrevSim.X=%.3f CurrSim.X=%.3f"),
				TickDelta.X, AlphaUsed, StepsThisTick,
				PrevSimLocation.X, AuthoritativeLocation.X);
		}
	}

	LastPostTickLocation = PostInterpLoc;
	LastPostTickRotation = PostInterpRot;
	bHasLastPostTick = true;
}

void USubMovementComponent::SimulateStep(float DeltaTime)
{
	if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
	{
		if (Sub->Systems)
		{
			ApplyCommandState(Sub->Systems->GetCommandState());
		}
	}

	FloodedMassKg = ComputeFloodedMassKg();
	UpdateBallasts(DeltaTime);
	ApplyPhysics(DeltaTime);
}

void USubMovementComponent::UpdateBallasts(float DeltaTime)
{
	for (FBallastTank& Tank : Ballasts)
	{
		if (Tank.PumpState == EPumpState::Dead)
		{
			continue;
		}

		float FlowRate = Tank.PumpFlowRate;
		if (Tank.PumpState == EPumpState::Degraded)
		{
			FlowRate *= 0.5f;
		}

		const float Delta = Tank.TargetFill - Tank.FillLevel;
		const float Change = FMath::Sign(Delta) * FMath::Min(FMath::Abs(Delta), FlowRate * DeltaTime);
		Tank.FillLevel = FMath::Clamp(Tank.FillLevel + Change, 0.f, 1.f);
	}
}

void USubMovementComponent::ApplyPhysics(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FTransform OwnerTransform = Owner->GetActorTransform();
	const FVector LocalVelCm = OwnerTransform.InverseTransformVector(Velocity);
	const FVector LocalVelM = LocalVelCm / 100.f;
	const float ForwardSpeedAbs = FMath::Abs(LocalVelCm.X);

	// ── 1. Engine spool ─────────────────────────────────────────────────
	const float SpoolRate = (FMath::Abs(ThrustInput) > FMath::Abs(SpooledPower))
		? EngineSpoolUpRate : EngineSpoolDownRate;
	SpooledPower = FMath::FInterpTo(SpooledPower, ThrustInput, DeltaTime, SpoolRate);
	EffectivePowerInput = SpooledPower;

	// ── 2. Forces ───────────────────────────────────────────────────────
	const float TotalMass = ComputeTotalMass();
	const float InvMass = 1.f / FMath::Max(1.f, TotalMass);

	// Buoyancy vs gravity
	const float BuoyancyN = ComputeBuoyancyForce();
	const float GravityN = TotalMass * G_SI;
	const float NetVerticalN = BuoyancyN - GravityN;

	FVector Acceleration = FVector::ZeroVector;
	Acceleration.Z = (NetVerticalN * InvMass) * 100.f;

	// Thrust (uses spooled power, not raw input)
	float EngineHealth = 1.f;
	if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(Owner))
	{
		if (Sub->Systems)
		{
			EngineHealth = FMath::Clamp(Sub->Systems->GetEngineHealth01(), 0.f, 1.f);
		}
	}
	const FVector ForwardDir = Owner->GetActorForwardVector();
	Acceleration += ForwardDir * (SpooledPower * MaxThrust * EngineHealth * InvMass) * 100.f;

	// Drag (quadratic, per-axis in local space — Fossen D(v) diagonal)
	FVector LocalDragN;
	LocalDragN.X = -0.5f * WaterDensity * DragCoefficients.X * CrossSections.X * LocalVelM.X * FMath::Abs(LocalVelM.X);
	LocalDragN.Y = -0.5f * WaterDensity * DragCoefficients.Y * CrossSections.Y * LocalVelM.Y * FMath::Abs(LocalVelM.Y);
	LocalDragN.Z = -0.5f * WaterDensity * DragCoefficients.Z * CrossSections.Z * LocalVelM.Z * FMath::Abs(LocalVelM.Z);

	const FVector WorldDragN = OwnerTransform.TransformVector(LocalDragN);
	Acceleration += (WorldDragN * InvMass) * 100.f;

	// ── 3. Rudder (gamified direct rate with speed authority) ────────────
	FRotator NewRotation = Owner->GetActorRotation();

	// Authority ramps from RudderStandstillAuthority (at speed=0) to 1.0 (at full authority speed).
	const float SpeedFraction = FMath::Clamp(ForwardSpeedAbs / FMath::Max(1.f, RudderFullAuthoritySpeed), 0.f, 1.f);
	const float SpeedAuthority = FMath::Lerp(RudderStandstillAuthority, 1.f, SpeedFraction);

	const float TargetYawRate = RudderInput * RudderTurnRate * SpeedAuthority;
	YawRateDegPerSec += (TargetYawRate - YawRateDegPerSec * YawRateDamping) * DeltaTime;
	YawRateDegPerSec = FMath::Clamp(YawRateDegPerSec, -RudderTurnRate, RudderTurnRate);

	// Option C — contact-aware yaw damping. The rate state itself keeps
	// tracking rudder input (so releasing contact feels immediate), but the
	// yaw actually committed to the rotation this tick is scaled down while
	// the previous step hit something. This kills the "full thrust + wall +
	// rudder" saccade where rotation kept driving the hull into geometry
	// the sweep then had to push back out.
	const float ContactYawScale = bLastStepHadBlockingHit
		? FMath::Clamp(ContactYawDampingFactor, 0.f, 1.f)
		: 1.f;
	NewRotation.Yaw += YawRateDegPerSec * DeltaTime * ContactYawScale;

	// ── 4. Pitch (hydroplane + ballast trim + BG restoring moment) ──────
	const float HydroplaneSpeedFactor = HydroplaneAuthoritySpeed > KINDA_SMALL_NUMBER
		? FMath::Clamp(ForwardSpeedAbs / HydroplaneAuthoritySpeed, 0.f, 1.f)
		: 1.f;

	float BallastTrimRateBias = 0.f;
	if (bEnableBallastTrimPitch && Ballasts.Num() >= 2)
	{
		const float FrontFill = Ballasts[0].FillLevel;
		const float RearFill = Ballasts.Last().FillLevel;
		const float TrimBias = RearFill - FrontFill;
		BallastTrimRateBias = TrimBias * BallastTrimPitchRate;
	}

	const float HydroplanePitchAccel = DivePlaneInput * PitchFromHydroplaneAccel * HydroplaneSpeedFactor;
	const float BallastPitchTarget = -ComputeCenterOfMassXOffset() / 100.f * BallastPitchFactor;
	const float BallastPitchRateCorrection = (BallastPitchTarget - NewRotation.Pitch) * 0.6f;

	// BG restoring moment: B above G creates pendulum-like return to level.
	// RestoreMoment = -BG * sin(pitch) * damping. This is the key physics
	// that makes submarines naturally return to level without hydroplane input.
	const float PitchRad = FMath::DegreesToRadians(NewRotation.Pitch);
	const float BG_RestoreDegPerSec2 = -BG_DistanceCm * FMath::Sin(PitchRad) * PitchRestorationDamping;

	PitchRateDegPerSec += (
		HydroplanePitchAccel +
		BallastTrimRateBias +
		BallastPitchRateCorrection +
		BG_RestoreDegPerSec2 -
		(PitchRateDegPerSec * PitchRateDamping)
	) * DeltaTime;

	NewRotation.Pitch = FMath::Clamp(
		NewRotation.Pitch + PitchRateDegPerSec * DeltaTime,
		-MaxDivePlanePitch,
		MaxDivePlanePitch
	);

	// ── 5. Pitch-to-vertical coupling ───────────────────────────────────
	const float VerticalFromPitchAccel = LocalVelCm.X * FMath::Sin(PitchRad) * VerticalFromPitchFactor;
	Acceleration.Z += VerticalFromPitchAccel;

	// ── 6. Integrate (semi-implicit Euler) ──────────────────────────────
	Velocity += Acceleration * DeltaTime;

	FVector ClampedLocalVelocity = OwnerTransform.InverseTransformVector(Velocity);

	// Pitch-dependent forward speed cap. Nose-down gets a gain (gravity helps),
	// nose-up gets a penalty (gravity fights us). PitchVmaxInfluence controls
	// the swing; defaults to 0.2 (so ±10% at ±MaxDivePlanePitch of ±30°).
	// Independent of input thrust — limits terminal speed, not acceleration.
	const float PitchSin = FMath::Sin(PitchRad);
	const float PitchFactor = FMath::Clamp(1.f - PitchSin * PitchVmaxInfluence,
		1.f - PitchVmaxInfluence, 1.f + PitchVmaxInfluence);
	const float EffectiveMaxForward = MaxForwardSpeed * PitchFactor;
	const float EffectiveMaxReverse = MaxReverseSpeed * PitchFactor;

	ClampedLocalVelocity.X = FMath::Clamp(ClampedLocalVelocity.X, -EffectiveMaxReverse, EffectiveMaxForward);
	if (FMath::Abs(SpooledPower) < 0.01f)
	{
		ClampedLocalVelocity.X = FMath::FInterpTo(ClampedLocalVelocity.X, 0.f, DeltaTime, IdleForwardSpeedDamping);
	}
	ClampedLocalVelocity.Y = FMath::FInterpTo(ClampedLocalVelocity.Y, 0.f, DeltaTime, LateralSpeedDamping);
	ClampedLocalVelocity.Z = FMath::Clamp(ClampedLocalVelocity.Z, -MaxVerticalSpeed, MaxVerticalSpeed);
	Velocity = OwnerTransform.TransformVector(ClampedLocalVelocity);

	ForwardSpeedCmS = ClampedLocalVelocity.X;

	// ── 7. Debug logging ────────────────────────────────────────────────
	if (bDebugLogSubMovement)
	{
		static float ServerDebugLogTimer = 0.f;
		ServerDebugLogTimer += DeltaTime;
		if (ServerDebugLogTimer >= 0.5f)
		{
			ServerDebugLogTimer = 0.f;
			UE_LOG(LogSubMovement, Log,
				TEXT("Physics | Spool=%.2f | Vel=%s | FwdSpd=%.1f | Yaw=%.2f | Pitch=%.2f | Depth=%.1f | Mass=%.0f | BG_Restore=%.2f"),
				SpooledPower,
				*Velocity.ToCompactString(),
				ForwardSpeedCmS,
				YawRateDegPerSec,
				PitchRateDegPerSec,
				CurrentDepth,
				TotalMass,
				BG_RestoreDegPerSec2);
		}
	}

	// ── 8. Move with collision sweep ────────────────────────────────────
	// The submarine root is a USceneComponent (no shape), so a direct
	// AddActorWorldOffset(..., bSweep=true, ...) would silently do nothing
	// because USceneComponent::MoveComponent ignores the sweep flag. We
	// instead run an explicit ComponentSweepMulti against the submarine's
	// designated movement collision component (GetMovementCollisionComponent
	// — typically HullMesh) and then translate the whole actor by the
	// adjusted delta. For this to block external geometry (Traversal Route,
	// walls, terrain), the submarine BP must assign a collidable static mesh
	// to HullMesh (or equivalent override of GetMovementCollisionComponent).
	const FVector DeltaLocation = Velocity * DeltaTime;
	bool bHadBlockingHit = false;

	ASubmarineBase* SubForSweep = Cast<ASubmarineBase>(Owner);
	UPrimitiveComponent* SweepShape = SubForSweep ? SubForSweep->GetMovementCollisionComponent() : nullptr;
	UWorld* World = Owner->GetWorld();
	const bool bCanSweep = World && SweepShape && SweepShape->IsCollisionEnabled();

	const auto MoveWithSlide = [Owner, SweepShape, World, bCanSweep, this](const FVector& MoveDelta, FVector& InOutVelocity)
	{
		if (MoveDelta.IsNearlyZero())
		{
			return false;
		}

		// Fallback: no sweep shape / collision disabled. Translate without a
		// hit test — same behavior as the original code when the sub has no
		// hull collision configured.
		if (!bCanSweep)
		{
			Owner->AddActorWorldOffset(MoveDelta, false, nullptr, ETeleportType::None);
			return false;
		}

		if (UsesComplexAsSimpleSweep(SweepShape))
		{
			if (bDebugLogCollisionSweeps || bDebugLogSubMovement)
			{
				UE_LOG(
					LogSubMovement,
					Warning,
					TEXT("HullSweep unsupported | Comp=%s | Reason=UseComplexAsSimple cannot be used as the moving sweep shape in the current engine path"),
					*GetNameSafe(SweepShape));
			}

			Owner->AddActorWorldOffset(MoveDelta, false, nullptr, ETeleportType::None);
			return false;
		}

		FComponentQueryParams Params(SCENE_QUERY_STAT(SubHullSweep));
		Params.AddIgnoredActor(Owner);
		Params.bTraceComplex = SweepShape->bTraceComplexOnMove;

		// Iterative sweep → advance → slide → re-sweep. Each iteration consumes
		// the remaining portion of the move along one contact plane; on the
		// next pass we re-sweep the projected remainder rather than applying
		// it blind. This is what kills the "enter / exit / re-collide" chatter
		// on grazing contacts and inner corners. Bounded to MaxSlideIterations
		// so a pathological case (two near-parallel walls) can't live-lock.
		FVector RemainingDelta = MoveDelta;
		bool bAnyBlocking = false;
		const int32 IterationBound = FMath::Clamp(MaxSlideIterations, 1, 4);

		for (int32 Iteration = 0; Iteration < IterationBound; ++Iteration)
		{
			if (RemainingDelta.IsNearlyZero())
			{
				break;
			}

			const FVector Start = SweepShape->GetComponentLocation();
			const FVector End = Start + RemainingDelta;
			const FQuat SweepRot = SweepShape->GetComponentQuat();

			TArray<FHitResult> Hits;
			const bool bAnyHit = World->ComponentSweepMulti(Hits, SweepShape, Start, End, SweepRot, Params);
			FHitResult Blocking;
			bool bHasBlocking = false;
			if (bAnyHit)
			{
				for (const FHitResult& H : Hits)
				{
					if (H.bBlockingHit)
					{
						Blocking = H;
						bHasBlocking = true;
						break;
					}
				}
			}

			if (!bHasBlocking)
			{
				Owner->AddActorWorldOffset(RemainingDelta, false, nullptr, ETeleportType::None);
				RemainingDelta = FVector::ZeroVector;
				break;
			}

			bAnyBlocking = true;

			// Advance up to the hit, then depenetrate if the sweep started inside geometry.
			const float HitTime = FMath::Clamp(Blocking.Time, 0.f, 1.f);
			const FVector AdvanceDelta = RemainingDelta * HitTime;
			if (!AdvanceDelta.IsNearlyZero())
			{
				Owner->AddActorWorldOffset(AdvanceDelta, false, nullptr, ETeleportType::None);
			}

			if (Blocking.bStartPenetrating)
			{
				const FVector Depen = Blocking.Normal * FMath::Max(2.f, Blocking.PenetrationDepth + 1.f);
				Owner->AddActorWorldOffset(Depen, false, nullptr, ETeleportType::None);
			}

			// Project the leftover motion onto the hit plane and project the
			// outgoing velocity so subsequent iterations (and the next tick)
			// see a tangent-only velocity along every contact normal hit
			// this tick.
			const float RemainingFraction = 1.f - HitTime;
			RemainingDelta = FVector::VectorPlaneProject(RemainingDelta * RemainingFraction, Blocking.Normal);
			InOutVelocity = FVector::VectorPlaneProject(InOutVelocity, Blocking.Normal);

			if (bDebugLogCollisionSweeps)
			{
				UE_LOG(LogSubMovement, Log,
					TEXT("HullSweep hit | Iter=%d | Comp=%s | OtherActor=%s | OtherComp=%s | Normal=%s | Time=%.3f"),
					Iteration,
					*GetNameSafe(SweepShape),
					*GetNameSafe(Blocking.GetActor()),
					*GetNameSafe(Blocking.GetComponent()),
					*Blocking.Normal.ToCompactString(),
					HitTime);
			}
		}

		// If iterations were exhausted and a non-zero remainder is still
		// pending, DROP it. The previous "apply unswept as fallback" path
		// could push the hull through a corner / double-wall geometry
		// because no sweep was performed for that final segment. The
		// player feels a tiny stick in true corners, but the sub never
		// teleports through walls. Sticky > tunneling.
		if (!RemainingDelta.IsNearlyZero() && bDebugLogCollisionSweeps)
		{
			UE_LOG(LogSubMovement, Verbose,
				TEXT("HullSweep | iter exhausted, dropping residual=%s (mag=%.2f)"),
				*RemainingDelta.ToCompactString(), RemainingDelta.Size());
		}

		return bAnyBlocking;
	};

	const FVector HorizontalDelta = FVector(DeltaLocation.X, DeltaLocation.Y, 0.f);
	const FVector VerticalDelta = FVector(0.f, 0.f, DeltaLocation.Z);
	bHadBlockingHit |= MoveWithSlide(HorizontalDelta, Velocity);
	bHadBlockingHit |= MoveWithSlide(VerticalDelta, Velocity);
	if (bHadBlockingHit)
	{
		Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, DeltaTime, ContactVelocityDamping);
	}

	// Latch contact state so the next SimulateStep (Option C — yaw damping)
	// and TickComponent (Option A — visual extrapolation gate) can react.
	bLastStepHadBlockingHit = bHadBlockingHit;

	Owner->SetActorRotation(NewRotation, ETeleportType::None);
	CurrentDepth = FMath::Max(0.f, -Owner->GetActorLocation().Z / 100.f);
}

float USubMovementComponent::ComputeTotalMass() const
{
	// Raw water mass and the water mass the tanks would hold at neutral fill.
	float WaterMass = 0.f;
	float NeutralWaterMass = 0.f;
	for (const FBallastTank& Tank : Ballasts)
	{
		WaterMass += Tank.FillLevel * Tank.Volume * WaterDensity;
		NeutralWaterMass += NeutralBuoyancyFill01 * Tank.Volume * WaterDensity;
	}

	// Amplify only the deviation from neutral. A full tank (or empty tank)
	// therefore pushes the sub harder on the vertical axis while a tank held
	// at NeutralBuoyancyFill01 still produces zero net effect (neutral stays
	// neutral regardless of scale).
	const float ScaledDeviation = (WaterMass - NeutralWaterMass) * BallastEffectScale;
	const float EffectiveWaterMass = NeutralWaterMass + ScaledDeviation;

	return BaseMass + EffectiveWaterMass + (FloodedMassKg * FloodedMassInfluence);
}

void USubMovementComponent::ApplyPerformanceProfileFromDefinition(const USubmarineDefinition* Definition)
{
	if (!Definition)
	{
		return;
	}

	bool bMassChanged = false;
	if (Definition->BaseMassKg > KINDA_SMALL_NUMBER)
	{
		BaseMass = Definition->BaseMassKg;
		bMassChanged = true;
	}
	if (Definition->MaxForwardSpeedCmS > KINDA_SMALL_NUMBER)
	{
		MaxForwardSpeed = Definition->MaxForwardSpeedCmS;
	}
	if (Definition->MaxReverseSpeedCmS > KINDA_SMALL_NUMBER)
	{
		MaxReverseSpeed = Definition->MaxReverseSpeedCmS;
	}
	if (Definition->MaxVerticalSpeedCmS > KINDA_SMALL_NUMBER)
	{
		MaxVerticalSpeed = Definition->MaxVerticalSpeedCmS;
	}
	if (Definition->MaxThrustN > KINDA_SMALL_NUMBER)
	{
		MaxThrust = Definition->MaxThrustN;
	}

	if (bMassChanged)
	{
		InitializeNeutralBuoyancy();
	}
}

float USubMovementComponent::ComputeBuoyancyForce() const
{
	return WaterDensity * SubmergedVolume * G_SI;
}

float USubMovementComponent::ComputeCenterOfMassXOffset() const
{
	float TotalWaterMass = 0.f;
	float WeightedX = 0.f;

	for (const FBallastTank& Tank : Ballasts)
	{
		const float WaterMass = Tank.FillLevel * Tank.Volume * WaterDensity;
		TotalWaterMass += WaterMass;
		WeightedX += Tank.LocalPosition.X * WaterMass;
	}

	return TotalWaterMass > 0.f ? WeightedX / TotalWaterMass : 0.f;
}

float USubMovementComponent::GetPressureAtDepth(float DepthMeters) const
{
	return 1.f + (WaterDensity * G_SI * DepthMeters) / 101325.f;
}

void USubMovementComponent::SetBallastTarget(int32 Index, float Target)
{
	if (!Ballasts.IsValidIndex(Index))
	{
		return;
	}

	Ballasts[Index].TargetFill = FMath::Clamp(Target, 0.f, 1.f);
}

void USubMovementComponent::ResyncAllBallasts()
{
	for (FBallastTank& Tank : Ballasts)
	{
		Tank.TargetFill = GlobalTargetFill;
	}
}

void USubMovementComponent::ApplyCommandState(const FSubmarineCommandState& CommandState)
{
	SetThrustInput(CommandState.HelmThrottleCmd);
	SetRudderInput(CommandState.HelmYawCmd);
	SetDivePlaneInput(CommandState.HelmTrimCmd);
	GlobalTargetFill = CommandState.GlobalBallastTarget01;
}

float USubMovementComponent::ComputeFloodedMassKg() const
{
	if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
	{
		// Prefer SubFlood (new pipeline) when initialized
		if (Sub->SubFlood && Sub->SubFlood->IsInitialized())
		{
			return Sub->SubFlood->GetTotalWaterMassKg();
		}

		if (Sub->Compartments)
		{
			return Sub->Compartments->GetTotalWaterMassKg();
		}
	}

	return 0.f;
}

void USubMovementComponent::HandleReplicatedNetState(const FSubmarineNetState& NewState)
{
	AActor* Owner = GetOwner();
	if (!Owner || Owner->HasAuthority())
	{
		return;
	}

	const float FixedSimDt = FixedSimulationHz > KINDA_SMALL_NUMBER ? (1.f / FixedSimulationHz) : (1.f / 30.f);
	const int32 PreviousFrame = bHasReceivedSnapshot ? TargetSnapshot.SimFrame : NewState.SimFrame;
	const int32 FrameDelta = (bHasReceivedSnapshot && NewState.SimFrame >= PreviousFrame)
		? (NewState.SimFrame - PreviousFrame)
		: 1;
	const float RawInterpDuration = FMath::Max(FixedSimDt, FrameDelta * FixedSimDt);
	const float MaxInterpDurationSec = CVarSubMaxInterpDurationSec.GetValueOnGameThread();
	const float NewInterpDuration = (MaxInterpDurationSec > 0.f)
		? FMath::Min(RawInterpDuration, FMath::Max(FixedSimDt, MaxInterpDurationSec))
		: RawInterpDuration;
	const float SnapshotDistanceCm = FVector::Dist(Owner->GetActorLocation(), NewState.WorldLocation);

	UE_LOG(
		LogSubMovement,
		Log,
		TEXT("Snapshot received | Frame=%d->%d | Loc=%s | Dist=%.1f | InterpDuration=%.3f"),
		PreviousFrame,
		NewState.SimFrame,
		*NewState.WorldLocation.ToCompactString(),
		SnapshotDistanceCm,
		NewInterpDuration);

	if (!bHasReceivedSnapshot)
	{
		PrevSnapshot = NewState;
		TargetSnapshot = NewState;
		InterpAlpha = 1.f;
		InterpDuration = NewInterpDuration;
		DebugLogTimer = 0.f;
		bHasReceivedSnapshot = true;
		ClientExtrapolationElapsedSec = 0.f;

		Velocity = NewState.LinearVelocity;
		YawRateDegPerSec = NewState.AngularVelocity.Z;
		PitchRateDegPerSec = NewState.AngularVelocity.Y;
		CurrentDepth = NewState.DepthMeters;
		FloodedMassKg = NewState.FloodedMassKg;
		ForwardSpeedCmS = NewState.ForwardSpeed;
		Owner->SetActorLocationAndRotation(NewState.WorldLocation, NewState.QuantizedRotation, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	// Use the currently presented transform as the start of the new lerp,
	// not the theoretical end of the previous one. This prevents visual jumps
	// when a snapshot arrives before the previous lerp completes (alpha < 1)
	// or when a snapshot arrives during post-alpha extrapolation.
	PrevSnapshot.WorldLocation = Owner->GetActorLocation();
	PrevSnapshot.QuantizedRotation = Owner->GetActorRotation();
	TargetSnapshot = NewState;
	InterpAlpha = 0.f;
	InterpDuration = NewInterpDuration;
	ClientExtrapolationElapsedSec = 0.f;

	Velocity = TargetSnapshot.LinearVelocity;
	YawRateDegPerSec = TargetSnapshot.AngularVelocity.Z;
	PitchRateDegPerSec = TargetSnapshot.AngularVelocity.Y;
	CurrentDepth = TargetSnapshot.DepthMeters;
	FloodedMassKg = TargetSnapshot.FloodedMassKg;
	ForwardSpeedCmS = TargetSnapshot.ForwardSpeed;

	if (SnapshotDistanceCm > InterpSnapDistanceCm)
	{
		UE_LOG(
			LogSubMovement,
			Warning,
			TEXT("SUBMARINE SNAP | Distance=%.1f cm | OldLoc=%s -> NewLoc=%s | Frame=%d"),
			SnapshotDistanceCm,
			*Owner->GetActorLocation().ToCompactString(),
			*TargetSnapshot.WorldLocation.ToCompactString(),
			TargetSnapshot.SimFrame);

		OnSubmarineSnapped.Broadcast(SnapshotDistanceCm, NewState);
		InterpAlpha = 1.f;
		Owner->SetActorLocationAndRotation(TargetSnapshot.WorldLocation, TargetSnapshot.QuantizedRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void USubMovementComponent::InterpolateClient(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner || !bHasReceivedSnapshot)
	{
		if (!bDebugLogSubMovement)
		{
			DebugLogTimer = 0.f;
		}
		return;
	}

	// Phase 1: lerp from PrevSnapshot to TargetSnapshot during InterpDuration.
	if (InterpAlpha < 1.f)
	{
		InterpAlpha = FMath::Clamp(InterpAlpha + (DeltaTime / FMath::Max(KINDA_SMALL_NUMBER, InterpDuration)), 0.f, 1.f);

		const FVector SmoothedLocation = FMath::Lerp(
			FVector(PrevSnapshot.WorldLocation),
			FVector(TargetSnapshot.WorldLocation),
			InterpAlpha);
		const FRotator SmoothedRotation = FMath::Lerp(
			PrevSnapshot.QuantizedRotation,
			TargetSnapshot.QuantizedRotation,
			InterpAlpha);

		Owner->SetActorLocationAndRotation(SmoothedLocation, SmoothedRotation, false, nullptr, ETeleportType::None);
	}
	else if (MaxClientExtrapolationSec > KINDA_SMALL_NUMBER)
	{
		// Phase 2: snapshot lerp finished, extrapolate from current pose using
		// the replicated linear / angular velocities to bridge the gap until
		// the next snapshot arrives. Bounded by MaxClientExtrapolationSec to
		// avoid wild drift on dropped packets.
		ClientExtrapolationElapsedSec = FMath::Min(ClientExtrapolationElapsedSec + DeltaTime, MaxClientExtrapolationSec);

		// Extrap step is just DeltaTime worth of velocity since we already
		// applied prior steps to the actor on previous frames; the cap above
		// only stops further accumulation, it doesn't gate this frame's step.
		Owner->AddActorWorldOffset(Velocity * DeltaTime, false, nullptr, ETeleportType::None);

		if (!FMath::IsNearlyZero(YawRateDegPerSec) || !FMath::IsNearlyZero(PitchRateDegPerSec))
		{
			const FRotator AngularStep(PitchRateDegPerSec * DeltaTime, YawRateDegPerSec * DeltaTime, 0.f);
			Owner->AddActorWorldRotation(AngularStep, false, nullptr, ETeleportType::None);
		}
	}

	if (bDebugLogSubMovement)
	{
		DebugLogTimer += DeltaTime;
		if (DebugLogTimer >= 1.f)
		{
			DebugLogTimer = 0.f;
			UE_LOG(
				LogSubMovement,
				Log,
				TEXT("Interp | Alpha=%.3f | Duration=%.3f | Extrap=%.3fs | Loc=%s -> %s"),
				InterpAlpha,
				InterpDuration,
				ClientExtrapolationElapsedSec,
				*PrevSnapshot.WorldLocation.ToCompactString(),
				*TargetSnapshot.WorldLocation.ToCompactString());
		}
	}
	else
	{
		DebugLogTimer = 0.f;
	}
}

void USubMovementComponent::InitializeNeutralBuoyancy()
{
	if (!bAutoNeutralBuoyancyOnBeginPlay)
	{
		return;
	}

	const float NeutralFill = FMath::Clamp(NeutralBuoyancyFill01, 0.f, 1.f);
	float TotalBallastVolume = 0.f;
	for (FBallastTank& Tank : Ballasts)
	{
		TotalBallastVolume += Tank.Volume;
		Tank.FillLevel = NeutralFill;
		Tank.TargetFill = NeutralFill;
	}

	GlobalTargetFill = NeutralFill;
	const float NeutralBallastMassKg = TotalBallastVolume * NeutralFill * WaterDensity;
	const float DesiredNeutralMassKg = FMath::Max(1.f, BaseMass + NeutralBallastMassKg + NeutralBuoyancyMassBiasKg);
	SubmergedVolume = DesiredNeutralMassKg / FMath::Max(1.f, WaterDensity);
}
