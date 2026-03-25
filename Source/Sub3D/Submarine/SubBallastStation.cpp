#include "SubBallastStation.h"

#include "SubmarineBase.h"
#include "SubMovementComponent.h"
#include "SubmarineSystemsComponent.h"

ASubBallastStation::ASubBallastStation()
{
	StationType = ESubStationType::Ballast;
}

void ASubBallastStation::SetGlobalBallastTarget(float GlobalTarget)
{
	if (!HasAuthority() || !OwningSubmarine)
	{
		return;
	}

	if (OwningSubmarine->Systems)
	{
		OwningSubmarine->Systems->SetGlobalBallastTarget(GlobalTarget);
	}
	else if (OwningSubmarine->SubMovement)
	{
		OwningSubmarine->SubMovement->GlobalTargetFill = FMath::Clamp(GlobalTarget, 0.f, 1.f);
	}
}

void ASubBallastStation::SetBallastTargetByIndex(int32 Index, float Target)
{
	if (!HasAuthority() || !OwningSubmarine)
	{
		return;
	}

	if (OwningSubmarine->Systems)
	{
		OwningSubmarine->Systems->SetBallastTargetByIndex(Index, Target);
	}
	else if (OwningSubmarine->SubMovement)
	{
		OwningSubmarine->SubMovement->SetBallastTarget(Index, Target);
	}
}

void ASubBallastStation::ResyncBallasts()
{
	if (!HasAuthority() || !OwningSubmarine)
	{
		return;
	}

	if (OwningSubmarine->Systems)
	{
		OwningSubmarine->Systems->ResyncAllBallasts();
	}
	else if (OwningSubmarine->SubMovement)
	{
		OwningSubmarine->SubMovement->ResyncAllBallasts();
	}
}

int32 ASubBallastStation::GetBallastCount() const
{
	if (!OwningSubmarine || !OwningSubmarine->SubMovement)
	{
		return 0;
	}

	return OwningSubmarine->SubMovement->Ballasts.Num();
}

float ASubBallastStation::GetBallastFillLevel(int32 Index) const
{
	if (!OwningSubmarine || !OwningSubmarine->SubMovement || !OwningSubmarine->SubMovement->Ballasts.IsValidIndex(Index))
	{
		return 0.f;
	}

	return OwningSubmarine->SubMovement->Ballasts[Index].FillLevel;
}
