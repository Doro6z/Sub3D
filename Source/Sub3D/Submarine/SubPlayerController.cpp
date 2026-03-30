#include "SubPlayerController.h"
#include "SubStationInterface.h"
#include "SubmarineBase.h"
#include "SubSonarComponent.h"
#include "SubSonarSystemComponent.h"
#include "SubDoorActor.h"
#include "SubmarineSystemsComponent.h"
#include "SubmarineCompartmentComponent.h"
#include "SubMovementComponent.h"
#include "SubCrewCharacter.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubController, Log, All);

void ASubPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASubPlayerController, CurrentControlMode);
	DOREPLIFETIME(ASubPlayerController, CurrentStation);
	DOREPLIFETIME(ASubPlayerController, CurrentStationType);
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

	CurrentStation = Station;
	CurrentStationType = ISubStationInterface::Execute_GetStationType(Station);
	ASubmarineBase* StationSubmarine = ISubStationInterface::Execute_GetOwningSubmarine(Station);

	ISubStationInterface::Execute_RequestEnterStation(Station, this);

	if (ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetPawn()))
	{
		if (StationSubmarine && Crew->CurrentSubmarine != StationSubmarine)
		{
			Crew->SetCurrentSubmarine(StationSubmarine);
		}
	}

	if (CurrentStationType == ESubStationType::Helm)
	{
		CurrentControlMode = ECrewControlMode::HelmDriving;
		if (ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetPawn()))
		{
			Crew->ForceHelm();
		}
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
