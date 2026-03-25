#include "SubCrewMovementComponent.h"

#include "SubCrewCharacter.h"
#include "SubInteriorFrameComponent.h"
#include "SubmarineBase.h"
#include "SubMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubCrewMovement, Log, All);

USubCrewMovementComponent::USubCrewMovementComponent()
{
}

void USubCrewMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsEmbarked())
	{
		UpdateRelativeState();
		ApplyYawCompensation();
		CheckAndLogBaseChange();
		LogPeriodicState(DeltaTime);
		DebugDrawState();
	}
	else
	{
		if (LastKnownBase.IsValid())
		{
			const UPrimitiveComponent* PreviousBase = LastKnownBase.Get();
			UE_LOG(
				LogSubCrewMovement,
				Log,
				TEXT("Embark ended | clearing tracked base: %s on %s"),
				*GetNameSafe(PreviousBase),
				*GetNameSafe(PreviousBase ? PreviousBase->GetOwner() : nullptr));
		}

		LastKnownBase.Reset();
		DebugLogTimer = 0.f;
	}
}

void USubCrewMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
	// D4: Stock based-movement re-enabled as the stabilization experiment.
	Super::UpdateBasedMovement(DeltaSeconds);
}

void USubCrewMovementComponent::UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation)
{
	// D4: Stock based-rotation re-enabled.
	// Controller yaw follow is handled separately in ApplyYawCompensation().
	Super::UpdateBasedRotation(FinalRotation, ReducedRotation);
}

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

void USubCrewMovementComponent::UpdateRelativeState()
{
	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	if (!Frame || !CharacterOwner)
	{
		return;
	}

	RelativeLocation = Frame->WorldToLocal(CharacterOwner->GetActorLocation());
	RelativeRotation = Frame->WorldToLocalRotation(CharacterOwner->GetActorRotation());

	if (bDebugLogCrewMovement)
	{
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("RelativeState | WorldLoc=%s | RelLoc=%s | RelRot=%s"),
			*CharacterOwner->GetActorLocation().ToCompactString(),
			*RelativeLocation.ToCompactString(),
			*RelativeRotation.ToCompactString());
	}
}

void USubCrewMovementComponent::ApplyYawCompensation()
{
	if (!CharacterOwner || !CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	if (!Frame || !Frame->IsFrameValid())
	{
		return;
	}

	const float YawDelta = Frame->GetFrameRotationDelta().Yaw;
	if (FMath::Abs(YawDelta) <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (AController* Controller = CharacterOwner->GetController())
	{
		FRotator ControlRotation = Controller->GetControlRotation();
		ControlRotation.Yaw = FRotator::NormalizeAxis(ControlRotation.Yaw + YawDelta);
		Controller->SetControlRotation(ControlRotation);

		if (bDebugLogCrewMovement)
		{
			UE_LOG(
				LogSubCrewMovement,
				Log,
				TEXT("YawCompensation | DeltaYaw=%.3f | NewControlYaw=%.3f"),
				YawDelta,
				ControlRotation.Yaw);
		}
	}
}

void USubCrewMovementComponent::CheckAndLogBaseChange()
{
	if (!CharacterOwner)
	{
		return;
	}

	UPrimitiveComponent* CurrentBase = CharacterOwner->GetMovementBase();
	UPrimitiveComponent* PreviousBase = LastKnownBase.Get();

	if (CurrentBase == PreviousBase)
	{
		return;
	}

	if (!PreviousBase && CurrentBase)
	{
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("Base acquired: %s on %s"),
			*GetNameSafe(CurrentBase),
			*GetNameSafe(CurrentBase->GetOwner()));
	}
	else if (PreviousBase && !CurrentBase)
	{
		UE_LOG(
			LogSubCrewMovement,
			Warning,
			TEXT("Base lost! Was: %s on %s"),
			*GetNameSafe(PreviousBase),
			*GetNameSafe(PreviousBase->GetOwner()));
	}
	else if (PreviousBase && CurrentBase)
	{
		UE_LOG(
			LogSubCrewMovement,
			Log,
			TEXT("Base changed: %s on %s -> %s on %s"),
			*GetNameSafe(PreviousBase),
			*GetNameSafe(PreviousBase->GetOwner()),
			*GetNameSafe(CurrentBase),
			*GetNameSafe(CurrentBase->GetOwner()));
	}

	LastKnownBase = CurrentBase;
}

void USubCrewMovementComponent::DebugDrawState()
{
	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	if (!bDebugDrawCrewMovement || !Frame || !Frame->IsFrameValid() || !CharacterOwner || !GetWorld())
	{
		return;
	}

	const FTransform SubTransform = Frame->GetSubTransform();
	const FVector FrameOrigin = SubTransform.GetLocation();
	const FVector ExpectedWorldPosition = Frame->LocalToWorld(RelativeLocation);
	const FVector ActualWorldPosition = CharacterOwner->GetActorLocation();

	DrawDebugSphere(GetWorld(), FrameOrigin, 24.f, 12, FColor::Green, false, 0.f, 0, 1.5f);
	DrawDebugSphere(GetWorld(), ExpectedWorldPosition, 16.f, 12, FColor::Cyan, false, 0.f, 0, 1.25f);
	DrawDebugSphere(GetWorld(), ActualWorldPosition, 16.f, 12, FColor::Yellow, false, 0.f, 0, 1.25f);
	DrawDebugLine(GetWorld(), ExpectedWorldPosition, ActualWorldPosition, FColor::Red, false, 0.f, 0, 1.25f);
	DrawDebugDirectionalArrow(
		GetWorld(),
		FrameOrigin,
		FrameOrigin + (SubTransform.GetUnitAxis(EAxis::X) * 100.f),
		20.f,
		FColor::Blue,
		false,
		0.f,
		0,
		2.f);
}

void USubCrewMovementComponent::LogPeriodicState(float DeltaTime)
{
	if (!bDebugLogCrewMovement || !CharacterOwner)
	{
		DebugLogTimer = 0.f;
		return;
	}

	DebugLogTimer += DeltaTime;
	if (DebugLogTimer < 1.f)
	{
		return;
	}

	DebugLogTimer = 0.f;

	const USubInteriorFrameComponent* Frame = GetInteriorFrame();
	const UPrimitiveComponent* CurrentBase = CharacterOwner->GetMovementBase();
	const FTransform SubTransform = Frame ? Frame->GetSubTransform() : FTransform::Identity;
	const FVector FrameDeltaLocation = Frame ? Frame->GetFrameLocationDelta() : FVector::ZeroVector;
	const FRotator FrameDeltaRotation = Frame ? Frame->GetFrameRotationDelta() : FRotator::ZeroRotator;

	UE_LOG(
		LogSubCrewMovement,
		Log,
		TEXT("CrewState | Embarked=%d | Mode=%s | Base=%s on %s | WorldLoc=%s | RelLoc=%s | RelRot=%s | SubLoc=%s | FrameDeltaLoc=%s | FrameDeltaRot=%s"),
		IsEmbarked() ? 1 : 0,
		*GetMovementName(),
		*GetNameSafe(CurrentBase),
		*GetNameSafe(CurrentBase ? CurrentBase->GetOwner() : nullptr),
		*CharacterOwner->GetActorLocation().ToCompactString(),
		*RelativeLocation.ToCompactString(),
		*RelativeRotation.ToCompactString(),
		*SubTransform.GetLocation().ToCompactString(),
		*FrameDeltaLocation.ToCompactString(),
		*FrameDeltaRotation.ToCompactString());
}

void USubCrewMovementComponent::InitializeForSubmarine()
{
	USubInteriorFrameComponent* Frame = GetInteriorFrame();
	const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(CharacterOwner);
	const bool bFrameValid = Frame && Frame->IsFrameValid() && CharacterOwner;

	if (bFrameValid)
	{
		UpdateRelativeState();
	}

	// Tick ordering fix: ensure this CMC ticks AFTER the submarine has moved AND after the frame delta is computed.
	// Without this, UpdateBasedMovement sees zero base delta because the sub
	// hasn't simulated yet this frame, causing one-frame-lag jitter.
	// We also depend on the InteriorFrame to ensure Yaw compensation uses fresh deltas.
	bool bSubTickSet = false;
	bool bFrameTickSet = false;
	
	if (Crew && Crew->CurrentSubmarine)
	{
		if (USubMovementComponent* SubMov = Crew->CurrentSubmarine->SubMovement)
		{
			AddTickPrerequisiteComponent(SubMov);
			bSubTickSet = true;
		}

		if (USubInteriorFrameComponent* FrameComp = Crew->CurrentSubmarine->InteriorFrame)
		{
			AddTickPrerequisiteComponent(FrameComp);
			bFrameTickSet = true;
		}
	}

	LastKnownBase.Reset();
	DebugLogTimer = 0.f;

	UE_LOG(
		LogSubCrewMovement,
		Log,
		TEXT("InitializeForSubmarine | Sub=%s | CharacterLoc=%s | RelLoc=%s | FrameValid=%d | SubPrereq=%d | FramePrereq=%d"),
		*GetNameSafe(Crew ? Crew->CurrentSubmarine : nullptr),
		CharacterOwner ? *CharacterOwner->GetActorLocation().ToCompactString() : TEXT("None"),
		*RelativeLocation.ToCompactString(),
		bFrameValid ? 1 : 0,
		bSubTickSet ? 1 : 0,
		bFrameTickSet ? 1 : 0);
}
