#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructuralHullTypes.h"
#include "SubmarineRuntimeTypes.h"
#include "DoorFloodVfxComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USubFloodComponent;
class USubHullComponent;
class USubmarineCompartmentComponent;
class ASubmarineBase;

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API UDoorFloodVfxComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDoorFloodVfxComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Flood|DoorVFX")
	int32 GetActiveCascadeCount() const { return ActiveCascadeCount; }

	UFUNCTION(BlueprintCallable, Category = "Flood|DoorVFX")
	void RefreshFromCurrentFloodState();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX")
	TObjectPtr<UNiagaraSystem> CascadeEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX", meta = (ClampMin = "0.0"))
	float HeightDeltaThresholdCm = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX", meta = (ClampMin = "1.0"))
	float MaxHeightDeltaCm = 150.f;

	// ── Flow-rate-driven cascade (P3.8) ──────────────────────────────────────
	// Replaces the legacy HeightDelta-driven sizing. Reads the per-edge `CurrentFlowRateLitersPerSec`
	// directly from USubFloodComponent::EdgeStates (server-authored, replicated). Includes the
	// effect of OpenRatio naturally — a half-open door produces half the flow rate, hence half the
	// cascade intensity. Set `bUseFlowRateMode = false` to fall back to the legacy HeightDelta path
	// (kept for A/B comparison while the new path bakes in).

	/** Master toggle for the new flow-rate-driven cascade path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|FlowRate")
	bool bUseFlowRateMode = true;

	/** Minimum |FlowRateLps| before a cascade is spawned. Filters out trickle / equilibrium noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|FlowRate", meta = (ClampMin = "0.0"))
	float FlowRateThresholdLps = 200.f;

	/** Flow rate that maps to full cascade intensity (Intensity01 = 1.0). Higher rates clamp at 1.0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|FlowRate", meta = (ClampMin = "1.0"))
	float MaxFlowRateLps = 8000.f;

	// ── Head-delta signal (preferred — direct visual mapping) ────────────────
	// When bUseFlowRateMode == true AND bUseHeadDeltaSignal == true, intensity is derived from
	// the signed head delta on the edge (FFloodEdgeState::CurrentHeadDeltaCm) via a smoothstep:
	//   Intensity01 = smoothstep(StartCm, MaxVisualCm, |Δh|)
	// then asymmetric InterpTo smoothing (ramp-up faster than ramp-down). Visual designers tune
	// "violent flow looks like X cm head" more easily than "X liters per sec". Flow rate Lps
	// remains the fallback when bUseHeadDeltaSignal == false.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|HeadDelta")
	bool bUseHeadDeltaSignal = true;

	/** |Δh| at which the cascade becomes barely visible (Intensity01 ≈ 0). Below = invisible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|HeadDelta", meta = (ClampMin = "0.0"))
	float HeadDeltaStartCm = 5.f;

	/** |Δh| at which the cascade saturates (Intensity01 = 1). Above = clamped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|HeadDelta", meta = (ClampMin = "1.0"))
	float HeadDeltaMaxCm = 120.f;

	/** Source water speed at full intensity (cm/s). Sent to the Niagara as `User.SourceSpeedCmS`. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|HeadDelta", meta = (ClampMin = "0.0"))
	float SourceSpeedMaxCmS = 300.f;

	/** Source water speed at zero intensity (cm/s). Even at low intensity, a baseline speed
	 *  reads as "water has momentum"; 0 = no movement (rarely desired). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|HeadDelta", meta = (ClampMin = "0.0"))
	float SourceSpeedMinCmS = 80.f;

	// ── Spawn offset (P3.8 polish) ──────────────────────────────────────────
	// Compense the Sphere Location offset interne au template FLIP_Hose, et permet de placer la
	// source au seuil (sill) de la porte plutôt qu'au centre. Default = (-60 cm Z world, 0 along
	// flow) → spawn juste au-dessus du sol de la porte, dans son plan vertical.

	/** Spawn offset along FlowDirection (cm). Positive = push source forward into the receiving
	 *  compartment so cascade doesn't backflow into the source compartment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|Source", meta = (ClampMin = "-200.0", ClampMax = "200.0"))
	float SpawnOffsetFlowDirCm = 0.f;

	/** Vertical world-Z offset for spawn position (cm). Negative = move spawn down toward door
	 *  sill. Doors are typically ~200 cm tall; centre at ~100 cm; sill at ~10 cm. -60 puts spawn
	 *  in the lower third of the doorway. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|Source", meta = (ClampMin = "-200.0", ClampMax = "200.0"))
	float SpawnOffsetWorldZCm = -60.f;

	/** Uniform scale applied to the Niagara component. Scales the FLIP domain + particle size +
	 *  emission volume together. Default 10 because the FLIP_Hose template authors at ~1 m³ which
	 *  reads tiny at submarine scale (cascade window ~2m wide). Tune in PIE. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|Source", meta = (ClampMin = "0.1", ClampMax = "50.0"))
	float CascadeScaleMultiplier = 10.f;

	/** Intensity ramp-up speed (FInterpTo). Higher = faster ramp when flow appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|HeadDelta", meta = (ClampMin = "0.1"))
	float IntensityRampUpSpeed = 10.f;

	/** Intensity ramp-down speed (FInterpTo). Lower than ramp-up so the cascade fades gracefully
	 *  instead of cutting off when flow drops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|HeadDelta", meta = (ClampMin = "0.1"))
	float IntensityRampDownSpeed = 4.f;

	/** Niagara user param for the local-space flow direction (replaces FlowDirectionParam when
	 *  bUseHeadDeltaSignal=true). Local space because Niagara emitter is `Local Space = true`. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|HeadDelta|Parameters")
	FName FlowDirectionLocalParam = TEXT("FlowDirectionLocal");

	/** Niagara user param for the source speed (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|HeadDelta|Parameters")
	FName SourceSpeedParam = TEXT("SourceSpeedCmS");

	/**
	 * How strongly the FlowDirection biases toward world-down (gravity). 0 = pure door-forward
	 * direction; 1 = equal blend; 2 = gravity dominates. Water cascading through a doorway
	 * physically falls under gravity once past the threshold; for FP visual feel, a bias of 1.5–2
	 * looks right (the cascade visibly cascades DOWN rather than shooting straight across).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|FlowRate", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float GravityBlendStrength = 1.5f;

	// ── Debug (P3.8) ─────────────────────────────────────────────────────────
	/** Per-frame Output Log dump of the cascade pipeline. Shows edge count, per-edge OpenRatio +
	 *  flow rate, threshold check, door lookup result, and final spawn decision. Filter the log
	 *  by "DoorCascade" to see only these lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|Debug")
	bool bLogVfxDecisions = false;

	/** Debug arrow drawn at each door currently spawning a cascade. Arrow start = door world
	 *  position, length scaled by intensity, direction = FlowDirection. Useful to verify the
	 *  asset is positioned correctly and oriented in the flow direction. PIE only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|Debug")
	bool bDrawDebugArrows = false;

	/** Debug text per open door, drawn above its centre, showing live values:
	 *  - Δh (cm) head delta
	 *  - Q (L/s) flow rate
	 *  - Open % door open ratio
	 *  - Smoothed intensity (0..1)
	 *  - State: ACTIVE / DRAIN / SKIP_FLOW / SKIP_DH
	 *  PIE only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|Debug")
	bool bDrawDebugValues = false;

	// ── Lifecycle (P3.8 polish) ──────────────────────────────────────────────
	/**
	 * Grace period (seconds) between "flow dropped below threshold" and the cascade being hard-
	 * deactivated. During this window FlowIntensity01 is held at 0 (the spawn rate stops) but
	 * the Niagara component stays active so in-flight FLIP particles drain naturally over their
	 * lifetime — avoids the "fluids pop on/off" hard cut documented in the Niagara Fluids
	 * lifecycle research. 2s covers typical particle lifetime of 1.0–1.5s with margin.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|Lifecycle", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float DeactivateGracePeriodSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX", meta = (ClampMin = "0"))
	int32 MaxActiveCascades = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|Parameters")
	FName FlowIntensityParam = TEXT("FlowIntensity01");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|Parameters")
	FName FlowDirectionParam = TEXT("FlowDirection");

protected:
	UFUNCTION()
	void HandleSubFloodUpdated(const TArray<FCompartmentState>& InStates);

	UFUNCTION()
	void HandleFloodInitialized();

private:
	void ActivateSubFloodPath();

	struct FDoorCascadeCandidate
	{
		FName DoorId;
		FTransform WorldTransform;
		float HeightDeltaCm;      // legacy field (flow-rate-mapped or raw |Δh|), kept for sort
		FVector FlowDirection;    // world-space, gravity-blended (legacy)
		float Intensity01 = 0.f;  // final smoothstep output (head-delta path)
		FVector FlowDirectionLocal = FVector::ZeroVector;  // local-to-cascade-component
		float SourceSpeedCmS = 0.f;
	};

	UNiagaraComponent* GetOrCreateCascadeComponent(int32 CascadeIndex);
	void ApplyCascadeParameters(UNiagaraComponent* Component, const FDoorCascadeCandidate& Candidate) const;
	void DeactivateUnusedCascades(int32 FirstUnusedIndex);
	void DestroyPooledCascades();

	float GetCompartmentWaterHeightCmFromStates(const TArray<FCompartmentState>& States, FName CompartmentId) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<USubFloodComponent> SubFlood = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USubHullComponent> SubHull = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USubmarineCompartmentComponent> CompartmentComp = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> CascadePool;

	/** Per-slot state parallel to CascadePool. Tracks which door this slot is currently serving
	 *  (NAME_None = free) and when the slot last had a candidate with flow > threshold.
	 *  The slot stays active for `DeactivateGracePeriodSeconds` after AssignedDoorId stops
	 *  showing up in candidates, FlowIntensity01 ramped to 0 so particles drain naturally.
	 *  After grace, DeactivateImmediate releases the GPU buffers and AssignedDoorId is cleared. */
	struct FCascadeSlotState
	{
		FName AssignedDoorId = NAME_None;
		float LastActiveTimeSeconds = 0.f;
		float SmoothedIntensity01 = 0.f;  // FInterpTo target tracking (asymmetric ramp)
	};
	TArray<FCascadeSlotState> SlotStates;

	int32 ActiveCascadeCount = 0;
};
