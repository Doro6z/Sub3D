#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Engine/EngineTypes.h"
#include "CompartmentVolumeComponent.generated.h"

class AAudioVolume;
class APostProcessVolume;
class USubFloodComponent;
class UStaticMesh;

/** Collision channel for compartment / hull-boundary probes. Matches DefaultEngine.ini entry "CompartmentProbe". */
#define ECC_CompartmentProbe ECC_GameTraceChannel3

/**
 * Editor-placed box volume that defines a compartment region inside the submarine.
 * Two roles:
 *  - (legacy) Authoring hint for bake pipelines (CapacityLitersOverride, WalkableFloorZCmOverride).
 *  - (runtime) Environmental zone probe: overlaps with the crew capsule on channel
 *              ECC_CompartmentProbe and drives ASubCrewCharacter::CurrentCompartment.
 * Multiple volumes with the same CompartmentId are merged during bake.
 * Place these as children of the submarine root in the BP.
 */
UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API UCompartmentVolumeComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UCompartmentVolumeComponent();

	/** Compartment name. Volumes sharing the same name are merged into one compartment. Also the key used by USubFloodComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FName CompartmentId = NAME_None;

	/** Display name shown in UI (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FText DisplayName;

	/** Water capacity in liters. 0 = auto-calculate from volume box dimensions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment", meta = (ClampMin = "0.0"))
	float CapacityLitersOverride = 0.f;

	/** Floor Z in local space of the submarine. 0 = use bottom of the box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	float WalkableFloorZCmOverride = 0.f;

	/** Oxygen level 0..1. Stub for FP (always 1). Future: consumed over time, produced by life support. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment|Env", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float O2Level01 = 1.f;

	/** Audio volume to activate when the crew enters this compartment. Not wired in FP. */
	UPROPERTY(EditAnywhere, Category = "Compartment|Audio")
	TObjectPtr<AAudioVolume> LinkedAudioVolume = nullptr;

	/** Post-process volume to activate when the crew enters this compartment. Not wired in FP. */
	UPROPERTY(EditAnywhere, Category = "Compartment|FX")
	TObjectPtr<APostProcessVolume> LinkedPostProcessVolume = nullptr;

	/** Editor wireframe color for this compartment volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Visual")
	FColor VolumeColor = FColor(50, 180, 220, 255);

	// ── Filled-face X-ray visualization (placement aid) ──────────────────────────
	// Renders 6 translucent faces with per-face colors so volumes can be precisely
	// placed/scaled in the BP editor even when hull meshes occlude the wireframe.
	// All faces drawn in foreground (visible through any opaque mesh) when bDrawXRay
	// is true. Disable both bShowFilledFaces and bDrawXRay to fall back to standard
	// UBoxComponent wireframe-only rendering.

	/** If true, render 6 translucent filled faces with per-face colors (placement aid). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Visual|XRay")
	bool bShowFilledFaces = true;

	/** If true, draw in foreground depth priority — visible through opaque meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Visual|XRay")
	bool bDrawXRay = true;

	/** Translucency of the 6 filled faces (0 = invisible, 1 = opaque). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Visual|XRay", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bShowFilledFaces"))
	float FaceOpacity = 0.18f;

	/** Color of the +X face (bow side in BP-local space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Visual|XRay", meta = (EditCondition = "bShowFilledFaces"))
	FLinearColor FaceColorXPos = FLinearColor(1.0f, 0.3f, 0.3f);

	/** Color of the -X face (stern side). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Visual|XRay", meta = (EditCondition = "bShowFilledFaces"))
	FLinearColor FaceColorXNeg = FLinearColor(0.3f, 1.0f, 1.0f);

	/** Color of the +Y face (starboard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Visual|XRay", meta = (EditCondition = "bShowFilledFaces"))
	FLinearColor FaceColorYPos = FLinearColor(0.3f, 1.0f, 0.3f);

	/** Color of the -Y face (port). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Visual|XRay", meta = (EditCondition = "bShowFilledFaces"))
	FLinearColor FaceColorYNeg = FLinearColor(1.0f, 1.0f, 0.3f);

	/** Color of the +Z face (top). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Visual|XRay", meta = (EditCondition = "bShowFilledFaces"))
	FLinearColor FaceColorZPos = FLinearColor(0.6f, 0.6f, 1.0f);

	/** Color of the -Z face (bottom). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Visual|XRay", meta = (EditCondition = "bShowFilledFaces"))
	FLinearColor FaceColorZNeg = FLinearColor(1.0f, 0.4f, 1.0f);

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	/** Public accessor for the protected UShapeComponent::LineThickness — used by our scene proxy. */
	float GetEditorLineThickness() const { return LineThickness; }

	/**
	 * Per-compartment water surface mesh ("water cap"). When set, UFloodWaterPlaneComponent
	 * uses this mesh at scale (1,1,1) instead of the generic engine plane. Authored in
	 * Blender to match the compartment's horizontal cross-section at deck level — its
	 * geometry IS the containment: the material no longer needs to clip spatially, only
	 * to polish the edge where the water meets hull/bulkheads.
	 * See: reports/guides/2026-04-24_water_cap_authoring_and_material_functions.md
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Flood")
	TObjectPtr<UStaticMesh> WaterPlaneMeshOverride = nullptr;

	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// ── Flood data provider (Phase A 2026-04-23) ─────────────────────────────
	// These are pure lookups into the owning submarine's USubFloodComponent. Purely
	// passive — used by UFloodWaterPlaneComponent (visual) and ASubCrewCharacter
	// (underwater detection). BP-friendly for art designer.

	/** Absolute water height in cm from compartment floor (from SubFlood sim). 0 if no flood state. */
	UFUNCTION(BlueprintPure, Category = "Compartment|Flood")
	float GetWaterHeightCm() const;

	/** Normalized fill 0..1 (CurrentWaterLiters / CapacityLiters). 0 if no flood state. */
	UFUNCTION(BlueprintPure, Category = "Compartment|Flood")
	float GetWaterLevel01() const;

	/**
	 * Local-space flood floor used by the manual volume path. When WalkableFloorZCmOverride is set,
	 * it wins over the raw bottom of the box so the visible water starts at the authored floor.
	 */
	UFUNCTION(BlueprintPure, Category = "Compartment|Flood")
	float GetFloodBottomLocalZCm() const;

	/** Effective floodable height in cm for this volume, from authored floor to box top. */
	UFUNCTION(BlueprintPure, Category = "Compartment|Flood")
	float GetFloodMaxHeightCm() const;

	/**
	 * World-space location of the water surface for this compartment. X/Y = volume center,
	 * Z = volume bottom + WaterHeightCm rotated by sub orientation. Used by water plane
	 * visuals to position themselves, and by crew underwater detection.
	 */
	UFUNCTION(BlueprintPure, Category = "Compartment|Flood")
	FVector GetWaterSurfaceWorldLocation() const;

	/** Returns the owning submarine's USubFloodComponent, or nullptr if not resolved. Cached. */
	UFUNCTION(BlueprintPure, Category = "Compartment|Flood")
	USubFloodComponent* GetFlood() const;

private:
	// Transient cache to avoid iterating the owner's components each tick.
	mutable TWeakObjectPtr<USubFloodComponent> CachedFlood;
};
