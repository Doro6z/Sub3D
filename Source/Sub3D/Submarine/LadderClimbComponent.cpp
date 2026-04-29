#include "LadderClimbComponent.h"
#include "SubCrewCharacter.h"
#include "SubCrewMovementComponent.h"
#include "SubmarineBase.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubLadder, Log, All);

ULadderClimbComponent::ULadderClimbComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);  // No replicated state today; entry/exit go via crew rep.
}

ASubmarineBase* ULadderClimbComponent::GetOwningSubmarine() const
{
	for (const AActor* Cur = GetOwner(); Cur; Cur = Cur->GetAttachParentActor())
	{
		if (ASubmarineBase* Sub = const_cast<ASubmarineBase*>(Cast<ASubmarineBase>(Cur)))
		{
			return Sub;
		}
	}
	return nullptr;
}

FVector ULadderClimbComponent::GetClimbDirectionLocal() const
{
	const FVector Delta = ClimbEndLocal - ClimbStartLocal;
	return Delta.IsNearlyZero() ? FVector::UpVector : Delta.GetSafeNormal();
}

float ULadderClimbComponent::GetClimbLengthCm() const
{
	return static_cast<float>((ClimbEndLocal - ClimbStartLocal).Size());
}

FTransform ULadderClimbComponent::ComputeClimbLocalPose(float Progress01) const
{
	const float Clamped = FMath::Clamp(Progress01, 0.f, 1.f);
	const FVector LocalLoc = FMath::Lerp(ClimbStartLocal, ClimbEndLocal, Clamped);
	const FRotator LocalRot(0.f, ClimbFacingYawLocalDeg, 0.f);
	return FTransform(LocalRot.Quaternion(), LocalLoc);
}

FTransform ULadderClimbComponent::ComputeClimbWorldPose(float Progress01) const
{
	const ASubmarineBase* Sub = GetOwningSubmarine();
	const FTransform SubXf = Sub ? Sub->GetActorTransform() : FTransform::Identity;
	return ComputeClimbLocalPose(Progress01) * SubXf;
}

bool ULadderClimbComponent::ValidateEntry(ASubCrewCharacter* Interactor, bool bFromBottom) const
{
	if (!Interactor)
	{
		UE_LOG(LogSubLadder, Warning, TEXT("Ladder entry rejected: null interactor"));
		return false;
	}
	if (CurrentClimber.IsValid())
	{
		UE_LOG(LogSubLadder, Warning, TEXT("Ladder entry rejected: occupied by %s, %s denied"),
			*GetNameSafe(CurrentClimber.Get()), *GetNameSafe(Interactor));
		return false;
	}
	const ASubmarineBase* Sub = GetOwningSubmarine();
	if (!Sub || Interactor->CurrentSubmarine != Sub)
	{
		UE_LOG(LogSubLadder, Warning, TEXT("Ladder entry rejected: %s not on the owning sub %s"),
			*GetNameSafe(Interactor), *GetNameSafe(Sub));
		return false;
	}
	const USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(Interactor->GetCharacterMovement());
	if (!CrewMov || !CrewMov->IsGridAuthoritative())
	{
		UE_LOG(LogSubLadder, Warning, TEXT("Ladder entry rejected: %s not embarked"),
			*GetNameSafe(Interactor));
		return false;
	}
	return true;
}

void ULadderClimbComponent::StartClimbInternal(ASubCrewCharacter* Crew, bool bFromBottom)
{
	if (!Crew) { return; }
	USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(Crew->GetCharacterMovement());
	if (!CrewMov) { return; }

	CurrentClimber = Crew;
	const float StartProgress = bFromBottom ? 0.f : 1.f;
	CrewMov->BeginLadderClimb(this, StartProgress);

	UE_LOG(LogSubLadder, Log, TEXT("Ladder entry accepted | Crew=%s | FromBottom=%d | StartProgress=%.2f"),
		*GetNameSafe(Crew), bFromBottom ? 1 : 0, StartProgress);
}

void ULadderClimbComponent::TryEnterClimbFromBottom(ASubCrewCharacter* Interactor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogSubLadder, Warning, TEXT("TryEnterClimbFromBottom called on non-authority — ignored"));
		return;
	}
	if (ValidateEntry(Interactor, /*bFromBottom*/ true))
	{
		StartClimbInternal(Interactor, true);
	}
}

void ULadderClimbComponent::TryEnterClimbFromTop(ASubCrewCharacter* Interactor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogSubLadder, Warning, TEXT("TryEnterClimbFromTop called on non-authority — ignored"));
		return;
	}
	if (ValidateEntry(Interactor, /*bFromBottom*/ false))
	{
		StartClimbInternal(Interactor, false);
	}
}

void ULadderClimbComponent::NotifyClimbFinished(ASubCrewCharacter* Crew)
{
	if (CurrentClimber.Get() == Crew)
	{
		UE_LOG(LogSubLadder, Log, TEXT("Ladder climb finished | Crew=%s"), *GetNameSafe(Crew));
		CurrentClimber.Reset();
	}
}
