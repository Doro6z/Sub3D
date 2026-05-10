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
	for (const FGeneratedCompartmentDef& Comp : Definition->Compartments)
	{
		FFloodCompartmentState State;
		State.CompartmentId = Comp.CompartmentId;
		State.CapacityLiters = Comp.CapacityLiters;
		State.MaxWaterHeightCm = FMath::Max(MinCompartmentHeightCm, Comp.MaxWaterHeightCm);
		State.WalkableFloorZCm = Comp.WalkableFloorZCm;
		State.CurrentWaterLiters = 0.f;
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

		// Initial closed state. Resolution rules:
		//  1. If the connection is typed Open → permanently bClosed=false (never closable).
		//  2. Otherwise → use the connection's bStartsClosed (Door / Hatch / ExteriorHatch).
		//  3. No matching connection → bClosed=true (safe default; an unknown edge stays sealed).
		if (!Edge.ClosureId.IsNone())
		{
			const FGeneratedConnectionDef* Conn = Definition->FindConnection(Edge.ClosureId);
			if (Conn && Conn->ConnectionType == EConnectionType::Open)
			{
				ES.bClosed = false;
			}
			else
			{
				ES.bClosed = Conn ? Conn->bStartsClosed : true;
			}
		}
		else
		{
			ES.bClosed = false;
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
		ES.bClosed = true; // doors start closed

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

void USubFloodComponent::SetDoorState(FName ConnectionId, bool bClosed)
{
	if (!HasAuthority(this))
	{
		return;
	}

	if (ConnectionId.IsNone())
	{
		return;
	}

	FFloodEdgeState* Edge = FindEdge(ConnectionId);
	if (Edge)
	{
		Edge->bClosed = bClosed;
		UE_LOG(LogSubFlood, Log, TEXT("SetDoorState: edge '%s' bClosed=%d"),
			*ConnectionId.ToString(), bClosed ? 1 : 0);
	}
	else
	{
		UE_LOG(LogSubFlood, Warning, TEXT("SetDoorState: no edge found for ConnectionId '%s'"),
			*ConnectionId.ToString());
	}
}

void USubFloodComponent::SetDoorStateByCompartments(FName CompA, FName CompB, bool bClosed)
{
	if (!HasAuthority(this) || CompA.IsNone() || CompB.IsNone())
	{
		return;
	}

	for (FFloodEdgeState& Edge : EdgeStates)
	{
		const bool bMatch =
			(Edge.VolumeA == CompA && Edge.VolumeB == CompB) ||
			(Edge.VolumeA == CompB && Edge.VolumeB == CompA);
		if (bMatch)
		{
			Edge.bClosed = bClosed;
			UE_LOG(LogSubFlood, Log,
				TEXT("SetDoorStateByCompartments: edge '%s' (A=%s B=%s) bClosed=%d"),
				*Edge.ClosureId.ToString(),
				*Edge.VolumeA.ToString(), *Edge.VolumeB.ToString(),
				bClosed ? 1 : 0);
			return;
		}
	}

	UE_LOG(LogSubFlood, Warning,
		TEXT("SetDoorStateByCompartments: no edge found for compartments A='%s' B='%s'"),
		*CompA.ToString(), *CompB.ToString());
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
		if (!Edge.bExteriorEdge || Edge.bClosed)
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
		float DesiredRateLps; // liters per second
	};
	TArray<FPendingTransfer> PendingTransfers;

	for (const FFloodEdgeState& Edge : EdgeStates)
	{
		if (Edge.bExteriorEdge || Edge.bClosed)
		{
			continue;
		}

		const FFloodCompartmentState* CompA = FindState(Edge.VolumeA);
		const FFloodCompartmentState* CompB = FindState(Edge.VolumeB);
		if (!CompA || !CompB)
		{
			continue;
		}

		// Absolute Z of each compartment's water surface (sub-local space). This includes
		// the compartment floor offset so vertical connections respect gravity: water in a
		// lower compartment only flows up to a higher one when its surface actually exceeds
		// the higher compartment's floor (otherwise the lower one fills up first).
		const float SurfaceA = CompA->WalkableFloorZCm + CompA->WaterHeightCm;
		const float SurfaceB = CompB->WalkableFloorZCm + CompB->WaterHeightCm;
		float HeightDeltaCm = SurfaceA - SurfaceB;

		// FP overflow rule for vertical connections: if one compartment sits significantly
		// higher than the other, the strict absolute-Z gate may block any flow even when the
		// lower compartment is full (e.g. Main_Hub max water = 112, Upper_Hub floor = 130 →
		// gap of 18cm physically unreachable). In that case, once the lower compartment is
		// essentially full and the higher one has headroom, force an overflow flow upward.
		// Models "water spills up the open staircase when the source can't hold any more."
		constexpr float VerticalThresholdCm = 50.f;
		constexpr float ForcedOverflowDeltaCm = 30.f;
		const float FloorDelta = CompA->WalkableFloorZCm - CompB->WalkableFloorZCm;
		const bool bAHigher = FloorDelta >  VerticalThresholdCm;
		const bool bBHigher = FloorDelta < -VerticalThresholdCm;
		if (bAHigher || bBHigher)
		{
			const FFloodCompartmentState* Lower  = bAHigher ? CompB : CompA;
			const FFloodCompartmentState* Higher = bAHigher ? CompA : CompB;
			const bool bLowerFull  = (Lower->WaterLevelNormalized  >= 0.99f);
			const bool bHigherFull = (Higher->WaterLevelNormalized >= 1.0f - KINDA_SMALL_NUMBER);
			if (bLowerFull && !bHigherFull)
			{
				// Drive a flow Lower → Higher. Sign matches whichever side is the source.
				HeightDeltaCm = (Lower == CompA) ? ForcedOverflowDeltaCm : -ForcedOverflowDeltaCm;
			}
		}

		if (FMath::IsNearlyZero(HeightDeltaCm, KINDA_SMALL_NUMBER))
		{
			continue;
		}

		const FFloodCompartmentState* Source = HeightDeltaCm > 0.f ? CompA : CompB;
		const FFloodCompartmentState* Dest = HeightDeltaCm > 0.f ? CompB : CompA;

		const float SourceLitersPerCm = Source->MaxWaterHeightCm > KINDA_SMALL_NUMBER
			? Source->CapacityLiters / Source->MaxWaterHeightCm
			: 0.f;
		const float DestLitersPerCm = Dest->MaxWaterHeightCm > KINDA_SMALL_NUMBER
			? Dest->CapacityLiters / Dest->MaxWaterHeightCm
			: 0.f;
		const float HeightResponsePerLiter = (SourceLitersPerCm > KINDA_SMALL_NUMBER ? 1.f / SourceLitersPerCm : 0.f)
			+ (DestLitersPerCm > KINDA_SMALL_NUMBER ? 1.f / DestLitersPerCm : 0.f);
		if (HeightResponsePerLiter <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float ConnectionAreaScale = FMath::Clamp(
			Edge.PassageAreaCm2 / FMath::Max(1.f, InternalConnectionAreaDivisorCm2),
			0.f,
			8.f);
		const float DesiredTransferLiters = FMath::Abs(HeightDeltaCm) / HeightResponsePerLiter;
		const float DestHeadroom = FMath::Max(0.f, Dest->CapacityLiters - Dest->CurrentWaterLiters);
		const float DesiredRateLps = FMath::Clamp(
			(DesiredTransferLiters / FMath::Max(KINDA_SMALL_NUMBER, DeltaTime)) * ConnectionAreaScale,
			0.f,
			FMath::Min(MaxInternalFlowLitersPerSec, DestHeadroom / FMath::Max(KINDA_SMALL_NUMBER, DeltaTime)));

		if (DesiredRateLps <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// Find indices for source and dest
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
		Transfer.SourceIndex = SrcIdx;
		Transfer.DestIndex = DstIdx;
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
			const float Headroom = FMath::Max(0.f, CompartmentStates[i].CapacityLiters - CompartmentStates[i].CurrentWaterLiters);
			const float HeadroomRateLps = Headroom / FMath::Max(KINDA_SMALL_NUMBER, DeltaTime);
			const float AvailableForInternal = FMath::Max(0.f, HeadroomRateLps - CommittedInflowRate[i]);
			InflowScale[i] = FMath::Min(1.f, AvailableForInternal / TotalDesiredInflow[i]);
		}

		// Apply both budgets and commit to FloodRateIn/Out
		for (const FPendingTransfer& T : PendingTransfers)
		{
			const float ActualRateLps = T.DesiredRateLps * InflowScale[T.DestIndex];
			if (ActualRateLps <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			CompartmentStates[T.SourceIndex].FloodRateOut += ActualRateLps;
			CompartmentStates[T.DestIndex].FloodRateIn += ActualRateLps;
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

	// 5. Apply net flow (water is conserved: total out <= source available)
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
