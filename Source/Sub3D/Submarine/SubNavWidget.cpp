#include "SubNavWidget.h"
#include "SubCrewCharacter.h"
#include "SubPlayerController.h"
#include "SubmarineBase.h"
#include "SubMovementComponent.h"

void USubNavStationWidget::InitForCrew(ASubCrewCharacter* Crew)
{
	OwnerCrew = Crew;
	OwnerController = Crew ? Cast<ASubPlayerController>(Crew->GetController()) : nullptr;
	if (OwnerController.IsValid())
	{
		if (ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine())
		{
			SubMovement = Submarine->SubMovement;
			return;
		}
	}

	if (Crew && Crew->CurrentSubmarine)
	{
		SubMovement = Crew->CurrentSubmarine->SubMovement;
	}
}

void USubNavStationWidget::ResolveRuntimeRefs()
{
	if (!OwnerController.IsValid())
	{
		OwnerController = Cast<ASubPlayerController>(GetOwningPlayer());
	}

	if (!OwnerCrew.IsValid() && OwnerController.IsValid())
	{
		OwnerCrew = Cast<ASubCrewCharacter>(OwnerController->GetPawn());
	}

	if (!SubMovement.IsValid() && OwnerController.IsValid())
	{
		if (ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine())
		{
			SubMovement = Submarine->SubMovement;
			return;
		}
	}

	if (!SubMovement.IsValid() && OwnerCrew.IsValid() && OwnerCrew->CurrentSubmarine)
	{
		SubMovement = OwnerCrew->CurrentSubmarine->SubMovement;
	}
}

// ── Ballast control ───────────────────────────────────────────────────────────

void USubNavStationWidget::SetGlobalBallast(float Target)
{
	ResolveRuntimeRefs();
	const float Clamped = FMath::Clamp(Target, 0.f, 1.f);

	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteBallastGlobal(Clamped);
		return;
	}

	if (OwnerCrew.IsValid())
	{
		// Legacy fallback
		OwnerCrew->Server_ResyncBallasts(Clamped);
	}
}

void USubNavStationWidget::SetBallastByIndex(int32 Index, float Target)
{
	ResolveRuntimeRefs();
	const float Clamped = FMath::Clamp(Target, 0.f, 1.f);

	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteBallastByIndex(Index, Clamped);
		return;
	}

	if (OwnerCrew.IsValid())
	{
		// Legacy fallback
		OwnerCrew->Server_SetBallastTarget(Index, Clamped);
	}
}

// ── Read state ────────────────────────────────────────────────────────────────

float USubNavStationWidget::GetBallastFillLevel(int32 Index) const
{
	const_cast<USubNavStationWidget*>(this)->ResolveRuntimeRefs();
	if (!SubMovement.IsValid() || !SubMovement->Ballasts.IsValidIndex(Index)) return -1.f;
	return SubMovement->Ballasts[Index].FillLevel;
}

float USubNavStationWidget::GetDepth() const
{
	const_cast<USubNavStationWidget*>(this)->ResolveRuntimeRefs();
	return SubMovement.IsValid() ? SubMovement->CurrentDepth : 0.f;
}

float USubNavStationWidget::GetSpeedKmh() const
{
	const_cast<USubNavStationWidget*>(this)->ResolveRuntimeRefs();
	if (!SubMovement.IsValid() || !SubMovement->GetOwner()) return 0.f;
	// Helm speed should reflect longitudinal speed, not total velocity magnitude.
	const FVector LocalVelocity = SubMovement->GetOwner()->GetActorTransform().InverseTransformVector(SubMovement->Velocity);
	return FMath::Abs(LocalVelocity.X) * 0.036f;
}

float USubNavStationWidget::GetPitch() const
{
	const_cast<USubNavStationWidget*>(this)->ResolveRuntimeRefs();
	if (!SubMovement.IsValid()) return 0.f;
	return SubMovement->GetOwner() ? SubMovement->GetOwner()->GetActorRotation().Pitch : 0.f;
}
