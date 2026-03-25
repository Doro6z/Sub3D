#include "SubEngineStation.h"

#include "SubmarineBase.h"
#include "SubMovementComponent.h"
#include "SubmarineSystemsComponent.h"

ASubEngineStation::ASubEngineStation()
{
	StationType = ESubStationType::Engine;
}

bool ASubEngineStation::HasEngineAuthority() const
{
	return HasAuthority() && OwningSubmarine && (OwningSubmarine->Systems || OwningSubmarine->SubMovement);
}

void ASubEngineStation::SetEngineInputs(float ThrustInput, float RudderInput, float DiveInput)
{
	if (!HasEngineAuthority())
	{
		return;
	}

	if (OwningSubmarine->Systems)
	{
		OwningSubmarine->Systems->SetHelmThrottleCommand(ThrustInput);
		OwningSubmarine->Systems->SetHelmYawCommand(RudderInput);
		OwningSubmarine->Systems->SetHelmTrimCommand(DiveInput);
	}
	else if (OwningSubmarine->SubMovement)
	{
		OwningSubmarine->SubMovement->SetThrustInput(ThrustInput);
		OwningSubmarine->SubMovement->SetRudderInput(RudderInput);
		OwningSubmarine->SubMovement->SetDivePlaneInput(DiveInput);
	}
}

void ASubEngineStation::SetPumpActive(bool bActive)
{
	if (!HasEngineAuthority())
	{
		return;
	}

	if (OwningSubmarine->Systems)
	{
		OwningSubmarine->Systems->SetPumpActive(bActive);
	}
}
