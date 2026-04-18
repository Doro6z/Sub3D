#include "SubmarineSystemsComponent.h"

#include "Generator/SubmarineDefinition.h"
#include "Net/UnrealNetwork.h"
#include "SubFloodComponent.h"
#include "SubMovementComponent.h"
#include "SubmarineBase.h"
#include "TurretActor.h"

namespace
{
const FName StabilizationAxisSpeed(TEXT("Speed"));
const FName StabilizationAxisDepth(TEXT("Depth"));
const FName StabilizationAxisPitch(TEXT("Pitch"));
}

USubmarineSystemsComponent::USubmarineSystemsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void USubmarineSystemsComponent::BeginPlay()
{
	Super::BeginPlay();

	// Resolve pump compartment from generated definition if available.
	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->GeneratedDefinition && Sub->GeneratedDefinition->Compartments.Num() > 0)
		{
			DefaultPumpCompartmentId = Sub->GeneratedDefinition->Compartments[0].CompartmentId;
		}
	}

	PushPumpStateToHull();
}

void USubmarineSystemsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UpdateStabilization(DeltaTime);
	PushPumpStateToHull();
}

void USubmarineSystemsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USubmarineSystemsComponent, CommandState);
	DOREPLIFETIME(USubmarineSystemsComponent, EngineHealth01);
	DOREPLIFETIME(USubmarineSystemsComponent, ElectricalHealth01);
}

void USubmarineSystemsComponent::SetHelmThrottleCommand(float Value)
{
	CommandState.HelmThrottleCmd = FMath::Clamp(Value, -1.f, 1.f);
	NotifyManualInput(StabilizationAxisSpeed);

	// Absolute command from slider / UI. Cancel any active ramp so the key
	// ramp doesn't fight the widget value on the next stabilization tick.
	ThrottleRampIntent = 0.f;

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->SetPowerInput(CommandState.HelmThrottleCmd);
		}
	}
}

void USubmarineSystemsComponent::SetThrottleRampIntent(float Intent)
{
	ThrottleRampIntent = FMath::Clamp(Intent, -1.f, 1.f);

	// While the key is held the player is actively commanding throttle; keep
	// AutoSpeed suspended the same way a slider movement would.
	if (!FMath::IsNearlyZero(ThrottleRampIntent))
	{
		NotifyManualInput(StabilizationAxisSpeed);
	}
}

void USubmarineSystemsComponent::SetRudderRampIntent(float Intent)
{
	RudderRampIntent = FMath::Clamp(Intent, -1.f, 1.f);
	// No NotifyManualInput here because rudder has no auto-equivalent today
	// (no AutoYaw axis in the suspend table). When auto-yaw is added in the
	// future, mirror the throttle pattern.
}

void USubmarineSystemsComponent::SetDivePlaneRampIntent(float Intent)
{
	DivePlaneRampIntent = FMath::Clamp(Intent, -1.f, 1.f);

	// Active player input on the dive plane should suspend AutoPitch the
	// same way moving the trim slider does.
	if (!FMath::IsNearlyZero(DivePlaneRampIntent))
	{
		NotifyManualInput(StabilizationAxisPitch);
	}
}

void USubmarineSystemsComponent::SetHelmYawCommand(float Value)
{
	CommandState.HelmYawCmd = FMath::Clamp(Value, -1.f, 1.f);

	// Absolute set from slider/UI. Cancel any active ramp so the key ramp
	// doesn't fight the widget value on the next stabilization tick.
	RudderRampIntent = 0.f;

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->SetRudderInput(CommandState.HelmYawCmd);
		}
	}
}

void USubmarineSystemsComponent::SetHelmTrimCommand(float Value)
{
	CommandState.HelmTrimCmd = FMath::Clamp(Value, -1.f, 1.f);
	CommandState.MainTrimBiasCmd = CommandState.HelmTrimCmd;
	NotifyManualInput(StabilizationAxisPitch);

	// Absolute set from slider/UI. Cancel any active ramp.
	DivePlaneRampIntent = 0.f;

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->SetDivePlaneInput(CommandState.HelmTrimCmd);
		}
	}
}

void USubmarineSystemsComponent::SetRudderHoldEnabled(bool bEnabled)
{
	CommandState.bRudderHoldEnabled = bEnabled;
}

void USubmarineSystemsComponent::SetPlaneHoldEnabled(bool bEnabled)
{
	CommandState.bPlaneHoldEnabled = bEnabled;
}

void USubmarineSystemsComponent::SetStabilizationMasterEnabled(bool bEnabled)
{
	CommandState.bStabilizationMasterEnabled = bEnabled;
}

void USubmarineSystemsComponent::SetAutoSpeedEnabled(bool bEnabled)
{
	CommandState.bAutoSpeedEnabled = bEnabled;
	SpeedSuspendTimer = 0.f;

	if (!bEnabled)
	{
		return;
	}

	if (const ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			CommandState.TargetSpeedCmS = Sub->SubMovement->ForwardSpeedCmS;
		}
	}
}

void USubmarineSystemsComponent::SetAutoDepthEnabled(bool bEnabled)
{
	CommandState.bAutoDepthEnabled = bEnabled;
	DepthSuspendTimer = 0.f;

	if (!bEnabled)
	{
		return;
	}

	CommandState.bBallastsActive = true;

	if (const ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			CommandState.TargetDepthMeters = Sub->SubMovement->CurrentDepth;
		}
	}
}

void USubmarineSystemsComponent::SetAutoPitchEnabled(bool bEnabled)
{
	CommandState.bAutoPitchEnabled = bEnabled;
	PitchSuspendTimer = 0.f;

	if (!bEnabled)
	{
		return;
	}

	if (const ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		CommandState.TargetPitchDeg = Sub ? Sub->GetActorRotation().Pitch : 0.f;
	}
}

void USubmarineSystemsComponent::SetTargetSpeedCmS(float SpeedCmS)
{
	float MinSpeed = -400.f;
	float MaxSpeed = 1250.f;

	if (const ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			MinSpeed = -Sub->SubMovement->MaxReverseSpeed;
			MaxSpeed = Sub->SubMovement->MaxForwardSpeed;
		}
	}

	CommandState.TargetSpeedCmS = FMath::Clamp(SpeedCmS, MinSpeed, MaxSpeed);
}

void USubmarineSystemsComponent::SetTargetDepthMeters(float DepthMeters)
{
	CommandState.TargetDepthMeters = FMath::Max(0.f, DepthMeters);
}

void USubmarineSystemsComponent::SetTargetPitchDeg(float PitchDeg)
{
	float MaxPitch = 30.f;

	if (const ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			MaxPitch = Sub->SubMovement->MaxDivePlanePitch;
		}
	}

	CommandState.TargetPitchDeg = FMath::Clamp(PitchDeg, -MaxPitch, MaxPitch);
}

void USubmarineSystemsComponent::NotifyManualInput(FName Axis)
{
	if (Axis == StabilizationAxisSpeed || Axis == FName(TEXT("Throttle")))
	{
		SpeedSuspendTimer = SuspendDurationSeconds;
		return;
	}

	if (Axis == StabilizationAxisDepth || Axis == FName(TEXT("Ballast")))
	{
		DepthSuspendTimer = SuspendDurationSeconds;
		return;
	}

	if (Axis == StabilizationAxisPitch || Axis == FName(TEXT("Trim")))
	{
		PitchSuspendTimer = SuspendDurationSeconds;
	}
}

bool USubmarineSystemsComponent::IsAutoSpeedActive() const
{
	return CommandState.bStabilizationMasterEnabled
		&& CommandState.bAutoSpeedEnabled
		&& SpeedSuspendTimer <= 0.f;
}

bool USubmarineSystemsComponent::IsAutoDepthActive() const
{
	return CommandState.bStabilizationMasterEnabled
		&& CommandState.bAutoDepthEnabled
		&& CommandState.bBallastsActive
		&& DepthSuspendTimer <= 0.f;
}

bool USubmarineSystemsComponent::IsAutoPitchActive() const
{
	return CommandState.bStabilizationMasterEnabled
		&& CommandState.bAutoPitchEnabled
		&& PitchSuspendTimer <= 0.f;
}

void USubmarineSystemsComponent::SetGlobalBallastTarget(float Target)
{
	CommandState.GlobalBallastTarget01 = FMath::Clamp(Target, 0.f, 1.f);
	NotifyManualInput(StabilizationAxisDepth);

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->GlobalTargetFill = CommandState.GlobalBallastTarget01;
			Sub->SubMovement->ResyncAllBallasts();
		}
	}
}

void USubmarineSystemsComponent::SetBallastTargetByIndex(int32 Index, float Target)
{
	NotifyManualInput(StabilizationAxisDepth);

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->SetBallastTarget(Index, FMath::Clamp(Target, 0.f, 1.f));
		}
	}
}

void USubmarineSystemsComponent::SetBallastsActive(bool bActive)
{
	CommandState.bBallastsActive = bActive;
	if (bActive)
	{
		ResyncAllBallasts();
	}
}

void USubmarineSystemsComponent::ResyncAllBallasts()
{
	if (!CommandState.bBallastsActive)
	{
		return;
	}

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->GlobalTargetFill = CommandState.GlobalBallastTarget01;
			Sub->SubMovement->ResyncAllBallasts();
		}
	}
}

void USubmarineSystemsComponent::SetPumpActive(bool bActive)
{
	CommandState.bPumpActive = bActive;
	PushPumpStateToHull();
}

void USubmarineSystemsComponent::SetPumpPower01(float Power01)
{
	CommandState.PumpPower01 = FMath::Clamp(Power01, 0.f, 1.f);
	PushPumpStateToHull();
}

void USubmarineSystemsComponent::SetEngineBoost(float Value)
{
	CommandState.EngineBoostCmd = FMath::Clamp(Value, 0.f, 1.f);
}

void USubmarineSystemsComponent::SetTurretAim(const FRotator& Aim)
{
	CommandState.TurretAimCmd = Aim;

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->ExteriorTurret)
		{
			Sub->ExteriorTurret->SetAimCommand(Aim);
		}
	}
}

void USubmarineSystemsComponent::SetTurretFireHeld(bool bHeld)
{
	CommandState.bTurretFireHeld = bHeld;

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->ExteriorTurret)
		{
			Sub->ExteriorTurret->SetFireHeld(bHeld);
		}
	}
}

void USubmarineSystemsComponent::OnRep_CommandState()
{
}

ASubmarineBase* USubmarineSystemsComponent::ResolveOwnerSubmarine() const
{
	return Cast<ASubmarineBase>(GetOwner());
}

void USubmarineSystemsComponent::PushPumpStateToHull() const
{
	const ASubmarineBase* Sub = ResolveOwnerSubmarine();
	if (!Sub)
	{
		return;
	}

	const float PumpRate = BasePumpRateLitersPerSec * FMath::Clamp(CommandState.PumpPower01, 0.f, 1.f);

	// Prefer SubFlood when initialized.
	if (Sub->SubFlood && Sub->SubFlood->IsInitialized())
	{
		Sub->SubFlood->SetPumpActive(DefaultPumpCompartmentId, CommandState.bPumpActive, PumpRate);
	}
}

void USubmarineSystemsComponent::UpdateStabilization(float DeltaTime)
{
	SpeedSuspendTimer = FMath::Max(0.f, SpeedSuspendTimer - DeltaTime);
	DepthSuspendTimer = FMath::Max(0.f, DepthSuspendTimer - DeltaTime);
	PitchSuspendTimer = FMath::Max(0.f, PitchSuspendTimer - DeltaTime);

	ASubmarineBase* Sub = ResolveOwnerSubmarine();
	if (!Sub || !Sub->SubMovement)
	{
		return;
	}

	USubMovementComponent* Movement = Sub->SubMovement;

	// Throttle ramp: advance HelmThrottleCmd while the player holds the key.
	// Skipped when auto-speed is active (auto owns the throttle) so the two
	// systems don't fight. Intent is cleared by SetHelmThrottleCommand when
	// the slider takes over.
	if (!IsAutoSpeedActive() && !FMath::IsNearlyZero(ThrottleRampIntent))
	{
		const float Delta = ThrottleRampIntent * ThrottleRampRate * DeltaTime;
		CommandState.HelmThrottleCmd = FMath::Clamp(CommandState.HelmThrottleCmd + Delta, -1.f, 1.f);
		Movement->SetPowerInput(CommandState.HelmThrottleCmd);
	}

	if (IsAutoSpeedActive())
	{
		const float SpeedError = CommandState.TargetSpeedCmS - Movement->ForwardSpeedCmS;
		CommandState.HelmThrottleCmd = FMath::Clamp(SpeedError * AutoSpeedGain, -1.f, 1.f);
		Movement->SetPowerInput(CommandState.HelmThrottleCmd);
	}

	if (IsAutoDepthActive())
	{
		// Target zero vertical velocity rather than a specific depth. When
		// the sub drifts upward (Velocity.Z > 0), add ballast (fill > neutral)
		// to make it sink; when it drifts down, remove ballast. The
		// GlobalBallastTarget01 slider replicates, so every client sees the
		// auto-controller move it in real time — the visible "self-moving
		// slider" the player expects as feedback.
		const float VerticalSpeedCmS = Movement->Velocity.Z;
		const float NeutralFill = FMath::Clamp(Movement->NeutralBuoyancyFill01, 0.f, 1.f);
		const float TargetBallast = FMath::Clamp(
			NeutralFill + VerticalSpeedCmS * AutoDepthVelocityGain,
			0.f, 1.f);

		CommandState.GlobalBallastTarget01 = FMath::FInterpTo(
			CommandState.GlobalBallastTarget01,
			TargetBallast,
			DeltaTime,
			AutoDepthResponseRate
		);

		Movement->GlobalTargetFill = CommandState.GlobalBallastTarget01;
		Movement->ResyncAllBallasts();
	}

	// Dive plane: priority order is AutoPitch > player ramp > auto-recenter.
	if (IsAutoPitchActive())
	{
		const float CurrentPitchDeg = FRotator::NormalizeAxis(Sub->GetActorRotation().Pitch);
		const float PitchErrorDeg = FMath::FindDeltaAngleDegrees(CurrentPitchDeg, CommandState.TargetPitchDeg);
		CommandState.HelmTrimCmd = FMath::Clamp(PitchErrorDeg * AutoPitchPlaneGain, -1.f, 1.f);
		CommandState.MainTrimBiasCmd = CommandState.HelmTrimCmd;
		Movement->SetDivePlaneInput(CommandState.HelmTrimCmd);
	}
	else if (!FMath::IsNearlyZero(DivePlaneRampIntent))
	{
		const float Delta = DivePlaneRampIntent * DivePlaneRampRate * DeltaTime;
		CommandState.HelmTrimCmd = FMath::Clamp(CommandState.HelmTrimCmd + Delta, -1.f, 1.f);
		CommandState.MainTrimBiasCmd = CommandState.HelmTrimCmd;
		Movement->SetDivePlaneInput(CommandState.HelmTrimCmd);
	}
	else if (!CommandState.bPlaneHoldEnabled)
	{
		CommandState.HelmTrimCmd = FMath::FInterpConstantTo(CommandState.HelmTrimCmd, 0.f, DeltaTime, PlaneReturnRate);
		CommandState.MainTrimBiasCmd = CommandState.HelmTrimCmd;
		Movement->SetDivePlaneInput(CommandState.HelmTrimCmd);
	}

	// Rudder: player ramp wins over auto-recenter so holding A/D doesn't get
	// fought by RudderReturnRate. Release the key (Intent=0) and the existing
	// "no hold" auto-recenter resumes.
	if (!FMath::IsNearlyZero(RudderRampIntent))
	{
		const float Delta = RudderRampIntent * RudderRampRate * DeltaTime;
		CommandState.HelmYawCmd = FMath::Clamp(CommandState.HelmYawCmd + Delta, -1.f, 1.f);
		Movement->SetRudderInput(CommandState.HelmYawCmd);
	}
	else if (!CommandState.bRudderHoldEnabled)
	{
		CommandState.HelmYawCmd = FMath::FInterpConstantTo(CommandState.HelmYawCmd, 0.f, DeltaTime, RudderReturnRate);
		Movement->SetRudderInput(CommandState.HelmYawCmd);
	}
}
