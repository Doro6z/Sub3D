#include "BreachVfxManagerComponent.h"

#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "SubHullComponent.h"

UBreachVfxManagerComponent::UBreachVfxManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBreachVfxManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	SubHull = GetOwner() ? GetOwner()->FindComponentByClass<USubHullComponent>() : nullptr;
	if (!SubHull)
	{
		return;
	}

	SubHull->OnBreachesUpdated.AddDynamic(this, &UBreachVfxManagerComponent::HandleBreachesUpdated);
	SubHull->OnFlowFieldsUpdated.AddDynamic(this, &UBreachVfxManagerComponent::HandleFlowFieldsUpdated);
	LatestBreaches = SubHull->GetBreachClusters();
	LatestFlowFields = SubHull->GetFlowFields();
	RefreshFromCurrentBreaches();
}

void UBreachVfxManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SubHull)
	{
		SubHull->OnBreachesUpdated.RemoveDynamic(this, &UBreachVfxManagerComponent::HandleBreachesUpdated);
		SubHull->OnFlowFieldsUpdated.RemoveDynamic(this, &UBreachVfxManagerComponent::HandleFlowFieldsUpdated);
	}

	DestroyPooledEffects();
	Super::EndPlay(EndPlayReason);
}

int32 UBreachVfxManagerComponent::ComputeDesiredActiveEffectCount(const TArray<FBreachClusterState>& Breaches) const
{
	return FMath::Min(Breaches.Num(), FMath::Max(0, MaxActiveEffects));
}

void UBreachVfxManagerComponent::RefreshFromCurrentBreaches()
{
	if (!SubHull)
	{
		return;
	}

	LatestBreaches = SubHull->GetBreachClusters();
	LatestFlowFields = SubHull->GetFlowFields();
	HandleBreachesUpdated(LatestBreaches);
}

void UBreachVfxManagerComponent::HandleBreachesUpdated(const TArray<FBreachClusterState>& Breaches)
{
	LatestBreaches = Breaches;

	AActor* Owner = GetOwner();
	USceneComponent* AttachParent = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Owner || !AttachParent)
	{
		ActiveEffectCount = 0;
		return;
	}

	TArray<FBreachClusterState> SortedBreaches = Breaches;
	SortBreachesByPriority(SortedBreaches);

	const int32 DesiredActiveCount = ComputeDesiredActiveEffectCount(SortedBreaches);
	int32 AssignedEffectCount = 0;

	for (int32 BreachIndex = 0; BreachIndex < DesiredActiveCount; ++BreachIndex)
	{
		const FBreachClusterState& Cluster = SortedBreaches[BreachIndex];
		UNiagaraSystem* EffectAsset = ResolveEffectForCluster(Cluster);
		if (!EffectAsset)
		{
			continue;
		}

		UNiagaraComponent* EffectComponent = GetOrCreateEffectComponent(AssignedEffectCount);
		if (!EffectComponent)
		{
			break;
		}

		const float EffectScale = FMath::Max(0.1f, Cluster.InscribedRadiusCm / 30.f);
		EffectComponent->SetAsset(EffectAsset, true);
		EffectComponent->SetRelativeLocation(Cluster.LocalCenter);
		EffectComponent->SetRelativeRotation(FRotationMatrix::MakeFromZ(Cluster.LocalNormal.GetSafeNormal()).Rotator());
		EffectComponent->SetRelativeScale3D(FVector(EffectScale));
		ApplyEffectParameters(EffectComponent, Cluster);
		EffectComponent->SetVisibility(true, true);
		EffectComponent->Activate(true);
		++AssignedEffectCount;
	}

	DeactivateUnusedEffects(AssignedEffectCount);
	ActiveEffectCount = AssignedEffectCount;
}

void UBreachVfxManagerComponent::HandleFlowFieldsUpdated(const TArray<FBreachFlowField>& FlowFields)
{
	LatestFlowFields = FlowFields;
	HandleBreachesUpdated(LatestBreaches);
}

void UBreachVfxManagerComponent::SortBreachesByPriority(TArray<FBreachClusterState>& Breaches)
{
	Breaches.Sort([](const FBreachClusterState& A, const FBreachClusterState& B)
	{
		if (!FMath::IsNearlyEqual(A.OpenAreaCm2, B.OpenAreaCm2))
		{
			return A.OpenAreaCm2 > B.OpenAreaCm2;
		}

		return A.InscribedRadiusCm > B.InscribedRadiusCm;
	});
}

UNiagaraSystem* UBreachVfxManagerComponent::ResolveEffectForCluster(const FBreachClusterState& Cluster) const
{
	if (Cluster.InscribedRadiusCm < LeakRadiusThresholdCm)
	{
		return LeakEffect ? LeakEffect : BreachEffect;
	}

	return BreachEffect ? BreachEffect : LeakEffect;
}

UNiagaraComponent* UBreachVfxManagerComponent::GetOrCreateEffectComponent(int32 EffectIndex)
{
	AActor* Owner = GetOwner();
	USceneComponent* AttachParent = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Owner || !AttachParent || EffectIndex < 0)
	{
		return nullptr;
	}

	if (EffectPool.IsValidIndex(EffectIndex) && EffectPool[EffectIndex])
	{
		return EffectPool[EffectIndex];
	}

	const FName ComponentName = *FString::Printf(TEXT("BreachEffect_%d"), EffectIndex);
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

	if (!EffectPool.IsValidIndex(EffectIndex))
	{
		EffectPool.SetNum(EffectIndex + 1);
	}

	EffectPool[EffectIndex] = NiagaraComponent;
	return NiagaraComponent;
}

const FBreachFlowField* UBreachVfxManagerComponent::FindFlowFieldForSheet(FName SheetId) const
{
	return LatestFlowFields.FindByPredicate([SheetId](const FBreachFlowField& Candidate)
	{
		return Candidate.SheetId == SheetId;
	});
}

void UBreachVfxManagerComponent::ApplyEffectParameters(UNiagaraComponent* EffectComponent, const FBreachClusterState& Cluster) const
{
	if (!EffectComponent)
	{
		return;
	}

	const float LeakRate01 = FMath::Clamp(Cluster.InscribedRadiusCm / FMath::Max(0.1f, LeakRateRadiusDivisorCm), 0.f, 1.f);
	const FBreachFlowField* FlowField = FindFlowFieldForSheet(Cluster.SheetId);
	const float Pressure01 = FlowField
		? FMath::Clamp(FlowField->ForceScale / FMath::Max(0.1f, LeakPressureForceDivisor), 0.f, 1.f)
		: 0.f;
	const float Intensity01 = FMath::Max(LeakRate01, Pressure01);

	if (!LeakRateParameter.IsNone())
	{
		EffectComponent->SetVariableFloat(LeakRateParameter, LeakRate01);
	}

	if (!LeakPressureParameter.IsNone())
	{
		EffectComponent->SetVariableFloat(LeakPressureParameter, Pressure01);
	}

	if (!LeakIntensityParameter.IsNone())
	{
		EffectComponent->SetVariableFloat(LeakIntensityParameter, Intensity01);
	}
}

void UBreachVfxManagerComponent::DeactivateUnusedEffects(int32 FirstUnusedIndex)
{
	for (int32 EffectIndex = FirstUnusedIndex; EffectIndex < EffectPool.Num(); ++EffectIndex)
	{
		UNiagaraComponent* EffectComponent = EffectPool[EffectIndex];
		if (!EffectComponent)
		{
			continue;
		}

		EffectComponent->Deactivate();
		EffectComponent->SetVisibility(false, true);
	}
}

void UBreachVfxManagerComponent::DestroyPooledEffects()
{
	for (UNiagaraComponent* EffectComponent : EffectPool)
	{
		if (IsValid(EffectComponent))
		{
			EffectComponent->DestroyComponent();
		}
	}

	EffectPool.Reset();
	ActiveEffectCount = 0;
}
