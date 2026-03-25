#include "SubTurretStation.h"

#include "SubmarineBase.h"
#include "SubmarineSystemsComponent.h"
#include "TurretActor.h"

ASubTurretStation::ASubTurretStation()
{
	StationType = ESubStationType::Turret;
	bExclusiveOccupancy = true;
}

void ASubTurretStation::SetTurretAim(const FRotator& Aim)
{
	if (!HasTurretAuthority())
	{
		return;
	}

	if (OwningSubmarine->Systems)
	{
		OwningSubmarine->Systems->SetTurretAim(Aim);
	}
	else if (ATurretActor* Turret = ResolveTurret())
	{
		Turret->SetAimCommand(Aim);
	}
}

void ASubTurretStation::SetTurretFireHeld(bool bHeld)
{
	if (!HasTurretAuthority())
	{
		return;
	}

	if (OwningSubmarine->Systems)
	{
		OwningSubmarine->Systems->SetTurretFireHeld(bHeld);
	}
	else if (ATurretActor* Turret = ResolveTurret())
	{
		Turret->SetFireHeld(bHeld);
	}
}

bool ASubTurretStation::HasTurretAuthority() const
{
	return HasAuthority() && OwningSubmarine != nullptr;
}

ATurretActor* ASubTurretStation::ResolveTurret() const
{
	return OwningSubmarine ? OwningSubmarine->ExteriorTurret : nullptr;
}
