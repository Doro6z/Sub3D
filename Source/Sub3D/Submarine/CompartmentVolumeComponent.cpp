#include "CompartmentVolumeComponent.h"

#include "Components/ActorComponent.h"
#include "Debug/Sub3DDebugSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SubFloodComponent.h"

UCompartmentVolumeComponent::UCompartmentVolumeComponent()
{
	// Probe role: QueryOnly + overlap on Pawn channel via "CompartmentProbe" profile
	// (DefaultEngine.ini). Crew capsule responds Overlap on ECC_CompartmentProbe, so
	// OnComponentBeginOverlap / OnComponentEndOverlap fire without interfering with
	// gameplay physics.
	SetCollisionProfileName(TEXT("CompartmentProbe"));
	SetGenerateOverlapEvents(true);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	SetVisibility(true);
	ShapeColor = FColor(50, 180, 220, 255);

	// Default box extent: 100cm each axis = 200x200x200cm volume.
	// Direct assignment instead of SetBoxExtent: the setter triggers
	// UBoxComponent::UpdateBodySetup, which calls NewObject<UBodySetup> with
	// no name. That is illegal inside a UObject constructor and crashes CDO
	// construction at editor launch.
	BoxExtent = FVector(100.f, 100.f, 100.f);
	LineThickness = 2.f;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UCompartmentVolumeComponent::OnRegister()
{
	Super::OnRegister();
	ShapeColor = VolumeColor;
}

void UCompartmentVolumeComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>();
	if (!Settings || !Settings->bDrawCompartmentVolumes)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Wireframe box of the compartment in world space, oriented with the sub.
	const FTransform WorldXform = GetComponentTransform();
	DrawDebugBox(
		World,
		WorldXform.GetLocation(),
		GetScaledBoxExtent(),
		WorldXform.GetRotation(),
		VolumeColor,
		false,
		-1.f,
		SDPG_World,
		2.f);

	// Label = compartment id + current water level (if flood initialized).
	const float Level01 = GetWaterLevel01();
	const float HeightCm = GetWaterHeightCm();
	const FString Label = FString::Printf(
		TEXT("%s  H=%.0fcm  L=%.2f"),
		*CompartmentId.ToString(),
		HeightCm,
		Level01);

	DrawDebugString(
		World,
		WorldXform.GetLocation() + FVector(0.f, 0.f, GetScaledBoxExtent().Z + 20.f),
		Label,
		nullptr,
		VolumeColor,
		0.f,
		true,
		1.f);
}

#if WITH_EDITOR
void UCompartmentVolumeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCompartmentVolumeComponent, VolumeColor))
	{
		ShapeColor = VolumeColor;
		MarkRenderStateDirty();
	}
}
#endif

// ── Flood data provider ──────────────────────────────────────────────────────

USubFloodComponent* UCompartmentVolumeComponent::GetFlood() const
{
	if (USubFloodComponent* Cached = CachedFlood.Get())
	{
		return Cached;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	USubFloodComponent* Found = Owner->FindComponentByClass<USubFloodComponent>();
	CachedFlood = Found;
	return Found;
}

float UCompartmentVolumeComponent::GetWaterHeightCm() const
{
	if (CompartmentId.IsNone())
	{
		return 0.f;
	}
	const USubFloodComponent* Flood = GetFlood();
	return Flood ? Flood->GetCompartmentWaterHeightCm(CompartmentId) : 0.f;
}

float UCompartmentVolumeComponent::GetWaterLevel01() const
{
	if (CompartmentId.IsNone())
	{
		return 0.f;
	}
	const USubFloodComponent* Flood = GetFlood();
	return Flood ? Flood->GetCompartmentFloodLevel01(CompartmentId) : 0.f;
}

float UCompartmentVolumeComponent::GetFloodBottomLocalZCm() const
{
	const AActor* Owner = GetOwner();
	const FTransform ActorTransform = Owner ? Owner->GetActorTransform() : FTransform::Identity;
	const FVector VolumeLocalCenter = ActorTransform.InverseTransformPosition(GetComponentLocation());
	const float BoxBottomLocalZ = VolumeLocalCenter.Z - GetScaledBoxExtent().Z;

	if (!FMath::IsNearlyZero(WalkableFloorZCmOverride))
	{
		return WalkableFloorZCmOverride;
	}

	return BoxBottomLocalZ;
}

float UCompartmentVolumeComponent::GetFloodMaxHeightCm() const
{
	const AActor* Owner = GetOwner();
	const FTransform ActorTransform = Owner ? Owner->GetActorTransform() : FTransform::Identity;
	const FVector VolumeLocalCenter = ActorTransform.InverseTransformPosition(GetComponentLocation());
	const float BoxTopLocalZ = VolumeLocalCenter.Z + GetScaledBoxExtent().Z;
	return FMath::Max(1.f, BoxTopLocalZ - GetFloodBottomLocalZCm());
}

FVector UCompartmentVolumeComponent::GetWaterSurfaceWorldLocation() const
{
	const AActor* Owner = GetOwner();
	const FTransform ActorTransform = Owner ? Owner->GetActorTransform() : FTransform::Identity;
	const FVector WorldUp = Owner ? Owner->GetActorUpVector() : FVector::UpVector;
	const FVector VolumeWorldCenter = GetComponentLocation();
	const FVector VolumeLocalCenter = ActorTransform.InverseTransformPosition(VolumeWorldCenter);

	// The manual Craniata path often places compartment boxes below the visible deck.
	// Use the authored walkable floor when available so the visual water starts where
	// the player expects instead of under the floor mesh.
	const FVector FloodFloorLocal(VolumeLocalCenter.X, VolumeLocalCenter.Y, GetFloodBottomLocalZCm());
	const FVector WorldBottom = ActorTransform.TransformPosition(FloodFloorLocal);

	// Raise by the water height along the sub's up axis (follows sub pitch/roll).
	const float HeightCm = GetWaterHeightCm();
	return WorldBottom + WorldUp * HeightCm;
}
