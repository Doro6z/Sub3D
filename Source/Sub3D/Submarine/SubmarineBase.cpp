#include "SubmarineBase.h"
#include "Sub3DDebugSettings.h"

#include "BreachVfxManagerComponent.h"
#include "CompartmentVolumeComponent.h"
#include "FloodWaterPlaneComponent.h"
#include "DoorFloodVfxComponent.h"
#include "FloodWaterVisualsComponent.h"
#include "GeneratedGeometry/SubmarineGeneratedGeometryComponent.h"
#include "ProceduralMeshComponent.h"
#include "Generator/SubmarineDefinition.h"
#include "Generator/SubmarineGenerator.h"
#include "Generator/SubmarineGeneratorSpec.h"
#include "Generator/SubmarineMeshBuilder.h"
#include "SubmarineLayoutAsset.h"
#include "HelmNavigationDisplayComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Net/UnrealNetwork.h"
#include "SubHullVisualDamageComponent.h"
#include "SubmarineFeedbackDirectorComponent.h"
#include "SubSonarComponent.h"
#include "SubSonarSystemComponent.h"
#include "TunnelNavigationRuntimeComponent.h"
#include "SubDoorActor.h"
#include "SubFloodComponent.h"
#include "SubHullComponent.h"
#include "SubMovementComponent.h"
#include "SubmarineCompartmentComponent.h"
#include "SubmarineRadarComponent.h"
#include "SubmarineStationManagerComponent.h"
#include "SubmarineSystemsComponent.h"
#include "SubLegacyLog.h"
#include "TurretActor.h"

DEFINE_LOG_CATEGORY(LogSubLegacy);

namespace
{
float GetCompartmentVolumeSelectionScore(const UCompartmentVolumeComponent* Volume)
{
	if (!Volume)
	{
		return -1.f;
	}

	const FVector Extent = Volume->GetScaledBoxExtent();
	return Extent.X * Extent.Y * Extent.Z;
}

bool IsComponentOnOrOwnedBySubmarine(const ASubmarineBase* Submarine, const UPrimitiveComponent* Component)
{
	if (!Submarine || !Component)
	{
		return false;
	}

	const AActor* ComponentOwner = Component->GetOwner();
	return ComponentOwner == Submarine
		|| (ComponentOwner && ComponentOwner->GetOwner() == Submarine)
		|| (ComponentOwner && ComponentOwner->GetAttachParentActor() == Submarine)
		|| (ComponentOwner && ComponentOwner->IsAttachedTo(Submarine));
}

bool HasAuthoritativeContext(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	return Actor->GetLocalRole() == ROLE_Authority;
}

int32 CountUsableSimpleShapes(const UBodySetup* BodySetup)
{
	if (!BodySetup)
	{
		return 0;
	}

	const FKAggregateGeom& AggGeom = BodySetup->AggGeom;
	return AggGeom.SphereElems.Num()
		+ AggGeom.BoxElems.Num()
		+ AggGeom.SphylElems.Num()
		+ AggGeom.ConvexElems.Num()
		+ AggGeom.TaperedCapsuleElems.Num();
}

bool HasAssignedCollisionProxyMesh(const UStaticMeshComponent* CollisionProxy)
{
	return IsValid(CollisionProxy) && IsValid(CollisionProxy->GetStaticMesh());
}

bool ValidateMovementCollisionComponentConfig(const UPrimitiveComponent* CollisionComp, FString& OutReason)
{
	if (!CollisionComp)
	{
		OutReason = TEXT("Movement collision component is null.");
		return false;
	}

	if (CollisionComp->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		OutReason = FString::Printf(
			TEXT("Movement collision component '%s' has collision disabled."),
			*GetNameSafe(CollisionComp));
		return false;
	}

	const UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(CollisionComp);
	if (!StaticMeshComp)
	{
		return true;
	}

	const UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
	if (!StaticMesh)
	{
		OutReason = FString::Printf(
			TEXT("Movement collision component '%s' has no static mesh assigned."),
			*GetNameSafe(CollisionComp));
		return false;
	}

	const UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	if (!BodySetup)
	{
		OutReason = FString::Printf(
			TEXT("Movement collision component '%s' uses static mesh '%s' with no BodySetup."),
			*GetNameSafe(CollisionComp),
			*GetNameSafe(StaticMesh));
		return false;
	}

	if (BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple)
	{
		OutReason = FString::Printf(
			TEXT("Movement collision component '%s' uses static mesh '%s' with Collision Complexity=UseComplexAsSimple. ")
			TEXT("The current engine sweep path for the moving submarine hull does not support triangle-mesh query shapes. ")
			TEXT("UseSimpleAndComplex or a dedicated movement collision proxy."),
			*GetNameSafe(CollisionComp),
			*GetNameSafe(StaticMesh));
		return false;
	}

	if (CountUsableSimpleShapes(BodySetup) == 0)
	{
		OutReason = FString::Printf(
			TEXT("Movement collision component '%s' uses static mesh '%s' with zero usable simple collision shapes. ")
			TEXT("Add box/capsule/convex simple collision for the moving hull sweep."),
			*GetNameSafe(CollisionComp),
			*GetNameSafe(StaticMesh));
		return false;
	}

	return true;
}

void LogMovementCollisionValidationIssue(const AActor* Owner, const FString& Reason)
{
	static TMap<TWeakObjectPtr<const AActor>, FString> LastLoggedReasons;
	const TWeakObjectPtr<const AActor> OwnerKey(Owner);
	const FString* LastReason = LastLoggedReasons.Find(OwnerKey);
	if (LastReason && *LastReason == Reason)
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("MovementCollision invalid | Owner=%s | Reason=%s"),
		*GetNameSafe(Owner),
		*Reason);

	LastLoggedReasons.Add(OwnerKey, Reason);
}
}

ASubmarineBase::ASubmarineBase()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);
	// Match the sim rate (60Hz) so each fixed-tick produces one snapshot. With 30Hz, each
	// snapshot covered 2 sim substeps; under flood-induced acceleration the inter-snapshot
	// motion delta grew large enough that any cadence variance produced visible jitter on
	// the client Hermite playback. 60Hz halves the delta and tightens the InterpDuration
	// variance window.
	SetNetUpdateFrequency(60.f);
	SetMinNetUpdateFrequency(30.f);

	SubmarineRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SubmarineRoot"));
	SetRootComponent(SubmarineRoot);

	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	HullMesh->SetupAttachment(SubmarineRoot);

	MovementCollisionProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MovementCollisionProxy"));
	MovementCollisionProxy->SetupAttachment(HullMesh);

	ApplyHullCollisionDefaults();

	SubMovement = CreateDefaultSubobject<USubMovementComponent>(TEXT("SubMovement"));
	SubHull = CreateDefaultSubobject<USubHullComponent>(TEXT("SubHull"));
	SubFlood = CreateDefaultSubobject<USubFloodComponent>(TEXT("SubFlood"));
	Systems = CreateDefaultSubobject<USubmarineSystemsComponent>(TEXT("Systems"));
	Compartments = CreateDefaultSubobject<USubmarineCompartmentComponent>(TEXT("Compartments"));
	StationManager = CreateDefaultSubobject<USubmarineStationManagerComponent>(TEXT("StationManager"));
	Radar = CreateDefaultSubobject<USubmarineRadarComponent>(TEXT("Radar"));
	BreachVfxManager = CreateDefaultSubobject<UBreachVfxManagerComponent>(TEXT("BreachVfxManager"));
	FloodWaterVisuals = CreateDefaultSubobject<UFloodWaterVisualsComponent>(TEXT("FloodWaterVisuals"));
	HullVisualDamage = CreateDefaultSubobject<USubHullVisualDamageComponent>(TEXT("HullVisualDamage"));
	DoorFloodVfx = CreateDefaultSubobject<UDoorFloodVfxComponent>(TEXT("DoorFloodVfx"));
	FeedbackManager = CreateDefaultSubobject<USubmarineFeedbackDirectorComponent>(TEXT("FeedbackManager"));
	Sonar = CreateDefaultSubobject<USubSonarComponent>(TEXT("Sonar"));
	SonarSystem = CreateDefaultSubobject<USubSonarSystemComponent>(TEXT("SonarSystem"));
	TunnelNavigationRuntime = CreateDefaultSubobject<UTunnelNavigationRuntimeComponent>(TEXT("TunnelNavigationRuntime"));
	HelmNavigationDisplay = CreateDefaultSubobject<UHelmNavigationDisplayComponent>(TEXT("HelmNavigationDisplay"));
	GeneratedGeometry = CreateDefaultSubobject<USubmarineGeneratedGeometryComponent>(TEXT("GeneratedGeometry"));

	HelmSocket = CreateDefaultSubobject<USceneComponent>(TEXT("HelmSocket"));
	HelmSocket->SetupAttachment(HullMesh);
	HelmSocket->SetRelativeLocation(FVector(-200.f, 0.f, 50.f));

	CrewSpawnSocketP1 = CreateDefaultSubobject<USceneComponent>(TEXT("CrewSpawnSocketP1"));
	CrewSpawnSocketP1->SetupAttachment(HullMesh);
	CrewSpawnSocketP1->SetRelativeLocation(FVector(-350.f, 0.f, 92.f));

	TurretHardpoint = CreateDefaultSubobject<USceneComponent>(TEXT("TurretHardpoint"));
	TurretHardpoint->SetupAttachment(HullMesh);
	TurretHardpoint->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
}

void ASubmarineBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyHullCollisionDefaults();
}

void ASubmarineBase::BeginPlay()
{
	Super::BeginPlay();

	ApplyHullCollisionDefaults();
	RefreshMovementCollisionBinding();

	// ── Step 1: Generate definition from spec if needed (all machines) ──
	// The pipeline Spec → Generate() → MeshBuilder is pure and deterministic.
	// Every machine produces the same Definition from the same Spec asset.
	if (!GeneratedDefinition && GeneratorSpec)
	{
		USubmarineGenerator* Generator = NewObject<USubmarineGenerator>(this);
		GeneratedDefinition = Generator->Generate(GeneratorSpec);

		if (GeneratedDefinition && GeneratorSpec->Envelope)
		{
			USubmarineMeshBuilder* MeshBuilder = NewObject<USubmarineMeshBuilder>(this);
			MeshBuilder->BuildMeshData(GeneratedDefinition, GeneratorSpec->Envelope);
		}
	}

	// Apply authored performance profile (mass, Vmax, MaxThrust) onto the
	// movement component. Handmade submarines (Craniata) and generated ones
	// share this path: populate the Definition asset, values land here.
	if (SubMovement && GeneratedDefinition)
	{
		SubMovement->ApplyPerformanceProfileFromDefinition(GeneratedDefinition);
	}

	// ── Step 2: Stations ─────────────────────────────────────────────────
	if (StationManager)
	{
		StationManager->DiscoverAttachedStations();

		if (HasAuthority()
			&& StationManager->GetRegisteredStations().Num() == 0
			&& GeneratedDefinition
			&& GeneratedDefinition->StationSlots.Num() > 0)
		{
			StationManager->SpawnStationsFromDefinition(GeneratedDefinition);
		}
	}

	// ── Step 3: Authority-only runtime systems ───────────────────────────
	if (HasAuthority())
	{
		ResolveExteriorTurret();

		if (SubFlood)
		{
			// Priority order:
			//  1) Manually-placed UCompartmentVolumeComponents (Craniata BP workflow) —
			//     they're authoritative for compartment spatial identity, crew overlap
			//     uses their CompartmentId. Flood graph must match.
			//  2) GeneratedDefinition (procedural generator path).
			//  3) LayoutAsset (Proto 7A legacy).
			TArray<UCompartmentVolumeComponent*> Volumes;
			GetComponents<UCompartmentVolumeComponent>(Volumes);

			if (Volumes.Num() > 0)
			{
				SubFlood->InitializeFromCompartmentVolumes(Volumes);
			}
			else if (GeneratedDefinition)
			{
				SubFlood->InitializeFromDefinition(GeneratedDefinition);
			}
			else if (SubHull && SubHull->LayoutAsset)
			{
				// LEGACY (Phase 7A, 2026-04-10) — Proto fallback path.
				UE_LOG(LogSubLegacy, Warning,
					TEXT("[LEGACY] ASubmarineBase::BeginPlay: initializing SubFlood from LayoutAsset '%s' on %s. ")
					TEXT("Will be removed in Phase 7B."),
					*SubHull->LayoutAsset->GetName(),
					*GetName());
				SubFlood->InitializeFromLayout(SubHull->LayoutAsset);
			}
			else
			{
				UE_LOG(
					LogSubLegacy,
					Warning,
					TEXT("ASubmarineBase::BeginPlay: SubFlood not initialized on %s — no volumes, Definition, or LayoutAsset."),
					*GetName());
			}
		}

		if (SubHull && SubFlood)
		{
			SubHull->OnBreachesUpdated.AddDynamic(this, &ASubmarineBase::HandleBreachesUpdatedForFlood);
		}
	}

	// ── Flood Visuals: spawn one UFloodWaterPlaneComponent per compartment volume ─
	// Runs on ALL net roles so each client renders water locally without needing
	// replication of plane transforms. The material (DefaultWaterMaterial) carries
	// all visual intelligence (Global DF clip, refraction); the component is 100%
	// passive C++ (updates Z + visibility from its SourceVolume).
	//
	// This block is OUTSIDE the HasAuthority() branch on purpose — clients have
	// the same UCompartmentVolumeComponents placed in the BP as the server.
	{
		const USub3DDebugSettings* DebugSettingsRef = GetDefault<USub3DDebugSettings>();
		if (!(DebugSettingsRef && DebugSettingsRef->bDisableFloodWaterPlanes))
		{
			TArray<UCompartmentVolumeComponent*> VisualVolumes;
			GetComponents<UCompartmentVolumeComponent>(VisualVolumes);

			TMap<FName, UCompartmentVolumeComponent*> SelectedVolumeByCompartment;
			TMap<FName, int32> VolumeCountByCompartment;

			for (UCompartmentVolumeComponent* Vol : VisualVolumes)
			{
				if (!Vol)
				{
					continue;
				}

				if (Vol->CompartmentId.IsNone())
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT("FloodWaterPlane skipped | Sub=%s | Volume=%s has no CompartmentId"),
						*GetName(),
						*GetNameSafe(Vol));
					continue;
				}

				VolumeCountByCompartment.FindOrAdd(Vol->CompartmentId)++;

				UCompartmentVolumeComponent*& Selected = SelectedVolumeByCompartment.FindOrAdd(Vol->CompartmentId);
				if (!Selected)
				{
					Selected = Vol;
					continue;
				}

				const bool bSelectedHasAuthoredCap = Selected->WaterPlaneMeshOverride != nullptr;
				const bool bCandidateHasAuthoredCap = Vol->WaterPlaneMeshOverride != nullptr;
				if (!bSelectedHasAuthoredCap && bCandidateHasAuthoredCap)
				{
					Selected = Vol;
					continue;
				}

				if (bSelectedHasAuthoredCap == bCandidateHasAuthoredCap
					&& GetCompartmentVolumeSelectionScore(Vol) > GetCompartmentVolumeSelectionScore(Selected))
				{
					Selected = Vol;
				}
			}

			for (const TPair<FName, int32>& Pair : VolumeCountByCompartment)
			{
				if (Pair.Value > 1)
				{
					if (UCompartmentVolumeComponent* const* Chosen = SelectedVolumeByCompartment.Find(Pair.Key))
					{
						UE_LOG(
							LogTemp,
							Warning,
							TEXT("FloodWaterPlane dedup | Sub=%s | CompartmentId=%s | VolumeCount=%d | ChosenVolume=%s. Multi-volume compartments share one visual water plane in the current runtime path."),
							*GetName(),
							*Pair.Key.ToString(),
							Pair.Value,
							*GetNameSafe(*Chosen));
					}
				}
			}

			for (const TPair<FName, UCompartmentVolumeComponent*>& Pair : SelectedVolumeByCompartment)
			{
				UCompartmentVolumeComponent* Vol = Pair.Value;
				if (!Vol)
				{
					continue;
				}

				UFloodWaterPlaneComponent* Plane = NewObject<UFloodWaterPlaneComponent>(this);
				if (!Plane)
				{
					continue;
				}

				Plane->SourceVolume = Vol;
				if (DefaultWaterMaterial)
				{
					Plane->WaterMaterial = DefaultWaterMaterial;
				}
				if (DefaultWaterPlaneMesh)
				{
					Plane->PlaneMesh = DefaultWaterPlaneMesh;
				}
				Plane->PlaneWorldSizeCm = DefaultWaterPlaneWorldSizeCm;
				Plane->SetupAttachment(Vol);
				Plane->RegisterComponent();
			}
		}
	}

	// ── Step 4: Geometry (all machines that display the submarine) ───────
	if (GeneratedDefinition && GeneratedGeometry)
	{
		GeneratedGeometry->BuildFromDefinition(GeneratedDefinition);

		// Rebind movement collision to use generated hull PMCs instead of HullMesh.
		RefreshMovementCollisionBinding();
	}

	// ── Step 5: Door actors (authority only) ─────────────────────────────
	// Spawn interactable door actors from the generator's connection graph.
	// Only the authority needs to spawn them; replication mirrors them to
	// clients. Doors without DoorActorClass set are skipped with a warning.
	if (HasAuthority())
	{
		SpawnDoorsFromDefinition();
	}
}

void ASubmarineBase::SpawnDoorsFromDefinition()
{
	if (!GeneratedDefinition)
	{
		return;
	}

	if (!GeneratorDoorActorClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SpawnDoorsFromDefinition] %s: GeneratorDoorActorClass not set. No doors will be spawned from %d connections."),
			*GetName(),
			GeneratedDefinition->Connections.Num());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		// Called without a world (e.g. from automation test via RebuildFromSpec).
		// SpawnActor requires a valid world; return safely so the caller can
		// still exercise the rest of the pipeline.
		return;
	}

	int32 SpawnedCount = 0;
	for (const FGeneratedConnectionDef& Conn : GeneratedDefinition->Connections)
	{
		// Only spawn actors for traversable connection types. Skip Open
		// (no physical door needed) and any future passive connection types.
		const bool bIsDoorLike =
			(Conn.ConnectionType == EConnectionType::Door) ||
			(Conn.ConnectionType == EConnectionType::Hatch) ||
			(Conn.ConnectionType == EConnectionType::ExteriorHatch);
		if (!bIsDoorLike)
		{
			continue;
		}

		// Local transform lives in submarine local space; convert to world.
		const FTransform WorldTransform = Conn.LocalTransform * GetActorTransform();

		// Spawn deferred so InitializeFromConnectionDef runs BEFORE BeginPlay.
		// SubDoorActor::BeginPlay copies bStartsClosed into bClosed, calls
		// RegisterWithCompartments (which reads DoorId + compartments), and
		// pushes the state to SubFlood. Doing the init after a non-deferred
		// SpawnActor would leave the door with DoorId=None on first tick and
		// fail to register against the flood graph.
		ASubDoorActor* Door = World->SpawnActorDeferred<ASubDoorActor>(
			GeneratorDoorActorClass,
			WorldTransform,
			/*Owner=*/this,
			/*Instigator=*/nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Door)
		{
			continue;
		}

		Door->InitializeFromConnectionDef(Conn, this);
		Door->FinishSpawning(WorldTransform);
		Door->AttachToComponent(SubmarineRoot, FAttachmentTransformRules::KeepWorldTransform);
		SpawnedGeneratorDoors.Add(Door);
		++SpawnedCount;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[SpawnDoorsFromDefinition] %s: spawned %d doors from %d connections"),
		*GetName(),
		SpawnedCount,
		GeneratedDefinition->Connections.Num());
}

void ASubmarineBase::DestroySpawnedGeneratorDoors()
{
	for (TObjectPtr<ASubDoorActor>& DoorPtr : SpawnedGeneratorDoors)
	{
		if (ASubDoorActor* Door = DoorPtr.Get())
		{
			Door->Destroy();
		}
	}
	SpawnedGeneratorDoors.Reset();
}

void ASubmarineBase::RebuildFromSpec()
{
	if (!GeneratorSpec)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[RebuildFromSpec] %s: no GeneratorSpec assigned. Assign one in the Details panel first."),
			*GetName());
		return;
	}

	// Clear existing generated state before regenerating.
	DestroySpawnedGeneratorDoors();
	if (GeneratedGeometry)
	{
		GeneratedGeometry->ClearGeometry();
	}
	GeneratedDefinition = nullptr;

	// Run the same generation chain as BeginPlay Step 1.
	USubmarineGenerator* Generator = NewObject<USubmarineGenerator>(this);
	GeneratedDefinition = Generator->Generate(GeneratorSpec);

	if (!GeneratedDefinition)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[RebuildFromSpec] %s: Generate() returned null from spec '%s'."),
			*GetName(),
			*GeneratorSpec->GetName());
		return;
	}

	if (GeneratorSpec->Envelope)
	{
		USubmarineMeshBuilder* MeshBuilder = NewObject<USubmarineMeshBuilder>(this);
		MeshBuilder->BuildMeshData(GeneratedDefinition, GeneratorSpec->Envelope);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[RebuildFromSpec] %s: GeneratorSpec '%s' has no Envelope. Mesh data will be empty."),
			*GetName(),
			*GeneratorSpec->GetName());
	}

	// Rebuild visible geometry (same as BeginPlay Step 4).
	if (GeneratedGeometry)
	{
		GeneratedGeometry->BuildFromDefinition(GeneratedDefinition);
	}

	// Respawn door actors from the fresh Definition. Safe no-op if no world
	// or no DoorActorClass; SpawnDoorsFromDefinition handles both cases.
	SpawnDoorsFromDefinition();

	UE_LOG(LogTemp, Log,
		TEXT("[RebuildFromSpec] %s: complete. Compartments=%d Connections=%d Stations=%d Doors=%d"),
		*GetName(),
		GeneratedDefinition->Compartments.Num(),
		GeneratedDefinition->Connections.Num(),
		GeneratedDefinition->StationSlots.Num(),
		SpawnedGeneratorDoors.Num());
}

void ASubmarineBase::ClearGeneratedState()
{
	DestroySpawnedGeneratorDoors();
	if (GeneratedGeometry)
	{
		GeneratedGeometry->ClearGeometry();
	}
	GeneratedDefinition = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[ClearGeneratedState] %s: cleared definition, geometry, and spawned doors."), *GetName());
}

void ASubmarineBase::ApplyHullCollisionDefaults()
{
	if (!HullMesh || !MovementCollisionProxy)
	{
		return;
	}

	const bool bUseMovementProxy = HasAssignedCollisionProxyMesh(MovementCollisionProxy);

	// Keep HullMesh query collision alive even when a movement proxy exists.
	// MovementCollisionProxy is the submarine-vs-world sweep shape; HullMesh is
	// still allowed to block crew pawns and camera/visibility traces.
	HullMesh->SetCollisionEnabled(bUseMovementProxy ? ECollisionEnabled::QueryOnly : ECollisionEnabled::QueryAndPhysics);
	HullMesh->SetCollisionProfileName(TEXT("SubmarineHull"));
	HullMesh->SetNotifyRigidBodyCollision(!bUseMovementProxy);
	HullMesh->SetGenerateOverlapEvents(false);
	HullMesh->SetCanEverAffectNavigation(false);
	HullMesh->SetMobility(EComponentMobility::Movable);
	HullMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	MovementCollisionProxy->SetCollisionProfileName(TEXT("SubmarineHull"));
	MovementCollisionProxy->SetCollisionEnabled(bUseMovementProxy ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	MovementCollisionProxy->SetNotifyRigidBodyCollision(bUseMovementProxy);
	MovementCollisionProxy->SetGenerateOverlapEvents(false);
	MovementCollisionProxy->SetCanEverAffectNavigation(false);
	MovementCollisionProxy->SetMobility(EComponentMobility::Movable);
	MovementCollisionProxy->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	MovementCollisionProxy->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	MovementCollisionProxy->SetHiddenInGame(true);
}

UPrimitiveComponent* ASubmarineBase::GetMovementCollisionComponent() const
{
	// Prefer generated hull collision when available.
	if (GeneratedGeometry && GeneratedGeometry->GetExteriorHullCollisionComponents().Num() > 0)
	{
		return GeneratedGeometry->GetExteriorHullCollisionComponents()[0];
	}

	if (HasAssignedCollisionProxyMesh(MovementCollisionProxy)
		&& MovementCollisionProxy->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		return MovementCollisionProxy;
	}

	return HullMesh;
}

void ASubmarineBase::RefreshMovementCollisionBinding()
{
	// Unbind previous single-component binding.
	if (BoundMovementCollisionComponent.IsValid())
	{
		BoundMovementCollisionComponent->OnComponentHit.RemoveDynamic(this, &ASubmarineBase::OnHullHit);
		BoundMovementCollisionComponent.Reset();
	}

	// Unbind any previously bound hull collision PMCs.
	for (TWeakObjectPtr<UPrimitiveComponent>& Comp : BoundHullCollisionComponents)
	{
		if (Comp.IsValid())
		{
			Comp->OnComponentHit.RemoveDynamic(this, &ASubmarineBase::OnHullHit);
		}
	}
	BoundHullCollisionComponents.Reset();

	// Bind generated hull collision PMCs if available.
	if (GeneratedGeometry && GeneratedGeometry->GetExteriorHullCollisionComponents().Num() > 0)
	{
		for (UProceduralMeshComponent* PMC : GeneratedGeometry->GetExteriorHullCollisionComponents())
		{
			if (IsValid(PMC))
			{
				PMC->OnComponentHit.RemoveDynamic(this, &ASubmarineBase::OnHullHit);
				PMC->OnComponentHit.AddUniqueDynamic(this, &ASubmarineBase::OnHullHit);
				BoundHullCollisionComponents.Add(PMC);
			}
		}
		BoundMovementCollisionComponent = GeneratedGeometry->GetExteriorHullCollisionComponents()[0];
	}
	else if (UPrimitiveComponent* CollisionComponent = GetMovementCollisionComponent())
	{
		// Fallback to handmade movement collision component.
		CollisionComponent->OnComponentHit.RemoveDynamic(this, &ASubmarineBase::OnHullHit);
		CollisionComponent->OnComponentHit.AddUniqueDynamic(this, &ASubmarineBase::OnHullHit);
		BoundMovementCollisionComponent = CollisionComponent;
	}
}

bool ASubmarineBase::CreateDebugBreachOnFirstExteriorSheet(float DamageAmount)
{
	if (!HasAuthoritativeContext(this))
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateDebugBreach: not authority — skipping"));
		return false;
	}

	if (DamageAmount <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateDebugBreach: DamageAmount=%.1f — skipping"), DamageAmount);
		return false;
	}

	if (!SubHull)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateDebugBreach: SubHull is null"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("CreateDebugBreach: StructuralSheets=%d, BreachClusters=%d"),
		SubHull->GetStructuralSheets().Num(),
		SubHull->GetBreachClusters().Num());

	const FStructuralSheetDef* TargetSheet = SubHull->GetStructuralSheets().FindByPredicate([](const FStructuralSheetDef& Sheet)
	{
		return Sheet.bCanOpenToExterior;
	});

	if (!TargetSheet && SubHull->GetStructuralSheets().Num() > 0)
	{
		TargetSheet = &SubHull->GetStructuralSheets()[0];
	}

	if (!TargetSheet)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateDebugBreach: no StructuralSheet found (Sheets=%d)"), SubHull->GetStructuralSheets().Num());
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("CreateDebugBreach: targeting Sheet=%s | Compartment=%s | Exterior=%d | Origin=%s"),
		*TargetSheet->SheetId.ToString(),
		*TargetSheet->ParentCompartmentId.ToString(),
		TargetSheet->bCanOpenToExterior ? 1 : 0,
		*TargetSheet->LocalOrigin.ToCompactString());

	const FVector WorldHitPoint = GetActorTransform().TransformPosition(TargetSheet->LocalOrigin);
	FPointDamageEvent DamageEvent;
	DamageEvent.HitInfo.bBlockingHit = true;
	DamageEvent.HitInfo.ImpactPoint = WorldHitPoint;
	DamageEvent.HitInfo.Location = WorldHitPoint;

	const float AppliedDamage = TakeDamage(DamageAmount, DamageEvent, nullptr, this);
	if (AppliedDamage <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateDebugBreach: TakeDamage returned 0 for amount=%.1f"), DamageAmount);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("CreateDebugBreach: after initial damage — BreachClusters=%d, TotalWater=%.1fL"),
		SubHull->GetBreachClusters().Num(),
		SubFlood ? SubFlood->GetTotalWaterLiters() : 0.f);

	if (SubHull && SubHull->GetBreachClusters().Num() == 0)
	{
		const float EscalatedDamage = FMath::Max(DamageAmount * 4.f, 1000.f);
		UE_LOG(LogTemp, Log, TEXT("CreateDebugBreach: no breach after initial hit — escalating to %.1f"), EscalatedDamage);
		TakeDamage(EscalatedDamage, DamageEvent, nullptr, this);
	}

	if (HullVisualDamage)
	{
		HullVisualDamage->RefreshFromCurrentBreaches();
	}

	const bool bSuccess = SubHull && SubHull->GetBreachClusters().Num() > 0;
	UE_LOG(LogTemp, Log, TEXT("CreateDebugBreach: result=%s | BreachClusters=%d | TotalWater=%.1fL"),
		bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
		SubHull ? SubHull->GetBreachClusters().Num() : 0,
		SubFlood ? SubFlood->GetTotalWaterLiters() : 0.f);

	return bSuccess;
}

void ASubmarineBase::BakeLayoutFromVolumes()
{
	// 1. Gather all CompartmentVolumeComponents on this actor.
	TArray<UCompartmentVolumeComponent*> Volumes;
	GetComponents<UCompartmentVolumeComponent>(Volumes);

	if (Volumes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BakeLayout: no CompartmentVolumeComponent found on %s"), *GetName());
		return;
	}

	// 2. Group volumes by CompartmentId and compute merged bounds.
	struct FCompartmentAccum
	{
		FName CompartmentId;
		FText DisplayName;
		float CapacityOverride;
		float FloorZOverride;
		FBox LocalBounds;
		int32 VolumeCount;
	};

	TMap<FName, FCompartmentAccum> AccumMap;
	const FTransform ActorInverse = GetActorTransform().Inverse();

	for (const UCompartmentVolumeComponent* Volume : Volumes)
	{
		if (Volume->CompartmentId.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("BakeLayout: skipping volume '%s' with empty CompartmentId"), *Volume->GetName());
			continue;
		}

		// Compute local-space AABB from the box component world bounds.
		const FVector WorldCenter = Volume->GetComponentLocation();
		const FVector WorldExtent = Volume->GetScaledBoxExtent();
		const FVector LocalCenter = ActorInverse.TransformPosition(WorldCenter);

		// Transform the 8 corners of the oriented box into local space for tight bounds.
		const FTransform BoxWorldTransform = Volume->GetComponentTransform();
		const FVector BoxExtent = Volume->GetScaledBoxExtent();
		FBox LocalBox(EForceInit::ForceInit);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector CornerLocal(
				(Corner & 1) ? BoxExtent.X : -BoxExtent.X,
				(Corner & 2) ? BoxExtent.Y : -BoxExtent.Y,
				(Corner & 4) ? BoxExtent.Z : -BoxExtent.Z);
			const FVector WorldCorner = BoxWorldTransform.TransformPosition(CornerLocal);
			const FVector ActorLocalCorner = ActorInverse.TransformPosition(WorldCorner);
			LocalBox += ActorLocalCorner;
		}

		FCompartmentAccum* Accum = AccumMap.Find(Volume->CompartmentId);
		if (!Accum)
		{
			FCompartmentAccum New;
			New.CompartmentId = Volume->CompartmentId;
			New.DisplayName = Volume->DisplayName;
			New.CapacityOverride = Volume->CapacityLitersOverride;
			New.FloorZOverride = Volume->WalkableFloorZCmOverride;
			New.LocalBounds = LocalBox;
			New.VolumeCount = 1;
			AccumMap.Add(Volume->CompartmentId, New);
		}
		else
		{
			Accum->LocalBounds += LocalBox;
			Accum->VolumeCount++;
			if (Volume->CapacityLitersOverride > 0.f)
			{
				Accum->CapacityOverride = Volume->CapacityLitersOverride;
			}
			if (!Volume->DisplayName.IsEmpty())
			{
				Accum->DisplayName = Volume->DisplayName;
			}
		}
	}

	if (AccumMap.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BakeLayout: no valid compartments found (all volumes had empty CompartmentId)"));
		return;
	}

	// 3. Create or find an existing layout asset.
	USubmarineLayoutAsset* Layout = nullptr;
	if (SubHull && SubHull->LayoutAsset)
	{
		Layout = const_cast<USubmarineLayoutAsset*>(SubHull->LayoutAsset.Get());
	}

	if (!Layout)
	{
		const FString AssetName = FString::Printf(TEXT("DA_Layout_%s"), *GetName());
		Layout = NewObject<USubmarineLayoutAsset>(GetOuter(), *AssetName, RF_Public | RF_Standalone);
	}

	Layout->Compartments.Reset();

	// 4. Build compartment definitions from accumulated bounds.
	for (const auto& Pair : AccumMap)
	{
		const FCompartmentAccum& Accum = Pair.Value;
		const FVector Size = Accum.LocalBounds.GetSize();

		FSubCompartmentDef Def;
		Def.CompartmentId = Accum.CompartmentId;
		Def.DisplayName = Accum.DisplayName.IsEmpty()
			? FText::FromName(Accum.CompartmentId)
			: Accum.DisplayName;
		Def.HydroBoundsMin = Accum.LocalBounds.Min;
		Def.HydroBoundsMax = Accum.LocalBounds.Max;
		Def.WalkableFloorZCm = Accum.FloorZOverride > 0.f
			? Accum.FloorZOverride
			: Accum.LocalBounds.Min.Z;

		if (Accum.CapacityOverride > 0.f)
		{
			Def.CapacityLiters = Accum.CapacityOverride;
		}
		else
		{
			// Auto-calculate: box volume in cm³ → liters (1 liter = 1000 cm³), with 70% fill factor.
			const float VolumeCm3 = Size.X * Size.Y * Size.Z;
			Def.CapacityLiters = FMath::Max(100.f, (VolumeCm3 / 1000.f) * 0.7f);
		}

		Layout->Compartments.Add(Def);

		UE_LOG(LogTemp, Log, TEXT("BakeLayout: [%s] bounds=(%.0f,%.0f,%.0f)→(%.0f,%.0f,%.0f) capacity=%.0fL volumes=%d"),
			*Accum.CompartmentId.ToString(),
			Accum.LocalBounds.Min.X, Accum.LocalBounds.Min.Y, Accum.LocalBounds.Min.Z,
			Accum.LocalBounds.Max.X, Accum.LocalBounds.Max.Y, Accum.LocalBounds.Max.Z,
			Def.CapacityLiters,
			Accum.VolumeCount);
	}

	// 5. Assign to SubHull.
	if (SubHull)
	{
		SubHull->LayoutAsset = Layout;
	}

	UE_LOG(LogTemp, Log, TEXT("BakeLayout: created %d compartments from %d volumes on %s"),
		Layout->Compartments.Num(), Volumes.Num(), *GetName());

#if WITH_EDITOR
	if (Layout)
	{
		Layout->MarkPackageDirty();
	}
#endif
}

void ASubmarineBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

float ASubmarineBase::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (GetWorld() && GetLevel())
	{
		Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	}

	if (!HasAuthoritativeContext(this) || DamageAmount <= 0.f || !SubHull)
	{
		return 0.f;
	}

	FVector WorldHitPoint = GetActorLocation();

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* PointDamageEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
		if (PointDamageEvent)
		{
			if (PointDamageEvent->HitInfo.bBlockingHit)
			{
				if (!PointDamageEvent->HitInfo.ImpactPoint.IsNearlyZero())
				{
					WorldHitPoint = PointDamageEvent->HitInfo.ImpactPoint;
				}
				else if (!PointDamageEvent->HitInfo.Location.IsNearlyZero())
				{
					WorldHitPoint = PointDamageEvent->HitInfo.Location;
				}
			}
			else if (!PointDamageEvent->HitInfo.ImpactPoint.IsNearlyZero())
			{
				WorldHitPoint = PointDamageEvent->HitInfo.ImpactPoint;
			}
			else if (!PointDamageEvent->HitInfo.Location.IsNearlyZero())
			{
				WorldHitPoint = PointDamageEvent->HitInfo.Location;
			}
		}
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		const FRadialDamageEvent* RadialDamageEvent = static_cast<const FRadialDamageEvent*>(&DamageEvent);
		if (RadialDamageEvent)
		{
			WorldHitPoint = RadialDamageEvent->Origin;
		}
	}

	const FVector LocalHitPosition = GetActorTransform().InverseTransformPosition(WorldHitPoint);
	SubHull->ApplyHullImpact(LocalHitPosition, DamageAmount, HullWeaponDamageRadiusCm);

	// Direct damage→SubFlood when SubHull has no structural sheets (pure generator path).
	if (SubFlood && SubFlood->IsInitialized()
		&& SubHull->GetStructuralSheets().Num() == 0
		&& GeneratedDefinition)
	{
		const FGeneratedCompartmentDef* Comp = GeneratedDefinition->FindCompartmentAtLocalLocation(LocalHitPosition);
		if (Comp)
		{
			const float Inflow = FMath::Clamp(
				DamageAmount * DamageToBreachInflowScale,
				0.f,
				GeneratedDefinition->MaxExteriorInflowLitersPerSec);
			SubFlood->CreateBreach(Comp->CompartmentId, Inflow, LocalHitPosition);
		}
	}

	if (FeedbackManager)
	{
		FeedbackManager->DispatchHullImpactFeedback(WorldHitPoint, DamageAmount, HullWeaponDamageRadiusCm);
	}
	return DamageAmount;
}

void ASubmarineBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASubmarineBase, CurrentPilot);
	DOREPLIFETIME(ASubmarineBase, RepState);
	DOREPLIFETIME(ASubmarineBase, ExteriorTurret);
}

void ASubmarineBase::SetPilot(AActor* NewPilot)
{
	if (HasAuthority())
	{
		CurrentPilot = NewPilot;
	}
}

void ASubmarineBase::ClearPilot()
{
	if (HasAuthority())
	{
		CurrentPilot = nullptr;
	}
}

void ASubmarineBase::OnHullHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	const AActor* HitActor = Hit.GetActor();
	if (HitActor == this
		|| (HitActor && (HitActor->GetOwner() == this || HitActor->GetAttachParentActor() == this || HitActor->IsAttachedTo(this)))
		|| (OtherComp && OtherComp->GetOwner() == this))
	{
		return;
	}

	const FVector ImpactNormal = !Hit.Normal.IsNearlyZero()
		? Hit.Normal.GetSafeNormal()
		: Hit.ImpactNormal.GetSafeNormal();
	if (ImpactNormal.IsNearlyZero())
	{
		return;
	}

	const FVector SelfVelocity = SubMovement ? SubMovement->Velocity : GetVelocity();
	const FVector OtherVelocity = OtherComp
		? OtherComp->GetComponentVelocity()
		: (OtherActor ? OtherActor->GetVelocity() : FVector::ZeroVector);
	const FVector RelativeVelocity = SelfVelocity - OtherVelocity;
	const float ApproachSpeedCmS = FMath::Max(0.f, FVector::DotProduct(RelativeVelocity, -ImpactNormal));

	if (ApproachSpeedCmS < HullCollisionDamageMinSpeedCmS)
	{
		if (GetDefault<USub3DDebugSettings>()->bLogSubHullCollisions)
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("Hull collision ignored | Speed=%.1f cm/s below threshold %.1f | Other=%s"),
				ApproachSpeedCmS,
				HullCollisionDamageMinSpeedCmS,
				*GetNameSafe(OtherActor));
		}
		return;
	}

	const float CatastrophicSpeedCmS = FMath::Max(HullCollisionDamageMinSpeedCmS + 1.f, HullCollisionCatastrophicSpeedCmS);
	const float SpeedAlpha = FMath::Clamp(
		(ApproachSpeedCmS - HullCollisionDamageMinSpeedCmS) / (CatastrophicSpeedCmS - HullCollisionDamageMinSpeedCmS),
		0.f,
		1.f);
	const float SpeedSeverity = FMath::Pow(SpeedAlpha, HullCollisionDamageExponent);
	const float SpeedDamage = HullCollisionDamageAtCatastrophicSpeed * SpeedSeverity;
	const float ImpulseDamage = NormalImpulse.Size() * HullImpactDamageScale;
	const float Damage = FMath::Max(SpeedDamage, ImpulseDamage);

	if (Damage < 1.f)
	{
		return;
	}

	const FVector LocalHitPosition = GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);

	if (SubHull)
	{
		SubHull->ApplyHullImpact(LocalHitPosition, Damage, HullImpactRadiusCm);
	}

	// Direct collision→SubFlood when SubHull has no structural sheets (pure generator path).
	if (SubFlood && SubFlood->IsInitialized()
		&& SubHull && SubHull->GetStructuralSheets().Num() == 0
		&& GeneratedDefinition)
	{
		const FGeneratedCompartmentDef* Comp = GeneratedDefinition->FindCompartmentAtLocalLocation(LocalHitPosition);
		if (Comp)
		{
			const float Inflow = FMath::Clamp(
				Damage * DamageToBreachInflowScale,
				0.f,
				GeneratedDefinition->MaxExteriorInflowLitersPerSec);
			SubFlood->CreateBreach(Comp->CompartmentId, Inflow, LocalHitPosition);
		}
	}

	if (FeedbackManager)
	{
		FeedbackManager->DispatchHullImpactFeedback(Hit.ImpactPoint, Damage, HullImpactRadiusCm);
	}

	if (GetDefault<USub3DDebugSettings>()->bLogSubHullCollisions)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Hull collision damage | Speed=%.1f cm/s | Damage=%.1f | ImpulseDamage=%.1f | Other=%s | Point=%s"),
			ApproachSpeedCmS,
			Damage,
			ImpulseDamage,
			*GetNameSafe(OtherActor),
			*Hit.ImpactPoint.ToCompactString());
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			3.f,
			FColor::Red,
			FString::Printf(TEXT("HULL HIT - %.0f damage"), Damage));
	}
}

void ASubmarineBase::RefreshRepState()
{
	RepState.WorldLocation = GetActorLocation();
	RepState.QuantizedRotation = GetActorRotation();

	if (SubMovement)
	{
		RepState.LinearVelocity = SubMovement->Velocity;
		RepState.AngularVelocity = FVector(0.f, SubMovement->GetPitchRateDegPerSec(), SubMovement->GetYawRateDegPerSec());
		RepState.LinearAcceleration = SubMovement->LinearAcceleration;
		RepState.AngularAccelerationDeg = SubMovement->AngularAccelerationDeg;
		RepState.RudderInput = SubMovement->GetRudderInput();
		RepState.DivePlaneInput = SubMovement->GetDivePlaneInput();
		RepState.ThrustInput = SubMovement->GetThrustInput();
		RepState.ForwardSpeed = FVector::DotProduct(SubMovement->Velocity, GetActorForwardVector());
		RepState.VerticalSpeed = SubMovement->Velocity.Z;
		RepState.DepthMeters = SubMovement->CurrentDepth;
		RepState.FloodedMassKg = SubMovement->FloodedMassKg;
		RepState.BallastGlobal01 = SubMovement->GlobalTargetFill;
		RepState.SimFrame = SubMovement->GetSimFrameCounter();
	}

	if (Systems)
	{
		RepState.MainTrim01 = Systems->GetCommandState().MainTrimBiasCmd;
		RepState.bPumpActive = Systems->GetCommandState().bPumpActive;
	}
}

float ASubmarineBase::GetCurrentDepthMeters() const
{
	return SubMovement ? SubMovement->CurrentDepth : 0.f;
}

FTransform ASubmarineBase::GetPrimaryCrewSpawnTransform() const
{
	return GetCrewSpawnTransformForSlot(0);
}

FTransform ASubmarineBase::GetCrewSpawnTransformForSlot(int32 SlotIndex) const
{
	const FName SocketName(*FString::Printf(TEXT("CrewSocket%d"), SlotIndex + 1));

	// 1) USceneComponent child by exact name (BP-authored sub-objects)
	TArray<USceneComponent*> SceneComps;
	GetComponents<USceneComponent>(SceneComps);
	for (USceneComponent* Comp : SceneComps)
	{
		if (Comp && Comp->GetFName() == SocketName)
		{
			return Comp->GetComponentTransform();
		}
	}

	// 2) Static-mesh socket of the same name on the HullMesh asset
	if (HullMesh && HullMesh->DoesSocketExist(SocketName))
	{
		return HullMesh->GetSocketTransform(SocketName);
	}

	// 3) Backward-compat fallback for slot 0 — the legacy CrewSpawnSocketP1 USceneComponent
	if (SlotIndex == 0 && IsValid(CrewSpawnSocketP1))
	{
		return CrewSpawnSocketP1->GetComponentTransform();
	}

	// 4) Last-resort fallbacks
	if (SlotIndex == 0 && IsValid(HelmSocket))
	{
		return HelmSocket->GetComponentTransform();
	}
	if (GeneratedDefinition && GeneratedDefinition->SpawnPoints.IsValidIndex(SlotIndex))
	{
		return GeneratedDefinition->SpawnPoints[SlotIndex].LocalTransform * GetActorTransform();
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[%s] No crew spawn socket found for slot %d (looking for '%s'). Falling back to actor transform — players will overlap at sub origin."),
		*GetName(), SlotIndex, *SocketName.ToString());
	return GetActorTransform();
}

float ASubmarineBase::GetTotalFloodWaterMassKg() const
{
	if (SubFlood && SubFlood->IsInitialized())
	{
		return SubFlood->GetTotalWaterMassKg();
	}

	return Compartments ? Compartments->GetTotalWaterMassKg() : 0.f;
}

void ASubmarineBase::ResolveExteriorTurret()
{
	if (ExteriorTurret)
	{
		return;
	}

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for (AActor* Actor : AttachedActors)
	{
		if (ATurretActor* Turret = Cast<ATurretActor>(Actor))
		{
			ExteriorTurret = Turret;
			return;
		}
	}
}

void ASubmarineBase::SetFreezeMovementForTesting(bool bFreeze)
{
	if (!HasAuthority())
	{
		return;
	}

	bFreezeMovementForTesting = bFreeze;

	if (SubMovement)
	{
		SubMovement->Velocity = FVector::ZeroVector;
	}

	RefreshRepState();
}

bool ASubmarineBase::IsMovementCollisionReady() const
{
	const UPrimitiveComponent* CollisionComp = GetMovementCollisionComponent();
	FString Reason;
	const bool bReady = ValidateMovementCollisionComponentConfig(CollisionComp, Reason);
	if (!bReady)
	{
		LogMovementCollisionValidationIssue(this, Reason);
	}
	return bReady;
}

bool ASubmarineBase::ValidateSpawnCollision() const
{
	const UPrimitiveComponent* CollisionComp = GetMovementCollisionComponent();
	FString Reason;
	if (!ValidateMovementCollisionComponentConfig(CollisionComp, Reason))
	{
		LogMovementCollisionValidationIssue(this, Reason);
		return false;
	}

	return true;
}

ASubDoorActor* ASubmarineBase::FindAttachedDoorById(FName DoorId) const
{
	if (DoorId.IsNone())
	{
		return nullptr;
	}

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		ASubDoorActor* DoorActor = Cast<ASubDoorActor>(AttachedActor);
		if (DoorActor && DoorActor->DoorId == DoorId)
		{
			return DoorActor;
		}
	}

	return nullptr;
}

TArray<UPrimitiveComponent*> ASubmarineBase::GetInteriorWalkableComponents() const
{
	TArray<UPrimitiveComponent*> Result;

	// Generated path: procedural interior floor collision from the generator.
	if (GeneratedGeometry)
	{
		for (UProceduralMeshComponent* PMC : GeneratedGeometry->GetInteriorFloorCollisionComponents())
		{
			if (IsValid(PMC))
			{
				Result.Add(PMC);
			}
		}
	}

	// Manual path: any primitive component tagged with ManualWalkableTag.
	// Used by handmade submarines (Craniata) that ship walkable surfaces as authored meshes.
	if (!ManualWalkableTag.IsNone())
	{
		TArray<UPrimitiveComponent*> Tagged;
		GetComponents<UPrimitiveComponent>(Tagged);
		for (UPrimitiveComponent* PC : Tagged)
		{
			if (IsValid(PC) && PC->ComponentHasTag(ManualWalkableTag))
			{
				Result.AddUnique(PC);
			}
		}
	}

	return Result;
}

bool ASubmarineBase::IsInteriorWalkableComponent(const UPrimitiveComponent* Component) const
{
	if (!IsValid(Component))
	{
		return false;
	}

	const TArray<UPrimitiveComponent*> WalkableComponents = GetInteriorWalkableComponents();
	if (WalkableComponents.Contains(const_cast<UPrimitiveComponent*>(Component)))
	{
		return true;
	}

	if (!IsComponentOnOrOwnedBySubmarine(this, Component))
	{
		return false;
	}

	if (Component == GetMovementCollisionComponent() || Component == HullMesh)
	{
		return false;
	}

	if (Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		return false;
	}

	if (Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
	{
		return false;
	}

	return Component->CanCharacterStepUpOn != ECB_No;
}

FTransform ASubmarineBase::GetCrewEmbarkTransform() const
{
	return GetPrimaryCrewSpawnTransform();
}

void ASubmarineBase::OnRep_RepState()
{
	if (SubMovement)
	{
		SubMovement->HandleReplicatedNetState(RepState);
	}
}

void ASubmarineBase::HandleBreachesUpdatedForFlood(const TArray<FBreachClusterState>& Breaches)
{
	if (!SubFlood || !SubFlood->IsInitialized() || !SubHull)
	{
		return;
	}

	const TArray<FStructuralSheetDef>& Sheets = SubHull->GetStructuralSheets();

	// Aggregate exterior breach inflow per compartment.
	TMap<FName, float> CompartmentInflow;
	TMap<FName, FVector> CompartmentBreachCenter;
	TMap<FName, int32> CompartmentBreachCount;

	for (const FBreachClusterState& Cluster : Breaches)
	{
		if (!Cluster.bTouchesExterior)
		{
			continue;
		}

		const FStructuralSheetDef* Sheet = Sheets.FindByPredicate([&Cluster](const FStructuralSheetDef& S)
		{
			return S.SheetId == Cluster.SheetId;
		});

		if (!Sheet || Sheet->ParentCompartmentId.IsNone())
		{
			continue;
		}

		const FName CompId = Sheet->ParentCompartmentId;
		const float AreaScale = FMath::Max(0.f, Cluster.OpenAreaCm2 / FMath::Max(1.f, SubHull->ExteriorFloodAreaDivisorCm2));
		const float Inflow = FMath::Clamp(SubHull->BaseLeakFlowLitersPerSec * AreaScale, 0.f, SubHull->MaxExteriorFloodInLitersPerSec);

		CompartmentInflow.FindOrAdd(CompId) += Inflow;
		CompartmentBreachCenter.FindOrAdd(CompId) += Cluster.LocalCenter;
		CompartmentBreachCount.FindOrAdd(CompId)++;
	}

	// Push aggregated breaches to SubFlood.
	for (const auto& Pair : CompartmentInflow)
	{
		const int32 Count = CompartmentBreachCount[Pair.Key];
		const FVector Center = CompartmentBreachCenter[Pair.Key] / FMath::Max(1, Count);
		SubFlood->CreateBreach(Pair.Key, Pair.Value, Center);
	}

	// Remove breaches for compartments that no longer have exterior breach clusters.
	const TArray<FCompartmentBreachState>& ExistingBreaches = SubFlood->GetBreaches();
	for (const FCompartmentBreachState& Existing : ExistingBreaches)
	{
		if (Existing.bBreached && !CompartmentInflow.Contains(Existing.CompartmentId))
		{
			SubFlood->RemoveBreach(Existing.CompartmentId);
		}
	}
}
