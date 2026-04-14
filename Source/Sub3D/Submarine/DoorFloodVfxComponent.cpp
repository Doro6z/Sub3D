#include "DoorFloodVfxComponent.h"

#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "SubDoorActor.h"
#include "SubHullComponent.h"
#include "SubmarineBase.h"
#include "SubmarineCompartmentComponent.h"

UDoorFloodVfxComponent::UDoorFloodVfxComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDoorFloodVfxComponent::BeginPlay()
{
	Super::BeginPlay();

	SubHull = GetOwner() ? GetOwner()->FindComponentByClass<USubHullComponent>() : nullptr;
	CompartmentComp = GetOwner() ? GetOwner()->FindComponentByClass<USubmarineCompartmentComponent>() : nullptr;

	if (!SubHull)
	{
		return;
	}

	SubHull->OnCompartmentFloodUpdated.AddDynamic(this, &UDoorFloodVfxComponent::HandleCompartmentFloodUpdated);
}

void UDoorFloodVfxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SubHull)
	{
		SubHull->OnCompartmentFloodUpdated.RemoveDynamic(this, &UDoorFloodVfxComponent::HandleCompartmentFloodUpdated);
	}

	DestroyPooledCascades();
	Super::EndPlay(EndPlayReason);
}

void UDoorFloodVfxComponent::RefreshFromCurrentFloodState()
{
	if (!SubHull)
	{
		return;
	}

	HandleCompartmentFloodUpdated(SubHull->GetCompartmentStates());
}

void UDoorFloodVfxComponent::HandleCompartmentFloodUpdated(const TArray<FCompartmentRuntimeState>& CompartmentStates)
{
	if (!CascadeEffect || !CompartmentComp)
	{
		DeactivateUnusedCascades(0);
		ActiveCascadeCount = 0;
		return;
	}

	AActor* Owner = GetOwner();
	USceneComponent* AttachParent = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Owner || !AttachParent)
	{
		ActiveCascadeCount = 0;
		return;
	}

	TArray<FDoorCascadeCandidate> Candidates;
	GatherCascadeCandidates(CompartmentStates, Candidates);

	// Sort by height delta descending — prioritize the most dramatic cascades.
	Candidates.Sort([](const FDoorCascadeCandidate& A, const FDoorCascadeCandidate& B)
	{
		return A.HeightDeltaCm > B.HeightDeltaCm;
	});

	const int32 DesiredCount = FMath::Min(Candidates.Num(), FMath::Max(0, MaxActiveCascades));
	int32 AssignedCount = 0;

	for (int32 Index = 0; Index < DesiredCount; ++Index)
	{
		const FDoorCascadeCandidate& Candidate = Candidates[Index];

		UNiagaraComponent* Component = GetOrCreateCascadeComponent(AssignedCount);
		if (!Component)
		{
			break;
		}

		// Position at the door in local space relative to the submarine.
		const FVector LocalPosition = Owner->GetActorTransform().InverseTransformPosition(Candidate.WorldTransform.GetLocation());
		const FRotator LocalRotation = (Owner->GetActorTransform().GetRotation().Inverse() * Candidate.WorldTransform.GetRotation()).Rotator();

		Component->SetAsset(CascadeEffect, true);
		Component->SetRelativeLocation(LocalPosition);
		Component->SetRelativeRotation(LocalRotation);

		const float IntensityScale = FMath::Clamp(Candidate.HeightDeltaCm / FMath::Max(1.f, MaxHeightDeltaCm), 0.f, 1.f);
		Component->SetRelativeScale3D(FVector(FMath::Max(0.2f, IntensityScale)));

		ApplyCascadeParameters(Component, Candidate);
		Component->SetVisibility(true, true);
		Component->Activate(true);
		++AssignedCount;
	}

	DeactivateUnusedCascades(AssignedCount);
	ActiveCascadeCount = AssignedCount;
}

void UDoorFloodVfxComponent::GatherCascadeCandidates(const TArray<FCompartmentRuntimeState>& CompartmentStates, TArray<FDoorCascadeCandidate>& OutCandidates) const
{
	OutCandidates.Reset();

	const ASubmarineBase* SubBase = Cast<ASubmarineBase>(GetOwner());
	if (!SubBase || !CompartmentComp)
	{
		return;
	}

	const TArray<FDoorState>& Doors = CompartmentComp->GetDoors();
	for (const FDoorState& Door : Doors)
	{
		// Only open doors produce cascades.
		if (Door.bClosed || Door.DoorId.IsNone())
		{
			continue;
		}

		const float HeightA = GetCompartmentWaterHeightCm(CompartmentStates, Door.CompartmentA);
		const float HeightB = GetCompartmentWaterHeightCm(CompartmentStates, Door.CompartmentB);
		const float Delta = FMath::Abs(HeightA - HeightB);

		if (Delta < HeightDeltaThresholdCm)
		{
			continue;
		}

		// Resolve the door actor position.
		const ASubDoorActor* DoorActor = SubBase->FindAttachedDoorById(Door.DoorId);
		if (!DoorActor)
		{
			continue;
		}

		FDoorCascadeCandidate Candidate;
		Candidate.DoorId = Door.DoorId;
		Candidate.WorldTransform = DoorActor->GetActorTransform();
		Candidate.HeightDeltaCm = Delta;
		// Flow goes from higher compartment toward lower — use door forward as approximation.
		Candidate.FlowDirection = (HeightA > HeightB)
			? DoorActor->GetActorForwardVector()
			: -DoorActor->GetActorForwardVector();
		OutCandidates.Add(Candidate);
	}
}

UNiagaraComponent* UDoorFloodVfxComponent::GetOrCreateCascadeComponent(int32 CascadeIndex)
{
	AActor* Owner = GetOwner();
	USceneComponent* AttachParent = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Owner || !AttachParent || CascadeIndex < 0)
	{
		return nullptr;
	}

	if (CascadePool.IsValidIndex(CascadeIndex) && CascadePool[CascadeIndex])
	{
		return CascadePool[CascadeIndex];
	}

	const FName ComponentName = *FString::Printf(TEXT("DoorCascade_%d"), CascadeIndex);
	UNiagaraComponent* NiagaraComponent = NewObject<UNiagaraComponent>(Owner, ComponentName, RF_Transient);
	if (!NiagaraComponent)
	{
		return nullptr;
	}

	Owner->AddInstanceComponent(NiagaraComponent);
	NiagaraComponent->SetupAttachment(AttachParent);
	NiagaraComponent->SetAutoActivate(false);
	NiagaraComponent->SetVisibility(false, true);
	NiagaraComponent->SetUsingAbsoluteLocation(false);
	NiagaraComponent->SetUsingAbsoluteRotation(false);
	NiagaraComponent->SetUsingAbsoluteScale(false);
	NiagaraComponent->RegisterComponent();

	if (!CascadePool.IsValidIndex(CascadeIndex))
	{
		CascadePool.SetNum(CascadeIndex + 1);
	}

	CascadePool[CascadeIndex] = NiagaraComponent;
	return NiagaraComponent;
}

void UDoorFloodVfxComponent::ApplyCascadeParameters(UNiagaraComponent* Component, const FDoorCascadeCandidate& Candidate) const
{
	if (!Component)
	{
		return;
	}

	const float Intensity01 = FMath::Clamp(Candidate.HeightDeltaCm / FMath::Max(1.f, MaxHeightDeltaCm), 0.f, 1.f);

	if (!FlowIntensityParam.IsNone())
	{
		Component->SetVariableFloat(FlowIntensityParam, Intensity01);
	}

	if (!FlowDirectionParam.IsNone())
	{
		Component->SetVariableVec3(FlowDirectionParam, Candidate.FlowDirection);
	}
}

void UDoorFloodVfxComponent::DeactivateUnusedCascades(int32 FirstUnusedIndex)
{
	for (int32 Index = FirstUnusedIndex; Index < CascadePool.Num(); ++Index)
	{
		UNiagaraComponent* Component = CascadePool[Index];
		if (!Component)
		{
			continue;
		}

		Component->Deactivate();
		Component->SetVisibility(false, true);
	}
}

void UDoorFloodVfxComponent::DestroyPooledCascades()
{
	for (UNiagaraComponent* Component : CascadePool)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}

	CascadePool.Reset();
	ActiveCascadeCount = 0;
}

float UDoorFloodVfxComponent::GetCompartmentWaterHeightCm(const TArray<FCompartmentRuntimeState>& States, FName CompartmentId) const
{
	const FCompartmentRuntimeState* State = States.FindByPredicate([CompartmentId](const FCompartmentRuntimeState& Candidate)
	{
		return Candidate.CompartmentId == CompartmentId;
	});

	return State ? State->WaterHeightCm : 0.f;
}
