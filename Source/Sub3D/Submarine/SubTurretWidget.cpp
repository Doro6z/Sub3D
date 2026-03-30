#include "SubTurretWidget.h"
#include "SubPlayerController.h"
#include "SubmarineBase.h"
#include "SubmarineSystemsComponent.h"
#include "SubCrewCharacter.h"
#include "TurretActor.h"

void USubTurretWidget::SetTurretAim(const FRotator& NewAim)
{
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteTurretAim(NewAim);
	}
}

void USubTurretWidget::SetTurretFireHeld(bool bHeld)
{
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteTurretFire(bHeld);
	}
}

FRotator USubTurretWidget::GetCurrentTurretAim() const
{
	const_cast<USubTurretWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->ExteriorTurret)
	{
		return OwnerCrew->CurrentSubmarine->ExteriorTurret->CurrentAim;
	}
	return FRotator::ZeroRotator;
}

bool USubTurretWidget::IsTurretOnline() const
{
	const_cast<USubTurretWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->ExteriorTurret)
	{
		return OwnerCrew->CurrentSubmarine->ExteriorTurret->bOnline;
	}
	return false;
}
