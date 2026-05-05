#include "FloodWaterPlaneComponent.h"

#include "CompartmentVolumeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/Sub3DDebugSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogFloodWaterPlane, Log, All);

UFloodWaterPlaneComponent::UFloodWaterPlaneComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Default to engine's basic plane mesh so designers don't need to set anything to get a visible plane.
	struct FPlaneFinder
	{
		ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Asset;
		FPlaneFinder() : Asset(TEXT("/Engine/BasicShapes/Plane.Plane")) {}
	};
	static FPlaneFinder PlaneFinder;
	if (PlaneFinder.Asset.Succeeded())
	{
		PlaneMesh = PlaneFinder.Asset.Get();
	}
}

void UFloodWaterPlaneComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsurePlaneMesh();
	RefreshFromFlood();
}

void UFloodWaterPlaneComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshFromFlood();
}

void UFloodWaterPlaneComponent::RefreshFromFlood()
{
	const UCompartmentVolumeComponent* Source = SourceVolume.Get();
	if (!Source || !PlaneMeshComponent)
	{
		return;
	}

	const float Level01 = Source->GetWaterLevel01();
	const float HeightCm = Source->GetWaterHeightCm();
	ApplyWaterState(Level01, HeightCm);
}

void UFloodWaterPlaneComponent::EnsurePlaneMesh()
{
	if (PlaneMeshComponent)
	{
		return;
	}

	PlaneMeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("WaterPlaneMesh"));
	PlaneMeshComponent->SetupAttachment(this);
	PlaneMeshComponent->RegisterComponent();

	// Purely visual — no collision, no shadow casting (water surface rendered via material).
	PlaneMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaneMeshComponent->SetGenerateOverlapEvents(false);
	PlaneMeshComponent->SetCastShadow(false);
	PlaneMeshComponent->SetReceivesDecals(false);
	PlaneMeshComponent->bRenderCustomDepth = false;

	// Mesh selection priority:
	//   1. SourceVolume->WaterPlaneMeshOverride — per-compartment authored water cap. Its
	//      geometry matches the compartment's horizontal cross-section at deck level and
	//      IS the spatial containment. Use it at scale (1,1,1).
	//   2. PlaneMesh (engine BasicShapes/Plane by default) — generic fallback scaled to
	//      PlaneWorldSizeCm. Will visibly overflow past the hull; intended only for
	//      bringup before per-compartment caps are authored.
	UStaticMesh* SelectedMesh = PlaneMesh;
	bool bUsingAuthoredCap = false;
	if (const UCompartmentVolumeComponent* Source = SourceVolume.Get())
	{
		if (Source->WaterPlaneMeshOverride)
		{
			SelectedMesh = Source->WaterPlaneMeshOverride;
			bUsingAuthoredCap = true;
		}
	}

	if (SelectedMesh)
	{
		PlaneMeshComponent->SetStaticMesh(SelectedMesh);
	}

	if (bUsingAuthoredCap)
	{
		// Authored cap mesh — trust the authored dimensions, no scaling.
		PlaneMeshComponent->SetWorldScale3D(FVector::OneVector);
	}
	else
	{
		// Generic engine plane (100x100 cm). Size to the compartment's XY footprint so the
		// plane is spatially contained by geometry, not by shader ray-march.
		// Falls back to PlaneWorldSizeCm only when SourceVolume is not yet assigned.
		if (const UCompartmentVolumeComponent* Source = SourceVolume.Get())
		{
			const FVector Extent = Source->GetScaledBoxExtent(); // half-extents
			const float ScaleX = (Extent.X * 2.f) / 100.f;
			const float ScaleY = (Extent.Y * 2.f) / 100.f;
			PlaneMeshComponent->SetWorldScale3D(FVector(ScaleX, ScaleY, 1.f));
		}
		else
		{
			const float ScaleXY = PlaneWorldSizeCm / 100.f;
			PlaneMeshComponent->SetWorldScale3D(FVector(ScaleXY, ScaleXY, 1.f));
		}
	}

	if (WaterMaterial)
	{
		MaterialMID = PlaneMeshComponent->CreateDynamicMaterialInstance(0, WaterMaterial);
	}
	else
	{
		const UCompartmentVolumeComponent* Source = SourceVolume.Get();
		UE_LOG(
			LogFloodWaterPlane,
			Warning,
			TEXT("Flood water plane on %s (compartment=%s) has no WaterMaterial. Assign ASubmarineBase.DefaultWaterMaterial on the BP."),
			*GetNameSafe(GetOwner()),
			Source ? *Source->CompartmentId.ToString() : TEXT("<none>"));
	}

	// Plane starts hidden. ApplyWaterState will toggle based on actual flood level.
	PlaneMeshComponent->SetVisibility(false, true);
	bLastVisible = false;
}

void UFloodWaterPlaneComponent::ApplyWaterState(float NewLevel01, float NewHeightCm)
{
	if (!PlaneMeshComponent || !SourceVolume.IsValid())
	{
		return;
	}

	// ── Position ────────────────────────────────────────────────────────────
	const FVector SurfaceWorld = SourceVolume->GetWaterSurfaceWorldLocation();
	// Keep X/Y aligned with the volume's world X/Y (inherits sub motion); override Z with surface height.
	const FVector VolumeXY = SourceVolume->GetComponentLocation();
	const FVector PlaneWorldLoc(VolumeXY.X, VolumeXY.Y, SurfaceWorld.Z);
	PlaneMeshComponent->SetWorldLocation(PlaneWorldLoc);

	// Plane stays horizontal (Z=world up) but follows the compartment's yaw so the scaled
	// XY footprint stays aligned with the sub's local XY axes as the sub turns.
	// Pitch and roll remain zero — water surface is always inertially horizontal for FP.
	const float SubYaw = SourceVolume->GetComponentRotation().Yaw;
	PlaneMeshComponent->SetWorldRotation(FRotator(0.f, SubYaw, 0.f));

	// ── Visibility ──────────────────────────────────────────────────────────
	const bool bShouldBeVisible = NewLevel01 > VisibilityThreshold01;
	if (bShouldBeVisible != bLastVisible)
	{
		PlaneMeshComponent->SetVisibility(bShouldBeVisible, true);
		bLastVisible = bShouldBeVisible;
		BP_OnVisibilityChanged(bShouldBeVisible);
	}

	// ── Material parameters (if MID available) ──────────────────────────────
	// Drive the per-compartment MID with everything the material needs for:
	//  - sim-driven look  (WaterLevel01, WaterHeightCm)
	//  - AABB containment (OBB in CV local space — pure geometry, no ray-march):
	//      CV_Center_WS       : world-space location of the CompartmentVolume center
	//      CV_HalfExtent      : CV's scaled half-extents (sub-local axis-aligned box)
	//      CV_W2L_Row0/1/2    : CV world-to-local transform (4x3) as 3 float4 rows
	// The plane mesh is already sized to the compartment footprint; the AABB shader check
	// is a secondary safety clip for edge pixels and sub tilt.
	// No Global Distance Field dependency — containment is structural, not inferred.
	if (MaterialMID)
	{
		MaterialMID->SetScalarParameterValue(TEXT("WaterLevel01"), NewLevel01);
		MaterialMID->SetScalarParameterValue(TEXT("WaterHeightCm"), NewHeightCm);

		const FTransform CVTransform = SourceVolume->GetComponentTransform();
		const FTransform CVInv = CVTransform.Inverse();
		const FMatrix W2L = CVInv.ToMatrixWithScale();

		MaterialMID->SetVectorParameterValue(TEXT("CV_Center_WS"), CVTransform.GetLocation());
		MaterialMID->SetVectorParameterValue(TEXT("CV_HalfExtent"), SourceVolume->GetScaledBoxExtent());

		// Native rows of the W2L matrix. Shader pattern (pre-subtracts CVCenter so no translation
		// needed here, only the rotation+scale block):
		//   Delta = WorldPos - CVCenter
		//   P_local.x = Delta.x*Row0.x + Delta.y*Row1.x + Delta.z*Row2.x
		// i.e. Row_i.xyz is the i-th native row of W2L (W2L.M[i][0..2]). .w is the translation
		// column which the shader currently ignores (Delta-based translation handling).
		auto MakeRow = [&](int32 Row) -> FLinearColor
		{
			return FLinearColor(
				static_cast<float>(W2L.M[Row][0]),
				static_cast<float>(W2L.M[Row][1]),
				static_cast<float>(W2L.M[Row][2]),
				static_cast<float>(W2L.M[Row][3]));
		};
		MaterialMID->SetVectorParameterValue(TEXT("CV_W2L_Row0"), MakeRow(0));
		MaterialMID->SetVectorParameterValue(TEXT("CV_W2L_Row1"), MakeRow(1));
		MaterialMID->SetVectorParameterValue(TEXT("CV_W2L_Row2"), MakeRow(2));
		MaterialMID->SetVectorParameterValue(TEXT("CV_W2L_Row3"), MakeRow(2)); // material param name mismatch guard
	}

	// ── Debug draw (independent of material) ────────────────────────────────
	if (const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>())
	{
		if (Settings->bDrawCompartmentWater && SourceVolume.IsValid())
		{
			UWorld* World = GetWorld();
			if (World && NewLevel01 > VisibilityThreshold01)
			{
				// Flat wireframe slab at the water surface, covering the compartment XY extent.
				const FVector Extent = SourceVolume->GetScaledBoxExtent();
				const FVector Center = FVector(VolumeXY.X, VolumeXY.Y, SurfaceWorld.Z);
				const FVector SlabExtent(Extent.X, Extent.Y, 1.f);
				const FColor SlabColor = FColor::Cyan;
				DrawDebugBox(World, Center, SlabExtent, SlabColor, false, -1.f, SDPG_World, 3.f);

				const FString Label = FString::Printf(
					TEXT("[water] %s  H=%.0fcm  L=%.2f"),
					*SourceVolume->CompartmentId.ToString(),
					NewHeightCm,
					NewLevel01);
				DrawDebugString(World, Center + FVector(0.f, 0.f, 30.f), Label, nullptr, SlabColor, 0.f, true, 1.f);
			}
		}
	}

	// ── Level change event ──────────────────────────────────────────────────
	if (FMath::Abs(NewLevel01 - LastLevel01) > LevelChangeEventThreshold01)
	{
		LastLevel01 = NewLevel01;
		BP_OnWaterLevelChanged(NewLevel01, NewHeightCm);
	}
}
