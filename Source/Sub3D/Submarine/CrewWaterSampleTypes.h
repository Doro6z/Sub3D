#pragma once

#include "CoreMinimal.h"
#include "CrewWaterSampleTypes.generated.h"

/**
 * Derived water-immersion state of the crew. Computed from per-probe submergence flags.
 *
 *   Dry             : aucune probe submergée — walking normal
 *   Wading          : Feet submergée, Torso et Head dry — walk ralenti
 *   Swimming        : Torso submergée, Head dry — swim mode (ApplyWaterMovementState peut basculer
 *                     selon WaterImmersion01 ≥ SwimThreshold01)
 *   FullySubmerged  : Head submergée — breath-hold actif, drowning timer
 *
 * Notes :
 *  - Ce Mode est **informatif** (HUD, animation, audio). Le swim trigger côté movement reste
 *    piloté par WaterImmersion01 continu via ApplyWaterMovementState (D3 + D11).
 *  - Hystérésis : appliquée per-probe (EnterMargin/ExitMargin) en amont, pas sur le Mode.
 */
UENUM(BlueprintType)
enum class ECrewWaterMode : uint8
{
	Dry             UMETA(DisplayName = "Dry"),
	Wading          UMETA(DisplayName = "Wading"),
	Swimming        UMETA(DisplayName = "Swimming"),
	FullySubmerged  UMETA(DisplayName = "Fully Submerged")
};

/**
 * Per-probe sample of the crew's water state at a specific body point.
 *
 * Filled per-tick by UCrewWaterStateComponent::RefreshWaterState. Reads in world Z and
 * uses USubFloodComponent::GetCompartmentSurfaceWorldZ for tilt-correct surface elevation
 * (water is gravity-aligned, NOT sub-aligned).
 *
 * Probes are resolved independently — each one can sit in a different compartment than the
 * crew capsule centre, which matters in multi-level Craniata compartments (e.g. Main_Bow
 * upper/lower) and along door thresholds.
 */
USTRUCT(BlueprintType)
struct SUB3D_API FCrewWaterProbe
{
	GENERATED_BODY()

	/** World-space position of the probe (computed from capsule or camera). */
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	FVector WorldLocation = FVector::ZeroVector;

	/** Compartment id resolved via ASubmarineBase::FindCompartmentIdAtLocalLocation.
	 *  NAME_None if the probe is outside any compartment (ocean / between volumes). */
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	FName CompartmentId = NAME_None;

	/** World Z of the water surface at this probe's compartment.
	 *  Equals OceanSurfaceZ when CompartmentId is NAME_None.
	 *  Always tilt-correct (water-horizontal). */
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	float SurfaceWorldZ = 0.f;

	/** True if the probe is below its compartment's surface. */
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	bool bSubmerged = false;

	/** Positive cm below the surface (0 if not submerged). */
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	float DepthBelowSurfaceCm = 0.f;
};

/**
 * Aggregated water state of the crew, computed in one synchronous pass per tick.
 *
 * Layout :
 *   - View  : active camera position (drives Underwater PP alpha)
 *   - Head  : capsule top minus HeadOffset (drives respiration logic)
 *   - Torso : capsule centre (informative — wading vs swim is decided by WaterImmersion01 thresholds)
 *   - Feet  : capsule bottom (informative — splash audio, wet feet)
 *
 * `WaterImmersion01` keeps the continuous semantics used by
 * ASubCrewCharacter::ApplyWaterMovementState (existing) :
 *     (SurfaceWorldZ - FeetWorldZ) / (HeadWorldZ - FeetWorldZ)
 * clamped to [0, 1]. This is the only value the movement layer consumes ; per-probe flags
 * are informative outputs for VFX / audio / respiration, NOT inputs to swim mode thresholds.
 */
USTRUCT(BlueprintType)
struct SUB3D_API FCrewWaterSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	FCrewWaterProbe View;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	FCrewWaterProbe Head;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	FCrewWaterProbe Torso;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	FCrewWaterProbe Feet;

	/** Continuous [0..1] immersion fraction along the body axis. Consumed by
	 *  ASubCrewCharacter::ApplyWaterMovementState (drives walk / wade / swim thresholds). */
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	float WaterImmersion01 = 0.f;

	/** Convenience aggregate : true when any probe is submerged. */
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	bool bAnySubmerged = false;

	/** Derived water-immersion mode. Informative (HUD / animation / audio). Swim trigger côté
	 *  movement reste piloté par WaterImmersion01 + ApplyWaterMovementState thresholds (D3 + D11). */
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Water")
	ECrewWaterMode Mode = ECrewWaterMode::Dry;
};
