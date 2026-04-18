#include "DoorFloodVfxComponent.h"

#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "SubDoorActor.h"
#include "SubFloodComponent.h"
#include "SubmarineBase.h"
#include "SubmarineCompartmentComponent.h"

UDoorFloodVfxComponent::UDoorFloodVfxComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDoorFloodVfxComponent::BeginPlay()
{
	Super::BeginPlay();

	CompartmentComp = GetOwner() ? GetOwner()->FindComponentByClass<USubmarineCompartmentComponent>() : nullptr;
	SubFlood = GetOwner() ? GetOwner()->FindComponentByClass<USubFloodComponent>() : nullptr;

	if (!SubFlood)
	{
		return;
	}

	if (SubFlood->IsInitialized())
	{
		ActivateSubFloodPath();
		return;
	}

	SubFlood->OnFloodInitialized.AddDynamic(this, &UDoorFloodVfxComponent::HandleFloodInitialized);
}

void UDoorFloodVfxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SubFlood)
	{
		SubFlood->OnFloodInitialized.RemoveDynamic(this, &UDoorFloodVfxComponent::HandleFloodInitialized);
		SubFlood->OnFloodStateUpdated.RemoveDynamic(this, &UDoorFloodVfxComponent::HandleSubFloodUpdated);
	}

	DestroyPooledCascades();
	Super::EndPlay(EndPlayReason);
}

void UDoorFloodVfxComponent::HandleFloodInitialized()
{
	if (!SubFlood || !SubFlood->IsInitialized())
	{
		return;
	}

	SubFlood->OnFloodInitialized.RemoveDynamic(this, &UDoorFloodVfxComponent::HandleFloodInitialized);
	ActivateSubFloodPath();
}

void UDoorFloodVfxComponent::ActivateSubFloodPath()
{
	SubFlood->OnFloodStateUpdated.AddDynamic(this, &UDoorFloodVfxComponent::HandleSubFloodUpdated);
	RefreshFromCurrentFloodState();
}

void UDoorFloodVfxComponent::RefreshFromCurrentFloodState()
{
	if (!SubFlood || !SubFlood->IsInitialized())
	{
		return;
	}

	TArray<FCompartmentState> States;
	SubFlood->ExportCompartmentStates(States);
	HandleSubFloodUpdated(States);
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

float UDoorFloodVfxComponent::GetCompartmentWaterHeightCmFromStates(const TArray<FCompartmentState>& States, FName CompartmentId) const
{
	const FCompartmentState* State = States.FindByPredicate([CompartmentId](const FCompartmentState& Candidate)
	{
		return Candidate.CompartmentId == CompartmentId;
	});

	return State ? State->WaterHeightCm : 0.f;
}

void UDoorFloodVfxComponent::HandleSubFloodUpdated(const TArray<FCompartmentState>& InStates)
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

	// Gather cascade candidates using FCompartmentState water heights
	TArray<FDoorCascadeCandidate> Candidates;
	Candidates.Reset();

	const ASubmarineBase* SubBase = Cast<ASubmarineBase>(Owner);
	if (SubBase && CompartmentComp)
	{
		const TArray<FDoorState>& Doors = CompartmentComp->GetDoors();
		for (const FDoorState& Door : Doors)
		{
			if (Door.bClosed || Door.DoorId.IsNone())
			{
				continue;
			}

			const float HeightA = GetCompartmentWaterHeightCmFromStates(InStates, Door.CompartmentA);
			const float HeightB = GetCompartmentWaterHeightCmFromStates(InStates, Door.CompartmentB);
			const float Delta = FMath::Abs(HeightA - HeightB);

			if (Delta < HeightDeltaThresholdCm)
			{
				continue;
			}

			const ASubDoorActor* DoorActor = SubBase->FindAttachedDoorById(Door.DoorId);
			if (!DoorActor)
			{
				continue;
			}

			FDoorCascadeCandidate Candidate;
			Candidate.DoorId = Door.DoorId;
			Candidate.WorldTransform = DoorActor->GetActorTransform();
			Candidate.HeightDeltaCm = Delta;
			Candidate.FlowDirection = (HeightA > HeightB)
				? DoorActor->GetActorForwardVector()
				: -DoorActor->GetActorForwardVector();
			Candidates.Add(Candidate);
		}
	}

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
