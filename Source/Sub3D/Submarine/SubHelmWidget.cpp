#include "SubHelmWidget.h"
#include "SubCrewCharacter.h"
#include "SubPlayerController.h"
#include "SubmarineBase.h"
#include "SubMovementComponent.h"

void USubHelmWidget::InitForCrew(ASubCrewCharacter* Crew)
{
	OwnerCrew = Crew;
	OwnerController = Crew ? Cast<ASubPlayerController>(Crew->GetController()) : nullptr;
	if (Crew && Crew->CurrentSubmarine)
		SubMovement = Crew->CurrentSubmarine->SubMovement;
}

void USubHelmWidget::ResolveRuntimeRefs()
{
	if (!OwnerController)
	{
		OwnerController = Cast<ASubPlayerController>(GetOwningPlayer());
	}

	if (!OwnerCrew && OwnerController)
	{
		OwnerCrew = Cast<ASubCrewCharacter>(OwnerController->GetPawn());
	}

	if (!SubMovement && OwnerCrew && OwnerCrew->CurrentSubmarine)
	{
		SubMovement = OwnerCrew->CurrentSubmarine->SubMovement;
	}
}

// ── Ballast control ───────────────────────────────────────────────────────────

void USubHelmWidget::SetGlobalBallast(float Target)
{
	ResolveRuntimeRefs();
	const float Clamped = FMath::Clamp(Target, 0.f, 1.f);

	if (OwnerController)
	{
		OwnerController->ServerRouteBallastGlobal(Clamped);
		return;
	}

	if (OwnerCrew)
	{
		// Legacy fallback
		OwnerCrew->Server_ResyncBallasts(Clamped);
	}
}

void USubHelmWidget::SetBallastByIndex(int32 Index, float Target)
{
	ResolveRuntimeRefs();
	const float Clamped = FMath::Clamp(Target, 0.f, 1.f);

	if (OwnerController)
	{
		OwnerController->ServerRouteBallastByIndex(Index, Clamped);
		return;
	}

	if (OwnerCrew)
	{
		// Legacy fallback
		OwnerCrew->Server_SetBallastTarget(Index, Clamped);
	}
}

// ── Read state ────────────────────────────────────────────────────────────────

float USubHelmWidget::GetBallastFillLevel(int32 Index) const
{
	const_cast<USubHelmWidget*>(this)->ResolveRuntimeRefs();
	if (!SubMovement || !SubMovement->Ballasts.IsValidIndex(Index)) return -1.f;
	return SubMovement->Ballasts[Index].FillLevel;
}

float USubHelmWidget::GetDepth() const
{
	const_cast<USubHelmWidget*>(this)->ResolveRuntimeRefs();
	return SubMovement ? SubMovement->CurrentDepth : 0.f;
}

float USubHelmWidget::GetSpeedKmh() const
{
	const_cast<USubHelmWidget*>(this)->ResolveRuntimeRefs();
	if (!SubMovement || !SubMovement->GetOwner()) return 0.f;
	// Helm speed should reflect longitudinal speed, not total velocity magnitude.
	const FVector LocalVelocity = SubMovement->GetOwner()->GetActorTransform().InverseTransformVector(SubMovement->Velocity);
	return FMath::Abs(LocalVelocity.X) * 0.036f;
}

float USubHelmWidget::GetPitch() const
{
	const_cast<USubHelmWidget*>(this)->ResolveRuntimeRefs();
	if (!SubMovement) return 0.f;
	return SubMovement->GetOwner() ? SubMovement->GetOwner()->GetActorRotation().Pitch : 0.f;
}
