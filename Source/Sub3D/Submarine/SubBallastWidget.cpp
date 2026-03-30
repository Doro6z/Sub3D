#include "SubBallastWidget.h"
#include "SubmarineBase.h"
#include "SubmarineSystemsComponent.h"
#include "SubCrewCharacter.h"

float USubBallastWidget::GetCurrentDepthMeters() const
{
	const_cast<USubBallastWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine)
	{
		// Depth is Z * -0.01 (meters) assuming UE origin at surface
		return OwnerCrew->CurrentSubmarine->GetActorLocation().Z * -0.01f;
	}
	return 0.f;
}

float USubBallastWidget::GetTargetDepthMeters() const
{
	const_cast<USubBallastWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->Systems)
	{
		return OwnerCrew->CurrentSubmarine->Systems->GetCommandState().TargetDepthMeters;
	}
	return 0.f;
}

bool USubBallastWidget::IsAutoDepthActive() const
{
	const_cast<USubBallastWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->Systems)
	{
		return OwnerCrew->CurrentSubmarine->Systems->GetCommandState().bAutoDepthEnabled;
	}
	return false;
}

float USubBallastWidget::GetGlobalBallastLevel() const
{
	const_cast<USubBallastWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->Systems)
	{
		return OwnerCrew->CurrentSubmarine->Systems->GetCommandState().GlobalBallastTarget01;
	}
	return 0.f;
}

bool USubBallastWidget::IsBallastActive() const
{
	const_cast<USubBallastWidget*>(this)->ResolveRuntimeRefs();
	if (OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine && OwnerCrew->CurrentSubmarine->Systems)
	{
		return OwnerCrew->CurrentSubmarine->Systems->GetCommandState().bBallastsActive;
	}
	return false;
}
