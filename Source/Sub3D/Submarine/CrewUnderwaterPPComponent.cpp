#include "CrewUnderwaterPPComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/PostProcessComponent.h"
#include "CompartmentVolumeComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SubCrewCharacter.h"
#include "SubCrewMovementComponent.h"

UCrewUnderwaterPPComponent::UCrewUnderwaterPPComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UCrewUnderwaterPPComponent::BeginPlay()
{
	Super::BeginPlay();
	CachedCrew = Cast<ASubCrewCharacter>(GetOwner());
	EnsurePostProcessComponent();
}

void UCrewUnderwaterPPComponent::EnsurePostProcessComponent()
{
	if (PostProcessComp)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	PostProcessComp = NewObject<UPostProcessComponent>(Owner, TEXT("UnderwaterPostProcess"));
	if (!PostProcessComp)
	{
		return;
	}

	PostProcessComp->bUnbound = true;            // applies globally when owning camera renders
	PostProcessComp->BlendWeight = 0.f;
	PostProcessComp->bEnabled = true;
	PostProcessComp->Priority = 5.f;              // above default ambient PPVs

	// Attach under the owner's root for actor-lifetime management.
	if (USceneComponent* Root = Owner->GetRootComponent())
	{
		PostProcessComp->SetupAttachment(Root);
	}
	PostProcessComp->RegisterComponent();

	if (UnderwaterPostProcessMaterial)
	{
		PostProcessMID = UMaterialInstanceDynamic::Create(UnderwaterPostProcessMaterial, this);
		if (PostProcessMID)
		{
			PostProcessComp->AddOrUpdateBlendable(PostProcessMID, 1.f);
		}
	}
}

void UCrewUnderwaterPPComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ASubCrewCharacter* Crew = CachedCrew.Get();
	if (!Crew || !PostProcessComp)
	{
		return;
	}

	// Local effect — only meaningful on the owning/autonomous client.
	if (!Crew->IsLocallyControlled())
	{
		PostProcessComp->BlendWeight = 0.f;
		return;
	}

	// ── Resolve camera Z vs water surface Z ─────────────────────────────────
	const UCameraComponent* Camera = Crew->GetActiveViewCamera();
	if (!Camera)
	{
		return;
	}

	const float CameraZ = Camera->GetComponentLocation().Z;
	const float SurfaceZ = ResolveWaterSurfaceZ();

	LastDistanceCm = CameraZ - SurfaceZ;  // negative = underwater
	bIsUnderwater = LastDistanceCm < 0.f;

	// ── Blend alpha toward target ───────────────────────────────────────────
	const float Target = bIsUnderwater ? 1.f : 0.f;
	const float Speed = bIsUnderwater ? BlendInSpeed : BlendOutSpeed;
	CurrentBlendAlpha = FMath::FInterpTo(CurrentBlendAlpha, Target, DeltaTime, Speed);
	PostProcessComp->BlendWeight = CurrentBlendAlpha;

	// Optional material param feed-through for designer's effects.
	if (PostProcessMID)
	{
		PostProcessMID->SetScalarParameterValue(TEXT("UnderwaterAlpha"), CurrentBlendAlpha);
		PostProcessMID->SetScalarParameterValue(TEXT("DistanceToSurfaceCm"), LastDistanceCm);
	}

	// ── BP events ───────────────────────────────────────────────────────────
	if (bIsUnderwater && !bWasUnderwaterLastTick)
	{
		BP_OnEnterWater();
	}
	else if (!bIsUnderwater && bWasUnderwaterLastTick)
	{
		BP_OnExitWater();
	}
	bWasUnderwaterLastTick = bIsUnderwater;

	// Waterline proximity: fire continuously while camera is close to surface.
	if (FMath::Abs(LastDistanceCm) < WaterlineCrossThresholdCm)
	{
		BP_OnWaterlineProximity(LastDistanceCm);
	}
}

float UCrewUnderwaterPPComponent::ResolveWaterSurfaceZ() const
{
	const ASubCrewCharacter* Crew = CachedCrew.Get();
	if (!Crew)
	{
		return OceanSurfaceZ;
	}

	// EVA: always underwater (stub ocean) — surface is the global ocean Z.
	if (const USubCrewMovementComponent* CrewMov = Crew->GetCrewMovement())
	{
		if (CrewMov->EmbarkState == ECrewEmbarkState::Outside)
		{
			return OceanSurfaceZ;
		}
	}

	// Interior: surface of current compartment.
	if (const UCompartmentVolumeComponent* Vol = Crew->CurrentCompartment.Get())
	{
		return Vol->GetWaterSurfaceWorldLocation().Z;
	}

	// No compartment (e.g. ghost state) — default to ocean.
	return OceanSurfaceZ;
}
