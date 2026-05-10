#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "StructuralHullTypes.h"
#include "SubmarineRuntimeTypes.h"
#include "SubmarineBase.generated.h"

class USubMovementComponent;
class USubHullComponent;
class USubFloodComponent;
class USubmarineDefinition;
class USubmarineGeneratorSpec;
class USubmarineGeneratedGeometryComponent;
class USubmarineSystemsComponent;
class USubmarineCompartmentComponent;
class USubmarineStationManagerComponent;
class USubmarineRadarComponent;
class UBreachVfxManagerComponent;
class UCompartmentVolumeComponent;
class UDoorFloodVfxComponent;
class USubHullVisualDamageComponent;
class USubmarineLayoutAsset;
class USubmarineFeedbackDirectorComponent;
class USubSonarComponent;
class USubSonarSystemComponent;
class UTunnelNavigationRuntimeComponent;
class UHelmNavigationDisplayComponent;
class ATurretActor;
class ASubDoorActor;
class UPrimitiveComponent;

/**
 * The submarine entity. APawn, not ACharacter.
 * Physics via USubMovementComponent (math-based, no Chaos).
 * Server authoritative. Replicates movement to all clients.
 * Not possessed directly — players possess ASubCrewCharacter.
 */
UCLASS()
class SUB3D_API ASubmarineBase : public APawn
{
	GENERATED_BODY()

public:
	ASubmarineBase();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// ── Components ────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SubmarineRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* HullMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MovementCollisionProxy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubMovementComponent* SubMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubHullComponent* SubHull;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubFloodComponent* SubFlood;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubmarineSystemsComponent* Systems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubmarineCompartmentComponent* Compartments;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubmarineStationManagerComponent* StationManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubmarineRadarComponent* Radar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBreachVfxManagerComponent* BreachVfxManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubHullVisualDamageComponent* HullVisualDamage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UDoorFloodVfxComponent* DoorFloodVfx;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "Feedback Director"))
	USubmarineFeedbackDirectorComponent* FeedbackManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubSonarComponent* Sonar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubSonarSystemComponent* SonarSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTunnelNavigationRuntimeComponent* TunnelNavigationRuntime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHelmNavigationDisplayComponent* HelmNavigationDisplay;

	// Attach point inside the sub for the helm station
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* HelmSocket;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* CrewSpawnSocketP1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* TurretHardpoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubmarineGeneratedGeometryComponent* GeneratedGeometry;

	// --- Generator pipeline --------------------------------------------------

	/** If set and GeneratedDefinition is null, Generate() runs in BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator")
	TObjectPtr<USubmarineGeneratorSpec> GeneratorSpec;

	/** If set, SubFlood initializes from this definition in BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator")
	TObjectPtr<USubmarineDefinition> GeneratedDefinition;

	/**
	 * Class used to spawn interactable door actors from
	 * GeneratedDefinition->Connections. Set this to a blueprint subclass of
	 * ASubDoorActor (BP_Door) on the submarine actor in the level. If left
	 * unset, no doors are spawned and the generator path emits a warning.
	 * Named with the Generator prefix to avoid shadowing the legacy
	 * DoorActorClass that already exists on ASubmarineCompilerActor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator")
	TSubclassOf<ASubDoorActor> GeneratorDoorActorClass;

	/**
	 * Class used to spawn ASubHatchActor for connections of EConnectionType::Hatch
	 * (horizontal trap-door between stacked decks). If left null, falls back to
	 * GeneratorDoorActorClass so existing setups don't break — the visual will look
	 * like a vertical door even on a horizontal opening, which is functionally fine
	 * but visually wrong. Set this to a BP_Hatch subclass for proper visuals.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Generator")
	TSubclassOf<class ASubHatchActor> GeneratorHatchActorClass;

	/**
	 * Spawn ASubDoorActor instances for each traversable connection in
	 * GeneratedDefinition. Called from BeginPlay after BuildFromDefinition.
	 * Safe to call with a null world (returns silently).
	 */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Generator")
	void SpawnDoorsFromDefinition();

	/**
	 * Returns the current count of doors spawned via SpawnDoorsFromDefinition.
	 * Intended for automation tests that need to observe the lifecycle
	 * (spawn / clear / respawn) from outside the class. Production gameplay
	 * should iterate FindAttachedDoorById or similar instead.
	 */
	UFUNCTION(BlueprintPure, Category = "Submarine|Generator|Debug")
	int32 GetSpawnedGeneratorDoorCount() const { return SpawnedGeneratorDoors.Num(); }

	/**
	 * Editor-time regeneration of the submarine from GeneratorSpec.
	 * Clears the current GeneratedDefinition and any materialized PMCs, then
	 * runs Generate() + BuildMeshData() + BuildFromDefinition() in order.
	 * Does not touch SubFlood, StationManager, or breach bridges — those are
	 * BeginPlay-only. Intended for fast iteration on hull shape in editor
	 * without restarting PIE.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Submarine|Generator|Debug")
	void RebuildFromSpec();

	/**
	 * Editor-time clear of the generated state (Definition + materialized PMCs).
	 * Leaves the actor in a clean pre-generation state. Does not touch SubFlood
	 * or StationManager. Intended as a reset before RebuildFromSpec.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Submarine|Generator|Debug")
	void ClearGeneratedState();

	// --- Pilot tracking --------------------------------------------------

	// The crew member currently at the helm (null = unmanned)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Submarine")
	AActor* CurrentPilot;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RepState, Category = "Submarine")
	FSubmarineNetState RepState;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Replicated, Category = "Submarine")
	ATurretActor* ExteriorTurret;

	UFUNCTION(BlueprintCallable, Category = "Submarine")
	void SetPilot(AActor* NewPilot);

	UFUNCTION(BlueprintCallable, Category = "Submarine")
	void ClearPilot();

	/**
	 * Resolve which compartment a sub-local point falls inside, considering ALL placed
	 * UCompartmentVolumeComponent boxes (union of boxes — handles multi-volume L-shaped
	 * compartments where two CVs share the same CompartmentId).
	 *
	 * CV is the single source of truth for compartment geometry. Returns NAME_None if no
	 * volume contains the point. (DA HydroBounds fallback removed 2026-05-10 — option B.)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Submarine|Compartments")
	FName FindCompartmentIdAtLocalLocation(const FVector& LocalPosition) const;

	/**
	 * Compute the encapsulating AABB (sub-local space) of all UCompartmentVolumeComponent
	 * boxes that share the given CompartmentId. CV is now the single source of truth for
	 * compartment geometry — this replaces the legacy FGeneratedCompartmentDef::HydroBounds
	 * reads at all live call sites (crew bounds, breach center, teleport target, etc.).
	 *
	 * Returns false if no CV matches.
	 */
	bool GetCompartmentLocalBounds(FName CompartmentId, FBox& OutLocalBounds) const;

	/**
	 * Inject a water perturbation at a world-space point. Iterates every UFloodWaterPlaneComponent
	 * on this sub and forwards the call — each plane's InjectAtWorldPoint self-filters to its own
	 * compartment bounds. Returns true if any plane accepted the inject.
	 *
	 * BP-friendly entry for crew interaction: e.g. UInteractionComponent traces from view → hit ImpactPoint
	 * → call InjectWaterAtWorldPoint(Hit.ImpactPoint).
	 */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Water")
	bool InjectWaterAtWorldPoint(FVector WorldPos, float Force = 30.f, float Radius = 80.f);

	// ── Flood Visuals (Water Planes) ─────────────────────────────────────
	// Defaults applied to every UFloodWaterPlaneComponent spawned at bootstrap
	// (one per logical CompartmentId selected from the placed volume components).
	// Art designer sets
	// these on the BP class; each plane inherits unless overridden.

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Submarine|FloodVisuals")
	TObjectPtr<class UMaterialInterface> DefaultWaterMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Submarine|FloodVisuals")
	TObjectPtr<class UStaticMesh> DefaultWaterPlaneMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Submarine|FloodVisuals", meta = (ClampMin = "100.0"))
	float DefaultWaterPlaneWorldSizeCm = 8000.f;

	/** Niagara system spawned at the breach point on first detection by every UFloodWaterPlaneComponent.
	 *  Propagated to each plane at spawn time; the plane's per-instance BreachWaterImpactVfx wins if set. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Submarine|FloodVisuals")
	TObjectPtr<class UNiagaraSystem> DefaultBreachWaterImpactVfx = nullptr;

	// ── Hull damage (Proto 02) ──────────────────────────────────────────

	// Legacy fallback: when a physics impulse is available, convert it to hull damage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Damage")
	float HullImpactDamageScale = 0.0001f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Damage")
	float HullImpactRadiusCm = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Damage")
	float HullWeaponDamageRadiusCm = 35.f;

	// Pure generator path: damage-to-inflow conversion when SubHull has no structural sheets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Damage", meta = (ClampMin = "0.0"))
	float DamageToBreachInflowScale = 5.f;

	// A4: low-speed scraping should not breach the hull.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Damage", meta = (ClampMin = "0.0"))
	float HullCollisionDamageMinSpeedCmS = 300.f;

	// At or above this approach speed, collisions use full catastrophic damage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Damage", meta = (ClampMin = "1.0"))
	float HullCollisionCatastrophicSpeedCmS = 1250.f;

	// Catastrophic collision damage before hull-cell falloff/material scaling.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Damage", meta = (ClampMin = "0.0"))
	float HullCollisionDamageAtCatastrophicSpeed = 220.f;

	// Curves the severity ramp between min and catastrophic speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Damage", meta = (ClampMin = "1.0"))
	float HullCollisionDamageExponent = 2.f;

	/**
	 * Diagnostic freeze toggle. When true, the sub's physics tick zeroes velocity/rates
	 * and early-returns. Useful for isolating crew rebase jitter from sub-induced jitter.
	 * NOT touched by the bootstrap pipeline — designer-controlled per-instance via the
	 * editor inspector (or via SetFreezeMovementForTesting BP node at runtime).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Submarine|Debug")
	bool bFreezeMovementForTesting = false;

	// Callback for hull collisions
	UFUNCTION()
	void OnHullHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Collision")
	void ApplyHullCollisionDefaults();

	UFUNCTION(BlueprintPure, Category = "Submarine|Collision")
	virtual UPrimitiveComponent* GetMovementCollisionComponent() const;

	UFUNCTION(BlueprintCallable, Category = "Submarine|Collision")
	void RefreshMovementCollisionBinding();

	// ── Interior / Crew support ──────────────────────────────────────────

	/** Component tag that marks a primitive component as a manually authored walkable surface.
	 *  Used by handmade submarines (Craniata path) where there is no generated geometry to
	 *  expose floor collision. Tag the desired components in the Details panel to opt in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Submarine|Interior")
	FName ManualWalkableTag = TEXT("HandmadeWalkable");

	/** Returns components that provide walkable surfaces for crew inside the submarine. */
	UFUNCTION(BlueprintPure, Category = "Submarine|Interior")
	virtual TArray<UPrimitiveComponent*> GetInteriorWalkableComponents() const;

	/** Returns true when the component is a valid crew walkable surface on this submarine. */
	UFUNCTION(BlueprintPure, Category = "Submarine|Interior")
	virtual bool IsInteriorWalkableComponent(const UPrimitiveComponent* Component) const;

	/** Returns the canonical transform for crew embark (corrected for floor support). */
	UFUNCTION(BlueprintPure, Category = "Submarine|Interior")
	virtual FTransform GetCrewEmbarkTransform() const;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Submarine|Damage|Debug")
	bool CreateDebugBreachOnFirstExteriorSheet(float DamageAmount = 150.f);

	// LEGACY (Phase 7A, 2026-04-10) — Proto authoring helper.
	// Produces a SubmarineLayoutAsset from editor-placed CompartmentVolumeComponents.
	// Do not call from new authoring flows. The SubmarineGenerator path (Phase 5D)
	// will replace this entirely. Will be removed in Phase 7B.
	/** Scans all CompartmentVolumeComponents on this actor, merges by CompartmentId,
	 *  and creates or updates a SubmarineLayoutAsset assigned to SubHull.
	 *  Volumes with the same CompartmentId are unioned into a single compartment. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Submarine|Layout")
	void BakeLayoutFromVolumes();

	UFUNCTION(BlueprintCallable, Category = "Submarine")
	void RefreshRepState();

	UFUNCTION(BlueprintPure, Category = "Submarine")
	float GetCurrentDepthMeters() const;

	UFUNCTION(BlueprintPure, Category = "Submarine")
	FTransform GetPrimaryCrewSpawnTransform() const;

	/**
	 * Returns the world transform for the crew spawn slot. Looks up, in order:
	 *   1) a USceneComponent child named "CrewSocket{N}" where N = SlotIndex + 1
	 *   2) a static-mesh socket of the same name on HullMesh
	 *   3) legacy CrewSpawnSocketP1 (slot 0 only)
	 *   4) actor transform (warning logged)
	 * Slot 0 → CrewSocket1, slot 1 → CrewSocket2, etc.
	 */
	UFUNCTION(BlueprintCallable, Category = "Submarine")
	FTransform GetCrewSpawnTransformForSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Submarine")
	float GetTotalFloodWaterMassKg() const;

	UFUNCTION(BlueprintCallable, Category = "Submarine")
	void ResolveExteriorTurret();

	UFUNCTION(BlueprintCallable, Category = "Submarine|Debug")
	void SetFreezeMovementForTesting(bool bFreeze);

	UFUNCTION(BlueprintPure, Category = "Submarine|Collision")
	virtual bool IsMovementCollisionReady() const;

	UFUNCTION(BlueprintCallable, Category = "Submarine|Collision")
	virtual bool ValidateSpawnCollision() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Doors")
	ASubDoorActor* FindAttachedDoorById(FName DoorId) const;

	UFUNCTION()
	void OnRep_RepState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Bridge SubHull breach clusters to SubFlood inflow when SubFlood is the active sim. */
	UFUNCTION()
	void HandleBreachesUpdatedForFlood(const TArray<FBreachClusterState>& Breaches);

	/** Destroy any doors previously spawned by SpawnDoorsFromDefinition. */
	void DestroySpawnedGeneratorDoors();

	UPROPERTY(Transient)
	TWeakObjectPtr<UPrimitiveComponent> BoundMovementCollisionComponent;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UPrimitiveComponent>> BoundHullCollisionComponents;

	/**
	 * Door actors spawned from GeneratedDefinition->Connections by
	 * SpawnDoorsFromDefinition. Tracked so ClearGeneratedState and
	 * RebuildFromSpec can destroy them cleanly before regenerating.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASubDoorActor>> SpawnedGeneratorDoors;
};
