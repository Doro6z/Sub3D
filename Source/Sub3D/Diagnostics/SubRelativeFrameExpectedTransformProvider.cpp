#include "Diagnostics/SubRelativeFrameExpectedTransformProvider.h"
#include "Submarine/SubInteriorFrameComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

bool USubRelativeFrameExpectedTransformProvider::GetExpectedTransform(const AActor* MonitoredActor, FTransform& OutTransform) const
{
	if (!MonitoredActor) return false;

	USubInteriorFrameComponent* InteriorFrame = FindActiveInteriorFrame(MonitoredActor);
	if (!InteriorFrame || !InteriorFrame->IsFrameValid())
	{
		OutTransform = MonitoredActor->GetActorTransform();
		bHasLastKnownRelative = false;
		return true;
	}

	const ACharacter* Character = Cast<ACharacter>(MonitoredActor);
	bool bIsWalking = Character && Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsMovingOnGround();

	if (bIsWalking)
	{
		FTransform ActorTransform = MonitoredActor->GetActorTransform();
		FTransform FrameTransform = InteriorFrame->GetSubTransform();
		
		LastKnownRelativeTransform = ActorTransform.GetRelativeTransform(FrameTransform);
		bHasLastKnownRelative = true;

		OutTransform = ActorTransform;
		return true;
	}
	else if (bHasLastKnownRelative)
	{
		FTransform FrameTransform = InteriorFrame->GetSubTransform();
		OutTransform = LastKnownRelativeTransform * FrameTransform;
		return true;
	}

	return false;
}

USubInteriorFrameComponent* USubRelativeFrameExpectedTransformProvider::FindActiveInteriorFrame(const AActor* TargetActor) const
{
	if (const ACharacter* Character = Cast<ACharacter>(TargetActor))
	{
		if (UPrimitiveComponent* MovementBase = Character->GetMovementBase())
		{
			if (AActor* BaseOwner = MovementBase->GetOwner())
			{
				return BaseOwner->FindComponentByClass<USubInteriorFrameComponent>();
			}
		}
	}
	
	return nullptr;
}
