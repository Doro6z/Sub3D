#include "FloodWaterPlaneComponent.h"

#include "CompartmentVolumeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/Sub3DDebugSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Generator/SubmarineDefinition.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"
#include "SubFloodComponent.h"
#include "SubmarineBase.h"
#include "SubMovementComponent.h"
#include "Types/CompartmentWaterBake.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogFloodWaterPlane, Log, All);

namespace
{
/**
 * 1→4 mesh subdivision: each input triangle is split into 4 by inserting a vertex at each edge
 * midpoint. Edge midpoints are SHARED between adjacent triangles via a hash map keyed on the
 * sorted (lo, hi) vertex index pair — so the result has no cracks and stays topologically sound.
 *
 * UVs and normals are linearly interpolated for the new midpoint vertices (and re-normalized for
 * normals). Repeat call to densify further: 1 level = 4× tris, 2 levels = 16×, 3 levels = 64×.
 *
 * Used to repair the bake's fan-triangulated cap mesh, which has one center vertex shared by all
 * triangles. With WPO heightfield deformation, that center forms a visible spike; subdividing
 * adds interior points that sample the heightfield independently and smooth the surface.
 */
void SubdivideMesh1to4(TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FVector2D>& UV0)
{
	const int32 OldVertCount = Vertices.Num();
	const int32 OldTriCount = Triangles.Num() / 3;
	if (OldTriCount == 0) return;

	const bool bHasNormals = (Normals.Num() == OldVertCount);
	const bool bHasUVs = (UV0.Num() == OldVertCount);

	// Reserve approx capacity (vertex count grows by ~OldTriCount * 1.5; tri count quadruples).
	Vertices.Reserve(OldVertCount + OldTriCount * 2);
	if (bHasNormals) Normals.Reserve(OldVertCount + OldTriCount * 2);
	if (bHasUVs) UV0.Reserve(OldVertCount + OldTriCount * 2);

	// Edge → midpoint vertex index. Key is sorted (lo, hi) so lookup is undirected.
	TMap<TPair<int32, int32>, int32> MidpointCache;
	MidpointCache.Reserve(OldTriCount * 3);

	auto GetOrCreateMid = [&](int32 A, int32 B) -> int32
	{
		const TPair<int32, int32> Key(FMath::Min(A, B), FMath::Max(A, B));
		if (const int32* Found = MidpointCache.Find(Key))
		{
			return *Found;
		}
		const int32 NewIdx = Vertices.Add((Vertices[A] + Vertices[B]) * 0.5f);
		if (bHasNormals)
		{
			const FVector NMid = ((Normals[A] + Normals[B]) * 0.5f).GetSafeNormal();
			Normals.Add(NMid.IsNearlyZero() ? FVector::UpVector : NMid);
		}
		if (bHasUVs)
		{
			UV0.Add((UV0[A] + UV0[B]) * 0.5f);
		}
		MidpointCache.Add(Key, NewIdx);
		return NewIdx;
	};

	TArray<int32> NewTriangles;
	NewTriangles.Reserve(OldTriCount * 12); // 4 tris × 3 indices

	for (int32 t = 0; t < OldTriCount; ++t)
	{
		const int32 V0 = Triangles[t * 3 + 0];
		const int32 V1 = Triangles[t * 3 + 1];
		const int32 V2 = Triangles[t * 3 + 2];
		const int32 M01 = GetOrCreateMid(V0, V1);
		const int32 M12 = GetOrCreateMid(V1, V2);
		const int32 M20 = GetOrCreateMid(V2, V0);
		NewTriangles.Append({V0, M01, M20});
		NewTriangles.Append({M01, V1, M12});
		NewTriangles.Append({M20, M12, V2});
		NewTriangles.Append({M01, M12, M20});
	}

	Triangles = MoveTemp(NewTriangles);
}

/** Reverse triangle winding in-place: (V0, V1, V2) → (V0, V2, V1). Flips visible face direction. */
void FlipTriangleWinding(TArray<int32>& Triangles)
{
	const int32 NumTris = Triangles.Num() / 3;
	for (int32 t = 0; t < NumTris; ++t)
	{
		Swap(Triangles[t * 3 + 1], Triangles[t * 3 + 2]);
	}
}
} // namespace

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

	// Tick prereq: must run AFTER the sub's movement tick so we read its CURRENT-frame
	// position when computing cap mesh world transform. Without this, the cap can lag
	// one frame behind the sub when the sub is moving — visible as wobble/jitter.
	if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
	{
		if (Sub->SubMovement)
		{
			AddTickPrerequisiteComponent(Sub->SubMovement);
		}
	}

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

	// Try the bake path first (P2.6). If a UCompartmentWaterBake exists for this compartment in
	// the owning sub's DA, drive a UProceduralMeshComponent from the bake's slice closest to the
	// current water surface. The legacy SMC plane is hidden when the bake is active.
	const bool bBakeActive = RefreshBakeCapMesh(HeightCm);
	if (bBakeActive)
	{
		// Hide the legacy plane (the bake renders the cap instead).
		if (PlaneMeshComponent->IsVisible())
		{
			PlaneMeshComponent->SetVisibility(false, true);
		}
		// Visibility on the bake PMC is driven by Level01 threshold.
		const bool bShouldBeVisible = Level01 > VisibilityThreshold01;
		if (BakeCapMeshComp && bShouldBeVisible != bLastVisible)
		{
			BakeCapMeshComp->SetVisibility(bShouldBeVisible, true);
			bLastVisible = bShouldBeVisible;
			BP_OnVisibilityChanged(bShouldBeVisible);
		}
		// Drive the bake MID with the same params (the material can read Level01 / HeightCm).
		if (BakeCapMID)
		{
			BakeCapMID->SetScalarParameterValue(TEXT("WaterLevel01"), Level01);
			BakeCapMID->SetScalarParameterValue(TEXT("WaterHeightCm"), HeightCm);
		}

		// ── Heightfield (P3.4) ────────────────────────────────────────────────
		// Active only on the bake path: requires CachedBake's LocalBounds for grid mapping
		// and the BakeCapMID for parameter binding.
		if (bEnableHeightfield)
		{
			EnsureHeightfieldInitialized();
			if (bHeightfieldInitialized)
			{
				const float World = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
				HeightfieldAccum += World;
				const float StepDt = 1.f / FMath::Max(1.f, HeightfieldUpdateHz);
				int32 StepsThisFrame = 0;
				constexpr int32 MaxStepsPerFrame = 4; // safety cap on hitches
				while (HeightfieldAccum >= StepDt && StepsThisFrame < MaxStepsPerFrame)
				{
					TickHeightfieldStep();
					HeightfieldAccum -= StepDt;
					++StepsThisFrame;
				}
				// If we still have accumulated dt past the cap (huge hitch), drop it to avoid
				// spiral of death — the field absorbs the loss as residual damping.
				if (StepsThisFrame == MaxStepsPerFrame)
				{
					HeightfieldAccum = 0.f;
				}
				if (StepsThisFrame > 0)
				{
					PushHeightfieldToTexture();
				}
				if (BakeCapMID)
				{
					BakeCapMID->SetScalarParameterValue(HeightfieldAmplitudeParamName, HeightfieldAmplitudeCm);
				}
			}
		}

		// ── Breach reaction (P3.5) ────────────────────────────────────────────
		// One-shot inject + Niagara on first detection of a replicated breach for this
		// compartment, plus optional recurring inject pulses while it stays active.
		RefreshBreachReaction();

		// ── Slosh modal (P3.6) ────────────────────────────────────────────────
		// Spring-damper integrating the sub's velocity delta as an impulse on tilt+offset.
		// Re-applies cap mesh transform with the current slosh state on top of the base water Z.
		if (bEnableSlosh)
		{
			const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
			UpdateSloshModal(Dt);
			ApplyCapMeshTransformWithSlosh();
		}

		// Level change event.
		if (FMath::Abs(Level01 - LastLevel01) > LevelChangeEventThreshold01)
		{
			LastLevel01 = Level01;
			BP_OnWaterLevelChanged(Level01, HeightCm);
		}
		return;
	}

	// Fallback: legacy flat plane.
	ApplyWaterState(Level01, HeightCm);
}

UCompartmentWaterBake* UFloodWaterPlaneComponent::ResolveBake()
{
	if (CachedBake) return CachedBake;
	if (bBakeResolveAttempted) return nullptr; // resolved to nullptr previously, don't retry every tick

	bBakeResolveAttempted = true;

	const UCompartmentVolumeComponent* Source = SourceVolume.Get();
	if (!Source || Source->CompartmentId.IsNone()) return nullptr;

	const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner());
	if (!Sub || !Sub->GeneratedDefinition) return nullptr;

	if (TObjectPtr<UCompartmentWaterBake>* Found = Sub->GeneratedDefinition->WaterBakes.Find(Source->CompartmentId))
	{
		CachedBake = *Found;
	}
	return CachedBake;
}

bool UFloodWaterPlaneComponent::RefreshBakeCapMesh(float WaterHeightLocalCm)
{
	UCompartmentWaterBake* Bake = ResolveBake();
	if (!Bake || Bake->Slices.Num() == 0 || Bake->CapMeshesPerSlice.Num() == 0)
	{
		return false;
	}

	// Lazily create the PMC attached to the SUBMARINE ROOT (NOT to `this`). The bake stores cap
	// mesh vertices in submarine-local space, so attaching to `this` (which is itself attached to
	// UCompartmentVolumeComponent at its sub-local RelativeLocation) would double-offset the
	// vertices by the volume center.
	if (!BakeCapMeshComp)
	{
		USceneComponent* SubRoot = nullptr;
		if (AActor* Owner = GetOwner())
		{
			SubRoot = Owner->GetRootComponent();
		}
		BakeCapMeshComp = NewObject<UProceduralMeshComponent>(GetOwner(),
			FName(*FString::Printf(TEXT("BakeCapMesh_%s"),
				SourceVolume.IsValid() ? *SourceVolume->CompartmentId.ToString() : TEXT("?"))));
		if (SubRoot)
		{
			BakeCapMeshComp->SetupAttachment(SubRoot);
		}
		BakeCapMeshComp->RegisterComponent();
		BakeCapMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BakeCapMeshComp->SetGenerateOverlapEvents(false);
		BakeCapMeshComp->SetCastShadow(false);
		BakeCapMeshComp->SetReceivesDecals(false);
		BakeCapMeshComp->bUseAsyncCooking = false;
		BakeCapMeshComp->SetVisibility(false, true);

		if (WaterMaterial)
		{
			BakeCapMID = BakeCapMeshComp->CreateDynamicMaterialInstance(0, WaterMaterial);
		}
	}

	// Map water level 0..1 to the FIRST/LAST VALID slice's Z range — not the volume's Z range.
	// The user's authored compartment volume is often oversized in Z (extends above the actual
	// ceiling and/or below the actual floor) so that the bake captures generous margins. Using
	// the volume Z as the water-level reference makes L=1.0 sit ABOVE the actual ceiling (the
	// compartment looks empty even though sim is full).
	//
	// The valid-slice range IS the actual room: slices outside the room have empty cap meshes
	// (rejected as degenerate). First valid Z ≈ floor, last valid Z ≈ ceiling. Lerp by L01 maps
	// the visual water surface to the actual room's interior regardless of volume oversize.
	int32 FirstValidIdx = INDEX_NONE;
	int32 LastValidIdx = INDEX_NONE;
	for (int32 i = 0; i < Bake->Slices.Num() && i < Bake->CapMeshesPerSlice.Num(); ++i)
	{
		if (Bake->CapMeshesPerSlice[i].Vertices.Num() >= 3)
		{
			if (FirstValidIdx == INDEX_NONE) FirstValidIdx = i;
			LastValidIdx = i;
		}
	}
	if (FirstValidIdx == INDEX_NONE)
	{
		// No valid cap meshes anywhere — bake produced nothing usable for this compartment.
		return false;
	}

	const float Level01 = SourceVolume.IsValid()
		? FMath::Clamp(SourceVolume->GetWaterLevel01(), 0.f, 1.f)
		: 0.f;
	const float SliceZFirst = Bake->Slices[FirstValidIdx].SliceZ_Local;
	const float SliceZLast = Bake->Slices[LastValidIdx].SliceZ_Local;
	const float WaterZLocal = FMath::Lerp(SliceZFirst, SliceZLast, Level01);

	(void)WaterHeightLocalCm; // No longer used for slice picking; kept on the API for callers.

	int32 BestIdx = FirstValidIdx;
	float BestDiff = TNumericLimits<float>::Max();
	for (int32 i = FirstValidIdx; i <= LastValidIdx; ++i)
	{
		if (Bake->CapMeshesPerSlice[i].Vertices.Num() < 3) continue;
		const float Diff = FMath::Abs(Bake->Slices[i].SliceZ_Local - WaterZLocal);
		if (Diff < BestDiff)
		{
			BestDiff = Diff;
			BestIdx = i;
		}
	}

	const bool bValidIndex = Bake->CapMeshesPerSlice.IsValidIndex(BestIdx);
	const FCachedBakeMesh* CapMesh = bValidIndex ? &Bake->CapMeshesPerSlice[BestIdx] : nullptr;
	const bool bValidMesh = CapMesh && CapMesh->Vertices.Num() >= 3 && CapMesh->Triangles.Num() >= 3;

	if (BestIdx != LastSliceIndex)
	{
		if (!bValidMesh)
		{
			// Slice has no usable cap (typical for the topmost/bottom-most slice if it's outside
			// the actual room). Clear any existing mesh section so we don't render stale geometry
			// from a previous slice at a misleading Z height.
			if (BakeCapMeshComp->GetNumSections() > 0)
			{
				BakeCapMeshComp->ClearAllMeshSections();
			}
		}
		else
		{
			// Copy bake-source arrays so subdivision + winding flip don't mutate the asset.
			TArray<FVector> WorkVerts = CapMesh->Vertices;
			TArray<int32> WorkTris = CapMesh->Triangles;
			TArray<FVector> WorkNormals = CapMesh->Normals;
			TArray<FVector2D> WorkUV0 = CapMesh->UV0;

			// Repair bake fan-topology: subdivide so heightfield WPO has interior sample points
			// instead of one shared center vertex producing a spike under wave displacement.
			const int32 SubLevels = FMath::Clamp(CapMeshSubdivisionLevels, 0, 4);
			for (int32 Lvl = 0; Lvl < SubLevels; ++Lvl)
			{
				SubdivideMesh1to4(WorkVerts, WorkTris, WorkNormals, WorkUV0);
			}

			// Optional winding flip: bake's fan triangulation may produce a downward-facing mesh
			// depending on the contour winding. EditAnywhere bool lets the artist correct without
			// re-baking. Default true based on observed Craniata bake.
			if (bFlipCapMeshWinding)
			{
				FlipTriangleWinding(WorkTris);
			}

			TArray<FVector2D> EmptyUV1;
			TArray<FColor> EmptyColors;
			TArray<FProcMeshTangent> EmptyTangents;
			BakeCapMeshComp->CreateMeshSection(0,
				WorkVerts, WorkTris, WorkNormals,
				WorkUV0, EmptyColors, EmptyTangents, /*bCreateCollision*/ false);
			if (BakeCapMID)
			{
				BakeCapMeshComp->SetMaterial(0, BakeCapMID);
			}

			UE_LOG(LogFloodWaterPlane, Display,
				TEXT("Cap mesh built | Comp=%s | SubLevels=%d | Verts=%d | Tris=%d | Flipped=%s"),
				SourceVolume.IsValid() ? *SourceVolume->CompartmentId.ToString() : TEXT("?"),
				SubLevels, WorkVerts.Num(), WorkTris.Num() / 3,
				bFlipCapMeshWinding ? TEXT("yes") : TEXT("no"));
		}
		LastSliceIndex = BestIdx;
	}

	// Cache the base water Z (sub-local). Final transform incl. slosh offset/tilt is applied
	// later via ApplyCapMeshTransformWithSlosh — called from RefreshFromFlood after slosh tick.
	CurrentBaseWaterZLocal = WaterZLocal;
	ApplyCapMeshTransformWithSlosh();
	return true;
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

// ─────────────────────────────────────────────────────────────────────────────
//  Heightfield CPU 2D (P3.4 — port from Sub3DWaterProto::URoomWaterRenderer)
// ─────────────────────────────────────────────────────────────────────────────

void UFloodWaterPlaneComponent::EnsureHeightfieldInitialized()
{
	if (bHeightfieldInitialized)
	{
		return;
	}
	if (!CachedBake || !BakeCapMID)
	{
		return; // bake not resolved yet, or MID not created — RefreshBakeCapMesh will trigger us next tick
	}

	const int32 W = FMath::Max(16, HeightfieldGridX);
	const int32 H = FMath::Max(16, HeightfieldGridY);
	const int32 Total = W * H;

	Heights.SetNumZeroed(Total);
	Velocities.SetNumZeroed(Total);

	HeightfieldTex = UTexture2D::CreateTransient(W, H, PF_R32_FLOAT);
	if (HeightfieldTex)
	{
		HeightfieldTex->Filter = TF_Bilinear;
		HeightfieldTex->AddressX = TA_Clamp;
		HeightfieldTex->AddressY = TA_Clamp;
		HeightfieldTex->UpdateResource();

		BakeCapMID->SetTextureParameterValue(HeightfieldTextureParamName, HeightfieldTex);
		BakeCapMID->SetVectorParameterValue(LocalBoundsMinParamName, FLinearColor(CachedBake->LocalBoundsMin));
		BakeCapMID->SetVectorParameterValue(LocalBoundsMaxParamName, FLinearColor(CachedBake->LocalBoundsMax));
		BakeCapMID->SetScalarParameterValue(HeightfieldAmplitudeParamName, HeightfieldAmplitudeCm);

		UE_LOG(LogFloodWaterPlane, Display,
			TEXT("Heightfield material params bound | Comp=%s | TexParam=%s | AmplitudeParam=%s | BoundsMin=%s/%s BoundsMax=%s/%s"),
			SourceVolume.IsValid() ? *SourceVolume->CompartmentId.ToString() : TEXT("?"),
			*HeightfieldTextureParamName.ToString(),
			*HeightfieldAmplitudeParamName.ToString(),
			*LocalBoundsMinParamName.ToString(), *CachedBake->LocalBoundsMin.ToString(),
			*LocalBoundsMaxParamName.ToString(), *CachedBake->LocalBoundsMax.ToString());
	}

	bHeightfieldInitialized = true;

	UE_LOG(LogFloodWaterPlane, Display,
		TEXT("Heightfield initialized | Comp=%s | Grid=%dx%d | Bounds=[%s..%s]"),
		SourceVolume.IsValid() ? *SourceVolume->CompartmentId.ToString() : TEXT("?"),
		W, H,
		*CachedBake->LocalBoundsMin.ToString(),
		*CachedBake->LocalBoundsMax.ToString());
}

void UFloodWaterPlaneComponent::TickHeightfieldStep()
{
	const int32 W = HeightfieldGridX;
	const int32 H = HeightfieldGridY;
	if (Heights.Num() != W * H || Velocities.Num() != W * H)
	{
		return;
	}

	// Neumann reflective BC on all cells (incl. boundary): out-of-grid neighbor mirrors center.
	// Critical: external systems (Phase 4 boundary-sync at doors) write to boundary cells; without
	// BC the boundary loop, those values stick forever and feed the interior as a permanent source.
	const float StepDt = 1.f / FMath::Max(1.f, HeightfieldUpdateHz);

	TArray<float> NewHeights;
	NewHeights.SetNumUninitialized(W * H);

	for (int32 y = 0; y < H; ++y)
	{
		for (int32 x = 0; x < W; ++x)
		{
			const int32 idx = y * W + x;
			const float h_center = Heights[idx];

			const float h_left  = (x > 0)     ? Heights[idx - 1] : h_center;
			const float h_right = (x < W - 1) ? Heights[idx + 1] : h_center;
			const float h_top   = (y > 0)     ? Heights[idx - W] : h_center;
			const float h_bot   = (y < H - 1) ? Heights[idx + W] : h_center;

			const float h_avg = (h_left + h_right + h_top + h_bot) * 0.25f;
			const float laplacian = h_avg - h_center;

			Velocities[idx] += laplacian * WaveSpeed * StepDt;
			Velocities[idx] *= Damping;
			NewHeights[idx] = h_center + Velocities[idx] * StepDt;
		}
	}

	Heights = MoveTemp(NewHeights);
}

void UFloodWaterPlaneComponent::PushHeightfieldToTexture()
{
	if (!HeightfieldTex || Heights.Num() == 0)
	{
		return;
	}

	struct FUpdateData
	{
		FUpdateTextureRegion2D Region;
		uint32 SrcPitch = 0;
		TArray<float> Buffer;
	};

	FUpdateData* Data = new FUpdateData();
	Data->Region = FUpdateTextureRegion2D(0, 0, 0, 0, HeightfieldGridX, HeightfieldGridY);
	Data->SrcPitch = HeightfieldGridX * sizeof(float);
	Data->Buffer = Heights; // copy — async cleanup deletes Data only after upload finishes

	HeightfieldTex->UpdateTextureRegions(
		/*MipIndex*/   0,
		/*NumRegions*/ 1,
		/*Regions*/    &Data->Region,
		/*SrcPitch*/   Data->SrcPitch,
		/*SrcBpp*/     sizeof(float),
		/*SrcData*/    reinterpret_cast<uint8*>(Data->Buffer.GetData()),
		/*Cleanup*/    [Data](uint8*, const FUpdateTextureRegion2D*) { delete Data; });
}

void UFloodWaterPlaneComponent::InjectAt(FVector2D LocalPosXY, float Force, float Radius)
{
	if (!bEnableHeightfield || !bHeightfieldInitialized || Heights.Num() == 0 || !CachedBake)
	{
		// Loud signal so the user sees why the inject did nothing instead of silent no-op.
		UE_LOG(LogFloodWaterPlane, Warning,
			TEXT("InjectAt skipped | Comp=%s | Reason=%s"),
			SourceVolume.IsValid() ? *SourceVolume->CompartmentId.ToString() : TEXT("?"),
			!bEnableHeightfield ? TEXT("HeightfieldDisabled")
				: !bHeightfieldInitialized ? TEXT("NotInitialized (cap mesh not yet rendered — fill compartment first)")
				: Heights.Num() == 0 ? TEXT("HeightsEmpty")
				: TEXT("NoBake"));
#if !UE_BUILD_SHIPPING
		// Still draw the marker even on skip, so the user can see WHERE they tried to inject.
		if (const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>())
		{
			if (Settings->bDrawWaterInjectMarkers)
			{
				if (UWorld* World = GetWorld())
				{
					if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
					{
						if (const USceneComponent* SubRoot = Sub->GetRootComponent())
						{
							const FVector LocalPos3D(LocalPosXY.X, LocalPosXY.Y, 0.f);
							const FVector WorldPos = SubRoot->GetComponentTransform().TransformPosition(LocalPos3D);
							DrawDebugSphere(World, WorldPos, FMath::Max(8.f, Radius * 0.25f), 12, FColor::Yellow, false, Settings->WaterInjectMarkerLifetime);
							DrawDebugString(World, WorldPos + FVector(0, 0, 30.f),
								TEXT("[InjectAt SKIPPED — fill compartment first]"),
								nullptr, FColor::Yellow, Settings->WaterInjectMarkerLifetime, true, 1.f);
						}
					}
				}
			}
		}
#endif
		return;
	}

	const int32 W = HeightfieldGridX;
	const int32 H = HeightfieldGridY;

	const FVector& Min = CachedBake->LocalBoundsMin;
	const FVector& Max = CachedBake->LocalBoundsMax;
	const float SpanX = Max.X - Min.X;
	const float SpanY = Max.Y - Min.Y;
	if (SpanX <= 0.f || SpanY <= 0.f)
	{
		return;
	}

	const float u = (LocalPosXY.X - Min.X) / SpanX;
	const float v = (LocalPosXY.Y - Min.Y) / SpanY;
	const int32 cx = FMath::Clamp(static_cast<int32>(u * W), 0, W - 1);
	const int32 cy = FMath::Clamp(static_cast<int32>(v * H), 0, H - 1);

	const float CellWorldSize = SpanX / static_cast<float>(W);
	const int32 RadiusCells = FMath::Max(1, FMath::CeilToInt(Radius / CellWorldSize));

	for (int32 dy = -RadiusCells; dy <= RadiusCells; ++dy)
	{
		for (int32 dx = -RadiusCells; dx <= RadiusCells; ++dx)
		{
			const int32 nx = cx + dx;
			const int32 ny = cy + dy;
			if (nx < 0 || nx >= W || ny < 0 || ny >= H)
			{
				continue;
			}
			const float dist = FMath::Sqrt(static_cast<float>(dx * dx + dy * dy));
			const float falloff = FMath::Max(0.f, 1.0f - dist / static_cast<float>(RadiusCells));
			Heights[ny * W + nx] += Force * falloff;
		}
	}

#if !UE_BUILD_SHIPPING
	// Visible debug sphere + radius circle at the inject point so the artist sees WHERE the
	// inject happened (independent from the cap mesh, which is hidden when the compartment is
	// empty). Plus a one-line log.
	if (const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>())
	{
		if (Settings->bDrawWaterInjectMarkers)
		{
			if (UWorld* World = GetWorld())
			{
				if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
				{
					if (const USceneComponent* SubRoot = Sub->GetRootComponent())
					{
						// Place the marker at the cap mesh Z (water surface) if the cap is rendered,
						// else at the compartment volume center Z. Either way the X/Y is the inject XY.
						float MarkerZ_Local = 0.f;
						if (BakeCapMeshComp)
						{
							MarkerZ_Local = BakeCapMeshComp->GetRelativeLocation().Z;
						}
						const FVector LocalPos3D(LocalPosXY.X, LocalPosXY.Y, MarkerZ_Local);
						const FVector WorldPos = SubRoot->GetComponentTransform().TransformPosition(LocalPos3D);
						const float Lifetime = Settings->WaterInjectMarkerLifetime;
						const FColor Color = Force >= 0.f ? FColor::Cyan : FColor::Magenta;
						DrawDebugSphere(World, WorldPos, FMath::Max(8.f, Radius * 0.18f), 12, Color, false, Lifetime, SDPG_World, 2.f);
						// Radius circle (horizontal disc) for the falloff extent.
						DrawDebugCircle(World, WorldPos, Radius, 32, Color, false, Lifetime, SDPG_World, 2.f,
							FVector(1, 0, 0), FVector(0, 1, 0), false);
						DrawDebugString(World, WorldPos + FVector(0, 0, 25.f),
							FString::Printf(TEXT("[InjectAt] F=%.1f R=%.0fcm"), Force, Radius),
							nullptr, Color, Lifetime, true, 1.f);
					}
				}
			}
		}
	}

	UE_LOG(LogFloodWaterPlane, Verbose,
		TEXT("InjectAt | Comp=%s | LocalXY=(%.1f, %.1f) | Force=%.2f | Radius=%.1fcm | Cells affected=%d"),
		SourceVolume.IsValid() ? *SourceVolume->CompartmentId.ToString() : TEXT("?"),
		LocalPosXY.X, LocalPosXY.Y, Force, Radius, (RadiusCells * 2 + 1) * (RadiusCells * 2 + 1));
#endif
}

bool UFloodWaterPlaneComponent::InjectAtWorldPoint(FVector WorldPos, float Force, float Radius)
{
	if (!CachedBake)
	{
		return false;
	}
	const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner());
	if (!Sub)
	{
		return false;
	}

	// World → sub-local. The bake's LocalBoundsMin/Max are in sub-local frame, so we need the
	// inverse of the submarine root transform.
	const USceneComponent* SubRoot = Sub->GetRootComponent();
	if (!SubRoot)
	{
		return false;
	}
	const FVector LocalPos3D = SubRoot->GetComponentTransform().InverseTransformPosition(WorldPos);

	const FVector& Min = CachedBake->LocalBoundsMin;
	const FVector& Max = CachedBake->LocalBoundsMax;
	if (LocalPos3D.X < Min.X - Radius || LocalPos3D.X > Max.X + Radius ||
		LocalPos3D.Y < Min.Y - Radius || LocalPos3D.Y > Max.Y + Radius)
	{
		return false;
	}

	InjectAt(FVector2D(LocalPos3D.X, LocalPos3D.Y), Force, Radius);
	return true;
}

void UFloodWaterPlaneComponent::ResetHeightfield()
{
	if (Heights.Num() > 0)
	{
		FMemory::Memzero(Heights.GetData(), Heights.Num() * sizeof(float));
	}
	if (Velocities.Num() > 0)
	{
		FMemory::Memzero(Velocities.GetData(), Velocities.Num() * sizeof(float));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Breach reaction (P3.5)
// ─────────────────────────────────────────────────────────────────────────────

void UFloodWaterPlaneComponent::RefreshBreachReaction()
{
	const UCompartmentVolumeComponent* Source = SourceVolume.Get();
	if (!Source || Source->CompartmentId.IsNone())
	{
		return;
	}
	const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner());
	if (!Sub)
	{
		return;
	}
	const USubFloodComponent* Flood = Sub->FindComponentByClass<USubFloodComponent>();
	if (!Flood)
	{
		return;
	}

	// Find the breach record for this compartment, if any. Replicated → identical on server + clients.
	const FCompartmentBreachState* BreachForUs = Flood->GetBreaches().FindByPredicate(
		[&](const FCompartmentBreachState& B)
		{
			return B.CompartmentId == Source->CompartmentId && B.bBreached && B.InflowRateLitersPerSec > 0.f;
		});

	const bool bNowBreached = (BreachForUs != nullptr);

	// State transition: not breached → breached. One-shot impulse + Niagara spawn.
	if (bNowBreached && !bBreachActive)
	{
		bBreachActive = true;
		LastBreachLocalCenter = BreachForUs->BreachLocalCenter;
		BreachInjectAccum = 0.f;

		// Initial impulse into the heightfield.
		InjectAt(FVector2D(LastBreachLocalCenter.X, LastBreachLocalCenter.Y),
			BreachInjectForce, BreachInjectRadiusCm);

		// Spawn Niagara at world location (sub-local → world via root).
		if (BreachWaterImpactVfx)
		{
			if (USceneComponent* SubRoot = Sub->GetRootComponent())
			{
				const FVector WorldLoc = SubRoot->GetComponentTransform().TransformPosition(LastBreachLocalCenter);
				BreachVfxInstance = UNiagaraFunctionLibrary::SpawnSystemAttached(
					BreachWaterImpactVfx,
					SubRoot,
					NAME_None,
					WorldLoc,
					FRotator::ZeroRotator,
					EAttachLocation::KeepWorldPosition,
					/*bAutoDestroy*/ false);
			}
		}

		UE_LOG(LogFloodWaterPlane, Display,
			TEXT("Breach reaction | Comp=%s | LocalCenter=%s | Inflow=%.1f L/s"),
			*Source->CompartmentId.ToString(),
			*LastBreachLocalCenter.ToString(),
			BreachForUs->InflowRateLitersPerSec);
	}
	// State transition: breached → no longer breached. Stop Niagara, reset state.
	else if (!bNowBreached && bBreachActive)
	{
		bBreachActive = false;
		LastBreachLocalCenter = FVector::ZeroVector;
		BreachInjectAccum = 0.f;

		if (BreachVfxInstance)
		{
			BreachVfxInstance->Deactivate();
			BreachVfxInstance->DestroyComponent();
			BreachVfxInstance = nullptr;
		}
	}
	// Continuous: while breached, optionally re-inject smaller pulses to keep ripples alive.
	else if (bNowBreached && bBreachActive && BreachRecurringInjectHz > 0.f)
	{
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
		BreachInjectAccum += Dt;
		const float Period = 1.f / FMath::Max(0.01f, BreachRecurringInjectHz);
		if (BreachInjectAccum >= Period)
		{
			BreachInjectAccum = 0.f;
			InjectAt(FVector2D(LastBreachLocalCenter.X, LastBreachLocalCenter.Y),
				BreachRecurringInjectForce, BreachInjectRadiusCm * 0.5f);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Slosh modal (P3.6)
// ─────────────────────────────────────────────────────────────────────────────

void UFloodWaterPlaneComponent::UpdateSloshModal(float Dt)
{
	if (Dt <= 0.f) return;

	const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner());
	const USubMovementComponent* SubMov = Sub ? Sub->SubMovement : nullptr;
	const USceneComponent* SubRoot = Sub ? Sub->GetRootComponent() : nullptr;
	if (!SubMov || !SubRoot)
	{
		return;
	}

	const FVector CurrentWorldVel = SubMov->Velocity;

	// Seed on first call so we don't generate a huge fake impulse from (Vel - 0) / Dt.
	if (!bSloshSeeded)
	{
		LastSubWorldVelocity = CurrentWorldVel;
		bSloshSeeded = true;
	}

	// World accel → sub-local accel (X = forward, Y = right, Z = up in sub frame).
	const FVector WorldAccel = (CurrentWorldVel - LastSubWorldVelocity) / Dt;
	LastSubWorldVelocity = CurrentWorldVel;
	const FVector LocalAccel = SubRoot->GetComponentTransform().InverseTransformVector(WorldAccel);

	// Impulse into the spring-damper. Accel-driven (mass implicit in the gain). Negative signs:
	//   +X accel (forward) → water lags behind → tilt toward rear → pitch DOWN at front
	//   +Y accel (right)   → water lags left   → roll LEFT
	//   +Z accel (up)      → water lags down   → OffsetZ DOWN
	SloshTiltVel.X += -LocalAccel.X * SloshTiltGain;
	SloshTiltVel.Y += -LocalAccel.Y * SloshTiltGain;
	SloshOffsetVelZ += -LocalAccel.Z * SloshOffsetGain;

	// Spring-damper integration.
	//   Stiffness k = (2π * f)²
	//   Damping   c = 2 * ζ * (2π * f)
	//   v += (-k * x - c * v) * dt
	//   x += v * dt
	const float Omega = 2.f * PI * FMath::Max(0.01f, SloshNaturalFreqHz);
	const float Stiffness = Omega * Omega;
	const float DampingCoef = 2.f * SloshDampingRatio * Omega;

	// Tilt + offset spring-damper integration. Inlined 3× to avoid lambda template deduction
	// issues — FVector2D members are double under UE5 LWC, while SloshOffsetZ is float.
	SloshTiltVel.X += (-SloshTilt.X * Stiffness - SloshTiltVel.X * DampingCoef) * Dt;
	SloshTilt.X += SloshTiltVel.X * Dt;

	SloshTiltVel.Y += (-SloshTilt.Y * Stiffness - SloshTiltVel.Y * DampingCoef) * Dt;
	SloshTilt.Y += SloshTiltVel.Y * Dt;

	SloshOffsetVelZ += (-SloshOffsetZ * Stiffness - SloshOffsetVelZ * DampingCoef) * Dt;
	SloshOffsetZ += SloshOffsetVelZ * Dt;

	// Hard clamps (visible-quality safety; sustained extreme accel would saturate otherwise).
	const float TiltCap = FMath::Max(0.f, MaxSloshTiltDeg) / FMath::Max(0.01f, MaxSloshTiltDeg);
	(void)TiltCap;
	// SloshTilt is unitless fraction in -1..+1 conceptually; clamp directly.
	SloshTilt.X = FMath::Clamp(SloshTilt.X, -1.f, 1.f);
	SloshTilt.Y = FMath::Clamp(SloshTilt.Y, -1.f, 1.f);
	SloshOffsetZ = FMath::Clamp(SloshOffsetZ, -MaxSloshOffsetCm, MaxSloshOffsetCm);
}

void UFloodWaterPlaneComponent::ApplyCapMeshTransformWithSlosh()
{
	if (!BakeCapMeshComp) return;

	const ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner());
	if (!Sub) return;

	// Water surface is gravity-aligned: stays HORIZONTAL in world regardless of sub pitch/roll.
	// Only sub yaw is inherited (so the cap shape rotates with the sub heading and the bake's
	// sub-local XY footprint covers the compartment correctly). Slosh tilts add small inertial
	// pitch/roll on top as a visual response to sub acceleration.
	//
	// Note: world Z = sub.world.Z + sub-local water Z. This treats the compartment as if the sub
	// were level (no per-compartment vertical adjustment when sub is pitched). For typical FP
	// attitudes (≤15°) the slip is sub-cm; full correctness would require per-compartment world Z
	// tracking, deferred post-FP.
	//
	// Side benefit: vertex Local Position stays anchored to the cap mesh component (which now
	// follows the sub yaw frame, not the full sub orientation). Material nodes that read Local
	// Position (heightfield UV, Gerstner) get sub-anchored waves automatically.
	const FVector SubWorldLoc = Sub->GetActorLocation();
	const float SubYaw = Sub->GetActorRotation().Yaw;

	const FVector CapWorldPos(
		SubWorldLoc.X,
		SubWorldLoc.Y,
		SubWorldLoc.Z + CurrentBaseWaterZLocal + SloshOffsetZ);

	const FRotator CapWorldRot(
		SloshTilt.X * MaxSloshTiltDeg,   // pitch (slosh only)
		SubYaw,                           // yaw inherits sub
		SloshTilt.Y * MaxSloshTiltDeg);  // roll (slosh only)

	BakeCapMeshComp->SetWorldLocation(CapWorldPos);
	BakeCapMeshComp->SetWorldRotation(CapWorldRot);
}
