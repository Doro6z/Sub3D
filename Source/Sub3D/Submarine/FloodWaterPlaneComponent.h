#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FloodWaterPlaneComponent.generated.h"

class UCompartmentVolumeComponent;
class UCompartmentWaterBake;
class UStaticMesh;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UNiagaraSystem;
class UNiagaraComponent;

/**
 * Visual water plane driven by a single UCompartmentVolumeComponent. 100% passive C++:
 * updates world Z to match the compartment's water surface, toggles visibility, and fires
 * BP events on changes. All visual intelligence (hull clipping via Global Distance Fields,
 * caustics, refraction) lives in the assigned Material — this component does not decide
 * what the water looks like.
 *
 * Resolution priority for the rendered surface:
 *   1. UCompartmentWaterBake (per-compartment cap mesh) looked up via
 *      OwnerSubmarine->GeneratedDefinition->WaterBakes[CompartmentId]. PMC-driven.
 *   2. PlaneMesh (engine BasicShapes/Plane) scaled by PlaneWorldSizeCm. Generic flat plane.
 *
 * Art designer workflow:
 *  - Bake water via Sub3D Debug Panel > Authoring > Bake Water (one-time per sub layout change).
 *  - Assign `WaterMaterial` on the ASubmarineBase (propagated to all compartments).
 *  - Tweak `PlaneWorldSizeCm` if the sub is unusually large/small.
 *  - Listen to BP_OnWaterLevelChanged / BP_OnVisibilityChanged for VFX/SFX hooks.
 */
UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API UFloodWaterPlaneComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFloodWaterPlaneComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Compartment volume driving this plane's water height. Required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water")
	TWeakObjectPtr<UCompartmentVolumeComponent> SourceVolume;

	/** Mesh used as the FALLBACK water surface when no UCompartmentWaterBake is available.
	 *  Default: engine's BasicShapes/Plane (1m x 1m). Scaled by PlaneWorldSizeCm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Visual")
	TObjectPtr<UStaticMesh> PlaneMesh = nullptr;

	/** Material applied to the plane (or PMC). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Visual")
	TObjectPtr<UMaterialInterface> WaterMaterial = nullptr;

	/** World-space size of the plane (X=Y). Used only by the legacy fallback plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Visual", meta = (ClampMin = "100.0"))
	float PlaneWorldSizeCm = 8000.f;

	/** Plane is hidden when the compartment's WaterLevel01 is below this threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Visual", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VisibilityThreshold01 = 0.02f;

	/** Minimum absolute change in WaterLevel01 between ticks before BP_OnWaterLevelChanged fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Events", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LevelChangeEventThreshold01 = 0.005f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Flood|Water|Events")
	void BP_OnWaterLevelChanged(float NewLevel01, float NewHeightCm);

	UFUNCTION(BlueprintImplementableEvent, Category = "Flood|Water|Events")
	void BP_OnVisibilityChanged(bool bNowVisible);

	UFUNCTION(BlueprintCallable, Category = "Flood|Water")
	void RefreshFromFlood();

	UFUNCTION(BlueprintPure, Category = "Flood|Water")
	UStaticMeshComponent* GetPlaneMeshComponent() const { return PlaneMeshComponent; }

	UFUNCTION(BlueprintPure, Category = "Flood|Water")
	float GetCurrentWaterLevel01() const { return LastLevel01; }

	/** Inspector access to the cap mesh's runtime MID for parameter dump / debugging. */
	UMaterialInstanceDynamic* GetBakeCapMID() const { return BakeCapMID; }

	/** Inspector access to the R32F heightfield texture for previewing in the editor. */
	UTexture2D* GetHeightfieldTexture() const { return HeightfieldTex; }

	// ── Heightfield (P3.4 — surface vivante) ────────────────────────────────
	// Wave equation 2D CPU + texture R32F push-to-material. Active only when the bake path
	// is rendering (cap mesh present). Material reads HeightfieldTextureParamName and offsets
	// WPO by HeightfieldAmplitudeParamName * sampledValue to deform the cap surface.
	//
	// Coordinate system: heightfield grid spans the bake's LocalBoundsMin/Max XY. UV (0,0)
	// = LocalBoundsMin, UV (1,1) = LocalBoundsMax. Material binds the same bounds via
	// `LocalBoundsMin` / `LocalBoundsMax` parameters.

	/** Master toggle. False = no heightfield sim, no texture push, no MID param set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Heightfield")
	bool bEnableHeightfield = true;

	/** Grid resolution X. Texture R32F is created at this resolution × HeightfieldGridY. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Heightfield", meta = (ClampMin = "16", ClampMax = "256"))
	int32 HeightfieldGridX = 64;

	/** Grid resolution Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Heightfield", meta = (ClampMin = "16", ClampMax = "256"))
	int32 HeightfieldGridY = 64;

	/** Wave equation propagation speed. Higher = faster ripple spread. Practical range 50–1500. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Heightfield", meta = (ClampMin = "0.1"))
	float WaveSpeed = 200.0f;

	/** Per-step velocity damping. 0.995 = ~0.5% energy loss per step. Lower = faster decay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Heightfield", meta = (ClampMin = "0.9", ClampMax = "1.0"))
	float Damping = 0.995f;

	/** Fixed-step rate of the wave equation. Independent of frame rate; accumulator drains dt into ticks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Heightfield", meta = (ClampMin = "10.0", ClampMax = "240.0"))
	float HeightfieldUpdateHz = 60.f;

	/** Vertical amplitude in cm passed to the material's amplitude scalar. Material multiplies sampled R32F by this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Heightfield", meta = (ClampMin = "0.0"))
	float HeightfieldAmplitudeCm = 10.f;

	/** Material texture parameter name receiving the R32F heightfield. Must match the
	 *  Texture Sample node name in the cap material. M_Phase0_Test uses "HeightFieldTex". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Heightfield|MaterialParams")
	FName HeightfieldTextureParamName = TEXT("HeightFieldTex");

	/** Material scalar parameter name receiving HeightfieldAmplitudeCm. M_Phase0_Test uses "HeightfieldAmplitude". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Heightfield|MaterialParams")
	FName HeightfieldAmplitudeParamName = TEXT("HeightfieldAmplitude");

	/** Material vector parameter name receiving CachedBake->LocalBoundsMin (sub-local cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Heightfield|MaterialParams")
	FName LocalBoundsMinParamName = TEXT("LocalBoundsMin");

	/** Material vector parameter name receiving CachedBake->LocalBoundsMax (sub-local cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Heightfield|MaterialParams")
	FName LocalBoundsMaxParamName = TEXT("LocalBoundsMax");

	// ── Cap mesh topology (P3.4 polish) ─────────────────────────────────────
	// The bake produces a fan-triangulated polygon (one center vertex, N perimeter vertices).
	// That topology has TWO drawbacks for heightfield WPO:
	//   1. The center vertex displaces independently of the perimeter, creating a visible spike.
	//   2. Triangle density is uneven — perimeter has fine sampling, interior has none.
	// Subdivision 1→4 (each triangle splits into 4 via mid-edge vertices) repairs both at runtime.

	/** Number of 1→4 subdivisions applied to the bake cap mesh before submission to the PMC.
	 *  0 = use bake topology as-is (fan with center spike when waves hit). 2 = 16x triangle count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|CapMesh", meta = (ClampMin = "0", ClampMax = "4"))
	int32 CapMeshSubdivisionLevels = 2;

	/** Reverse triangle winding when submitting cap mesh to PMC. Use when the bake produced a mesh
	 *  facing down (visible only from below) — toggling this flips it without re-baking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|CapMesh")
	bool bFlipCapMeshWinding = true;

	/**
	 * Generate vertical "skirt" geometry below the cap perimeter. Scenario: sub tilted bow-up,
	 * Main_Hub contains water reaching the door level; the player stands in the dry Main_Bow
	 * adjacent compartment and opens the door. Without skirts, looking through the door at
	 * Main_Hub shows nothing (the flat cap disc is thinner than the door height and the camera's
	 * sight line passes under it). The skirt is a vertical strip dropping from the cap perimeter
	 * down to the compartment floor, **outward-facing** so it's visible from the dry side.
	 *
	 * Inside-the-compartment views see backfaces (culled) — fine because once the camera is
	 * submerged the underwater PP takes over and the cap is mostly seen from below.
	 *
	 * See `2026-05-10_underwater_rendering_plan.md` §6.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|CapMesh")
	bool bGenerateSkirts = true;

	/** Vertical extent of the skirt below the water surface (cm). Must reach below the door's
	 *  lower edge for the water column to appear contiguous when viewed through an open doorway.
	 *  Default 600cm covers Craniata's tallest compartments + 20° tilt margin. The skirt pokes
	 *  below the floor on most cases — fine because the floor mesh occludes it from the inside;
	 *  outside views (camera in adjacent dry compartment) see only the portion within the door. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|CapMesh", meta = (ClampMin = "10.0"))
	float SkirtHeightCm = 600.f;

	// ── Slosh modal (P3.6) ──────────────────────────────────────────────────
	// Spring-damper that pushes the cap mesh's vertical offset + tilt in response to the sub's
	// linear accel (sub-local frame). Forward accel → water tilts rear, lateral accel → water rolls,
	// vertical accel → water bounces. Pure visual; does not feed back into the heightfield.

	/** Master toggle for the slosh modal effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Slosh")
	bool bEnableSlosh = true;

	/** Natural oscillation frequency in Hz. Lower = slower, larger swings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Slosh", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float SloshNaturalFreqHz = 0.8f;

	/** Damping ratio. 0 = undamped, 1 = critical, >1 = overdamped. 0.15-0.25 = visible swing without ringing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Slosh", meta = (ClampMin = "0.0", ClampMax = "1.5"))
	float SloshDampingRatio = 0.15f;

	/** Sub-local Z accel → vertical offset velocity gain. Higher = more vertical bounce. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Slosh", meta = (ClampMin = "0.0"))
	float SloshOffsetGain = 0.05f;

	/** Sub-local horizontal accel → tilt velocity gain. Higher = stronger tilt response. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Slosh", meta = (ClampMin = "0.0"))
	float SloshTiltGain = 0.0008f;

	/** Maximum tilt magnitude (degrees). Tilt is clamped to ±this on each axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Slosh", meta = (ClampMin = "0.0", ClampMax = "20.0"))
	float MaxSloshTiltDeg = 3.0f;

	/** Maximum vertical offset magnitude (cm). OffsetZ clamped to ±this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Slosh", meta = (ClampMin = "0.0"))
	float MaxSloshOffsetCm = 8.0f;

	// Crew wakes & sub inertia wave injections were rolled back 2026-05-09.
	// They were quickwin point-injects on a scalar wave equation that didn't constitute a
	// system. A proper design (stamps + momentum field, or Saint-Venant lite) is deferred to
	// a "P3.10 Water momentum" jalon — see TODO post-Phase-3.

	/**
	 * Inject a radial perturbation into the heightfield at the given local-space XY (sub-local frame).
	 * Force is added to Heights with a linear falloff over Radius cm. Has no effect if heightfield
	 * is not initialized or `bEnableHeightfield = false`.
	 */
	UFUNCTION(BlueprintCallable, Category = "Flood|Water|Heightfield")
	void InjectAt(FVector2D LocalPosXY, float Force, float Radius = 50.f);

	/** Helper: convert world-space hit to sub-local XY and call InjectAt. Returns false if the point
	 *  is outside the bake's LocalBounds (with Radius tolerance) — avoids polluting the field with stray clicks. */
	UFUNCTION(BlueprintCallable, Category = "Flood|Water|Heightfield")
	bool InjectAtWorldPoint(FVector WorldPos, float Force, float Radius = 50.f);

	/** Zero out Heights and Velocities arrays. */
	UFUNCTION(BlueprintCallable, Category = "Flood|Water|Heightfield")
	void ResetHeightfield();

	// ── Breach reaction (P3.5) ──────────────────────────────────────────────
	// On first detection of a replicated breach for this compartment, inject a wave at the
	// breach point and spawn a Niagara system. Both run on every client (heightfield is
	// client-side cosmetic, breach state replicates via USubFloodComponent::Breaches).

	/** Niagara system spawned at the breach point on first detection. Slot is left empty by default
	 *  — assign a "water gushing in" effect on the BP_Submarine_Craniata's UFloodWaterPlaneComponent
	 *  defaults, or via DefaultBreachWaterImpactVfx on ASubmarineBase (propagated). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Breach")
	TObjectPtr<UNiagaraSystem> BreachWaterImpactVfx = nullptr;

	/** Force injected into the heightfield on breach detection. Higher = bigger initial splash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Breach", meta = (ClampMin = "0.0"))
	float BreachInjectForce = 30.f;

	/** Falloff radius (cm) of the breach wave injection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Breach", meta = (ClampMin = "1.0"))
	float BreachInjectRadiusCm = 80.f;

	/** Hertz at which a residual jet keeps perturbing the surface while the breach is still
	 *  flowing. 0 = no recurring inject (one-shot only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Breach", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float BreachRecurringInjectHz = 4.f;

	/** Force per recurring inject pulse (much smaller than the initial impact). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Breach", meta = (ClampMin = "0.0"))
	float BreachRecurringInjectForce = 4.f;

private:
	/** Creates the PlaneMeshComponent child and assigns the mesh/material. */
	void EnsurePlaneMesh();

	/** Look up the UCompartmentWaterBake in the owning submarine's DA. Cached after first call. */
	UCompartmentWaterBake* ResolveBake();

	/** Build/update the PMC cap mesh from the bake's slice closest to the current water Z.
	 *  WaterHeightLocalCm is "height above flood floor" (UCompartmentVolumeComponent::GetWaterHeightCm).
	 *  Returns true when the bake path is active and rendering. */
	bool RefreshBakeCapMesh(float WaterHeightLocalCm);

	/** Applies Z + visibility from the source volume to the legacy plane. */
	void ApplyWaterState(float NewLevel01, float NewHeightCm);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PlaneMeshComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> BakeCapMeshComp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BakeCapMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCompartmentWaterBake> CachedBake = nullptr;

	int32 LastSliceIndex = -1;
	float LastLevel01 = -1.f;
	bool bLastVisible = false;
	bool bBakeResolveAttempted = false;

	// ── Heightfield runtime state (P3.4) ────────────────────────────────────

	/** Lazy init: creates buffers + texture sized to HeightfieldGridX × HeightfieldGridY,
	 *  binds HeightfieldTextureParamName / LocalBoundsMinParamName / LocalBoundsMaxParamName
	 *  / HeightfieldAmplitudeParamName
	 *  on BakeCapMID. Idempotent. Requires CachedBake to be resolved. */
	void EnsureHeightfieldInitialized();

	/** Wave equation update — Neumann reflective BC on all cells (incl. boundary). One step. */
	void TickHeightfieldStep();

	/** Push Heights (R32F) → HeightfieldTex via UpdateTextureRegions (async). */
	void PushHeightfieldToTexture();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> HeightfieldTex = nullptr;

	TArray<float> Heights;
	TArray<float> Velocities;
	float HeightfieldAccum = 0.f;
	bool bHeightfieldInitialized = false;

	// ── Breach state tracking (P3.5) ────────────────────────────────────────
	void RefreshBreachReaction();

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> BreachVfxInstance = nullptr;

	bool bBreachActive = false;
	FVector LastBreachLocalCenter = FVector::ZeroVector;
	float BreachInjectAccum = 0.f;

	// ── Slosh modal state (P3.6) ────────────────────────────────────────────
	void UpdateSloshModal(float Dt);
	void ApplyCapMeshTransformWithSlosh();

	float SloshOffsetZ = 0.f;
	float SloshOffsetVelZ = 0.f;
	FVector2D SloshTilt = FVector2D::ZeroVector;     // X = pitch fraction, Y = roll fraction
	FVector2D SloshTiltVel = FVector2D::ZeroVector;
	FVector LastSubWorldVelocity = FVector::ZeroVector;
	bool bSloshSeeded = false;
	float CurrentBaseWaterZLocal = 0.f;
};
