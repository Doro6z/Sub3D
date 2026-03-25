#include "SubMovementComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "SubmarineBase.h"
#include "SubmarineCompartmentComponent.h"
#include "SubHullComponent.h"
#include "SubmarineSystemsComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubMovement, Log, All);

static constexpr float G_SI = 9.81f;

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
}

void USubMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		InterpolateClient(DeltaTime);
		return;
	}

	SimAccumulator += DeltaTime;
	const float FixedSimDt = FixedSimulationHz > KINDA_SMALL_NUMBER ? (1.f / FixedSimulationHz) : (1.f / 30.f);
	bool bSimulated = false;

	while (SimAccumulator >= FixedSimDt)
	{
		SimulateStep(FixedSimDt);
		SimAccumulator -= FixedSimDt;
		++SimFrameCounter;
		bSimulated = true;
	}

	if (bSimulated)
	{
		if (ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
		{
			Sub->RefreshRepState();
		}
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

	const float TotalMass = ComputeTotalMass();
	const float BuoyancyN = ComputeBuoyancyForce();
	const float GravityN = TotalMass * G_SI;
	const float NetVerticalN = BuoyancyN - GravityN;

	FVector Acceleration = FVector::ZeroVector;
	Acceleration.Z = (NetVerticalN / FMath::Max(1.f, TotalMass)) * 100.f;

	const FVector ForwardDir = Owner->GetActorForwardVector();
	Acceleration += ForwardDir * (ThrustInput * MaxThrust / FMath::Max(1.f, TotalMass)) * 100.f;

	const FTransform OwnerTransform = Owner->GetActorTransform();
	const FVector LocalVelCm = OwnerTransform.InverseTransformVector(Velocity);
	const FVector LocalVelM = LocalVelCm / 100.f;

	FVector LocalDragN;
	LocalDragN.X = -0.5f * WaterDensity * DragCoefficients.X * CrossSections.X * LocalVelM.X * FMath::Abs(LocalVelM.X);
	LocalDragN.Y = -0.5f * WaterDensity * DragCoefficients.Y * CrossSections.Y * LocalVelM.Y * FMath::Abs(LocalVelM.Y);
	LocalDragN.Z = -0.5f * WaterDensity * DragCoefficients.Z * CrossSections.Z * LocalVelM.Z * FMath::Abs(LocalVelM.Z);

	const FVector WorldDragN = OwnerTransform.TransformVector(LocalDragN);
	Acceleration += (WorldDragN / FMath::Max(1.f, TotalMass)) * 100.f;

	FRotator NewRotation = Owner->GetActorRotation();
	NewRotation.Yaw += RudderInput * RudderTurnRate * DeltaTime;

	const float ForwardSpeedAbs = FMath::Abs(LocalVelCm.X);
	const float SpeedFactor = HydroplaneAuthoritySpeed > KINDA_SMALL_NUMBER
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

	const float HydroplanePitchAccel = DivePlaneInput * PitchFromHydroplaneAccel * SpeedFactor;
	const float BallastPitchTarget = -ComputeCenterOfMassXOffset() / 100.f * BallastPitchFactor;
	const float BallastPitchRateCorrection = (BallastPitchTarget - NewRotation.Pitch) * 0.6f;

	PitchRateDegPerSec += (
		HydroplanePitchAccel +
		BallastTrimRateBias +
		BallastPitchRateCorrection -
		(PitchRateDegPerSec * PitchRateDamping)
	) * DeltaTime;

	NewRotation.Pitch = FMath::Clamp(
		NewRotation.Pitch + PitchRateDegPerSec * DeltaTime,
		-MaxDivePlanePitch,
		MaxDivePlanePitch
	);

	const float PitchRad = FMath::DegreesToRadians(NewRotation.Pitch);
	const float VerticalFromPitchAccel = LocalVelCm.X * FMath::Sin(PitchRad) * VerticalFromPitchFactor;
	Acceleration.Z += VerticalFromPitchAccel;

	Velocity += Acceleration * DeltaTime;

	FVector ClampedLocalVelocity = OwnerTransform.InverseTransformVector(Velocity);
	ClampedLocalVelocity.X = FMath::Clamp(ClampedLocalVelocity.X, -MaxReverseSpeed, MaxForwardSpeed);
	if (FMath::Abs(ThrustInput) < 0.01f)
	{
		ClampedLocalVelocity.X = FMath::FInterpTo(ClampedLocalVelocity.X, 0.f, DeltaTime, IdleForwardSpeedDamping);
	}
	ClampedLocalVelocity.Y = FMath::FInterpTo(ClampedLocalVelocity.Y, 0.f, DeltaTime, LateralSpeedDamping);
	ClampedLocalVelocity.Z = FMath::Clamp(ClampedLocalVelocity.Z, -MaxVerticalSpeed, MaxVerticalSpeed);
	Velocity = OwnerTransform.TransformVector(ClampedLocalVelocity);

	const FVector DeltaLocation = Velocity * DeltaTime;
	bool bHadBlockingHit = false;
	const auto MoveWithSlide = [Owner](const FVector& MoveDelta, FVector& InOutVelocity)
	{
		if (MoveDelta.IsNearlyZero())
		{
			return false;
		}

		FHitResult Hit;
		Owner->AddActorWorldOffset(MoveDelta, true, &Hit, ETeleportType::None);
		if (!Hit.bBlockingHit)
		{
			return false;
		}

		if (Hit.bStartPenetrating)
		{
			const FVector Depenetration = Hit.Normal * FMath::Max(2.f, Hit.PenetrationDepth + 1.f);
			Owner->AddActorWorldOffset(Depenetration, false, nullptr, ETeleportType::None);
		}

		const float RemainingTime = 1.f - FMath::Clamp(Hit.Time, 0.f, 1.f);
		const FVector SlideDelta = FVector::VectorPlaneProject(MoveDelta * RemainingTime, Hit.Normal);
		if (!SlideDelta.IsNearlyZero())
		{
			FHitResult SlideHit;
			Owner->AddActorWorldOffset(SlideDelta, true, &SlideHit, ETeleportType::None);
		}

		InOutVelocity = FVector::VectorPlaneProject(InOutVelocity, Hit.Normal);
		return true;
	};

	const FVector HorizontalDelta = FVector(DeltaLocation.X, DeltaLocation.Y, 0.f);
	const FVector VerticalDelta = FVector(0.f, 0.f, DeltaLocation.Z);
	bHadBlockingHit |= MoveWithSlide(HorizontalDelta, Velocity);
	bHadBlockingHit |= MoveWithSlide(VerticalDelta, Velocity);
	if (bHadBlockingHit)
	{
		Velocity = FMath::VInterpTo(Velocity, FVector::ZeroVector, DeltaTime, ContactVelocityDamping);
	}

	Owner->SetActorRotation(NewRotation, ETeleportType::None);
	CurrentDepth = FMath::Max(0.f, -Owner->GetActorLocation().Z / 100.f);
}

float USubMovementComponent::ComputeTotalMass() const
{
	float WaterMass = 0.f;
	for (const FBallastTank& Tank : Ballasts)
	{
		WaterMass += Tank.FillLevel * Tank.Volume * WaterDensity;
	}

	return BaseMass + WaterMass + (FloodedMassKg * FloodedMassInfluence);
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
		if (Sub->Compartments)
		{
			return Sub->Compartments->GetTotalWaterMassKg();
		}

		if (Sub->SubHull)
		{
			return Sub->SubHull->GetTotalWaterLiters();
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
	const float NewInterpDuration = FMath::Max(FixedSimDt, FrameDelta * FixedSimDt);
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

		Velocity = NewState.LinearVelocity;
		CurrentDepth = NewState.DepthMeters;
		FloodedMassKg = NewState.FloodedMassKg;
		Owner->SetActorLocationAndRotation(NewState.WorldLocation, NewState.QuantizedRotation, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	// Use the currently presented transform as the start of the new lerp,
	// not the theoretical end of the previous one. This prevents visual jumps
	// when a snapshot arrives before the previous lerp completes (alpha < 1).
	PrevSnapshot.WorldLocation = Owner->GetActorLocation();
	PrevSnapshot.QuantizedRotation = Owner->GetActorRotation();
	TargetSnapshot = NewState;
	InterpAlpha = 0.f;
	InterpDuration = NewInterpDuration;

	Velocity = TargetSnapshot.LinearVelocity;
	CurrentDepth = TargetSnapshot.DepthMeters;
	FloodedMassKg = TargetSnapshot.FloodedMassKg;

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
	if (!Owner || !bHasReceivedSnapshot || InterpAlpha >= 1.f)
	{
		if (!bDebugLogSubMovement)
		{
			DebugLogTimer = 0.f;
		}
		return;
	}

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

	if (bDebugLogSubMovement)
	{
		DebugLogTimer += DeltaTime;
		if (DebugLogTimer >= 1.f)
		{
			DebugLogTimer = 0.f;
			UE_LOG(
				LogSubMovement,
				Log,
				TEXT("Interp | Alpha=%.3f | Duration=%.3f | Loc=%s -> %s | Rot=%s -> %s"),
				InterpAlpha,
				InterpDuration,
				*PrevSnapshot.WorldLocation.ToCompactString(),
				*TargetSnapshot.WorldLocation.ToCompactString(),
				*PrevSnapshot.QuantizedRotation.ToCompactString(),
				*TargetSnapshot.QuantizedRotation.ToCompactString());
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
