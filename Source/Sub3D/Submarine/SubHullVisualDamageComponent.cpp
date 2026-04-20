#include "SubHullVisualDamageComponent.h"
#include "Sub3DDebugSettings.h"

#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SubHullComponent.h"
#include "SubmarineBase.h"
#include "SubmarineLayoutAsset.h"

USubHullVisualDamageComponent::USubHullVisualDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USubHullVisualDamageComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveSubHullComponent();
	ResolveVisualHullComponent();

	if (!SubHull)
	{
		return;
	}

	SubHull->OnBreachesUpdated.AddDynamic(this, &USubHullVisualDamageComponent::HandleBreachesUpdated);
	RefreshFromCurrentBreaches();
}

void USubHullVisualDamageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SubHull)
	{
		SubHull->OnBreachesUpdated.RemoveDynamic(this, &USubHullVisualDamageComponent::HandleBreachesUpdated);
	}

	DynamicMaterials.Reset();
	ActiveBreachVisuals.Reset();
	VisualHullComponent = nullptr;

	Super::EndPlay(EndPlayReason);
}

bool USubHullVisualDamageComponent::ResolveSubHullComponent()
{
	if (SubHull)
	{
		return true;
	}

	SubHull = GetOwner() ? GetOwner()->FindComponentByClass<USubHullComponent>() : nullptr;
	return SubHull != nullptr;
}

void USubHullVisualDamageComponent::RefreshFromCurrentBreaches()
{
	ResolveSubHullComponent();
	ResolveVisualHullComponent();

	if (!SubHull)
	{
		ActiveBreachVisuals.Reset();
		ApplyMaterialParameters();
		return;
	}

	HandleBreachesUpdated(SubHull->GetBreachClusters());
}

void USubHullVisualDamageComponent::HandleBreachesUpdated(const TArray<FBreachClusterState>& Breaches)
{
	ResolveVisualHullComponent();
	ActiveBreachVisuals.Reset();

	TArray<FBreachClusterState> SortedBreaches = Breaches;
	SortedBreaches.Sort([](const FBreachClusterState& A, const FBreachClusterState& B)
	{
		if (!FMath::IsNearlyEqual(A.OpenAreaCm2, B.OpenAreaCm2))
		{
			return A.OpenAreaCm2 > B.OpenAreaCm2;
		}

		return A.InscribedRadiusCm > B.InscribedRadiusCm;
	});

	for (const FBreachClusterState& Cluster : SortedBreaches)
	{
		FSubHullBreachVisualState VisualState;
		if (!BuildVisualState(Cluster, VisualState))
		{
			continue;
		}

		ActiveBreachVisuals.Add(MoveTemp(VisualState));
		if (ActiveBreachVisuals.Num() >= MaxTrackedBreaches)
		{
			break;
		}
	}

	ApplyMaterialParameters();

	if (GetDefault<USub3DDebugSettings>()->bLogHullVisualBreaches)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("HullVisualDamage | Mesh=%s | VisualBreaches=%d"),
			*GetNameSafe(VisualHullComponent),
			ActiveBreachVisuals.Num());
	}
}

bool USubHullVisualDamageComponent::ResolveVisualHullComponent()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		VisualHullComponent = nullptr;
		DynamicMaterials.Reset();
		return false;
	}

	UMeshComponent* PreferredComponent = nullptr;
	TInlineComponentArray<UMeshComponent*> MeshComponents(Owner);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (!IsValid(MeshComponent))
		{
			continue;
		}

		if (MeshComponent->GetName().StartsWith(TEXT("PMC_ExteriorHull"))
			|| MeshComponent->ComponentHasTag(TEXT("ExteriorHull")))
		{
			PreferredComponent = MeshComponent;
			break;
		}
	}

	if (!PreferredComponent)
	{
		if (const ASubmarineBase* Submarine = Cast<ASubmarineBase>(Owner))
		{
			PreferredComponent = Submarine->HullMesh;
		}
	}

	if (VisualHullComponent == PreferredComponent)
	{
		return PreferredComponent != nullptr;
	}

	VisualHullComponent = PreferredComponent;
	DynamicMaterials.Reset();
	InitializeDynamicMaterials();
	return PreferredComponent != nullptr;
}

void USubHullVisualDamageComponent::InitializeDynamicMaterials()
{
	if (!VisualHullComponent)
	{
		DynamicMaterials.Reset();
		return;
	}

	const int32 MaterialCount = VisualHullComponent->GetNumMaterials();
	DynamicMaterials.SetNum(MaterialCount);

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		if (UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(VisualHullComponent->GetMaterial(MaterialIndex)))
		{
			DynamicMaterials[MaterialIndex] = ExistingMID;
			continue;
		}

		if (!bAutoCreateDynamicMaterials || !VisualHullComponent->GetMaterial(MaterialIndex))
		{
			continue;
		}

		DynamicMaterials[MaterialIndex] = VisualHullComponent->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
	}
}

void USubHullVisualDamageComponent::ApplyMaterialParameters()
{
	if (!bApplyMaterialParameters || !ResolveVisualHullComponent())
	{
		return;
	}

	InitializeDynamicMaterials();

	for (int32 MaterialIndex = 0; MaterialIndex < DynamicMaterials.Num(); ++MaterialIndex)
	{
		UMaterialInstanceDynamic* MaterialInstance = DynamicMaterials[MaterialIndex];
		if (!MaterialInstance)
		{
			continue;
		}

		int32 SlotVisualCount = 0;
		for (const FSubHullBreachVisualState& VisualState : ActiveBreachVisuals)
		{
			if (VisualState.MaterialSlotIndex != MaterialIndex || SlotVisualCount >= MaxTrackedBreaches)
			{
				continue;
			}

			if (!BreachCenterParameterPrefix.IsNone())
			{
				MaterialInstance->SetVectorParameterValue(
					MakeIndexedParameterName(BreachCenterParameterPrefix, SlotVisualCount),
					FLinearColor(VisualState.SheetSpaceCenter01.X, VisualState.SheetSpaceCenter01.Y, 0.f, 0.f));
			}

			if (!BreachRadiusParameterPrefix.IsNone())
			{
				MaterialInstance->SetScalarParameterValue(
					MakeIndexedParameterName(BreachRadiusParameterPrefix, SlotVisualCount),
					VisualState.VisibleRadius01);
			}

			++SlotVisualCount;
		}

		if (!BreachCountParameter.IsNone())
		{
			MaterialInstance->SetScalarParameterValue(BreachCountParameter, static_cast<float>(SlotVisualCount));
		}

		for (int32 ClearIndex = SlotVisualCount; ClearIndex < MaxTrackedBreaches; ++ClearIndex)
		{
			if (!BreachCenterParameterPrefix.IsNone())
			{
				MaterialInstance->SetVectorParameterValue(
					MakeIndexedParameterName(BreachCenterParameterPrefix, ClearIndex),
					FLinearColor::Black);
			}

			if (!BreachRadiusParameterPrefix.IsNone())
			{
				MaterialInstance->SetScalarParameterValue(
					MakeIndexedParameterName(BreachRadiusParameterPrefix, ClearIndex),
					0.f);
			}
		}
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TInlineComponentArray<UMeshComponent*> MeshComponents(Owner);
	static const FString InteriorPrefix = TEXT("PMC_Interior_Vis_");
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (!MeshComponent || !MeshComponent->GetName().StartsWith(InteriorPrefix))
		{
			continue;
		}

		const FString CompartmentSuffix = MeshComponent->GetName().Mid(InteriorPrefix.Len());
		const FName CompartmentId(*CompartmentSuffix);
		const int32 MaterialCount = MeshComponent->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			UMaterialInstanceDynamic* MaterialInstance = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(MaterialIndex));
			if (!MaterialInstance && bAutoCreateDynamicMaterials && MeshComponent->GetMaterial(MaterialIndex))
			{
				MaterialInstance = MeshComponent->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
			}

			if (!MaterialInstance)
			{
				continue;
			}

			int32 SlotVisualCount = 0;
			for (const FSubHullBreachVisualState& VisualState : ActiveBreachVisuals)
			{
				if (VisualState.CompartmentId != CompartmentId || SlotVisualCount >= MaxTrackedBreaches)
				{
					continue;
				}

				if (!BreachCenterParameterPrefix.IsNone())
				{
					MaterialInstance->SetVectorParameterValue(
						MakeIndexedParameterName(BreachCenterParameterPrefix, SlotVisualCount),
						FLinearColor(VisualState.SheetSpaceCenter01.X, VisualState.SheetSpaceCenter01.Y, 0.f, 0.f));
				}

				if (!BreachRadiusParameterPrefix.IsNone())
				{
					MaterialInstance->SetScalarParameterValue(
						MakeIndexedParameterName(BreachRadiusParameterPrefix, SlotVisualCount),
						VisualState.VisibleRadius01);
				}

				++SlotVisualCount;
			}

			if (!BreachCountParameter.IsNone())
			{
				MaterialInstance->SetScalarParameterValue(BreachCountParameter, static_cast<float>(SlotVisualCount));
			}

			for (int32 ClearIndex = SlotVisualCount; ClearIndex < MaxTrackedBreaches; ++ClearIndex)
			{
				if (!BreachCenterParameterPrefix.IsNone())
				{
					MaterialInstance->SetVectorParameterValue(
						MakeIndexedParameterName(BreachCenterParameterPrefix, ClearIndex),
						FLinearColor::Black);
				}

				if (!BreachRadiusParameterPrefix.IsNone())
				{
					MaterialInstance->SetScalarParameterValue(
						MakeIndexedParameterName(BreachRadiusParameterPrefix, ClearIndex),
						0.f);
				}
			}
		}
	}
}

bool USubHullVisualDamageComponent::BuildVisualState(const FBreachClusterState& Cluster, FSubHullBreachVisualState& OutState) const
{
	const FStructuralSheetDef* SheetDef = FindSheetDef(Cluster.SheetId);
	if (!SheetDef || !SheetDef->bCanOpenToExterior || !SheetDef->bSupportsVisualRupture)
	{
		return false;
	}

	const FVector TangentX = SheetDef->VisualLocalTangentX.GetSafeNormal();
	const FVector TangentY = SheetDef->VisualLocalTangentY.GetSafeNormal();
	if (TangentX.IsNearlyZero() || TangentY.IsNearlyZero())
	{
		return false;
	}

	const FVector2D ProjectionSizeCm(
		FMath::Max(1.f, SheetDef->VisualProjectionSizeCm.X),
		FMath::Max(1.f, SheetDef->VisualProjectionSizeCm.Y));
	const FVector LocalOffset = Cluster.LocalCenter - SheetDef->VisualLocalOrigin;
	const float SheetSpaceX = FVector::DotProduct(LocalOffset, TangentX);
	const float SheetSpaceY = FVector::DotProduct(LocalOffset, TangentY);
	const FStructuralSheetCompiledBinding* Binding = FindCompiledBinding(Cluster.SheetId);
	if (!Binding && GetDefault<USub3DDebugSettings>()->bLogHullVisualBreaches)
	{
		UE_LOG(LogTemp, Warning, TEXT("HullVisualDamage | Missing compiled binding for sheet %s"), *Cluster.SheetId.ToString());
	}
	const FVector2D ChartMin = Binding ? Binding->ChartMin : FVector2D(0.f, 0.f);
	const FVector2D ChartMax = Binding ? Binding->ChartMax : FVector2D(1.f, 1.f);
	const FVector2D SafeChartMin(
		FMath::Clamp(FMath::Min(ChartMin.X, ChartMax.X), 0.f, 1.f),
		FMath::Clamp(FMath::Min(ChartMin.Y, ChartMax.Y), 0.f, 1.f));
	const FVector2D SafeChartMax(
		FMath::Clamp(FMath::Max(ChartMin.X, ChartMax.X), 0.f, 1.f),
		FMath::Clamp(FMath::Max(ChartMin.Y, ChartMax.Y), 0.f, 1.f));
	const FVector2D ChartExtent = FVector2D(
		FMath::Max(KINDA_SMALL_NUMBER, SafeChartMax.X - SafeChartMin.X),
		FMath::Max(KINDA_SMALL_NUMBER, SafeChartMax.Y - SafeChartMin.Y));

	const float MaxVisibleRadiusCm = SheetDef->MaxVisibleRuptureRadiusCm > 0.f
		? SheetDef->MaxVisibleRuptureRadiusCm
		: 0.5f * FMath::Min(ProjectionSizeCm.X, ProjectionSizeCm.Y);
	const float VisibleRadiusCm = FMath::Clamp(Cluster.InscribedRadiusCm, 0.f, MaxVisibleRadiusCm);
	const FVector2D ProjectionSpanCm = FVector2D(
		ProjectionSizeCm.X * ChartExtent.X,
		ProjectionSizeCm.Y * ChartExtent.Y);
	const float VisibleRadius01 = VisibleRadiusCm / FMath::Max(ProjectionSpanCm.X, ProjectionSpanCm.Y);
	const float LocalCenter01X = FMath::Clamp(0.5f + (SheetSpaceX / ProjectionSizeCm.X), 0.f, 1.f);
	const float LocalCenter01Y = FMath::Clamp(0.5f + (SheetSpaceY / ProjectionSizeCm.Y), 0.f, 1.f);
	const FVector2D ChartSpaceCenter01(
		FMath::Lerp(SafeChartMin.X, SafeChartMax.X, LocalCenter01X),
		FMath::Lerp(SafeChartMin.Y, SafeChartMax.Y, LocalCenter01Y));

	OutState = FSubHullBreachVisualState();
	OutState.SheetId = Cluster.SheetId;
	OutState.CompartmentId = SheetDef->ParentCompartmentId;
	if (Binding && Binding->CompartmentIndex != INDEX_NONE && SubHull && SubHull->LayoutAsset
		&& SubHull->LayoutAsset->Compartments.IsValidIndex(Binding->CompartmentIndex))
	{
		OutState.CompartmentId = SubHull->LayoutAsset->Compartments[Binding->CompartmentIndex].CompartmentId;
	}
	OutState.MaterialSlotIndex = SheetDef->ExteriorVisualMaterialSlot;
	OutState.LocalCenter = Cluster.LocalCenter;
	OutState.SheetSpaceCenter01 = ChartSpaceCenter01;
	OutState.VisibleRadiusCm = VisibleRadiusCm;
	OutState.TargetVisibleRadiusCm = VisibleRadiusCm;
	OutState.VisibleRadius01 = VisibleRadius01;
	OutState.bTouchesExterior = Cluster.bTouchesExterior;
	return true;
}

const FStructuralSheetDef* USubHullVisualDamageComponent::FindSheetDef(FName SheetId) const
{
	if (SheetId.IsNone())
	{
		return nullptr;
	}

	const_cast<USubHullVisualDamageComponent*>(this)->ResolveSubHullComponent();
	if (!SubHull)
	{
		return nullptr;
	}

	return SubHull->GetStructuralSheets().FindByPredicate([SheetId](const FStructuralSheetDef& Candidate)
	{
		return Candidate.SheetId == SheetId;
	});
}

const FStructuralSheetCompiledBinding* USubHullVisualDamageComponent::FindCompiledBinding(FName SheetId) const
{
	if (SheetId.IsNone())
	{
		return nullptr;
	}

	const_cast<USubHullVisualDamageComponent*>(this)->ResolveSubHullComponent();
	if (!SubHull || !SubHull->LayoutAsset)
	{
		return nullptr;
	}

	return SubHull->LayoutAsset->CompiledSheetBindings.FindByPredicate([SheetId](const FStructuralSheetCompiledBinding& Candidate)
	{
		return Candidate.SheetId == SheetId;
	});
}

FName USubHullVisualDamageComponent::MakeIndexedParameterName(FName Prefix, int32 Index) const
{
	return FName(*FString::Printf(TEXT("%s%d"), *Prefix.ToString(), Index));
}
