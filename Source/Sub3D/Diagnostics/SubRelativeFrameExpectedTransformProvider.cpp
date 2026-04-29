#include "Diagnostics/SubRelativeFrameExpectedTransformProvider.h"
#include "Submarine/SubmarineBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

bool USubRelativeFrameExpectedTransformProvider::GetExpectedTransform(const AActor* MonitoredActor, FTransform& OutTransform) const
{
	if (!MonitoredActor) return false;

	ASubmarineBase* Sub = FindActiveSubmarine(MonitoredActor);
	if (!Sub)
	{
		OutTransform = MonitoredActor->GetActorTransform();
		bHasLastKnownRelative = false;
		return true;
	}

	const ACharacter* Character = Cast<ACharacter>(MonitoredActor);
	const bool bIsWalking = Character && Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsMovingOnGround();

	if (bIsWalking)
	{
		const FTransform ActorTransform = MonitoredActor->GetActorTransform();
		const FTransform FrameTransform = Sub->GetActorTransform();

		LastKnownRelativeTransform = ActorTransform.GetRelativeTransform(FrameTransform);
		bHasLastKnownRelative = true;

		OutTransform = ActorTransform;
		return true;
	}
	else if (bHasLastKnownRelative)
	{
		const FTransform FrameTransform = Sub->GetActorTransform();
		OutTransform = LastKnownRelativeTransform * FrameTransform;
		return true;
	}

	return false;
}

ASubmarineBase* USubRelativeFrameExpectedTransformProvider::FindActiveSubmarine(const AActor* TargetActor) const
{
	if (const ACharacter* Character = Cast<ACharacter>(TargetActor))
	{
		if (UPrimitiveComponent* MovementBase = Character->GetMovementBase())
		{
			if (AActor* BaseOwner = MovementBase->GetOwner())
			{
				return Cast<ASubmarineBase>(BaseOwner);
			}
		}
	}

	return nullptr;
}
