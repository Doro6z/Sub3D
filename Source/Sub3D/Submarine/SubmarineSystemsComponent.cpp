#include "SubmarineSystemsComponent.h"

#include "Net/UnrealNetwork.h"
#include "SubHullComponent.h"
#include "SubMovementComponent.h"
#include "SubmarineBase.h"
#include "TurretActor.h"

USubmarineSystemsComponent::USubmarineSystemsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void USubmarineSystemsComponent::BeginPlay()
{
	Super::BeginPlay();
	PushPumpStateToHull();
}

void USubmarineSystemsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UpdateAutoDepth(DeltaTime);
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

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->SetThrustInput(CommandState.HelmThrottleCmd);
		}
	}
}

void USubmarineSystemsComponent::SetHelmYawCommand(float Value)
{
	CommandState.HelmYawCmd = FMath::Clamp(Value, -1.f, 1.f);

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

	if (ASubmarineBase* Sub = ResolveOwnerSubmarine())
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->SetDivePlaneInput(CommandState.HelmTrimCmd);
		}
	}
}

void USubmarineSystemsComponent::SetAutoDepthEnabled(bool bEnabled)
{
	CommandState.bAutoDepthEnabled = bEnabled;

	if (bEnabled)
	{
		if (const ASubmarineBase* Sub = ResolveOwnerSubmarine())
		{
			if (Sub->SubMovement)
			{
				CommandState.TargetDepthMeters = Sub->SubMovement->CurrentDepth;
			}
		}
	}
}

void USubmarineSystemsComponent::SetTargetDepthMeters(float DepthMeters)
{
	CommandState.TargetDepthMeters = FMath::Max(0.f, DepthMeters);
}

void USubmarineSystemsComponent::SetGlobalBallastTarget(float Target)
{
	CommandState.GlobalBallastTarget01 = FMath::Clamp(Target, 0.f, 1.f);
	CommandState.bAutoDepthEnabled = false;

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
	CommandState.bAutoDepthEnabled = false;

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
	if (!Sub || !Sub->SubHull)
	{
		return;
	}

	const float PumpRate = BasePumpRateLitersPerSec * FMath::Clamp(CommandState.PumpPower01, 0.f, 1.f);
	Sub->SubHull->SetAllPumpsActive(CommandState.bPumpActive, PumpRate, DefaultPumpCompartmentId);
}

void USubmarineSystemsComponent::UpdateAutoDepth(float DeltaTime)
{
	if (!CommandState.bAutoDepthEnabled)
	{
		return;
	}

	ASubmarineBase* Sub = ResolveOwnerSubmarine();
	if (!Sub || !Sub->SubMovement)
	{
		return;
	}

	const float DepthError = CommandState.TargetDepthMeters - Sub->SubMovement->CurrentDepth;
	const float TargetBallast = FMath::Clamp(0.5f + DepthError * AutoDepthBallastGain, 0.f, 1.f);
	CommandState.GlobalBallastTarget01 = FMath::FInterpTo(
		CommandState.GlobalBallastTarget01,
		TargetBallast,
		DeltaTime,
		1.2f
	);

	Sub->SubMovement->GlobalTargetFill = CommandState.GlobalBallastTarget01;
	Sub->SubMovement->ResyncAllBallasts();
}
