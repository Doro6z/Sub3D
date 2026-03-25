#include "SubCrewMovementComponent.h"

#include "SubCrewCharacter.h"
#include "SubmarineBase.h"
#include "SubInteriorFrameComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

USubCrewMovementComponent::USubCrewMovementComponent()
{
}

// ── Tick ──────────────────────────────────────────────────────────────────────

void USubCrewMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	const bool bEmbarked = IsEmbarked();

	// Apply submarine frame compensation BEFORE CMC runs its movement.
	// Only for the locally controlled character — simulated proxies use
	// stock replication + based-movement.
	if (bEmbarked && CharacterOwner && CharacterOwner->IsLocallyControlled())
	{
		ApplySubmarineFrameCompensation();
	}
	else if (!bEmbarked)
	{
		bHasLastCompensatedTransform = false;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update relative state AFTER CMC has moved the character (input, gravity, etc.).
	if (bEmbarked)
	{
		UpdateRelativeState();
	}
}

// ── CMC overrides ────────────────────────────────────────────────────────────

void USubCrewMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
	// When embarked and locally controlled, our explicit compensation in
	// TickComponent handles following the submarine. Skip CMC stock
	// based-movement to avoid double compensation.
	if (IsEmbarked() && CharacterOwner && CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	Super::UpdateBasedMovement(DeltaSeconds);
}

void USubCrewMovementComponent::UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation)
{
	// Yaw compensation is handled via controller rotation in our
	// frame compensation. Skip stock based rotation when embarked.
	if (IsEmbarked() && CharacterOwner && CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	Super::UpdateBasedRotation(FinalRotation, ReducedRotation);
}

// ── Interior frame query ─────────────────────────────────────────────────────

USubInteriorFrameComponent* USubCrewMovementComponent::GetInteriorFrame() const
{
	if (const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner))
	{
		if (ASubmarineBase* Sub = Crew->CurrentSubmarine)
		{
			return Sub->InteriorFrame;
		}
	}
	return nullptr;
}

bool USubCrewMovementComponent::IsEmbarked() const
{
	return GetInteriorFrame() != nullptr;
}

// ── Frame compensation ───────────────────────────────────────────────────────

void USubCrewMovementComponent::ApplySubmarineFrameCompensation()
{
	USubInteriorFrameComponent* Frame = GetInteriorFrame();
	if (!Frame || !Frame->IsFrameValid() || !CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	const FTransform CurrentSubTransform = Frame->GetSubTransform();

	if (!bHasLastCompensatedTransform)
	{
		// First frame after boarding — seed state, no compensation yet.
		LastCompensatedSubTransform = CurrentSubTransform;
		bHasLastCompensatedTransform = true;
		UpdateRelativeState();
		return;
	}

	// --- Positional compensation ---
	// Compute where the character should be to maintain its relative position.
	const FVector DesiredWorldPos = CurrentSubTransform.TransformPosition(RelativeLocation);
	const FVector CurrentWorldPos = UpdatedComponent->GetComponentLocation();
	const FVector Compensation = DesiredWorldPos - CurrentWorldPos;

	const float CompensationSize = Compensation.Size();

	if (CompensationSize > SnapThresholdCm)
	{
		// Submarine teleported (network snap) — teleport character with it.
		UpdatedComponent->SetWorldLocation(DesiredWorldPos, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else if (CompensationSize > KINDA_SMALL_NUMBER)
	{
		// Normal compensation: move character with the submarine (no sweep).
		MoveUpdatedComponent(Compensation, UpdatedComponent->GetComponentRotation(), false);
	}

	// --- Yaw compensation ---
	// When the submarine rotates, rotate the controller so the player's
	// view stays consistent relative to the interior.
	const FQuat PrevQuat = LastCompensatedSubTransform.GetRotation();
	const FQuat CurrQuat = CurrentSubTransform.GetRotation();
	const FQuat DeltaQuat = CurrQuat * PrevQuat.Inverse();
	const float YawDelta = DeltaQuat.Rotator().Yaw;

	if (FMath::Abs(YawDelta) > KINDA_SMALL_NUMBER)
	{
		if (AController* PC = CharacterOwner->GetController())
		{
			FRotator ControlRot = PC->GetControlRotation();
			ControlRot.Yaw += YawDelta;
			PC->SetControlRotation(ControlRot);
		}
	}

	LastCompensatedSubTransform = CurrentSubTransform;
}

// ── Relative state ───────────────────────────────────────────────────────────

void USubCrewMovementComponent::UpdateRelativeState()
{
	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	if (!Frame || !CharacterOwner)
	{
		return;
	}

	RelativeLocation = Frame->WorldToLocal(CharacterOwner->GetActorLocation());
	RelativeRotation = Frame->WorldToLocalRotation(CharacterOwner->GetActorRotation());
}

void USubCrewMovementComponent::InitializeForSubmarine()
{
	USubInteriorFrameComponent* Frame = GetInteriorFrame();
	if (Frame && Frame->IsFrameValid() && CharacterOwner)
	{
		LastCompensatedSubTransform = Frame->GetSubTransform();
		bHasLastCompensatedTransform = true;
		UpdateRelativeState();
	}
}
