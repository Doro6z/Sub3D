#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrewWaterSampleTypes.h"
#include "CrewWaterStateComponent.generated.h"

class ASubCrewCharacter;
class UCapsuleComponent;
class UCompartmentVolumeComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPostProcessComponent;

/**
 * Crew water state + underwater post-process driver.
 *
 * Renamed 2026-05-11 (was UCrewUnderwaterPPComponent). The scope is now broader than
 * post-process — the component owns multi-probe water detection, mode derivation, and
 * the underwater PP material drive in a single place.
 *
 * Responsibilities :
 *  - Service `RefreshWaterState(FCrewWaterSample&)` called by ASubCrewCharacter::UpdateEnvironmentalEffects
 *  - Local tick drives the underwater PostProcessComponent (UnderwaterAlpha + HeadDepth)
 *  - Per-probe enter/exit events for VFX / audio / respiration hooks
 *  - Mode state machine (Dry / Wading / Swimming / FullySubmerged) for HUD + animation
 *
 * Runs the service on server + locally-authoritative (drives mouvement). Runs the PP only on
 * the owning/autonomous client (underwater is a local camera effect).
 */
UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API UCrewWaterStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCrewWaterStateComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Post-process material applied when submerged. Responsible for tint, distortion, caustiques, fog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Material")
	TObjectPtr<UMaterialInterface> UnderwaterPostProcessMaterial = nullptr;

	/** Blend speed when entering water (higher = faster). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Blend", meta = (ClampMin = "0.1"))
	float BlendInSpeed = 4.f;

	/** Blend speed when exiting water (higher = faster). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Blend", meta = (ClampMin = "0.1"))
	float BlendOutSpeed = 6.f;

	/** Camera Z distance to water surface that triggers the waterline proximity event each tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Waterline", meta = (ClampMin = "0.0"))
	float WaterlineCrossThresholdCm = 15.f;

	/**
	 * World Z of the ocean surface when the crew is EVA (EmbarkState == Outside).
	 * Default 0. Post-FP: replace with a proper ocean water volume or global plane reference.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Ocean")
	float OceanSurfaceZ = 0.f;

	// ── Probe configuration ───────────────────────────────────────────────────
	// Capsule-based body sampling, animation-independent.

	/** Vertical inset of the Head probe from the top of the capsule (cm). The capsule top is
	 *  slightly above the visible head mesh on most rigs, so we pull the probe down a few cm
	 *  to match the eye / mouth line. Default 10 cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Probes",
		meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float HeadProbeInsetCm = 10.f;

	// ── Hysteresis (Phase B) ──────────────────────────────────────────────────
	// Per-probe enter/exit margins prevent flickering at the waterline.

	/** Probe transitions to submerged when its Z < surface - EnterMarginCm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Hysteresis",
		meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float EnterMarginCm = 5.f;

	/** Probe transitions back to dry only when its Z > surface + ExitMarginCm. Kept smaller than
	 *  EnterMarginCm so a probe slightly below the surface stays submerged stably. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Hysteresis",
		meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float ExitMarginCm = 3.f;

	// ── BP events ──────────────────────────────────────────────────────────────
	// Per-probe enter/exit + mode change. View probe drives the legacy compatibility events
	// BP_OnEnterWater / BP_OnExitWater (kept for back-compat with any existing BP hooks).

	/** Fired once when the camera (View probe) crosses below the water surface. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnEnterWater();

	/** Fired once when the camera crosses above the water surface. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnExitWater();

	/**
	 * Fired each tick while the camera is within WaterlineCrossThresholdCm of the surface.
	 * Designer hook for waterline glitch mitigation: droplets on screen, chromatic aberration,
	 * splash distortion. DistanceCm is signed: negative = just below surface, positive = just above.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnWaterlineProximity(float DistanceCm);

	/** Head probe transitions (drives respiration hooks). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnHeadEnterWater();

	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnHeadExitWater();

	/** Torso probe transitions (informative — animation lean, audio). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnTorsoEnterWater();

	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnTorsoExitWater();

	/** Feet probe transitions (splash audio, wet-feet decals). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnFeetEnterWater();

	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnFeetExitWater();

	/** Derived water mode changed (Dry / Wading / Swimming / FullySubmerged). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnModeChanged(ECrewWaterMode NewMode);

	// ── Accessors ─────────────────────────────────────────────────────────────

	/** True while the View probe (camera) is below the water surface (post-hysteresis). */
	UFUNCTION(BlueprintPure, Category = "Underwater")
	bool IsUnderwater() const { return bIsUnderwater; }

	/** Current smoothed blend alpha (0..1). Drives the PP BlendWeight. */
	UFUNCTION(BlueprintPure, Category = "Underwater")
	float GetBlendAlpha() const { return CurrentBlendAlpha; }

	/** Raw signed distance camera-to-surface (negative = below). Useful for custom BP logic. */
	UFUNCTION(BlueprintPure, Category = "Underwater")
	float GetDistanceToSurfaceCm() const { return LastDistanceCm; }

	/** Access the PP component for advanced customization. */
	UFUNCTION(BlueprintPure, Category = "Underwater")
	UPostProcessComponent* GetPostProcessComponent() const { return PostProcessComp; }

	/**
	 * Synchronously samples the crew's water state at View, Head, Torso, Feet probes and
	 * fills the FCrewWaterSample. Each probe resolves its own compartment via
	 * ASubmarineBase::FindCompartmentIdAtLocalLocation (so head/feet can sit in different
	 * compartments at door thresholds or multi-volume layouts). Surface Z comes from
	 * USubFloodComponent::GetCompartmentSurfaceWorldZ (tilt-correct, water-horizontal).
	 *
	 * `WaterImmersion01` uses **the crew's CurrentCompartment surface** as the reference (D10) —
	 * NOT the per-probe surfaces — to keep the value stable for ApplyWaterMovementState
	 * thresholds. Per-probe `bSubmerged` flags are informative outputs (events / mode / audio).
	 *
	 * Called per tick by ASubCrewCharacter::UpdateEnvironmentalEffects as a synchronous service.
	 *
	 * Returns false if no crew / no submarine / no capsule (sample left zero-initialized).
	 */
	UFUNCTION(BlueprintCallable, Category = "Underwater")
	bool RefreshWaterState(FCrewWaterSample& Out);

	/** Last sample computed by RefreshWaterState. Read by debug HUD / other consumers without
	 *  re-running probe queries. Initialised to zero until first RefreshWaterState call. */
	UFUNCTION(BlueprintPure, Category = "Underwater")
	const FCrewWaterSample& GetLastSample() const { return LastSample; }

private:
	/** Helper for RefreshWaterState : fills one probe at the given world location.
	 *  `bPreviousSubmerged` drives the hysteresis margin (enter vs exit). */
	void FillProbe(FCrewWaterProbe& Probe, const FVector& WorldLocation,
		const class ASubmarineBase* Sub, const FTransform& SubXf,
		bool bPreviousSubmerged) const;

	/** Derives the mode from per-probe bSubmerged flags. Head precedence (drives FullySubmerged). */
	static ECrewWaterMode DeriveMode(const FCrewWaterSample& Sample);

	/** Creates the PostProcessComponent sub-object and assigns the material as a blendable. */
	void EnsurePostProcessComponent();

	/** Resolves the water surface Z for the current crew state (compartment interior or ocean).
	 *  Used by the local PP tick to drive UnderwaterAlpha from the View probe camera. */
	float ResolveWaterSurfaceZ() const;

	/** Draws the water probe spheres and on-screen water HUD when crew debug is enabled. */
	void DrawDebugWaterSample(const FCrewWaterSample& Sample, bool bSampleValid) const;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> PostProcessComp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PostProcessMID = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<ASubCrewCharacter> CachedCrew;

	bool bIsUnderwater = false;
	bool bWasUnderwaterLastTick = false;
	float CurrentBlendAlpha = 0.f;
	float LastDistanceCm = 0.f;

	// Per-probe last-tick submergence for hysteresis + transition event firing.
	bool bLastViewSubmerged = false;
	bool bLastHeadSubmerged = false;
	bool bLastTorsoSubmerged = false;
	bool bLastFeetSubmerged = false;
	ECrewWaterMode LastMode = ECrewWaterMode::Dry;

	/** Cached sample updated by RefreshWaterState. Read by TickComponent to push enriched
	 *  parameters to the underwater PP MID without re-computing the probes. */
	FCrewWaterSample LastSample;

	bool bLoggedMissingCrew = false;
	bool bLoggedMissingCapsule = false;
};
