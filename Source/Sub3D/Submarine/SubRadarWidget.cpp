#include "SubRadarWidget.h"
#include "SubmarineBase.h"
#include "SubmarineRadarComponent.h"
#include "SubCrewCharacter.h"

TArray<FRadarContact> USubRadarWidget::GetRadarContacts() const
{
	const_cast<USubRadarWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->Radar)
	{
		return OwnerCrew->CurrentSubmarine->Radar->GetContacts();
	}
	return TArray<FRadarContact>();
}

float USubRadarWidget::GetPingProgress() const
{
	const_cast<USubRadarWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->Radar)
	{
		return OwnerCrew->CurrentSubmarine->Radar->GetPingProgress01();
	}
	return 0.f;
}

float USubRadarWidget::GetRadarRange() const
{
	const_cast<USubRadarWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->Radar)
	{
		return OwnerCrew->CurrentSubmarine->Radar->PingRadiusCm;
	}
	return 10000.f;
}
