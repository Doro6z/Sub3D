#include "SubEngineWidget.h"
#include "SubPlayerController.h"
#include "SubmarineBase.h"
#include "SubmarineSystemsComponent.h"
#include "SubMovementComponent.h"
#include "SubCrewCharacter.h"

void USubEngineWidget::SetEngineBoost(float Value)
{
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteEngineBoost(Value);
	}
}

void USubEngineWidget::SetPumpActive(bool bActive)
{
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRoutePumpActive(bActive);
	}
}

void USubEngineWidget::SetPumpPower(float Value)
{
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRoutePumpPower(Value);
	}
}

float USubEngineWidget::GetEngineHealth() const
{
	const_cast<USubEngineWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->Systems)
	{
		return OwnerCrew->CurrentSubmarine->Systems->GetEngineHealth01();
	}
	return 1.0f;
}

float USubEngineWidget::GetElectricalHealth() const
{
	const_cast<USubEngineWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->Systems)
	{
		return OwnerCrew->CurrentSubmarine->Systems->GetElectricalHealth01();
	}
	return 1.0f;
}

bool USubEngineWidget::IsPumpActive() const
{
	const_cast<USubEngineWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->Systems)
	{
		return OwnerCrew->CurrentSubmarine->Systems->GetCommandState().bPumpActive;
	}
	return false;
}

float USubEngineWidget::GetPumpPower() const
{
	const_cast<USubEngineWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->Systems)
	{
		return OwnerCrew->CurrentSubmarine->Systems->GetCommandState().PumpPower01;
	}
	return 1.0f;
}
