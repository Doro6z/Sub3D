#include "SubInteractionComponent.h"

#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "InteractableComponent.h"
#include "SubCrewCharacter.h"
#include "SubHullComponent.h"
#include "SubmarineBase.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubInteraction, Log, All);

USubInteractionComponent::USubInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 10Hz is enough for UI feedback
}

void USubInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update focused targets for UI
	FocusedActor = ResolvePrimaryInteractTarget();
	FocusedInteractable = FocusedActor ? FocusedActor->FindComponentByClass<UInteractableComponent>() : nullptr;
}

void USubInteractionComponent::TryPrimaryInteract()
{
	AActor* TargetActor = ResolvePrimaryInteractTarget();
	if (!TargetActor)
	{
		if (bDebugInteractionTrace)
		{
			UE_LOG(LogSubInteraction, Warning, TEXT("TryPrimaryInteract: no interactable target resolved for %s"),
				*GetNameSafe(GetOwner()));
		}
		return;
	}

	if (bDebugInteractionTrace)
	{
		UE_LOG(LogSubInteraction, Log, TEXT("TryPrimaryInteract: resolved target %s for %s"),
			*GetNameSafe(TargetActor), *GetNameSafe(GetOwner()));
	}

	ServerTryPrimaryInteract(TargetActor);
}

void USubInteractionComponent::BeginToolAction()
{
}

void USubInteractionComponent::EndToolAction()
{
}

bool USubInteractionComponent::PerformRepairTrace()
{
	return ResolvePrimaryInteractTarget() != nullptr;
}

bool USubInteractionComponent::TryRepairFocusedTarget(float RepairStrength, float RadiusCm)
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	AActor* TargetActor = ResolvePrimaryInteractTarget(&Start, &End);
	if (!TargetActor)
	{
		return false;
	}

	FHitResult Hit;
	ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetOwner());
	UCameraComponent* ViewCamera = Crew ? Crew->GetActiveViewCamera() : nullptr;
	if (!Crew || !Crew->GetWorld() || !ViewCamera)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SubRepairTrace), false);
	Params.AddIgnoredActor(Crew);
	Crew->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (!Hit.GetActor())
	{
		return false;
	}

	ServerTryRepairTarget(Hit.GetActor(), Hit.ImpactPoint, RepairStrength, RadiusCm);
	return true;
}

void USubInteractionComponent::ServerTryPrimaryInteract_Implementation(AActor* TargetActor)
{
	ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetOwner());
	if (!Crew || !TargetActor)
	{
		return;
	}

	if (UInteractableComponent* Interactable = TargetActor->FindComponentByClass<UInteractableComponent>())
	{
		if (bDebugInteractionTrace)
		{
			UE_LOG(LogSubInteraction, Log, TEXT("ServerTryPrimaryInteract: TriggerInteract on %s for crew %s"),
				*GetNameSafe(TargetActor), *GetNameSafe(Crew));
		}
		Interactable->TriggerInteract(Crew);
	}
	else if (bDebugInteractionTrace)
	{
		UE_LOG(LogSubInteraction, Warning, TEXT("ServerTryPrimaryInteract: target %s has no UInteractableComponent"),
			*GetNameSafe(TargetActor));
	}
}

void USubInteractionComponent::ServerTryRepairTarget_Implementation(AActor* TargetActor, FVector_NetQuantize ImpactPoint, float RepairStrength, float RadiusCm)
{
	ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetOwner());
	if (!Crew || !TargetActor)
	{
		return;
	}

	ASubmarineBase* TargetSubmarine = Cast<ASubmarineBase>(TargetActor);
	if (!TargetSubmarine)
	{
		for (AActor* Cursor = TargetActor; Cursor != nullptr; Cursor = Cursor->GetOwner())
		{
			if (ASubmarineBase* OwnerSub = Cast<ASubmarineBase>(Cursor))
			{
				TargetSubmarine = OwnerSub;
				break;
			}
		}

		if (!TargetSubmarine)
		{
			for (AActor* Cursor = TargetActor->GetAttachParentActor(); Cursor != nullptr; Cursor = Cursor->GetAttachParentActor())
			{
				if (ASubmarineBase* AttachSub = Cast<ASubmarineBase>(Cursor))
				{
					TargetSubmarine = AttachSub;
					break;
				}
			}
		}
	}

	if (TargetSubmarine && TargetSubmarine->SubHull)
	{
		TargetSubmarine->SubHull->RepairAtWorldPoint(ImpactPoint, RepairStrength, RadiusCm);
	}
}

AActor* USubInteractionComponent::ResolvePrimaryInteractTarget(FVector* OutTraceStart, FVector* OutTraceEnd) const
{
	const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetOwner());
	UCameraComponent* ViewCamera = Crew ? Crew->GetActiveViewCamera() : nullptr;
	if (!Crew || !ViewCamera || !Crew->GetWorld())
	{
		return nullptr;
	}

	const FVector Start = ViewCamera->GetComponentLocation();
	const FVector End = Start + ViewCamera->GetForwardVector() * Crew->InteractDistance;

	if (OutTraceStart)
	{
		*OutTraceStart = Start;
	}
	if (OutTraceEnd)
	{
		*OutTraceEnd = End;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SubInteractionTrace), false);
	Params.AddIgnoredActor(Crew);
	Params.bReturnPhysicalMaterial = false;
	if (Crew->CurrentSubmarine)
	{
		Params.AddIgnoredActor(Crew->CurrentSubmarine);
	}

	if (Crew->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (bDebugInteractionTrace)
		{
			DrawDebugLine(Crew->GetWorld(), Start, Hit.ImpactPoint, FColor::Green, false, 1.5f, 0, 1.f);
			DrawDebugSphere(Crew->GetWorld(), Hit.ImpactPoint, 8.f, 12, FColor::Green, false, 1.5f);
			UE_LOG(LogSubInteraction, Log, TEXT("ResolvePrimaryInteractTarget: trace hit actor=%s component=%s distance=%.1f"),
				*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance);
		}

		if (AActor* InteractableActor = ResolveInteractableOwner(Hit.GetActor()))
		{
			return InteractableActor;
		}

		if (bDebugInteractionTrace)
		{
			UE_LOG(LogSubInteraction, Warning, TEXT("ResolvePrimaryInteractTarget: blocking hit %s is not interactable, trying fallback"),
				*GetNameSafe(Hit.GetActor()));
		}
	}
	else if (bDebugInteractionTrace)
	{
		DrawDebugLine(Crew->GetWorld(), Start, End, FColor::Red, false, 1.5f, 0, 1.f);
		UE_LOG(LogSubInteraction, Warning, TEXT("ResolvePrimaryInteractTarget: no direct visibility hit"));
	}

	return ResolveNearbyInteractableFallback(Start, End);
}

AActor* USubInteractionComponent::ResolveNearbyInteractableFallback(const FVector& TraceStart, const FVector& TraceEnd) const
{
	const ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(GetOwner());
	UCameraComponent* ViewCamera = Crew ? Crew->GetActiveViewCamera() : nullptr;
	if (!Crew || !ViewCamera || !Crew->GetWorld())
	{
		return nullptr;
	}

	AActor* BestActor = nullptr;
	float BestScore = -FLT_MAX;
	const FVector ViewOrigin = ViewCamera->GetComponentLocation();
	const FVector ViewForward = ViewCamera->GetForwardVector().GetSafeNormal();
	const float MaxDistanceSq = FMath::Square(Crew->InteractDistance);

	for (TActorIterator<AActor> It(Crew->GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == Crew || Candidate == Crew->CurrentSubmarine)
		{
			continue;
		}

		AActor* InteractableActor = ResolveInteractableOwner(Candidate);
		if (!InteractableActor || InteractableActor != Candidate)
		{
			continue;
		}

		const FVector CandidateLocation = Candidate->GetActorLocation();
		const FVector ToCandidate = CandidateLocation - ViewOrigin;
		const float DistanceSq = ToCandidate.SizeSquared();
		if (DistanceSq > MaxDistanceSq || DistanceSq <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector DirToCandidate = ToCandidate.GetSafeNormal();
		const float FacingDot = FVector::DotProduct(ViewForward, DirToCandidate);
		if (FacingDot < 0.55f)
		{
			continue;
		}

		FCollisionQueryParams VisibilityParams(SCENE_QUERY_STAT(SubInteractionFallbackTrace), false);
		VisibilityParams.AddIgnoredActor(Crew);
		if (Crew->CurrentSubmarine)
		{
			VisibilityParams.AddIgnoredActor(Crew->CurrentSubmarine);
		}

		FHitResult VisibilityHit;
		const bool bBlocked = Crew->GetWorld()->LineTraceSingleByChannel(
			VisibilityHit,
			ViewOrigin,
			CandidateLocation,
			ECC_Visibility,
			VisibilityParams);
		if (bBlocked && ResolveInteractableOwner(VisibilityHit.GetActor()) != Candidate)
		{
			continue;
		}

		const float Score = FacingDot * 10000.f - DistanceSq;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestActor = Candidate;
		}
	}

	if (bDebugInteractionTrace)
	{
		if (BestActor)
		{
			DrawDebugLine(Crew->GetWorld(), TraceStart, BestActor->GetActorLocation(), FColor::Cyan, false, 1.5f, 0, 1.f);
			DrawDebugSphere(Crew->GetWorld(), BestActor->GetActorLocation(), 12.f, 12, FColor::Cyan, false, 1.5f);
			UE_LOG(LogSubInteraction, Log, TEXT("ResolveNearbyInteractableFallback: selected %s"),
				*GetNameSafe(BestActor));
		}
		else
		{
			UE_LOG(LogSubInteraction, Warning, TEXT("ResolveNearbyInteractableFallback: no interactable candidate found"));
		}
	}

	return BestActor;
}

AActor* USubInteractionComponent::ResolveInteractableOwner(AActor* HitActor)
{
	for (AActor* Cursor = HitActor; Cursor != nullptr; Cursor = Cursor->GetOwner())
	{
		if (Cursor->FindComponentByClass<UInteractableComponent>())
		{
			return Cursor;
		}
	}

	for (AActor* Cursor = HitActor ? HitActor->GetAttachParentActor() : nullptr; Cursor != nullptr; Cursor = Cursor->GetAttachParentActor())
	{
		if (Cursor->FindComponentByClass<UInteractableComponent>())
		{
			return Cursor;
		}
	}

	return nullptr;
}
