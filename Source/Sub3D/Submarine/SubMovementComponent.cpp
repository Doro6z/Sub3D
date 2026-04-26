#include "SubMovementComponent.h"
#include "Sub3DDebugSettings.h"

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
#include "SubmarineSystemsComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubMovement, Log, All);

static constexpr float G_SI = 9.81f;

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

	// Tick order: SubFlood → SubMovement → InteriorFrame → CrewMovement.
	// SubFlood must advance and push FloodImpactKg before SimulateStep reads it.
	if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
	{
		if (Sub->SubFlood)
		{
			AddTickPrerequisiteComponent(Sub->SubFlood);
		}
	}
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

void USubMovementComponent::SetFloodImpactKg(float Value)
{
	FloodImpactKg = FMath::Max(0.f, Value);
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

	// Undo the previous frame's visual interpolation offset BEFORE the sim step so
	// the sim operates on the authoritative pose, not on the render-time lerped pose.
	if (bHasVisualOffset && Owner->HasAuthority())
	{
		Owner->SetActorLocationAndRotation(CurrSimLocation, CurrSimRotation, false, nullptr, ETeleportType::TeleportPhysics);
		bHasVisualOffset = false;
	}

	// Diagnostic freeze: when set on the submarine instance (editor inspector or BP node),
	// zero velocities and skip the sim/interp pipeline. Useful to isolate crew rebase jitter
	// from sub-induced motion. The bootstrap pipeline does NOT touch this flag.
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

			return;
		}
	}

	if (!Owner->HasAuthority())
	{
		// Non-authority: Hermite cubic interpolation between the two most recent snapshots.
		// Tangents = LinearVelocity * Δt at each endpoint. The replicated velocity guarantees
		// C¹ continuity at snapshot boundaries — the velocity at α=1 of segment N matches the
		// velocity at α=0 of segment N+1, eliminating the velocity-discontinuity jerk that
		// linear lerp produced when the sub was accelerating (e.g., sinking under flood load).
		// One-snapshot playback lag is unchanged.
		if (bHasReceivedClientSnapshot && ClientTargetSnapshotTime > ClientPrevSnapshotTime)
		{
			const double Now = Owner->GetWorld() ? Owner->GetWorld()->GetTimeSeconds() : 0.0;
			const double InterpDuration = ClientTargetSnapshotTime - ClientPrevSnapshotTime;
			const float Alpha = FMath::Clamp(
				static_cast<float>((Now - ClientTargetSnapshotTime) / InterpDuration), 0.f, 1.f);

			const float DeltaSec = static_cast<float>(InterpDuration);
			const FVector P0 = ClientPrevSnapshot.WorldLocation;
			const FVector P1 = ClientTargetSnapshot.WorldLocation;
			const FVector T0 = ClientPrevSnapshot.LinearVelocity * DeltaSec;
			const FVector T1 = ClientTargetSnapshot.LinearVelocity * DeltaSec;
			const FVector InterpLoc = FMath::CubicInterp(P0, T0, P1, T1, Alpha);

			// Rotation: linear interp. Hermite on rotation needs angular-velocity tangent
			// application, which is non-trivial because rotations don't compose like
			// translations. Position jerk dominates visually for a translating sub; revisit
			// if rotational jitter becomes prominent.
			const FRotator InterpRot = FMath::Lerp(
				ClientPrevSnapshot.QuantizedRotation,
				ClientTargetSnapshot.QuantizedRotation,
				Alpha);

			Owner->SetActorLocationAndRotation(InterpLoc, InterpRot, false, nullptr, ETeleportType::None);
		}
		return;
	}

	// Authoritative path. Sim runs at FixedSimulationHz (default 60Hz). At render rates
	// that differ from the sim rate, the actor root would otherwise advance in discrete
	// chunks (0/1/2 sim steps per render tick) which is perceptible as jitter. We smooth
	// the render pose via a Lerp(PrevSim, CurrSim, alpha) after the sim loop. The Undo
	// above runs at the start of next tick so the sim always starts from the authoritative
	// pose.
	SimAccumulator += DeltaTime;
	const float FixedSimDt = FixedSimulationHz > KINDA_SMALL_NUMBER ? (1.f / FixedSimulationHz) : (1.f / 30.f);
	bool bSimulated = false;

	while (SimAccumulator >= FixedSimDt)
	{
		PrevSimLocation = Owner->GetActorLocation();
		PrevSimRotation = Owner->GetActorRotation();
		SimulateStep(FixedSimDt);
		SimAccumulator -= FixedSimDt;
		++SimFrameCounter;
		bSimulated = true;
		bHasSimBuffer = true;
	}

	if (bSimulated)
	{
		if (ASubmarineBase* Sub = Cast<ASubmarineBase>(Owner))
		{
			// RepState must sample the authoritative pose, not the interp pose below.
			Sub->RefreshRepState();
		}
	}

	// Capture the latest sim pose as CurrSim, then apply the visual interp.
	CurrSimLocation = Owner->GetActorLocation();
	CurrSimRotation = Owner->GetActorRotation();

	if (bHasSimBuffer && !bLastStepHadBlockingHit)
	{
		const float Alpha = FMath::Clamp(SimAccumulator / FixedSimDt, 0.f, 1.f);
		const FVector InterpLocation = FMath::Lerp(PrevSimLocation, CurrSimLocation, Alpha);
		const FRotator InterpRotation = FMath::Lerp(PrevSimRotation, CurrSimRotation, Alpha);
		Owner->SetActorLocationAndRotation(InterpLocation, InterpRotation, false, nullptr, ETeleportType::None);
		bHasVisualOffset = true;
	}
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

	FloodedMassKg = FloodImpactKg;
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
	if (GetDefault<USub3DDebugSettings>()->bLogSubMovement)
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
			if (GetDefault<USub3DDebugSettings>()->bLogSubCollisionSweeps || GetDefault<USub3DDebugSettings>()->bLogSubMovement)
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

			if (GetDefault<USub3DDebugSettings>()->bLogSubCollisionSweeps)
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
		if (!RemainingDelta.IsNearlyZero() && GetDefault<USub3DDebugSettings>()->bLogSubCollisionSweeps)
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

void USubMovementComponent::HandleReplicatedNetState(const FSubmarineNetState& NewState)
{
	AActor* Owner = GetOwner();
	if (!Owner || Owner->HasAuthority())
	{
		return;
	}

	const double Now = Owner->GetWorld() ? Owner->GetWorld()->GetTimeSeconds() : 0.0;
	const double RealNow = FPlatformTime::Seconds();
	// Wall-clock gap since last snapshot receive. Independent of world time, so it keeps
	// advancing even when this client's world is throttled (alt-tab, focus loss). World-time
	// gap was useless here because both Now and ClientTargetSnapshotTime tick at the same
	// throttled rate during alt-tab → gap stays small → snap-apply never fired.
	const double RealGapSeconds = bHasReceivedClientSnapshot ? (RealNow - ClientLastReceiveRealTime) : 0.0;
	ClientLastReceiveRealTime = RealNow;

	if (!bHasReceivedClientSnapshot)
	{
		// First snapshot: nothing to lerp from yet. Snap-apply the pose so the actor starts
		// at the correct position. Subsequent snapshots will buffer for interpolation.
		ClientPrevSnapshot = NewState;
		ClientTargetSnapshot = NewState;
		ClientPrevSnapshotTime = Now;
		ClientTargetSnapshotTime = Now;
		bHasReceivedClientSnapshot = true;

		Owner->SetActorLocationAndRotation(
			NewState.WorldLocation, NewState.QuantizedRotation,
			false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		// Throttle / focus-loss / hitch detection (real-time based). When the editor loses
		// focus (alt-tab, another PIE window comes to front, OS app switch), UE throttles
		// the unfocused world. World time advances slowly. But the network keeps queuing
		// snapshots from the server. On focus return, snapshots arrive in rapid burst and
		// the client world processes them. World-time gap stays small (Now barely advanced),
		// but real-time gap is large. Snap directly to the latest authoritative pose to
		// skip the stale-snapshot avalanche.
		constexpr double SnapGapThresholdRealSeconds = 0.2;
		if (RealGapSeconds > SnapGapThresholdRealSeconds)
		{
			ClientPrevSnapshot = NewState;
			ClientTargetSnapshot = NewState;
			ClientPrevSnapshotTime = Now;
			ClientTargetSnapshotTime = Now;
			Owner->SetActorLocationAndRotation(
				NewState.WorldLocation, NewState.QuantizedRotation,
				false, nullptr, ETeleportType::TeleportPhysics);
		}
		else
		{
			// Normal shift: previous Target becomes Prev. New state becomes Target.
			//
			// Robust shift: if the previous segment's Hermite hadn't reached α=1 yet
			// (snapshot arrived early relative to the previous InterpDuration), the actor
			// is at some intermediate pose, NOT at the old Target's pose. Using the old
			// Target as new Prev would cause a visible jump from current rendered pose to
			// old Target on the next tick. Instead, anchor new Prev to the currently-rendered
			// pose. The replicated LinearVelocity (carried over from old Target) stays as
			// the new Prev's tangent — it's the server's authoritative velocity at the
			// previous snapshot moment.
			ClientPrevSnapshot = ClientTargetSnapshot;
			ClientPrevSnapshot.WorldLocation = Owner->GetActorLocation();
			ClientPrevSnapshot.QuantizedRotation = Owner->GetActorRotation();
			ClientPrevSnapshotTime = ClientTargetSnapshotTime;
			ClientTargetSnapshot = NewState;
			ClientTargetSnapshotTime = Now;
		}
	}

	// Non-positional state is sampled directly each snapshot — used by HUD / Sub3D debugger.
	Velocity = NewState.LinearVelocity;
	YawRateDegPerSec = NewState.AngularVelocity.Z;
	PitchRateDegPerSec = NewState.AngularVelocity.Y;
	CurrentDepth = NewState.DepthMeters;
	FloodedMassKg = NewState.FloodedMassKg;
	ForwardSpeedCmS = NewState.ForwardSpeed;

	if (GetDefault<USub3DDebugSettings>()->bLogSubMovement)
	{
		UE_LOG(
			LogSubMovement,
			Log,
			TEXT("Snapshot received | Frame=%d | Loc=%s | Δt=%.3fs"),
			NewState.SimFrame,
			*NewState.WorldLocation.ToCompactString(),
			static_cast<float>(ClientTargetSnapshotTime - ClientPrevSnapshotTime));
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
