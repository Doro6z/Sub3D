#include "FloodWaterVisualsComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "SubHullComponent.h"

UFloodWaterVisualsComponent::UFloodWaterVisualsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickEnabled(false);
}

void UFloodWaterVisualsComponent::BeginPlay()
{
	Super::BeginPlay();

	SubHull = GetOwner() ? GetOwner()->FindComponentByClass<USubHullComponent>() : nullptr;
	if (!SubHull)
	{
		return;
	}

	InitializeWaterPlanesFromHull();
	SubHull->OnCompartmentFloodUpdated.AddDynamic(this, &UFloodWaterVisualsComponent::HandleCompartmentFloodUpdated);
	RefreshFromCurrentFloodState();
}

void UFloodWaterVisualsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SubHull)
	{
		SubHull->OnCompartmentFloodUpdated.RemoveDynamic(this, &UFloodWaterVisualsComponent::HandleCompartmentFloodUpdated);
	}

	DestroyWaterPlanes();
	Super::EndPlay(EndPlayReason);
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
	if (!SubHull)
	{
		return;
	}

	if (WaterPlanes.Num() == 0)
	{
		InitializeWaterPlanesFromHull();
	}

	HandleCompartmentFloodUpdated(SubHull->GetCompartmentStates());
}

void UFloodWaterVisualsComponent::HandleCompartmentFloodUpdated(const TArray<FCompartmentRuntimeState>& InCompartmentStates)
{
	if (WaterPlanes.Num() == 0)
	{
		InitializeWaterPlanesFromHull();
	}

	bool bAnyPlaneNeedsTick = false;

	for (const FCompartmentRuntimeState& CompartmentState : InCompartmentStates)
	{
		FFloodWaterPlaneState* PlaneState = WaterPlanes.FindByPredicate([&](const FFloodWaterPlaneState& Candidate)
		{
			return Candidate.CompartmentId == CompartmentState.CompartmentId;
		});

		if (!PlaneState)
		{
			continue;
		}

		const FBox LocalBounds(
			FVector(PlaneState->LocalCenter.X - PlaneState->LocalSizeCm.X * 0.5f, PlaneState->LocalCenter.Y - PlaneState->LocalSizeCm.Y * 0.5f, PlaneState->LocalMinZ),
			FVector(PlaneState->LocalCenter.X + PlaneState->LocalSizeCm.X * 0.5f, PlaneState->LocalCenter.Y + PlaneState->LocalSizeCm.Y * 0.5f, PlaneState->LocalMaxZ));

		PlaneState->TargetLocalZ = ComputeSurfaceLocalZ(LocalBounds, CompartmentState.WaterLevelNormalized);
		PlaneState->bTargetVisible = CompartmentState.WaterLevelNormalized >= WaterVisibleThreshold;

		if (PlaneState->PlaneComponent && PlaneState->bTargetVisible)
		{
			PlaneState->PlaneComponent->SetVisibility(true, true);
		}

		bAnyPlaneNeedsTick |= PlaneState->bTargetVisible
			|| !FMath::IsNearlyEqual(PlaneState->CurrentLocalZ, PlaneState->TargetLocalZ, 0.5f);
	}

	SetComponentTickEnabled(bAnyPlaneNeedsTick);
}

void UFloodWaterVisualsComponent::InitializeWaterPlanesFromHull()
{
	DestroyWaterPlanes();

	if (!SubHull || !GetOwner() || !GetOwner()->GetRootComponent())
	{
		return;
	}

	const TArray<FCompartmentRuntimeState>& CompartmentStates = SubHull->GetCompartmentStates();
	WaterPlanes.Reserve(CompartmentStates.Num());

	for (int32 PlaneIndex = 0; PlaneIndex < CompartmentStates.Num(); ++PlaneIndex)
	{
		const FCompartmentRuntimeState& CompartmentState = CompartmentStates[PlaneIndex];
		FBox LocalBounds(ForceInitToZero);
		if (!BuildCompartmentBounds(CompartmentState.CompartmentId, LocalBounds))
		{
			continue;
		}

		FFloodWaterPlaneState PlaneState;
		PlaneState.CompartmentId = CompartmentState.CompartmentId;
		PlaneState.LocalCenter = FVector(LocalBounds.GetCenter().X, LocalBounds.GetCenter().Y, 0.f);
		PlaneState.LocalSizeCm = FVector2D(
			FMath::Max(10.f, LocalBounds.GetSize().X),
			FMath::Max(10.f, LocalBounds.GetSize().Y));
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

bool UFloodWaterVisualsComponent::BuildCompartmentBounds(FName CompartmentId, FBox& OutLocalBounds) const
{
	OutLocalBounds = FBox(EForceInit::ForceInit);
	if (!SubHull)
	{
		return false;
	}

	if (SubHull->GetCompartmentLocalBounds(CompartmentId, OutLocalBounds))
	{
		return true;
	}

	for (const FStructuralSheetDef& Sheet : SubHull->GetStructuralSheets())
	{
		if (Sheet.ParentCompartmentId != CompartmentId)
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
