#include "SubFloodComponent.h"

#include "CompartmentVolumeComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Sub3DDebugSettings.h"
#include "SubHullBoundaryComponent.h"
#include "SubMovementComponent.h"
#include "SubmarineBase.h"
#include "SubmarineDefinition.h"
#include "SubmarineLayoutAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubFlood, Log, All);

namespace
{
bool HasAuthority(const UActorComponent* Component)
{
	const AActor* Owner = Component ? Component->GetOwner() : nullptr;
	return Owner && Owner->GetLocalRole() == ROLE_Authority;
}

constexpr float WaterDensityKgPerLiter = 1.025f; // seawater
}

// --- Construction --------------------------------------------------------

USubFloodComponent::USubFloodComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Tick in editor preview viewports too — used only for the debug-label drawing block at the
	// top of TickComponent (compartment/connection markers must be visible in the BP editor and
	// level editor without needing PIE). The simulation block below is gated behind
	// `HasAuthority + CompartmentStates.Num() > 0`, both false in editor preview, so no sim runs.
	bTickInEditor = true;
	SetIsReplicatedByDefault(true);
}

void USubFloodComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USubFloodComponent, CompartmentStates);
	DOREPLIFETIME(USubFloodComponent, EdgeStates);
	DOREPLIFETIME(USubFloodComponent, Breaches);
}

// --- Initialization ------------------------------------------------------

void USubFloodComponent::InitializeFromDefinition(const USubmarineDefinition* Definition)
{
	CompartmentStates.Reset();
	EdgeStates.Reset();
	Breaches.Reset();

	if (!Definition || !Definition->IsValid())
	{
		UE_LOG(LogSubFlood, Warning, TEXT("InitializeFromDefinition: null or invalid definition"));
		return;
	}

	// Build compartment states from definition
	ASubmarineBase* SubBase = Cast<ASubmarineBase>(GetOwner());
	for (const FGeneratedCompartmentDef& Comp : Definition->Compartments)
	{
		FFloodCompartmentState State;
		State.CompartmentId = Comp.CompartmentId;
		State.CapacityLiters = Comp.CapacityLiters;
		State.MaxWaterHeightCm = FMath::Max(MinCompartmentHeightCm, Comp.MaxWaterHeightCm);
		State.WalkableFloorZCm = Comp.WalkableFloorZCm;
		State.CurrentWaterLiters = 0.f;

		// Populate the sub-local AABB from CV components (Approach 1 tilt-aware sim).
		// Falls back to a synthetic box derived from WalkableFloorZCm + MaxWaterHeightCm if
		// no CV is found — sim still runs but tilt response degrades to "uniform sub-local"
		// (the pre-tilt-aware behavior). Logged as warning so it's visible during init.
		FBox CvBox(ForceInit);
		if (SubBase && SubBase->GetCompartmentLocalBounds(Comp.CompartmentId, CvBox))
		{
			State.LocalBox = CvBox;
		}
		else
		{
			const float HalfXY = 200.f; // arbitrary 2m-wide column fallback
			const FVector Mn(-HalfXY, -HalfXY, Comp.WalkableFloorZCm);
			const FVector Mx(+HalfXY, +HalfXY, Comp.WalkableFloorZCm + State.MaxWaterHeightCm);
			State.LocalBox = FBox(Mn, Mx);
			UE_LOG(LogSubFlood, Warning,
				TEXT("InitializeFromDefinition: %s has no CV — tilt response will degrade to sub-local. ")
				TEXT("Place a UCompartmentVolumeComponent for this compartment in the BP."),
				*Comp.CompartmentId.ToString());
		}

		CompartmentStates.Add(State);
	}

	// Build edge states from flood graph
	constexpr float MinimumPassageAreaCm2 = 1000.f; // floor for Area-derived flow scale; some
	// import paths (vertical hatches) compute width*height = 0 and need a sensible default.
	for (const FFloodGraphEdge& Edge : Definition->FloodGraph.Edges)
	{
		FFloodEdgeState ES;
		ES.ClosureId = Edge.ClosureId;
		ES.VolumeA = Edge.VolumeA;
		ES.VolumeB = Edge.VolumeB;
		ES.PassageAreaCm2 = FMath::Max(Edge.PassageAreaCm2, MinimumPassageAreaCm2);
		ES.bExteriorEdge = Edge.bExteriorEdge;

		// Initial OpenRatio + LocalSpillPosition derivation. Resolution rules:
		//  1. ConnectionType=Open → OpenRatio=1 permanently (door cannot close it).
		//  2. Otherwise → OpenRatio = bStartsClosed ? 0 : 1.
		//  3. No matching connection → OpenRatio=0 (safe default; an unknown edge stays sealed).
		// LocalSpillPosition comes from Connection.LocalTransform.Translation (full XYZ in
		// sub-local space) so we can transform it to world each tick for the tilt-aware sim.
		if (!Edge.ClosureId.IsNone())
		{
			const FGeneratedConnectionDef* Conn = Definition->FindConnection(Edge.ClosureId);
			if (Conn)
			{
				ES.LocalSpillPosition = Conn->LocalTransform.GetTranslation();
				if (Conn->ConnectionType == EConnectionType::Open)
				{
					ES.OpenRatio = 1.f;
				}
				else
				{
					ES.OpenRatio = Conn->bStartsClosed ? 0.f : 1.f;
				}
			}
			else
			{
				ES.OpenRatio = 0.f;
			}
		}
		else
		{
			// Edge with no matching connection (rare): default to open. Spill defaults to origin.
			ES.OpenRatio = 1.f;
		}

		EdgeStates.Add(ES);
	}

	// Apply flood defaults from definition
	MaxExteriorInflowLitersPerSec = Definition->MaxExteriorInflowLitersPerSec;

	UpdateDerivedState();
	bClientInitialized = true;

	UE_LOG(LogSubFlood, Log, TEXT("Initialized: %d compartments, %d edges"),
		CompartmentStates.Num(), EdgeStates.Num());

	OnFloodInitialized.Broadcast();
}

void USubFloodComponent::InitializeFromCompartmentVolumes(const TArray<UCompartmentVolumeComponent*>& Volumes)
{
	CompartmentStates.Reset();
	EdgeStates.Reset();
	Breaches.Reset();

	if (Volumes.Num() == 0)
	{
		UE_LOG(LogSubFlood, Warning, TEXT("InitializeFromCompartmentVolumes: no volumes on %s"), *GetNameSafe(GetOwner()));
		return;
	}

	// Aggregate per CompartmentId: if the BP places multiple volumes with the same id,
	// sum their capacity and take the max Z extent as MaxWaterHeightCm.
	TMap<FName, FFloodCompartmentState> Aggregate;

	for (const UCompartmentVolumeComponent* Vol : Volumes)
	{
		if (!Vol || Vol->CompartmentId.IsNone())
		{
			continue;
		}

		const FVector Extent = Vol->GetScaledBoxExtent();
		const float EffectiveHeightCm = FMath::Max(MinCompartmentHeightCm, Vol->GetFloodMaxHeightCm());
		const float VolumeCm3 = FMath::Max(1.f, 4.f * Extent.X * Extent.Y * EffectiveHeightCm);
		const float Liters = VolumeCm3 / 1000.f;
		const float HeightCm = EffectiveHeightCm;

		FFloodCompartmentState* Existing = Aggregate.Find(Vol->CompartmentId);
		if (Existing)
		{
			Existing->CapacityLiters += Liters;
			Existing->MaxWaterHeightCm = FMath::Max(Existing->MaxWaterHeightCm, HeightCm);
		}
		else
		{
			FFloodCompartmentState State;
			State.CompartmentId = Vol->CompartmentId;
			State.CapacityLiters = Vol->CapacityLitersOverride > 0.f ? Vol->CapacityLitersOverride : Liters;
			State.MaxWaterHeightCm = HeightCm;
			State.CurrentWaterLiters = 0.f;
			Aggregate.Add(Vol->CompartmentId, State);
		}
	}

	CompartmentStates.Reserve(Aggregate.Num());
	for (auto& Pair : Aggregate)
	{
		CompartmentStates.Add(Pair.Value);

		UE_LOG(
			LogSubFlood,
			Log,
			TEXT("FloodInit (volumes) | Compartment=%s | Capacity=%.0fL | MaxHeight=%.0fcm"),
			*Pair.Value.CompartmentId.ToString(),
			Pair.Value.CapacityLiters,
			Pair.Value.MaxWaterHeightCm);
	}

	// No edges synthesized for FP — each compartment is isolated. Breaches drive inflow
	// directly on the target compartment via CreateBreach.

	UpdateDerivedState();
	bClientInitialized = true;
	OnFloodInitialized.Broadcast();

	UE_LOG(
		LogSubFlood,
		Log,
		TEXT("InitializeFromCompartmentVolumes completed | Owner=%s | CompartmentCount=%d"),
		*GetNameSafe(GetOwner()),
		CompartmentStates.Num());
}

void USubFloodComponent::InitializeFromLayout(const USubmarineLayoutAsset* Layout)
{
	// LEGACY (Phase 7A, 2026-04-10) — Proto init path from LayoutAsset.
	// Kept for Proto03/04 submarines that have no GeneratedDefinition yet.
	// Prefer InitializeFromDefinition. Will be removed in Phase 7B.
	UE_LOG(LogSubFlood, Warning,
		TEXT("[LEGACY] USubFloodComponent::InitializeFromLayout: using LayoutAsset path on %s. ")
		TEXT("Prefer InitializeFromDefinition(USubmarineDefinition*)."),
		*GetNameSafe(GetOwner()));

	CompartmentStates.Reset();
	EdgeStates.Reset();
	Breaches.Reset();

	if (!Layout || Layout->Compartments.Num() == 0)
	{
		UE_LOG(LogSubFlood, Warning, TEXT("InitializeFromLayout: null or empty layout"));
		return;
	}

	// Build compartment states from layout compartments.
	for (const FSubCompartmentDef& CompDef : Layout->Compartments)
	{
		if (CompDef.CompartmentId.IsNone())
		{
			continue;
		}

		FFloodCompartmentState State;
		State.CompartmentId = CompDef.CompartmentId;
		State.CapacityLiters = FMath::Max(1.f, CompDef.CapacityLiters);
		State.MaxWaterHeightCm = FMath::Max(MinCompartmentHeightCm, CompDef.HydroBoundsMax.Z - CompDef.HydroBoundsMin.Z);
		State.CurrentWaterLiters = 0.f;
		CompartmentStates.Add(State);
	}

	// Synthesize edges from structural sheets that bridge two compartments.
	for (const FStructuralSheetDef& Sheet : Layout->StructuralSheets)
	{
		if (Sheet.AdjacentCompartmentId.IsNone())
		{
			continue;
		}

		FFloodEdgeState ES;
		ES.ClosureId = Sheet.SheetId;
		ES.VolumeA = Sheet.ParentCompartmentId;
		ES.VolumeB = Sheet.AdjacentCompartmentId;
		ES.PassageAreaCm2 = Sheet.SizeCm.X * Sheet.SizeCm.Y;
		ES.bExteriorEdge = false;
		ES.OpenRatio = 0.f; // doors start closed

		// Check if there's a door definition matching this sheet.
		const FDoorDef* Door = Layout->Doors.FindByPredicate([&](const FDoorDef& D)
		{
			return D.BulkheadSheetId == Sheet.SheetId;
		});
		if (Door)
		{
			ES.PassageAreaCm2 = Door->WidthCm * Door->HeightCm;
		}

		EdgeStates.Add(ES);
	}

	UpdateDerivedState();
	bClientInitialized = true;

	UE_LOG(LogSubFlood, Log, TEXT("InitializeFromLayout: %d compartments, %d edges"),
		CompartmentStates.Num(), EdgeStates.Num());

	OnFloodInitialized.Broadcast();
}

// --- Tick ----------------------------------------------------------------

void USubFloodComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
	// Authoring debug labels — compartments + connections drawn on ALL net roles. Outside the
	// authority guard so clients see them too.
	if (const USub3DDebugSettings* SettingsLabels = GetDefault<USub3DDebugSettings>())
	{
		UWorld* W = GetWorld();
		const AActor* Owner = GetOwner();
		const FTransform SubXf = Owner ? Owner->GetActorTransform() : FTransform::Identity;

		// 1) Compartment labels at each volume's center
		if (W && Owner && SettingsLabels->bDrawCompartmentLabels)
		{
			TArray<UCompartmentVolumeComponent*> Volumes;
			Owner->GetComponents<UCompartmentVolumeComponent>(Volumes);
			for (const UCompartmentVolumeComponent* Vol : Volumes)
			{
				if (!Vol || Vol->CompartmentId.IsNone()) continue;
				const FVector Center = Vol->GetComponentLocation();
				DrawDebugString(W, Center, Vol->CompartmentId.ToString(), nullptr,
					FColor(180, 220, 255), 0.f, true, 1.3f);
			}
		}

		// 2) Connection markers at each Conn.LocalTransform from the DA
		const ASubmarineBase* SubAsBase = Cast<ASubmarineBase>(Owner);
		if (W && SubAsBase && SubAsBase->GeneratedDefinition && SettingsLabels->bDrawConnectionMarkers)
		{
			for (const FGeneratedConnectionDef& Conn : SubAsBase->GeneratedDefinition->Connections)
			{
				const FVector LocalLoc = Conn.LocalTransform.GetLocation();
				const FVector WorldPos = SubXf.TransformPosition(LocalLoc);
				FColor Color = FColor::White;
				FString TypeTag;
				switch (Conn.ConnectionType)
				{
					case EConnectionType::Door:          Color = FColor::Red;    TypeTag = TEXT("Door");          break;
					case EConnectionType::Hatch:         Color = FColor::Orange; TypeTag = TEXT("Hatch");         break;
					case EConnectionType::ExteriorHatch: Color = FColor::Blue;   TypeTag = TEXT("ExteriorHatch"); break;
					case EConnectionType::Open:          Color = FColor::Green;  TypeTag = TEXT("Open");          break;
					default:                                                    TypeTag = TEXT("?");             break;
				}
				DrawDebugSphere(W, WorldPos, 12.f, 12, Color, false, -1.f, SDPG_World, 1.f);
				const FString CompBStr = Conn.CompartmentB.IsNone() ? TEXT("(EXT)") : Conn.CompartmentB.ToString();
				const FString Label = FString::Printf(
					TEXT("[%s] %s\n%s ↔ %s"),
					*TypeTag,
					*Conn.ConnectionId.ToString(),
					*Conn.CompartmentA.ToString(),
					*CompBStr);
				DrawDebugString(W, WorldPos + FVector(0, 0, 25.f), Label, nullptr,
					Color, 0.f, true, 1.f);
			}
		}
	}

	// Breach markers — drawn on ALL net roles (Breaches is replicated). Outside the authority
	// guard below so clients also visualise. Disabled by default; toggle in Project Settings >
	// Game > Sub3D Debug > Submarine|Flood > Draw Breach Markers.
	if (const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>())
	{
		if (Settings->bDrawBreachMarkers && Breaches.Num() > 0)
		{
			if (UWorld* World = GetWorld())
			{
				if (const AActor* Owner = GetOwner())
				{
					const FTransform SubXf = Owner->GetActorTransform();
					const FVector Extent(Settings->BreachMarkerHalfExtentCm);
					for (const FCompartmentBreachState& B : Breaches)
					{
						if (!B.bBreached) continue;
						const FVector WorldPos = SubXf.TransformPosition(B.BreachLocalCenter);
						DrawDebugSolidBox(World, WorldPos, Extent, FColor::Red, false, -1.f, SDPG_World);
						DrawDebugBox(World, WorldPos, Extent, FColor::Black, false, -1.f, SDPG_World, 1.5f);
						const FString Label = FString::Printf(
							TEXT("[BREACH] %s  %.1f L/s"),
							*B.CompartmentId.ToString(),
							B.InflowRateLitersPerSec);
						DrawDebugString(World, WorldPos + FVector(0.f, 0.f, Settings->BreachMarkerHalfExtentCm + 20.f),
							Label, nullptr, FColor::Red, 0.f, true, 1.f);
					}
				}
			}
		}
	}
#endif

	if (!HasAuthority(this) || CompartmentStates.Num() == 0)
	{
		return;
	}

	AdvanceFlooding(DeltaTime);
	MaybeLogWaterLevels(DeltaTime);

	// Push current interior water mass to SubMovement as an input. SubMovement
	// declares a tick prereq on us, so the fixed-tick integrator always reads
	// this freshly-advanced value. Kept inside the authority guard — only the
	// server drives the sim and the sub's physics.
	if (ASubmarineBase* Sub = Cast<ASubmarineBase>(GetOwner()))
	{
		if (Sub->SubMovement)
		{
			Sub->SubMovement->SetFloodImpactKg(GetTotalWaterMassKg());
		}
	}
}

// --- Door state ----------------------------------------------------------

void USubFloodComponent::SetDoorOpenRatio(FName ConnectionId, float NewOpenRatio)
{
	if (!HasAuthority(this) || ConnectionId.IsNone())
	{
		return;
	}

	const float ClampedRatio = FMath::Clamp(NewOpenRatio, 0.f, 1.f);
	FFloodEdgeState* Edge = FindEdge(ConnectionId);
	if (Edge)
	{
		Edge->OpenRatio = ClampedRatio;
	}
	else
	{
		UE_LOG(LogSubFlood, Warning, TEXT("SetDoorOpenRatio: no edge found for ConnectionId '%s'"),
			*ConnectionId.ToString());
	}
}

void USubFloodComponent::SetDoorState(FName ConnectionId, bool bClosed)
{
	// Backward-compat: maps a binary state to the continuous OpenRatio.
	SetDoorOpenRatio(ConnectionId, bClosed ? 0.f : 1.f);
}

void USubFloodComponent::SetDoorOpenRatioByCompartments(FName CompA, FName CompB, float NewOpenRatio)
{
	if (!HasAuthority(this) || CompA.IsNone() || CompB.IsNone())
	{
		return;
	}

	const float ClampedRatio = FMath::Clamp(NewOpenRatio, 0.f, 1.f);
	for (FFloodEdgeState& Edge : EdgeStates)
	{
		const bool bMatch =
			(Edge.VolumeA == CompA && Edge.VolumeB == CompB) ||
			(Edge.VolumeA == CompB && Edge.VolumeB == CompA);
		if (bMatch)
		{
			Edge.OpenRatio = ClampedRatio;
			return;
		}
	}

	UE_LOG(LogSubFlood, Warning,
		TEXT("SetDoorOpenRatioByCompartments: no edge found for compartments A='%s' B='%s'"),
		*CompA.ToString(), *CompB.ToString());
}

void USubFloodComponent::SetDoorStateByCompartments(FName CompA, FName CompB, bool bClosed)
{
	SetDoorOpenRatioByCompartments(CompA, CompB, bClosed ? 0.f : 1.f);
}

// --- World-Z surface helpers (tilt-aware, Approach 1) --------------------

namespace
{
	/**
	 * Compute the 8 corners of the **water-volume** sub-box for a compartment, transformed to world.
	 * The water-volume box uses LocalBox.XY (full compartment footprint) but a restricted Z range of
	 * [WalkableFloorZCm, WalkableFloorZCm + MaxWaterHeightCm] — the actual fillable volume, ignoring
	 * any air headroom present in the CV component's full extent. Critical because LocalBox often
	 * spans well above MaxWaterHeightCm (ceiling), and using its full Z range would place the surface
	 * way too high for low fill fractions.
	 *
	 * Writes the min and max world Z observed across the 8 corners. Returns false if the LocalBox
	 * isn't valid; caller falls back to the sub-local approximation.
	 */
	bool BuildWaterVolumeBoxWorldZRange(const FFloodCompartmentState& Comp, const FTransform& SubXf, float& OutMinZ, float& OutMaxZ)
	{
		if (!Comp.LocalBox.IsValid)
		{
			return false;
		}
		const float FloorZ = Comp.WalkableFloorZCm;
		const float CeilZ  = Comp.WalkableFloorZCm + FMath::Max(1.f, Comp.MaxWaterHeightCm);
		OutMinZ = +FLT_MAX;
		OutMaxZ = -FLT_MAX;
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector Corner(
				(i & 1) ? Comp.LocalBox.Max.X : Comp.LocalBox.Min.X,
				(i & 2) ? Comp.LocalBox.Max.Y : Comp.LocalBox.Min.Y,
				(i & 4) ? CeilZ : FloorZ);
			const float Z = SubXf.TransformPosition(Corner).Z;
			OutMinZ = FMath::Min(OutMinZ, Z);
			OutMaxZ = FMath::Max(OutMaxZ, Z);
		}
		return true;
	}
}

float USubFloodComponent::ComputeBoxMinWorldZ(const FFloodCompartmentState& Comp, const FTransform& SubXf)
{
	float MinZ = 0.f, MaxZ = 0.f;
	if (BuildWaterVolumeBoxWorldZRange(Comp, SubXf, MinZ, MaxZ))
	{
		return MinZ;
	}
	return SubXf.GetTranslation().Z + Comp.WalkableFloorZCm;
}

float USubFloodComponent::ComputeSurfaceWorldZ(const FFloodCompartmentState& Comp, const FTransform& SubXf)
{
	float MinZ = 0.f, MaxZ = 0.f;
	if (!BuildWaterVolumeBoxWorldZRange(Comp, SubXf, MinZ, MaxZ))
	{
		// Fallback to sub-local interpretation when no box was populated.
		const FVector Surface = SubXf.TransformPosition(
			FVector(0.f, 0.f, Comp.WalkableFloorZCm + Comp.WaterHeightCm));
		return Surface.Z;
	}

	const float Fraction = (Comp.CapacityLiters > KINDA_SMALL_NUMBER)
		? FMath::Clamp(Comp.CurrentWaterLiters / Comp.CapacityLiters, 0.f, 1.f)
		: 0.f;
	return MinZ + Fraction * (MaxZ - MinZ);
}

// --- Breach --------------------------------------------------------------

void USubFloodComponent::CreateBreach(FName CompartmentId, float InflowRateLitersPerSec, FVector BreachLocalCenter)
{
	if (!HasAuthority(this))
	{
		return;
	}

	FFloodCompartmentState* State = FindState(CompartmentId);
	if (!State)
	{
		UE_LOG(LogSubFlood, Warning, TEXT("CreateBreach: compartment '%s' not found"), *CompartmentId.ToString());
		return;
	}

	const float ClampedInflow = FMath::Max(0.f, InflowRateLitersPerSec);
	State->BreachInflowLitersPerSec = ClampedInflow;

	// Update or add breach record
	FCompartmentBreachState* Existing = Breaches.FindByPredicate([&](const FCompartmentBreachState& B)
	{
		return B.CompartmentId == CompartmentId;
	});

	if (Existing)
	{
		Existing->bBreached = ClampedInflow > 0.f;
		Existing->InflowRateLitersPerSec = ClampedInflow;
		Existing->BreachLocalCenter = BreachLocalCenter;
	}
	else if (ClampedInflow > 0.f)
	{
		FCompartmentBreachState NewBreach;
		NewBreach.CompartmentId = CompartmentId;
		NewBreach.bBreached = true;
		NewBreach.InflowRateLitersPerSec = ClampedInflow;
		NewBreach.BreachLocalCenter = BreachLocalCenter;
		Breaches.Add(NewBreach);
	}

	// Hull boundary for crew EVA handoff. Spawned on first create, params refreshed on update.
	if (AActor* Owner = GetOwner())
	{
		const USub3DDebugSettings* DebugSettingsRef = GetDefault<USub3DDebugSettings>();
		if (DebugSettingsRef && DebugSettingsRef->bDisableBreachBoundaries)
		{
			if (TWeakObjectPtr<USubHullBoundaryComponent>* Tracked = BreachBoundariesByCompartment.Find(CompartmentId))
			{
				if (USubHullBoundaryComponent* Boundary = Tracked->Get())
				{
					Boundary->DestroyComponent();
				}
				BreachBoundariesByCompartment.Remove(CompartmentId);
			}
			return;
		}

		TWeakObjectPtr<USubHullBoundaryComponent>* Tracked = BreachBoundariesByCompartment.Find(CompartmentId);
		USubHullBoundaryComponent* Boundary = Tracked ? Tracked->Get() : nullptr;

		// Approximated outward orientation: +X radial from sub root toward the breach point.
		// Real breach geometry can override post-FP.
		auto OrientBoundary = [&](USubHullBoundaryComponent* B)
		{
			B->SetRelativeLocation(BreachLocalCenter);
			const FVector RadialDir = BreachLocalCenter.GetSafeNormal();
			const FRotator Rot = RadialDir.IsNearlyZero() ? FRotator::ZeroRotator : RadialDir.Rotation();
			B->SetRelativeRotation(Rot);
		};

		if (!Boundary && ClampedInflow > 0.f)
		{
			Boundary = NewObject<USubHullBoundaryComponent>(Owner);
			if (Boundary)
			{
				Boundary->Kind = EHullBoundaryKind::Breach;
				if (USceneComponent* Root = Owner->GetRootComponent())
				{
					Boundary->SetupAttachment(Root);
				}
				Boundary->RegisterComponent();
				OrientBoundary(Boundary);
				BreachBoundariesByCompartment.Add(CompartmentId, Boundary);

				UE_LOG(
					LogSubFlood,
					Log,
					TEXT("Breach boundary spawned | Compartment=%s | LocalCenter=%s"),
					*CompartmentId.ToString(),
					*BreachLocalCenter.ToCompactString());
			}
		}
		else if (Boundary)
		{
			OrientBoundary(Boundary);
		}
	}
}

void USubFloodComponent::RemoveBreach(FName CompartmentId)
{
	if (!HasAuthority(this))
	{
		return;
	}

	FFloodCompartmentState* State = FindState(CompartmentId);
	if (State)
	{
		State->BreachInflowLitersPerSec = 0.f;
	}

	Breaches.RemoveAll([&](const FCompartmentBreachState& B)
	{
		return B.CompartmentId == CompartmentId;
	});

	if (TWeakObjectPtr<USubHullBoundaryComponent>* Tracked = BreachBoundariesByCompartment.Find(CompartmentId))
	{
		if (USubHullBoundaryComponent* Boundary = Tracked->Get())
		{
			Boundary->DestroyComponent();
		}
		BreachBoundariesByCompartment.Remove(CompartmentId);

		UE_LOG(LogSubFlood, Log, TEXT("Breach boundary removed | Compartment=%s"), *CompartmentId.ToString());
	}
}

// --- Pump ----------------------------------------------------------------

void USubFloodComponent::SetPumpActive(FName CompartmentId, bool bActive, float RateLitersPerSec)
{
	if (!HasAuthority(this))
	{
		return;
	}

	FFloodCompartmentState* State = FindState(CompartmentId);
	if (!State)
	{
		return;
	}

	State->bPumpActive = bActive;
	State->PumpRateLitersPerSec = bActive ? FMath::Max(0.f, RateLitersPerSec) : 0.f;
}

// --- Debug ---------------------------------------------------------------

void USubFloodComponent::SetCompartmentFloodDirect(FName CompartmentId, float Level01)
{
	if (!HasAuthority(this))
	{
		return;
	}

	const float Clamped = FMath::Clamp(Level01, 0.f, 1.f);

	// Default to first compartment if none specified
	if (CompartmentId.IsNone() && CompartmentStates.Num() > 0)
	{
		CompartmentId = CompartmentStates[0].CompartmentId;
	}

	FFloodCompartmentState* State = FindState(CompartmentId);
	if (!State)
	{
		UE_LOG(LogSubFlood, Warning, TEXT("SetCompartmentFloodDirect: compartment '%s' not found"),
			*CompartmentId.ToString());
		return;
	}

	State->CurrentWaterLiters = Clamped * State->CapacityLiters;
	UpdateDerivedState();
	OnFloodStateUpdated.Broadcast(GetExportedStates());
}

// --- Queries -------------------------------------------------------------

float USubFloodComponent::GetCompartmentFloodLevel01(FName CompartmentId) const
{
	const FFloodCompartmentState* State = FindState(CompartmentId);
	return State ? State->WaterLevelNormalized : 0.f;
}

float USubFloodComponent::GetCompartmentWaterLiters(FName CompartmentId) const
{
	const FFloodCompartmentState* State = FindState(CompartmentId);
	return State ? State->CurrentWaterLiters : 0.f;
}

float USubFloodComponent::GetCompartmentWaterHeightCm(FName CompartmentId) const
{
	const FFloodCompartmentState* State = FindState(CompartmentId);
	return State ? State->WaterHeightCm : 0.f;
}

float USubFloodComponent::GetCompartmentSurfaceWorldZ(FName CompartmentId) const
{
	const FFloodCompartmentState* State = FindState(CompartmentId);
	if (!State)
	{
		return GetOwner() ? GetOwner()->GetActorLocation().Z : 0.f;
	}
	const FTransform SubXf = GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
	return ComputeSurfaceWorldZ(*State, SubXf);
}

float USubFloodComponent::GetTotalWaterLiters() const
{
	float Total = 0.f;
	for (const FFloodCompartmentState& State : CompartmentStates)
	{
		Total += State.CurrentWaterLiters;
	}
	return Total;
}

float USubFloodComponent::GetTotalWaterMassKg() const
{
	return GetTotalWaterLiters() * WaterDensityKgPerLiter;
}

void USubFloodComponent::ExportCompartmentStates(TArray<FCompartmentState>& OutStates) const
{
	OutStates.Reset(CompartmentStates.Num());
	for (const FFloodCompartmentState& S : CompartmentStates)
	{
		FCompartmentState Out;
		Out.CompartmentId = S.CompartmentId;
		Out.FloodLevel01 = S.WaterLevelNormalized;
		Out.WaterMassLiters = S.CurrentWaterLiters;
		Out.WaterHeightCm = S.WaterHeightCm;
		Out.InternalPressureKPa = 101.325f; // nominal, no pressure sim yet
		Out.ExternalPressureKPa = 101.325f;
		Out.PressureDeltaKPa = 0.f;
		Out.FloodRateIn = S.FloodRateIn;
		Out.bCritical = S.WaterLevelNormalized >= 0.8f;
		Out.bElectricalsWet = S.WaterLevelNormalized >= 0.15f;
		OutStates.Add(Out);
	}
}

// --- Simulation ----------------------------------------------------------

void USubFloodComponent::AdvanceFlooding(float DeltaTime)
{
	if (DeltaTime <= 0.f)
	{
		return;
	}

	// Reset per-tick rates
	for (FFloodCompartmentState& S : CompartmentStates)
	{
		S.FloodRateIn = 0.f;
		S.FloodRateOut = 0.f;
	}

	// 1. Breach inflows (exterior water entering through hull breaches)
	for (FFloodCompartmentState& S : CompartmentStates)
	{
		if (S.BreachInflowLitersPerSec > 0.f)
		{
			S.FloodRateIn += FMath::Min(S.BreachInflowLitersPerSec, MaxExteriorInflowLitersPerSec);
		}
	}

	// 2. Exterior edge inflows (open exterior hatches)
	for (const FFloodEdgeState& Edge : EdgeStates)
	{
		if (!Edge.bExteriorEdge || Edge.OpenRatio < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FFloodCompartmentState* Comp = FindState(Edge.VolumeA);
		if (!Comp)
		{
			continue;
		}

		// Open exterior hatch: water flows in proportional to passage area
		const float AreaScale = FMath::Max(0.f, Edge.PassageAreaCm2 / FMath::Max(1.f, InternalConnectionAreaDivisorCm2));
		const float Inflow = FMath::Clamp(AreaScale * 500.f, 0.f, MaxExteriorInflowLitersPerSec);
		Comp->FloodRateIn += Inflow;
	}

	// 3. Internal transfers (open doors between compartments)
	//    Two-pass: first compute desired flows per edge, then budget
	//    total outflow per compartment to conserve water mass.

	struct FPendingTransfer
	{
		int32 SourceIndex;
		int32 DestIndex;
		int32 EdgeIndex;       // for writing CurrentFlowRateLitersPerSec back after budget passes
		bool  bSourceIsA;      // true if Source == VolumeA → final flow rate sign is +
		float DesiredRateLps;  // liters per second (always positive after this point)
	};
	TArray<FPendingTransfer> PendingTransfers;

	// Reset every edge's per-tick flow rate. The compute below sets it for active edges; idle
	// edges stay at zero so VFX / audio components see a clean signal each tick.
	for (FFloodEdgeState& EdgeReset : EdgeStates)
	{
		EdgeReset.CurrentFlowRateLitersPerSec = 0.f;
		EdgeReset.CurrentHeadDeltaCm = 0.f;
	}

	// Tunables (Project Settings → Game → Sub3D Debug → Submarine|Flood|SimTuning).
	const USub3DDebugSettings* FloodSettings = GetDefault<USub3DDebugSettings>();
	const float DischargeCoefficient    = FloodSettings ? FloodSettings->FloodDischargeCoefficient    : 0.6f;
	const float FlowMultiplier          = FloodSettings ? FloodSettings->FloodFlowMultiplier          : 3.f;
	const float OverpressureMaxNorm     = FloodSettings ? FloodSettings->FloodOverpressureMaxNormalized : 1.05f;

	constexpr float Gravity_cm_s2 = 981.f;

	// World-Z tilt-aware sim (Approach 1 from research). Each compartment's water surface
	// elevation is the world-horizontal plane corresponding to its current volume given its
	// box geometry and the sub's current world transform. Door positions transformed to world
	// the same way. Heads above effective sill (max of compartment min-corner-Z and door Z).
	// At zero tilt this reduces to the previous sub-local formula plus a constant SubZ offset
	// — flow rates and behavior identical to before; tilt cases now physically correct.
	const FTransform SubWorldXf = GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;

	for (int32 EdgeIdx = 0; EdgeIdx < EdgeStates.Num(); ++EdgeIdx)
	{
		const FFloodEdgeState& Edge = EdgeStates[EdgeIdx];
		if (Edge.bExteriorEdge || Edge.OpenRatio < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FFloodCompartmentState* CompA = FindState(Edge.VolumeA);
		const FFloodCompartmentState* CompB = FindState(Edge.VolumeB);
		if (!CompA || !CompB)
		{
			continue;
		}

		// World-Z surfaces and effective sills.
		const float SurfaceA = ComputeSurfaceWorldZ(*CompA, SubWorldXf);
		const float SurfaceB = ComputeSurfaceWorldZ(*CompB, SubWorldXf);

		// Expose the signed head delta (A − B) to visual systems (door cascade Niagara).
		// More direct/predictable signal than the Bernoulli flow rate for VFX authoring —
		// |Δh| maps naturally onto a smoothstep for FlowIntensity01.
		EdgeStates[EdgeIdx].CurrentHeadDeltaCm = SurfaceA - SurfaceB;

		const float DoorWorldZ = SubWorldXf.TransformPosition(Edge.LocalSpillPosition).Z;

		// Effective sill on each side = max(this compartment's lowest world-Z corner, door world-Z).
		// When a compartment's box bottom sits above the door (e.g. Upper_Hub floor=130 connected
		// via a vertical hatch at spillZ=0 below it), the box's MinWorldZ is the relevant sill —
		// water below it doesn't physically exist (no floor below it).
		const float MinZA = ComputeBoxMinWorldZ(*CompA, SubWorldXf);
		const float MinZB = ComputeBoxMinWorldZ(*CompB, SubWorldXf);
		const float EffSillA = FMath::Max(MinZA, DoorWorldZ);
		const float EffSillB = FMath::Max(MinZB, DoorWorldZ);

		const float HeadA = FMath::Max(0.f, SurfaceA - EffSillA);
		const float HeadB = FMath::Max(0.f, SurfaceB - EffSillB);

		if (HeadA < KINDA_SMALL_NUMBER && HeadB < KINDA_SMALL_NUMBER)
		{
			continue;  // both sides below sill: no flow possible
		}

		// Effective passage area shrinks linearly with OpenRatio (partial-open animation = partial flow).
		const float EffectiveAreaCm2 = Edge.PassageAreaCm2 * Edge.OpenRatio;
		if (EffectiveAreaCm2 < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// Bernoulli orifice (both sides above sill = submerged) OR free weir (one side above).
		// Sign convention: positive flow = A→B.
		float SignedDesiredLps;
		if (HeadA > KINDA_SMALL_NUMBER && HeadB > KINDA_SMALL_NUMBER)
		{
			// Submerged orifice — flow driven by surface delta.
			const float DeltaH = SurfaceA - SurfaceB;
			if (FMath::IsNearlyZero(DeltaH, 0.05f))
			{
				continue;  // < 0.5mm head difference → equilibrium, skip
			}
			const float VelocityCmPerSec = DischargeCoefficient * FMath::Sqrt(2.f * Gravity_cm_s2 * FMath::Abs(DeltaH));
			// (cm² × cm/s) ÷ 1000 = L/s
			SignedDesiredLps = FMath::Sign(DeltaH) * (EffectiveAreaCm2 * VelocityCmPerSec) * 0.001f * FlowMultiplier;
		}
		else
		{
			// Free weir / outflow — only one side above sill, water spills toward the dry side.
			const float HeadSrc = FMath::Max(HeadA, HeadB);
			const float SignAB  = (HeadA > KINDA_SMALL_NUMBER) ? +1.f : -1.f;
			const float VelocityCmPerSec = DischargeCoefficient * FMath::Sqrt(2.f * Gravity_cm_s2 * HeadSrc);
			SignedDesiredLps = SignAB * (EffectiveAreaCm2 * VelocityCmPerSec) * 0.001f * FlowMultiplier;
		}

		if (FMath::IsNearlyZero(SignedDesiredLps, KINDA_SMALL_NUMBER))
		{
			continue;
		}

		const bool  bSourceIsA = SignedDesiredLps > 0.f;
		const FFloodCompartmentState* Source = bSourceIsA ? CompA : CompB;
		const FFloodCompartmentState* Dest   = bSourceIsA ? CompB : CompA;
		float DesiredRateLps = FMath::Abs(SignedDesiredLps);

		// Per-edge clamps before going into the budget passes:
		//   1. Global per-edge max (MaxInternalFlowLitersPerSec)
		//   2. Destination headroom with overpressure soft cap (1.05× capacity)
		const float OverpressureCapLiters = Dest->CapacityLiters * OverpressureMaxNorm;
		const float DestHeadroomLiters = FMath::Max(0.f, OverpressureCapLiters - Dest->CurrentWaterLiters);
		const float DestHeadroomLps = DestHeadroomLiters / FMath::Max(KINDA_SMALL_NUMBER, DeltaTime);
		DesiredRateLps = FMath::Min(DesiredRateLps, FMath::Min(MaxInternalFlowLitersPerSec, DestHeadroomLps));

		if (DesiredRateLps <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// Find indices for source and dest.
		const int32 SrcIdx = CompartmentStates.IndexOfByPredicate([&](const FFloodCompartmentState& S)
		{
			return &S == Source;
		});
		const int32 DstIdx = CompartmentStates.IndexOfByPredicate([&](const FFloodCompartmentState& S)
		{
			return &S == Dest;
		});

		if (SrcIdx == INDEX_NONE || DstIdx == INDEX_NONE)
		{
			continue;
		}

		FPendingTransfer Transfer;
		Transfer.SourceIndex   = SrcIdx;
		Transfer.DestIndex     = DstIdx;
		Transfer.EdgeIndex     = EdgeIdx;
		Transfer.bSourceIsA    = bSourceIsA;
		Transfer.DesiredRateLps = DesiredRateLps;
		PendingTransfers.Add(Transfer);
	}

	// Budget pass 1: source -- total outflow per compartment must not
	// exceed the water available in that compartment.
	{
		const int32 NumCompartments = CompartmentStates.Num();

		TArray<float> TotalDesiredOutflow;
		TotalDesiredOutflow.SetNumZeroed(NumCompartments);
		for (const FPendingTransfer& T : PendingTransfers)
		{
			TotalDesiredOutflow[T.SourceIndex] += T.DesiredRateLps;
		}

		TArray<float> OutflowScale;
		OutflowScale.SetNum(NumCompartments);
		for (int32 i = 0; i < NumCompartments; ++i)
		{
			if (TotalDesiredOutflow[i] <= KINDA_SMALL_NUMBER)
			{
				OutflowScale[i] = 1.f;
				continue;
			}
			const float AvailableRateLps = CompartmentStates[i].CurrentWaterLiters / FMath::Max(KINDA_SMALL_NUMBER, DeltaTime);
			OutflowScale[i] = FMath::Min(1.f, AvailableRateLps / TotalDesiredOutflow[i]);
		}

		// Apply source budget to get source-limited rates
		for (FPendingTransfer& T : PendingTransfers)
		{
			T.DesiredRateLps *= OutflowScale[T.SourceIndex];
		}
	}

	// Budget pass 2: destination -- total inflow per compartment must not
	// exceed the headroom (capacity - current water - already committed
	// inflows from breach/exterior in this tick).
	{
		const int32 NumCompartments = CompartmentStates.Num();

		// Headroom already consumed by breach/exterior inflows
		TArray<float> CommittedInflowRate;
		CommittedInflowRate.SetNumZeroed(NumCompartments);
		for (int32 i = 0; i < NumCompartments; ++i)
		{
			CommittedInflowRate[i] = CompartmentStates[i].FloodRateIn;
		}

		TArray<float> TotalDesiredInflow;
		TotalDesiredInflow.SetNumZeroed(NumCompartments);
		for (const FPendingTransfer& T : PendingTransfers)
		{
			if (T.DesiredRateLps > KINDA_SMALL_NUMBER)
			{
				TotalDesiredInflow[T.DestIndex] += T.DesiredRateLps;
			}
		}

		TArray<float> InflowScale;
		InflowScale.SetNum(NumCompartments);
		for (int32 i = 0; i < NumCompartments; ++i)
		{
			if (TotalDesiredInflow[i] <= KINDA_SMALL_NUMBER)
			{
				InflowScale[i] = 1.f;
				continue;
			}
			// Overpressure soft cap: allow water to exceed CapacityLiters by OverpressureMaxNorm
			// before headroom reaches zero. Avoids equalization stalls between two near-full
			// compartments (Barotrauma trick — see flood design doc §6.2).
			const float OverCap = CompartmentStates[i].CapacityLiters * OverpressureMaxNorm;
			const float Headroom = FMath::Max(0.f, OverCap - CompartmentStates[i].CurrentWaterLiters);
			const float HeadroomRateLps = Headroom / FMath::Max(KINDA_SMALL_NUMBER, DeltaTime);
			const float AvailableForInternal = FMath::Max(0.f, HeadroomRateLps - CommittedInflowRate[i]);
			InflowScale[i] = FMath::Min(1.f, AvailableForInternal / TotalDesiredInflow[i]);
		}

		// Apply both budgets, commit to FloodRateIn/Out, and write the signed per-edge flow
		// rate back to FFloodEdgeState for visual consumers (Niagara cascades, audio, FX).
		for (const FPendingTransfer& T : PendingTransfers)
		{
			const float ActualRateLps = T.DesiredRateLps * InflowScale[T.DestIndex];
			if (ActualRateLps <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			CompartmentStates[T.SourceIndex].FloodRateOut += ActualRateLps;
			CompartmentStates[T.DestIndex].FloodRateIn += ActualRateLps;

			// Sign convention: positive = A→B. bSourceIsA tells us whether the resolved source
			// is VolumeA; if yes the rate is positive, otherwise negative.
			if (EdgeStates.IsValidIndex(T.EdgeIndex))
			{
				EdgeStates[T.EdgeIndex].CurrentFlowRateLitersPerSec = T.bSourceIsA ? ActualRateLps : -ActualRateLps;
			}
		}
	}

	// 4. Pumps
	for (FFloodCompartmentState& S : CompartmentStates)
	{
		if (!S.bPumpActive || S.PumpRateLitersPerSec <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float AvailableWaterLiters = FMath::Clamp(
			S.CurrentWaterLiters + (S.FloodRateIn - S.FloodRateOut) * DeltaTime,
			0.f,
			S.CapacityLiters);
		const float MaxPumpByAvailable = AvailableWaterLiters / FMath::Max(KINDA_SMALL_NUMBER, DeltaTime);
		const float EffectivePumpOut = FMath::Min(S.PumpRateLitersPerSec, MaxPumpByAvailable);
		S.FloodRateOut += FMath::Max(0.f, EffectivePumpOut);
	}

	// 5. Apply net flow (water is conserved: total out <= source available).
	//    Hard cap stays at CapacityLiters for downstream consumers (FloodLevel = Cur/Cap), but
	//    pass-2 budgets allow up to OverpressureMaxNorm × CapacityLiters in transit so two
	//    near-full compartments can still equalize. Any excess gets clamped here and discarded;
	//    in practice the overpressure window is so small that the clamp triggers rarely.
	for (FFloodCompartmentState& S : CompartmentStates)
	{
		const float DeltaLiters = (S.FloodRateIn - S.FloodRateOut) * DeltaTime;
		S.CurrentWaterLiters = FMath::Clamp(S.CurrentWaterLiters + DeltaLiters, 0.f, S.CapacityLiters);
	}

	// 6. Derive normalized levels and heights
	UpdateDerivedState();
	OnFloodStateUpdated.Broadcast(GetExportedStates());
}

void USubFloodComponent::UpdateDerivedState()
{
	for (FFloodCompartmentState& S : CompartmentStates)
	{
		S.WaterLevelNormalized = S.CapacityLiters > 0.f
			? FMath::Clamp(S.CurrentWaterLiters / S.CapacityLiters, 0.f, 1.f)
			: 0.f;
		S.MaxWaterHeightCm = FMath::Max(MinCompartmentHeightCm, S.MaxWaterHeightCm);
		S.WaterHeightCm = S.WaterLevelNormalized * S.MaxWaterHeightCm;
	}
}

void USubFloodComponent::MaybeLogWaterLevels(float DeltaTime)
{
	const USub3DDebugSettings* Settings = GetDefault<USub3DDebugSettings>();
	const bool bEffectiveLog = bLogWaterLevels || (Settings && Settings->ShouldLogFlood());
	const float EffectiveInterval = Settings && Settings->ShouldLogFlood()
		? Settings->FloodLogIntervalSeconds
		: WaterLevelLogIntervalSeconds;

	if (!bEffectiveLog || CompartmentStates.Num() == 0)
	{
		WaterLevelLogAccumulator = 0.f;
		return;
	}

	WaterLevelLogAccumulator += DeltaTime;
	if (WaterLevelLogAccumulator < FMath::Max(0.1f, EffectiveInterval))
	{
		return;
	}
	WaterLevelLogAccumulator = 0.f;

	FString Summary;
	Summary.Reserve(CompartmentStates.Num() * 48);
	for (int32 i = 0; i < CompartmentStates.Num(); ++i)
	{
		const FFloodCompartmentState& S = CompartmentStates[i];
		if (i > 0)
		{
			Summary += TEXT(" | ");
		}
		Summary += FString::Printf(
			TEXT("%s H=%.1fcm L=%.2f W=%.0fL"),
			*S.CompartmentId.ToString(),
			S.WaterHeightCm,
			S.WaterLevelNormalized,
			S.CurrentWaterLiters);
	}

	UE_LOG(LogSubFlood, Log, TEXT("SubFlood | %s | %s"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("?"), *Summary);
}

// --- Internal helpers ----------------------------------------------------

FFloodCompartmentState* USubFloodComponent::FindState(FName CompartmentId)
{
	return CompartmentStates.FindByPredicate([&](const FFloodCompartmentState& S)
	{
		return S.CompartmentId == CompartmentId;
	});
}

const FFloodCompartmentState* USubFloodComponent::FindState(FName CompartmentId) const
{
	return CompartmentStates.FindByPredicate([&](const FFloodCompartmentState& S)
	{
		return S.CompartmentId == CompartmentId;
	});
}

FFloodEdgeState* USubFloodComponent::FindEdge(FName ClosureId)
{
	return EdgeStates.FindByPredicate([&](const FFloodEdgeState& E)
	{
		return E.ClosureId == ClosureId;
	});
}

void USubFloodComponent::OnRep_CompartmentStates()
{
	// Reset initialization flag when states are cleared (server re-initialization).
	if (CompartmentStates.Num() == 0)
	{
		bClientInitialized = false;
		return;
	}

	UpdateDerivedState();

	if (!bClientInitialized)
	{
		bClientInitialized = true;
		OnFloodInitialized.Broadcast();
	}

	OnFloodStateUpdated.Broadcast(GetExportedStates());
}

// --- Private helper for broadcast ----------------------------------------

TArray<FCompartmentState> USubFloodComponent::GetExportedStates() const
{
	TArray<FCompartmentState> Out;
	ExportCompartmentStates(Out);
	return Out;
}
