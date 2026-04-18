#include "FloodWaterVisualsComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "SubFloodComponent.h"
#include "SubHullComponent.h"
#include "SubLegacyLog.h"
#include "SubmarineBase.h"
#include "SubmarineDefinition.h"
#include "SubmarineLayoutAsset.h"

UFloodWaterVisualsComponent::UFloodWaterVisualsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickEnabled(false);
}

void UFloodWaterVisualsComponent::BeginPlay()
{
	Super::BeginPlay();

	SubHull = GetOwner() ? GetOwner()->FindComponentByClass<USubHullComponent>() : nullptr;
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

	// SubFlood not yet initialized — wait for SubmarineBase to init it.
	SubFlood->OnFloodInitialized.AddDynamic(this, &UFloodWaterVisualsComponent::HandleFloodInitialized);
}

void UFloodWaterVisualsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SubFlood)
	{
		SubFlood->OnFloodInitialized.RemoveDynamic(this, &UFloodWaterVisualsComponent::HandleFloodInitialized);
		SubFlood->OnFloodStateUpdated.RemoveDynamic(this, &UFloodWaterVisualsComponent::HandleSubFloodUpdated);
	}

	DestroyWaterPlanes();
	Super::EndPlay(EndPlayReason);
}

void UFloodWaterVisualsComponent::HandleFloodInitialized()
{
	if (!SubFlood || !SubFlood->IsInitialized())
	{
		return;
	}

	SubFlood->OnFloodInitialized.RemoveDynamic(this, &UFloodWaterVisualsComponent::HandleFloodInitialized);
	ActivateSubFloodPath();
}

void UFloodWaterVisualsComponent::ActivateSubFloodPath()
{
	const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner());
	if (Sub && Sub->GeneratedDefinition)
	{
		Definition = Sub->GeneratedDefinition;
	}

	DestroyWaterPlanes();

	if (Definition)
	{
		InitializeWaterPlanesFromDefinition();
	}
	else
	{
		InitializeWaterPlanesFromLayout();
	}

	SubFlood->OnFloodStateUpdated.AddDynamic(this, &UFloodWaterVisualsComponent::HandleSubFloodUpdated);
	RefreshFromCurrentFloodState();
}

void UFloodWaterVisualsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bAnyPlaneStillAnimating = false;
	for (FFloodWaterPlaneState& PlaneState : WaterPlanes)
	{
		if (!PlaneState.PlaneComponent)
		{
			continue;
		}

		const float PreviousZ = PlaneState.CurrentLocalZ;
		PlaneState.CurrentLocalZ = FMath::FInterpTo(PlaneState.CurrentLocalZ, PlaneState.TargetLocalZ, DeltaTime, InterpolationSpeed);

		if (!FMath::IsNearlyEqual(PreviousZ, PlaneState.CurrentLocalZ, 0.5f))
		{
			bAnyPlaneStillAnimating = true;
		}

		const bool bReachedHiddenTarget = !PlaneState.bTargetVisible
			&& FMath::IsNearlyEqual(PlaneState.CurrentLocalZ, PlaneState.TargetLocalZ, 0.5f);

		if (bReachedHiddenTarget)
		{
			PlaneState.PlaneComponent->SetVisibility(false, true);
		}
		else
		{
			PlaneState.PlaneComponent->SetVisibility(true, true);
		}

		UpdatePlaneVisual(PlaneState);

		if (PlaneState.bTargetVisible)
		{
			bAnyPlaneStillAnimating = true;
		}
	}

	SetComponentTickEnabled(bAnyPlaneStillAnimating);
}

int32 UFloodWaterVisualsComponent::GetVisibleWaterPlaneCount() const
{
	int32 VisibleCount = 0;
	for (const FFloodWaterPlaneState& PlaneState : WaterPlanes)
	{
		if (PlaneState.PlaneComponent && PlaneState.PlaneComponent->IsVisible())
		{
			++VisibleCount;
		}
	}

	return VisibleCount;
}

float UFloodWaterVisualsComponent::ComputeSurfaceLocalZ(const FBox& LocalBounds, float WaterLevelNormalized) const
{
	const float ClampedLevel = FMath::Clamp(WaterLevelNormalized, 0.f, 1.f);
	return LocalBounds.Min.Z + ClampedLevel * (LocalBounds.Max.Z - LocalBounds.Min.Z);
}

void UFloodWaterVisualsComponent::RefreshFromCurrentFloodState()
{
	if (!SubFlood || !SubFlood->IsInitialized())
	{
		return;
	}

	if (WaterPlanes.Num() == 0)
	{
		if (Definition)
		{
			InitializeWaterPlanesFromDefinition();
		}
		else
		{
			InitializeWaterPlanesFromLayout();
		}
	}

	TArray<FCompartmentState> States;
	SubFlood->ExportCompartmentStates(States);
	HandleSubFloodUpdated(States);
}

void UFloodWaterVisualsComponent::HandleSubFloodUpdated(const TArray<FCompartmentState>& InStates)
{
	if (WaterPlanes.Num() == 0)
	{
		if (Definition)
		{
			InitializeWaterPlanesFromDefinition();
		}
		else
		{
			InitializeWaterPlanesFromLayout();
		}
	}

	ApplyFloodLevels([&](FName Id, float& OutLevel, float& OutHeight) -> bool
	{
		const FCompartmentState* S = InStates.FindByPredicate([Id](const FCompartmentState& C)
		{
			return C.CompartmentId == Id;
		});
		if (!S) return false;
		OutLevel = S->FloodLevel01;
		OutHeight = S->WaterHeightCm;
		return true;
	});
}

void UFloodWaterVisualsComponent::ApplyFloodLevels(const TFunction<bool(FName, float&, float&)>& GetLevelAndHeight)
{
	bool bAnyPlaneNeedsTick = false;

	for (FFloodWaterPlaneState& PlaneState : WaterPlanes)
	{
		float Level = 0.f;
		float Height = 0.f;
		if (!GetLevelAndHeight(PlaneState.CompartmentId, Level, Height))
		{
			continue;
		}

		const FBox LocalBounds(
			FVector(PlaneState.LocalCenter.X - PlaneState.LocalSizeCm.X * 0.5f, PlaneState.LocalCenter.Y - PlaneState.LocalSizeCm.Y * 0.5f, PlaneState.LocalMinZ),
			FVector(PlaneState.LocalCenter.X + PlaneState.LocalSizeCm.X * 0.5f, PlaneState.LocalCenter.Y + PlaneState.LocalSizeCm.Y * 0.5f, PlaneState.LocalMaxZ));

		PlaneState.TargetLocalZ = ComputeSurfaceLocalZ(LocalBounds, Level);
		PlaneState.bTargetVisible = Level >= WaterVisibleThreshold;

		if (PlaneState.PlaneComponent && PlaneState.bTargetVisible)
		{
			PlaneState.PlaneComponent->SetVisibility(true, true);
		}

		bAnyPlaneNeedsTick |= PlaneState.bTargetVisible
			|| !FMath::IsNearlyEqual(PlaneState.CurrentLocalZ, PlaneState.TargetLocalZ, 0.5f);
	}

	SetComponentTickEnabled(bAnyPlaneNeedsTick);
}

void UFloodWaterVisualsComponent::InitializeWaterPlanesFromLayout()
{
	DestroyWaterPlanes();

	if (!SubHull || !SubHull->LayoutAsset || !GetOwner() || !GetOwner()->GetRootComponent())
	{
		return;
	}

	// LEGACY (Phase 7A, 2026-04-10) — Proto fallback. We reach this path only
	// when SubmarineBase has no GeneratedDefinition. Water plane geometry is
	// read from LayoutAsset. Will be removed in Phase 7B.
	UE_LOG(LogSubLegacy, Warning,
		TEXT("[LEGACY] UFloodWaterVisualsComponent: building water planes from LayoutAsset '%s' on %s. ")
		TEXT("This is a Proto03/04 fallback. Assign a GeneratedDefinition to use the generator path."),
		*SubHull->LayoutAsset->GetName(),
		*GetNameSafe(GetOwner()));

	const TArray<FSubCompartmentDef>& Compartments = SubHull->LayoutAsset->Compartments;
	WaterPlanes.Reserve(Compartments.Num());

	for (int32 PlaneIndex = 0; PlaneIndex < Compartments.Num(); ++PlaneIndex)
	{
		const FSubCompartmentDef& Comp = Compartments[PlaneIndex];
		FBox LocalBounds(ForceInitToZero);

		// Try hydro bounds from layout first.
		const FBox HydroBounds(Comp.HydroBoundsMin, Comp.HydroBoundsMax);
		if (HydroBounds.IsValid && HydroBounds.GetExtent().GetMin() > KINDA_SMALL_NUMBER)
		{
			LocalBounds = HydroBounds;
		}
		else if (!BuildCompartmentBoundsFromSheets(Comp.CompartmentId, LocalBounds))
		{
			continue;
		}

		FFloodWaterPlaneState PlaneState;
		PlaneState.CompartmentId = Comp.CompartmentId;
		PlaneState.LocalCenter = FVector(LocalBounds.GetCenter().X, LocalBounds.GetCenter().Y, 0.f);
		PlaneState.LocalSizeCm = FVector2D(
			FMath::Max(10.f, LocalBounds.GetSize().X + CompartmentBoundsPaddingCm * 2.f),
			FMath::Max(10.f, LocalBounds.GetSize().Y + CompartmentBoundsPaddingCm * 2.f));
		PlaneState.LocalMinZ = LocalBounds.Min.Z;
		PlaneState.LocalMaxZ = FMath::Max(LocalBounds.Min.Z + 1.f, LocalBounds.Max.Z);
		PlaneState.CurrentLocalZ = PlaneState.LocalMinZ;
		PlaneState.TargetLocalZ = PlaneState.LocalMinZ;
		PlaneState.bTargetVisible = false;
		PlaneState.PlaneComponent = CreatePlaneComponent(PlaneIndex, PlaneState);
		if (PlaneState.PlaneComponent)
		{
			UpdatePlaneVisual(PlaneState);
		}

		WaterPlanes.Add(PlaneState);
	}
}

void UFloodWaterVisualsComponent::InitializeWaterPlanesFromDefinition()
{
	DestroyWaterPlanes();

	if (!Definition || !GetOwner() || !GetOwner()->GetRootComponent())
	{
		return;
	}

	const TArray<FGeneratedCompartmentDef>& Compartments = Definition->Compartments;
	WaterPlanes.Reserve(Compartments.Num());

	for (int32 PlaneIndex = 0; PlaneIndex < Compartments.Num(); ++PlaneIndex)
	{
		const FGeneratedCompartmentDef& Comp = Compartments[PlaneIndex];

		const FBox LocalBounds(Comp.HydroBoundsMin, Comp.HydroBoundsMax);
		if (!LocalBounds.IsValid)
		{
			continue;
		}

		FFloodWaterPlaneState PlaneState;
		PlaneState.CompartmentId = Comp.CompartmentId;
		PlaneState.LocalCenter = FVector(LocalBounds.GetCenter().X, LocalBounds.GetCenter().Y, 0.f);
		PlaneState.LocalSizeCm = FVector2D(
			FMath::Max(10.f, LocalBounds.GetSize().X + CompartmentBoundsPaddingCm * 2.f),
			FMath::Max(10.f, LocalBounds.GetSize().Y + CompartmentBoundsPaddingCm * 2.f));
		PlaneState.LocalMinZ = LocalBounds.Min.Z;
		PlaneState.LocalMaxZ = FMath::Max(LocalBounds.Min.Z + 1.f, LocalBounds.Max.Z);
		PlaneState.CurrentLocalZ = PlaneState.LocalMinZ;
		PlaneState.TargetLocalZ = PlaneState.LocalMinZ;
		PlaneState.bTargetVisible = false;
		PlaneState.PlaneComponent = CreatePlaneComponent(PlaneIndex, PlaneState);
		if (PlaneState.PlaneComponent)
		{
			UpdatePlaneVisual(PlaneState);
		}

		WaterPlanes.Add(PlaneState);
	}
}

bool UFloodWaterVisualsComponent::BuildCompartmentBoundsFromSheets(FName CompartmentId, FBox& OutLocalBounds) const
{
	OutLocalBounds = FBox(EForceInit::ForceInit);
	if (!SubHull)
	{
		return false;
	}

	for (const FStructuralSheetDef& Sheet : SubHull->GetStructuralSheets())
	{
		if (Sheet.ParentCompartmentId != CompartmentId && Sheet.AdjacentCompartmentId != CompartmentId)
		{
			continue;
		}

		AppendSheetBounds(OutLocalBounds, Sheet);
	}

	return OutLocalBounds.IsValid != 0;
}

void UFloodWaterVisualsComponent::AppendSheetBounds(FBox& InOutBounds, const FStructuralSheetDef& Sheet)
{
	const FVector TangentX = Sheet.LocalTangentX.GetSafeNormal();
	const FVector TangentY = Sheet.LocalTangentY.GetSafeNormal();
	const FVector HalfExtentX = TangentX * (Sheet.SizeCm.X * 0.5f);
	const FVector HalfExtentY = TangentY * (Sheet.SizeCm.Y * 0.5f);

	InOutBounds += Sheet.LocalOrigin + HalfExtentX + HalfExtentY;
	InOutBounds += Sheet.LocalOrigin + HalfExtentX - HalfExtentY;
	InOutBounds += Sheet.LocalOrigin - HalfExtentX + HalfExtentY;
	InOutBounds += Sheet.LocalOrigin - HalfExtentX - HalfExtentY;
}

UStaticMeshComponent* UFloodWaterVisualsComponent::CreatePlaneComponent(int32 PlaneIndex, const FFloodWaterPlaneState& PlaneState)
{
	AActor* Owner = GetOwner();
	USceneComponent* AttachParent = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Owner || !AttachParent)
	{
		return nullptr;
	}

	const FName ComponentName = *FString::Printf(TEXT("FloodWater_%d"), PlaneIndex);
	UStaticMeshComponent* PlaneComponent = NewObject<UStaticMeshComponent>(Owner, ComponentName, RF_Transient);
	if (!PlaneComponent)
	{
		return nullptr;
	}

	Owner->AddInstanceComponent(PlaneComponent);
	PlaneComponent->SetupAttachment(AttachParent);
	PlaneComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaneComponent->SetGenerateOverlapEvents(false);
	PlaneComponent->SetCanEverAffectNavigation(false);
	PlaneComponent->SetCastShadow(false);
	PlaneComponent->SetVisibility(false, true);
	PlaneComponent->SetMobility(EComponentMobility::Movable);

	if (WaterPlaneMesh)
	{
		PlaneComponent->SetStaticMesh(WaterPlaneMesh);
	}

	if (WaterMaterial)
	{
		PlaneComponent->SetMaterial(0, WaterMaterial);
	}

	PlaneComponent->RegisterComponent();
	return PlaneComponent;
}

void UFloodWaterVisualsComponent::UpdatePlaneVisual(FFloodWaterPlaneState& PlaneState) const
{
	if (!PlaneState.PlaneComponent)
	{
		return;
	}

	const FVector MeshExtent = WaterPlaneMesh ? WaterPlaneMesh->GetBounds().BoxExtent : FVector(50.f, 50.f, 1.f);
	const float MeshSizeX = FMath::Max(1.f, MeshExtent.X * 2.f);
	const float MeshSizeY = FMath::Max(1.f, MeshExtent.Y * 2.f);

	PlaneState.PlaneComponent->SetRelativeLocation(FVector(PlaneState.LocalCenter.X, PlaneState.LocalCenter.Y, PlaneState.CurrentLocalZ));
	PlaneState.PlaneComponent->SetRelativeRotation(FRotator::ZeroRotator);
	PlaneState.PlaneComponent->SetRelativeScale3D(FVector(
		PlaneState.LocalSizeCm.X / MeshSizeX,
		PlaneState.LocalSizeCm.Y / MeshSizeY,
		1.f));
	if (WaterMaterial)
	{
		PlaneState.PlaneComponent->SetMaterial(0, WaterMaterial.Get());
	}
}

void UFloodWaterVisualsComponent::UpdateTickEnabled()
{
	bool bShouldTick = false;
	for (const FFloodWaterPlaneState& PlaneState : WaterPlanes)
	{
		if (PlaneState.bTargetVisible || !FMath::IsNearlyEqual(PlaneState.CurrentLocalZ, PlaneState.TargetLocalZ, 0.5f))
		{
			bShouldTick = true;
			break;
		}
	}

	SetComponentTickEnabled(bShouldTick);
}

void UFloodWaterVisualsComponent::DestroyWaterPlanes()
{
	for (FFloodWaterPlaneState& PlaneState : WaterPlanes)
	{
		if (IsValid(PlaneState.PlaneComponent))
		{
			PlaneState.PlaneComponent->DestroyComponent();
		}
	}

	WaterPlanes.Reset();
	SetComponentTickEnabled(false);
}
