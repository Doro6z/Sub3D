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
};
