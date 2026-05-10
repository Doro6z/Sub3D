#include "SubPlayerController.h"
#include "SubStationInterface.h"
#include "SubmarineBase.h"
#include "SubSonarComponent.h"
#include "SubSonarSystemComponent.h"
#include "SubDoorActor.h"
#include "SubFloodComponent.h"
#include "SubHullComponent.h"
#include "SubmarineSystemsComponent.h"
#include "Debug/CrewAnimDebugComponent.h"
#include "SubmarineCompartmentComponent.h"
#include "SubMovementComponent.h"
#include "SubCrewCharacter.h"
#include "SubPlayerHUDWidget.h"
#include "SubCrewAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Generator/SubmarineDefinition.h"
#include "Generator/SubmarineDefinitionTypes.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubController, Log, All);

void ASubPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASubPlayerController, CurrentControlMode);
	DOREPLIFETIME(ASubPlayerController, CurrentStation);
	DOREPLIFETIME(ASubPlayerController, CurrentStationType);
	DOREPLIFETIME(ASubPlayerController, AssignedSpawnSlot);
}

void ASubPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Create the HUD widget if we are the local player and a class is provided
	if (IsLocalController() && HUDWidgetClass)
	{
		HUDWidget = CreateWidget<USubPlayerHUDWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
			UE_LOG(LogSubController, Log, TEXT("[%s] HUD created and added to viewport."), *GetName());
		}
	}
}

void ASubPlayerController::ServerEnterStation_Implementation(AActor* Station)
{
	UE_LOG(
		LogSubController,
		Log,
		TEXT("[%s] ServerEnterStation called | Station=%s | Pawn=%s"),
		*GetName(),
		*GetNameSafe(Station),
		*GetNameSafe(GetPawn())
	);

	if (!Station || !Station->Implements<USubStationInterface>())
	{
		UE_LOG(LogSubController, Warning, TEXT("[%s] ServerEnterStation rejected | Invalid station or missing interface."), *GetName());
		return;
	}

	if (!ISubStationInterface::Execute_CanEnterStation(Station, this))
	{
		AController* ExistingOccupant = ISubStationInterface::Execute_GetCurrentOccupant(Station);
		ASubmarineBase* OwningSub = ISubStationInterface::Execute_GetOwningSubmarine(Station);
		UE_LOG(
			LogSubController,
			Warning,
			TEXT("[%s] ServerEnterStation rejected by CanEnter | Station=%s | Occupant=%s | OwningSubmarine=%s"),
			*GetName(),
			*GetNameSafe(Station),
			*GetNameSafe(ExistingOccupant),
			*GetNameSafe(OwningSub)
		);
		return;
	}

	ASubmarineBase* StationSubmarine = ISubStationInterface::Execute_GetOwningSubmarine(Station);

	// Hard-reject upstream invariant violations. Reaching this point with no crew, no
	// CurrentSubmarine, or a mismatched sub means the bootstrap pipeline failed to
	// embark the crew before they reached the helm. The previous "recover from station"
	// fallback masked these bugs; we now surface them and refuse the interaction.
	ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetPawn());
	if (!Crew)
	{
		UE_LOG(LogSubController, Error,
			TEXT("[%s] ServerEnterStation REJECTED | Pawn is not ASubCrewCharacter | Pawn=%s"),
			*GetName(), *GetNameSafe(GetPawn()));
		return;
	}
	if (!Crew->CurrentSubmarine)
	{
		UE_LOG(LogSubController, Error,
			TEXT("[%s] ServerEnterStation REJECTED | Crew->CurrentSubmarine is null. ")
			TEXT("Bootstrap failed to embark this crew. Station=%s | StationSub=%s"),
			*GetName(), *GetNameSafe(Station), *GetNameSafe(StationSubmarine));
		return;
	}
	if (StationSubmarine && Crew->CurrentSubmarine != StationSubmarine)
	{
		UE_LOG(LogSubController, Error,
			TEXT("[%s] ServerEnterStation REJECTED | Crew/Station sub mismatch. ")
			TEXT("CrewSub=%s | StationSub=%s | Station=%s"),
			*GetName(),
			*GetNameSafe(Crew->CurrentSubmarine),
			*GetNameSafe(StationSubmarine),
			*GetNameSafe(Station));
		return;
	}

	CurrentStation = Station;
	CurrentStationType = ISubStationInterface::Execute_GetStationType(Station);

	ISubStationInterface::Execute_RequestEnterStation(Station, this);

	if (CurrentStationType == ESubStationType::Helm)
	{
		CurrentControlMode = ECrewControlMode::HelmDriving;
		Crew->ForceHelm();
	}
	else
	{
		if (StationSubmarine && StationSubmarine->Sonar)
		{
			StationSubmarine->Sonar->StopContinuousPing();
		}

		CurrentControlMode = ECrewControlMode::StationUI;
		if (bSonarLeanActive)
		{
			bSonarLeanActive = false;
			BP_OnSonarLeanChanged(false);
		}
	}

	UE_LOG(
		LogSubController,
		Log,
		TEXT("[%s] ServerEnterStation accepted | Station=%s | Type=%d | Mode=%d"),
		*GetName(),
		*GetNameSafe(CurrentStation),
		static_cast<int32>(CurrentStationType),
		static_cast<int32>(CurrentControlMode)
	);

	ClientSetControlMode(CurrentControlMode, CurrentStationType);
}

void ASubPlayerController::ServerEnterHelm_Implementation(AActor* Station)
{
	// Backward-compatible alias for existing BP calls.
	ServerEnterStation_Implementation(Station);
}

void ASubPlayerController::ServerExitStation_Implementation()
{
	UE_LOG(
		LogSubController,
		Log,
		TEXT("[%s] ServerExitStation called | CurrentStation=%s | CurrentType=%d"),
		*GetName(),
		*GetNameSafe(CurrentStation),
		static_cast<int32>(CurrentStationType)
	);

	ASubmarineBase* CurrentSubmarine = ResolveCurrentSubmarine();
	if (CurrentSubmarine && CurrentSubmarine->Sonar)
	{
		CurrentSubmarine->Sonar->StopContinuousPing();
	}

	if (CurrentStation && CurrentStation->Implements<USubStationInterface>())
	{
		ISubStationInterface::Execute_RequestExitStation(CurrentStation, this);
	}

	CurrentStation = nullptr;
	CurrentStationType = ESubStationType::None;
	CurrentControlMode = ECrewControlMode::OnFoot;
	if (bSonarLeanActive)
	{
		bSonarLeanActive = false;
		BP_OnSonarLeanChanged(false);
	}

	if (ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetPawn()))
	{
		Crew->ReleaseHelm();
	}

	UE_LOG(LogSubController, Log, TEXT("[%s] ServerExitStation applied | Mode=OnFoot"), *GetName());

	ClientSetControlMode(CurrentControlMode, ESubStationType::None);
}

void ASubPlayerController::ServerExitHelm_Implementation()
{
	// Backward-compatible alias for existing BP calls.
	ServerExitStation_Implementation();
}

void ASubPlayerController::ClientSetControlMode_Implementation(ECrewControlMode Mode, ESubStationType StationType)
{
	UE_LOG(
		LogSubController,
		Log,
		TEXT("[%s] ClientSetControlMode | Old=%d -> New=%d | StationType=%d"),
		*GetName(),
		static_cast<int32>(CurrentControlMode),
		static_cast<int32>(Mode),
		static_cast<int32>(StationType)
	);
	CurrentControlMode = Mode;
	if (CurrentControlMode != ECrewControlMode::HelmDriving && bSonarLeanActive)
	{
		bSonarLeanActive = false;
		BP_OnSonarLeanChanged(false);
	}
	BP_OnControlModeChanged(Mode, StationType);
}

void ASubPlayerController::SetSonarLeanActive(bool bActive)
{
	const bool bAllowLean = (CurrentControlMode == ECrewControlMode::HelmDriving);
	const bool bNewValue = bAllowLean ? bActive : false;
	if (bSonarLeanActive == bNewValue)
	{
		return;
	}

	bSonarLeanActive = bNewValue;
	BP_OnSonarLeanChanged(bSonarLeanActive);
}

void ASubPlayerController::ServerRouteHelmThrust_Implementation(float Value)
{
	UE_LOG(
		LogSubController,
		Verbose,
		TEXT("[%s] ServerRouteHelmThrust | Value=%.3f | Mode=%d"),
		*GetName(),
		Value,
		static_cast<int32>(CurrentControlMode)
	);

	if (CurrentControlMode != ECrewControlMode::HelmDriving)
	{
		UE_LOG(LogSubController, Verbose, TEXT("[%s] HelmThrust ignored (Mode=%d)"), *GetName(), static_cast<int32>(CurrentControlMode));
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetHelmThrottleCommand(Value);
			UE_LOG(LogSubController, Verbose, TEXT("[%s] HelmThrust applied via Systems to %s"), *GetName(), *GetNameSafe(Submarine));
		}
		else if (Submarine->SubMovement)
		{
			Submarine->SubMovement->SetThrustInput(Value);
			UE_LOG(LogSubController, Verbose, TEXT("[%s] HelmThrust applied to %s"), *GetName(), *GetNameSafe(Submarine));
		}
	}
	else
	{
		UE_LOG(LogSubController, Warning, TEXT("[%s] HelmThrust failed: no submarine resolved."), *GetName());
	}
}

void ASubPlayerController::ServerRouteHelmThrottleRamp_Implementation(float Intent)
{
	UE_LOG(
		LogSubController,
		Verbose,
		TEXT("[%s] ServerRouteHelmThrottleRamp | Intent=%.3f | Mode=%d"),
		*GetName(),
		Intent,
		static_cast<int32>(CurrentControlMode)
	);

	if (CurrentControlMode != ECrewControlMode::HelmDriving)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetThrottleRampIntent(Intent);
		}
	}
	else
	{
		UE_LOG(LogSubController, Warning, TEXT("[%s] HelmThrottleRamp failed: no submarine resolved."), *GetName());
	}
}

void ASubPlayerController::ServerRouteHelmSteer_Implementation(float Value)
{
	UE_LOG(
		LogSubController,
		Verbose,
		TEXT("[%s] ServerRouteHelmSteer | Value=%.3f | Mode=%d"),
		*GetName(),
		Value,
		static_cast<int32>(CurrentControlMode)
	);

	if (CurrentControlMode != ECrewControlMode::HelmDriving)
	{
		UE_LOG(LogSubController, Verbose, TEXT("[%s] HelmSteer ignored (Mode=%d)"), *GetName(), static_cast<int32>(CurrentControlMode));
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetHelmYawCommand(Value);
			UE_LOG(LogSubController, Verbose, TEXT("[%s] HelmSteer applied via Systems to %s"), *GetName(), *GetNameSafe(Submarine));
		}
		else if (Submarine->SubMovement)
		{
			Submarine->SubMovement->SetRudderInput(Value);
			UE_LOG(LogSubController, Verbose, TEXT("[%s] HelmSteer applied to %s"), *GetName(), *GetNameSafe(Submarine));
		}
	}
	else
	{
		UE_LOG(LogSubController, Warning, TEXT("[%s] HelmSteer failed: no submarine resolved."), *GetName());
	}
}

void ASubPlayerController::ServerRouteHelmRudderRamp_Implementation(float Intent)
{
	UE_LOG(
		LogSubController,
		Verbose,
		TEXT("[%s] ServerRouteHelmRudderRamp | Intent=%.3f | Mode=%d"),
		*GetName(),
		Intent,
		static_cast<int32>(CurrentControlMode)
	);

	if (CurrentControlMode != ECrewControlMode::HelmDriving)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetRudderRampIntent(Intent);
		}
	}
	else
	{
		UE_LOG(LogSubController, Warning, TEXT("[%s] HelmRudderRamp failed: no submarine resolved."), *GetName());
	}
}

void ASubPlayerController::ServerRouteHelmDivePlaneRamp_Implementation(float Intent)
{
	UE_LOG(
		LogSubController,
		Verbose,
		TEXT("[%s] ServerRouteHelmDivePlaneRamp | Intent=%.3f | Mode=%d"),
		*GetName(),
		Intent,
		static_cast<int32>(CurrentControlMode)
	);

	if (CurrentControlMode != ECrewControlMode::HelmDriving)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetDivePlaneRampIntent(Intent);
		}
	}
	else
	{
		UE_LOG(LogSubController, Warning, TEXT("[%s] HelmDivePlaneRamp failed: no submarine resolved."), *GetName());
	}
}

void ASubPlayerController::ServerRouteHelmDive_Implementation(float Value)
{
	UE_LOG(
		LogSubController,
		Verbose,
		TEXT("[%s] ServerRouteHelmDive | Value=%.3f | Mode=%d"),
		*GetName(),
		Value,
		static_cast<int32>(CurrentControlMode)
	);

	if (CurrentControlMode != ECrewControlMode::HelmDriving)
	{
		UE_LOG(LogSubController, Verbose, TEXT("[%s] HelmDive ignored (Mode=%d)"), *GetName(), static_cast<int32>(CurrentControlMode));
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetHelmTrimCommand(Value);
			UE_LOG(LogSubController, Verbose, TEXT("[%s] HelmDive applied via Systems to %s"), *GetName(), *GetNameSafe(Submarine));
		}
		else if (Submarine->SubMovement)
		{
			Submarine->SubMovement->SetDivePlaneInput(Value);
			UE_LOG(LogSubController, Verbose, TEXT("[%s] HelmDive applied to %s"), *GetName(), *GetNameSafe(Submarine));
		}
	}
	else
	{
		UE_LOG(LogSubController, Warning, TEXT("[%s] HelmDive failed: no submarine resolved."), *GetName());
	}
}

void ASubPlayerController::ServerRouteRudderHoldEnabled_Implementation(bool bEnabled)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetRudderHoldEnabled(bEnabled);
		}
	}
}

void ASubPlayerController::ServerRoutePlaneHoldEnabled_Implementation(bool bEnabled)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetPlaneHoldEnabled(bEnabled);
		}
	}
}

void ASubPlayerController::ServerRouteStabilizationMaster_Implementation(bool bEnabled)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetStabilizationMasterEnabled(bEnabled);
		}
	}
}

void ASubPlayerController::ServerRouteAutoSpeedEnabled_Implementation(bool bEnabled)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetAutoSpeedEnabled(bEnabled);
		}
	}
}

void ASubPlayerController::ServerRouteAutoDepthEnabled_Implementation(bool bEnabled)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetAutoDepthEnabled(bEnabled);
		}
	}
}

void ASubPlayerController::ServerRouteAutoPitchEnabled_Implementation(bool bEnabled)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetAutoPitchEnabled(bEnabled);
		}
	}
}

void ASubPlayerController::ServerRouteTargetSpeedCmS_Implementation(float SpeedCmS)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetTargetSpeedCmS(SpeedCmS);
		}
	}
}

void ASubPlayerController::ServerRouteTargetDepthMeters_Implementation(float DepthMeters)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetTargetDepthMeters(DepthMeters);
		}
	}
}

void ASubPlayerController::ServerRouteTargetPitchDeg_Implementation(float PitchDeg)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetTargetPitchDeg(PitchDeg);
		}
	}
}

void ASubPlayerController::ServerRouteBallastGlobal_Implementation(float Target)
{
	UE_LOG(
		LogSubController,
		Verbose,
		TEXT("[%s] ServerRouteBallastGlobal | Target=%.3f | Mode=%d | StationType=%d"),
		*GetName(),
		Target,
		static_cast<int32>(CurrentControlMode),
		static_cast<int32>(CurrentStationType)
	);

	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		UE_LOG(LogSubController, Verbose, TEXT("[%s] BallastGlobal ignored in OnFoot mode."), *GetName());
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetGlobalBallastTarget(Target);
			UE_LOG(LogSubController, Verbose, TEXT("[%s] BallastGlobal applied via Systems to %s"), *GetName(), *GetNameSafe(Submarine));
		}
		else if (Submarine->SubMovement)
		{
			Submarine->SubMovement->GlobalTargetFill = FMath::Clamp(Target, 0.f, 1.f);
			Submarine->SubMovement->ResyncAllBallasts();
			UE_LOG(LogSubController, Verbose, TEXT("[%s] BallastGlobal applied to %s"), *GetName(), *GetNameSafe(Submarine));
		}
	}
	else
	{
		UE_LOG(LogSubController, Warning, TEXT("[%s] BallastGlobal failed: no submarine resolved."), *GetName());
	}
}

void ASubPlayerController::ServerRouteBallastByIndex_Implementation(int32 Index, float Target)
{
	UE_LOG(
		LogSubController,
		Verbose,
		TEXT("[%s] ServerRouteBallastByIndex | Index=%d Target=%.3f | Mode=%d | StationType=%d"),
		*GetName(),
		Index,
		Target,
		static_cast<int32>(CurrentControlMode),
		static_cast<int32>(CurrentStationType)
	);

	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		UE_LOG(LogSubController, Verbose, TEXT("[%s] BallastByIndex ignored in OnFoot mode."), *GetName());
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetBallastTargetByIndex(Index, FMath::Clamp(Target, 0.f, 1.f));
			UE_LOG(LogSubController, Verbose, TEXT("[%s] BallastByIndex applied via Systems to %s"), *GetName(), *GetNameSafe(Submarine));
		}
		else if (Submarine->SubMovement)
		{
			Submarine->SubMovement->SetBallastTarget(Index, FMath::Clamp(Target, 0.f, 1.f));
			UE_LOG(LogSubController, Verbose, TEXT("[%s] BallastByIndex applied to %s"), *GetName(), *GetNameSafe(Submarine));
		}
	}
	else
	{
		UE_LOG(LogSubController, Warning, TEXT("[%s] BallastByIndex failed: no submarine resolved."), *GetName());
	}
}

void ASubPlayerController::ServerRouteBallastActive_Implementation(bool bActive)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetBallastsActive(bActive);
		}
	}
}

void ASubPlayerController::ServerRoutePumpActive_Implementation(bool bActive)
{
	UE_LOG(
		LogSubController,
		Verbose,
		TEXT("[%s] ServerRoutePumpActive | Active=%d | Mode=%d | StationType=%d"),
		*GetName(),
		bActive ? 1 : 0,
		static_cast<int32>(CurrentControlMode),
		static_cast<int32>(CurrentStationType)
	);

	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetPumpActive(bActive);
		}
	}
}

void ASubPlayerController::ServerRoutePumpPower_Implementation(float Value)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetPumpPower01(Value);
		}
	}
}

void ASubPlayerController::ServerRouteEngineBoost_Implementation(float Value)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetEngineBoost(Value);
		}
	}
}

void ASubPlayerController::ServerRouteTurretAim_Implementation(FRotator Aim)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetTurretAim(Aim);
		}
	}
}

void ASubPlayerController::ServerRouteTurretFire_Implementation(bool bHeld)
{
	if (CurrentControlMode == ECrewControlMode::OnFoot)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Systems)
		{
			Submarine->Systems->SetTurretFireHeld(bHeld);
		}
	}
}

void ASubPlayerController::ServerRouteDoorToggle_Implementation(FName DoorId, bool bClosed)
{
	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (ASubDoorActor* DoorActor = Submarine->FindAttachedDoorById(DoorId))
		{
			DoorActor->SetDoorClosed(bClosed);
			return;
		}

		if (Submarine->Compartments)
		{
			Submarine->Compartments->SetDoorClosed(DoorId, bClosed);
		}
	}
}

void ASubPlayerController::ServerRouteSonarPing_Implementation()
{
	if (CurrentControlMode != ECrewControlMode::HelmDriving)
	{
		UE_LOG(LogSubController, Verbose, TEXT("[%s] SonarPing ignored (Mode=%d)"), *GetName(), static_cast<int32>(CurrentControlMode));
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Sonar)
		{
			const bool bAccepted = Submarine->Sonar->TryFirePing();
			UE_LOG(LogSubController, Verbose, TEXT("[%s] SonarPing routed | Accepted=%d | Sub=%s"), *GetName(), bAccepted ? 1 : 0, *GetNameSafe(Submarine));
		}
	}
}

void ASubPlayerController::ServerSetSonarPingHeld_Implementation(bool bHeld)
{
	if (CurrentControlMode != ECrewControlMode::HelmDriving && bHeld)
	{
		UE_LOG(LogSubController, Verbose, TEXT("[%s] SonarPingHeld ignored (Mode=%d)"), *GetName(), static_cast<int32>(CurrentControlMode));
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->Sonar)
		{
			if (bHeld)
			{
				Submarine->Sonar->StartContinuousPing();
			}
			else
			{
				Submarine->Sonar->StopContinuousPing();
			}
			UE_LOG(LogSubController, Verbose, TEXT("[%s] SonarPingHeld routed | Held=%d | Sub=%s"), *GetName(), bHeld ? 1 : 0, *GetNameSafe(Submarine));
		}
	}
}

void ASubPlayerController::TriggerSonarPing()
{
	ServerRouteSonarPing();
}

void ASubPlayerController::SetSonarPingHeld(bool bHeld)
{
	ServerSetSonarPingHeld(bHeld);
}

void ASubPlayerController::ServerSetSonarMode_Implementation(ESonarMode NewMode)
{
	if (CurrentControlMode != ECrewControlMode::HelmDriving)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->SonarSystem)
		{
			Submarine->SonarSystem->SetSonarMode(NewMode);
		}
	}
}

void ASubPlayerController::ServerSetSonarFocusBearing_Implementation(float NewBearingDeg)
{
	if (CurrentControlMode != ECrewControlMode::HelmDriving)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->SonarSystem)
		{
			Submarine->SonarSystem->SetFocusBearing(NewBearingDeg);
		}
	}
}

void ASubPlayerController::ServerSetSonarRangePreset_Implementation(int32 NewPresetIndex)
{
	if (CurrentControlMode != ECrewControlMode::HelmDriving)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->SonarSystem)
		{
			Submarine->SonarSystem->SetRangePresetIndex(NewPresetIndex);
		}
	}
}

void ASubPlayerController::ServerMarkSonarPriorityTrack_Implementation(int32 TrackId, bool bPriority)
{
	if (CurrentControlMode != ECrewControlMode::HelmDriving)
	{
		return;
	}

	if (ASubmarineBase* Submarine = ResolveCurrentSubmarine())
	{
		if (Submarine->SonarSystem)
		{
			Submarine->SonarSystem->MarkPriorityTrack(TrackId, bPriority);
		}
	}
}

void ASubPlayerController::SetSonarMode(ESonarMode NewMode)
{
	ServerSetSonarMode(NewMode);
}

void ASubPlayerController::SetSonarFocusBearing(float NewBearingDeg)
{
	ServerSetSonarFocusBearing(NewBearingDeg);
}

void ASubPlayerController::SetSonarRangePreset(int32 NewPresetIndex)
{
	ServerSetSonarRangePreset(NewPresetIndex);
}

void ASubPlayerController::MarkSonarPriorityTrack(int32 TrackId, bool bPriority)
{
	ServerMarkSonarPriorityTrack(TrackId, bPriority);
}

ASubmarineBase* ASubPlayerController::GetResolvedCurrentSubmarine() const
{
	return ResolveCurrentSubmarine();
}

ASubmarineBase* ASubPlayerController::ResolveCurrentSubmarine() const
{
	if (CurrentStation && CurrentStation->Implements<USubStationInterface>())
	{
		if (ASubmarineBase* StationSub = ISubStationInterface::Execute_GetOwningSubmarine(CurrentStation))
		{
			UE_LOG(LogSubController, Verbose, TEXT("[%s] ResolveCurrentSubmarine from Station=%s"), *GetName(), *GetNameSafe(StationSub));
			return StationSub;
		}
	}

	if (const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetPawn()))
	{
		UE_LOG(LogSubController, Verbose, TEXT("[%s] ResolveCurrentSubmarine from Crew=%s"), *GetName(), *GetNameSafe(Crew->CurrentSubmarine));
		return Crew->CurrentSubmarine;
	}

	UE_LOG(LogSubController, Warning, TEXT("[%s] ResolveCurrentSubmarine failed (station and crew are null)."), *GetName());
	return nullptr;
}

ASubmarineBase* ASubPlayerController::ResolveSubmarineForDevCheat() const
{
	if (ASubmarineBase* Sub = ResolveCurrentSubmarine())
	{
		return Sub;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ASubmarineBase> It(World); It; ++It)
	{
		ASubmarineBase* Sub = *It;
		if (Sub && Sub->SubFlood)
		{
			UE_LOG(LogSubController, Log,
				TEXT("[%s] ResolveSubmarineForDevCheat fallback: found %s in world."),
				*GetName(), *Sub->GetName());
			return Sub;
		}
	}

	return nullptr;
}

// ── Dev cheats ────────────────────────────────────────────────────────────────

void ASubPlayerController::DevCheat_CreateBreach(FName CompartmentId, float RateLps)
{
	// Route to server: SubFlood is authoritative. In Play-as-Client or dedicated server
	// setups the client-side CreateBreach would no-op silently.
	if (!HasAuthority())
	{
		Server_DevCheat_CreateBreach(CompartmentId, RateLps);
		UE_LOG(LogSubController, Log,
			TEXT("[DevCheat_CreateBreach] Routed to server for '%s' at %.1f L/s"),
			*CompartmentId.ToString(), RateLps);
		return;
	}

	ASubmarineBase* Sub = ResolveCurrentSubmarine();
	if (!Sub || !Sub->SubFlood)
	{
		UE_LOG(LogSubController, Warning, TEXT("[DevCheat_CreateBreach] No submarine or SubFlood resolved."));
		return;
	}

	Sub->SubFlood->CreateBreach(CompartmentId, RateLps);
	UE_LOG(LogSubController, Log,
		TEXT("[DevCheat_CreateBreach] %s: created breach on '%s' at %.1f L/s"),
		*Sub->GetName(), *CompartmentId.ToString(), RateLps);
}

void ASubPlayerController::Server_DevCheat_CreateBreach_Implementation(FName CompartmentId, float RateLps)
{
	ASubmarineBase* Sub = ResolveSubmarineForDevCheat();
	if (!Sub || !Sub->SubFlood)
	{
		UE_LOG(LogSubController, Warning, TEXT("[Server_DevCheat_CreateBreach] No submarine or SubFlood resolved (no possessed crew, no station, and no submarine in world)."));
		return;
	}

	Sub->SubFlood->CreateBreach(CompartmentId, RateLps);
	UE_LOG(LogSubController, Log,
		TEXT("[Server_DevCheat_CreateBreach] %s: created breach on '%s' at %.1f L/s"),
		*Sub->GetName(), *CompartmentId.ToString(), RateLps);
}

void ASubPlayerController::DevCheat_CreateBreachAtCrew(float RateLps)
{
	const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetPawn());
	if (!Crew)
	{
		UE_LOG(LogSubController, Warning,
			TEXT("[DevCheat_CreateBreachAtCrew] No SubCrew possessed — cannot resolve compartment / position."));
		return;
	}

	const FName CompId = Crew->CurrentCompartmentId;
	if (CompId.IsNone())
	{
		UE_LOG(LogSubController, Warning,
			TEXT("[DevCheat_CreateBreachAtCrew] Crew is outside any compartment (CurrentCompartmentId=None) — no-op."));
		return;
	}

	ASubmarineBase* Sub = ResolveCurrentSubmarine();
	if (!Sub)
	{
		UE_LOG(LogSubController, Warning, TEXT("[DevCheat_CreateBreachAtCrew] No submarine resolved."));
		return;
	}
	const USceneComponent* SubRoot = Sub->GetRootComponent();
	if (!SubRoot)
	{
		return;
	}

	const FVector CrewWorld = Crew->GetActorLocation();
	const FVector LocalCenter = SubRoot->GetComponentTransform().InverseTransformPosition(CrewWorld);

	if (!HasAuthority())
	{
		Server_DevCheat_CreateBreachAtCrew(CompId, LocalCenter, RateLps);
		UE_LOG(LogSubController, Log,
			TEXT("[DevCheat_CreateBreachAtCrew] Routed to server | Comp=%s | Local=%s | %.1f L/s"),
			*CompId.ToString(), *LocalCenter.ToString(), RateLps);
		return;
	}

	if (Sub->SubFlood)
	{
		Sub->SubFlood->CreateBreach(CompId, RateLps, LocalCenter);
		UE_LOG(LogSubController, Log,
			TEXT("[DevCheat_CreateBreachAtCrew] %s: breach @crew | Comp=%s | Local=%s | %.1f L/s"),
			*Sub->GetName(), *CompId.ToString(), *LocalCenter.ToString(), RateLps);
	}
}

void ASubPlayerController::Server_DevCheat_CreateBreachAtCrew_Implementation(FName CompartmentId, FVector LocalCenter, float RateLps)
{
	ASubmarineBase* Sub = ResolveSubmarineForDevCheat();
	if (!Sub || !Sub->SubFlood)
	{
		UE_LOG(LogSubController, Warning, TEXT("[Server_DevCheat_CreateBreachAtCrew] No submarine or SubFlood resolved."));
		return;
	}

	Sub->SubFlood->CreateBreach(CompartmentId, RateLps, LocalCenter);
	UE_LOG(LogSubController, Log,
		TEXT("[Server_DevCheat_CreateBreachAtCrew] %s: breach @crew | Comp=%s | Local=%s | %.1f L/s"),
		*Sub->GetName(), *CompartmentId.ToString(), *LocalCenter.ToString(), RateLps);
}

void ASubPlayerController::DevCheat_SetDoorClosed(FName ConnectionId, bool bClosed)
{
	ASubmarineBase* Sub = ResolveCurrentSubmarine();
	if (!Sub || !Sub->SubFlood)
	{
		UE_LOG(LogSubController, Warning, TEXT("[DevCheat_SetDoorClosed] No submarine or SubFlood resolved."));
		return;
	}

	Sub->SubFlood->SetDoorState(ConnectionId, bClosed);
	UE_LOG(LogSubController, Log,
		TEXT("[DevCheat_SetDoorClosed] %s: set door '%s' closed=%d"),
		*Sub->GetName(), *ConnectionId.ToString(), bClosed ? 1 : 0);
}

void ASubPlayerController::DevCheat_SetFloodLevel(FName CompartmentId, float Level01)
{
	ASubmarineBase* Sub = ResolveCurrentSubmarine();
	if (!Sub || !Sub->SubFlood)
	{
		UE_LOG(LogSubController, Warning, TEXT("[DevCheat_SetFloodLevel] No submarine or SubFlood resolved."));
		return;
	}

	Sub->SubFlood->SetCompartmentFloodDirect(CompartmentId, Level01);
	UE_LOG(LogSubController, Log,
		TEXT("[DevCheat_SetFloodLevel] %s: set '%s' to %.2f"),
		*Sub->GetName(), *CompartmentId.ToString(), Level01);
}

void ASubPlayerController::DevCheat_RepairAllBreaches()
{
	ASubmarineBase* Sub = ResolveCurrentSubmarine();
	if (!Sub || !Sub->SubFlood || !Sub->GeneratedDefinition)
	{
		UE_LOG(LogSubController, Warning, TEXT("[DevCheat_RepairAllBreaches] No submarine / SubFlood / Definition resolved."));
		return;
	}

	int32 Count = 0;
	for (const FGeneratedCompartmentDef& Comp : Sub->GeneratedDefinition->Compartments)
	{
		Sub->SubFlood->RemoveBreach(Comp.CompartmentId);
		++Count;
	}

	// Also clear the hull side if available so the breach bridge does not
	// re-open the flood inflow on the next tick.
	if (Sub->SubHull)
	{
		Sub->SubHull->ClearAllBreaches();
	}

	UE_LOG(LogSubController, Log,
		TEXT("[DevCheat_RepairAllBreaches] %s: cleared breaches across %d compartments"),
		*Sub->GetName(), Count);
}

void ASubPlayerController::DevCheat_TeleportToCompartment(FName CompartmentId)
{
	ASubmarineBase* Sub = ResolveCurrentSubmarine();
	if (!Sub || !Sub->GeneratedDefinition)
	{
		UE_LOG(LogSubController, Warning, TEXT("[DevCheat_TeleportToCompartment] No submarine or Definition resolved."));
		return;
	}

	const FGeneratedCompartmentDef* Comp = Sub->GeneratedDefinition->FindCompartment(CompartmentId);
	if (!Comp)
	{
		UE_LOG(LogSubController, Warning,
			TEXT("[DevCheat_TeleportToCompartment] Compartment '%s' not found."),
			*CompartmentId.ToString());
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		UE_LOG(LogSubController, Warning, TEXT("[DevCheat_TeleportToCompartment] No pawn possessed."));
		return;
	}

	// Compartment center in sub local space, lifted to floor level + 100cm crew clearance.
	// CV-derived (HydroBounds was deprecated 2026-05-10).
	FBox CvBounds(ForceInit);
	if (!Sub->GetCompartmentLocalBounds(CompartmentId, CvBounds))
	{
		UE_LOG(LogSubController, Warning,
			TEXT("[DevCheat_TeleportToCompartment] No CV bounds for '%s' — place a UCompartmentVolumeComponent in the BP."),
			*CompartmentId.ToString());
		return;
	}
	const FVector CvCenter = CvBounds.GetCenter();
	const FVector LocalCenter(
		CvCenter.X,
		CvCenter.Y,
		Comp->WalkableFloorZCm + 100.f);
	const FVector WorldCenter = Sub->GetActorTransform().TransformPosition(LocalCenter);

	ControlledPawn->SetActorLocation(WorldCenter, false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogSubController, Log,
		TEXT("[DevCheat_TeleportToCompartment] %s -> '%s' at world %s"),
		*ControlledPawn->GetName(), *CompartmentId.ToString(), *WorldCenter.ToString());
}

void ASubPlayerController::DevCheat_ListCompartments()
{
	ASubmarineBase* Sub = ResolveCurrentSubmarine();
	if (!Sub || !Sub->GeneratedDefinition)
	{
		UE_LOG(LogSubController, Warning, TEXT("[DevCheat_ListCompartments] No submarine or Definition resolved."));
		return;
	}

	UE_LOG(LogSubController, Log,
		TEXT("[DevCheat_ListCompartments] %s: %d compartments"),
		*Sub->GetName(), Sub->GeneratedDefinition->Compartments.Num());

	for (const FGeneratedCompartmentDef& Comp : Sub->GeneratedDefinition->Compartments)
	{
		const float Level = Sub->SubFlood ? Sub->SubFlood->GetCompartmentFloodLevel01(Comp.CompartmentId) : 0.f;
		UE_LOG(LogSubController, Log,
			TEXT("  %s (type=%d) capacity=%.0fL flood=%.2f"),
			*Comp.CompartmentId.ToString(),
			static_cast<int32>(Comp.SemanticType),
			Comp.CapacityLiters,
			Level);
	}
}

static USubCrewAnimInstance* GetLocalAnimInstance(ASubPlayerController* PC)
{
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn) return nullptr;
	USkeletalMeshComponent* Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>();
	return Mesh ? Cast<USubCrewAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
}

void ASubPlayerController::Anim(const FString& ParamName, float Value)
{
	USubCrewAnimInstance* AI = GetLocalAnimInstance(this);
	if (!AI)
	{
		UE_LOG(LogSubController, Warning, TEXT("Anim: No SubCrewAnimInstance found"));
		return;
	}

	const FString P = ParamName.ToLower();

	// Axes
	if      (P == "legaxis")        AI->LegSwingAxis = FMath::RoundToInt(Value);
	else if (P == "armaxis")        AI->ArmSwingAxis = FMath::RoundToInt(Value);
	else if (P == "elbowaxis")      AI->ElbowBendAxis = FMath::RoundToInt(Value);
	else if (P == "spinebendaxis")  AI->SpineBendAxis = FMath::RoundToInt(Value);
	else if (P == "spinetwistaxis") AI->SpineTwistAxis = FMath::RoundToInt(Value);
	else if (P == "negleg")         AI->bNegateLegSwing = Value > 0.5f;
	else if (P == "negarm")         AI->bNegateArmSwing = Value > 0.5f;
	else if (P == "negspine")       AI->bNegateSpineBend = Value > 0.5f;
	// Arm rest FRotator per arm (P/Y/R)
	else if (P == "armrestr_p")     AI->ArmRestR.Pitch = Value;
	else if (P == "armrestr_y")     AI->ArmRestR.Yaw = Value;
	else if (P == "armrestr_r")     AI->ArmRestR.Roll = Value;
	else if (P == "armrestl_p")     AI->ArmRestL.Pitch = Value;
	else if (P == "armrestl_y")     AI->ArmRestL.Yaw = Value;
	else if (P == "armrestl_r")     AI->ArmRestL.Roll = Value;
	else if (P == "forearmr_p")     AI->ForearmRestR.Pitch = Value;
	else if (P == "forearmr_y")     AI->ForearmRestR.Yaw = Value;
	else if (P == "forearmr_r")     AI->ForearmRestR.Roll = Value;
	else if (P == "forearml_p")     AI->ForearmRestL.Pitch = Value;
	else if (P == "forearml_y")     AI->ForearmRestL.Yaw = Value;
	else if (P == "forearml_r")     AI->ForearmRestL.Roll = Value;
	// Walk
	else if (P == "legswing")       AI->WalkLegSwingDeg = Value;
	else if (P == "armswing")       AI->WalkArmSwingDeg = Value;
	else if (P == "bob")            AI->WalkPelvisBobCm = Value;
	else if (P == "rate")           AI->WalkCycleRate = Value;
	else if (P == "calfbend")       AI->WalkCalfBendMultiplier = Value;
	// Body
	else if (P == "lowerbodyyaw")   AI->MaxLowerBodyYawDeg = Value;
	// Idle
	else if (P == "breathamp")      AI->BreathingAmplitudeDeg = Value;
	else if (P == "breathrate")     AI->BreathingRate = Value;
	else if (P == "posturebend")    AI->MaxPostureBendDeg = Value;
	// Sub motion
	else if (P == "sublean")        AI->SubLeanMultiplier = Value;
	else if (P == "substumble")     AI->SubStumbleMultiplier = Value;
	else
	{
		UE_LOG(LogSubController, Warning, TEXT("Anim: Unknown param '%s'. Use AnimList for list."), *ParamName);
		return;
	}

	UE_LOG(LogSubController, Log, TEXT("Anim: %s = %.2f"), *ParamName, Value);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
			FString::Printf(TEXT("Anim: %s = %.2f"), *ParamName, Value));
	}
}

void ASubPlayerController::AnimList()
{
	USubCrewAnimInstance* AI = GetLocalAnimInstance(this);
	if (!AI)
	{
		UE_LOG(LogSubController, Warning, TEXT("AnimList: No SubCrewAnimInstance found"));
		return;
	}

	UE_LOG(LogSubController, Log, TEXT("=== CREW ANIM PARAMS ==="));
	UE_LOG(LogSubController, Log, TEXT("-- Axes --"));
	UE_LOG(LogSubController, Log, TEXT("  legaxis        %d"), AI->LegSwingAxis);
	UE_LOG(LogSubController, Log, TEXT("  armaxis        %d"), AI->ArmSwingAxis);
	UE_LOG(LogSubController, Log, TEXT("  elbowaxis      %d"), AI->ElbowBendAxis);
	UE_LOG(LogSubController, Log, TEXT("  spinebendaxis  %d"), AI->SpineBendAxis);
	UE_LOG(LogSubController, Log, TEXT("  spinetwistaxis %d"), AI->SpineTwistAxis);
	UE_LOG(LogSubController, Log, TEXT("  negleg:%d negarm:%d negspine:%d"),
		AI->bNegateLegSwing ? 1 : 0, AI->bNegateArmSwing ? 1 : 0, AI->bNegateSpineBend ? 1 : 0);
	UE_LOG(LogSubController, Log, TEXT("-- Arm Rest (FRotator per arm) --"));
	UE_LOG(LogSubController, Log, TEXT("  armrestr       P%.1f Y%.1f R%.1f"), AI->ArmRestR.Pitch, AI->ArmRestR.Yaw, AI->ArmRestR.Roll);
	UE_LOG(LogSubController, Log, TEXT("  armrestl       P%.1f Y%.1f R%.1f"), AI->ArmRestL.Pitch, AI->ArmRestL.Yaw, AI->ArmRestL.Roll);
	UE_LOG(LogSubController, Log, TEXT("  forearmr       P%.1f Y%.1f R%.1f"), AI->ForearmRestR.Pitch, AI->ForearmRestR.Yaw, AI->ForearmRestR.Roll);
	UE_LOG(LogSubController, Log, TEXT("  forearml       P%.1f Y%.1f R%.1f"), AI->ForearmRestL.Pitch, AI->ForearmRestL.Yaw, AI->ForearmRestL.Roll);
	UE_LOG(LogSubController, Log, TEXT("-- Walk --"));
	UE_LOG(LogSubController, Log, TEXT("  legswing %.1f  armswing %.1f  bob %.1f  rate %.3f  calfbend %.1f"),
		AI->WalkLegSwingDeg, AI->WalkArmSwingDeg, AI->WalkPelvisBobCm, AI->WalkCycleRate, AI->WalkCalfBendMultiplier);
	UE_LOG(LogSubController, Log, TEXT("-- Body --"));
	UE_LOG(LogSubController, Log, TEXT("  lowerbodyyaw   %.1f"), AI->MaxLowerBodyYawDeg);
	UE_LOG(LogSubController, Log, TEXT("  posturebend    %.1f"), AI->MaxPostureBendDeg);
	UE_LOG(LogSubController, Log, TEXT("  breathamp %.1f  breathrate %.2f"), AI->BreathingAmplitudeDeg, AI->BreathingRate);
	UE_LOG(LogSubController, Log, TEXT("  sublean %.2f  substumble %.3f"), AI->SubLeanMultiplier, AI->SubStumbleMultiplier);
	UE_LOG(LogSubController, Log, TEXT("Usage: Anim <param> <value>  (e.g. Anim armrestr_r -85)"));
}

void ASubPlayerController::CrewAnimDump()
{
	ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetPawn());
	if (!Crew)
	{
		UE_LOG(LogSubController, Warning, TEXT("CrewAnimDump: possessed pawn is not ASubCrewCharacter"));
		return;
	}

	UCrewAnimDebugComponent* DebugComponent = Crew->FindComponentByClass<UCrewAnimDebugComponent>();
	if (!DebugComponent)
	{
		UE_LOG(LogSubController, Warning, TEXT("CrewAnimDump: no UCrewAnimDebugComponent on %s"), *GetNameSafe(Crew));
		return;
	}

	DebugComponent->DumpSnapshotToLog();
}
