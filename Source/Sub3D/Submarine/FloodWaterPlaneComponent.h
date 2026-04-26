#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FloodWaterPlaneComponent.generated.h"

class UCompartmentVolumeComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Visual water plane driven by a single UCompartmentVolumeComponent. 100% passive C++:
 * updates world Z to match the compartment's water surface, toggles visibility, and fires
 * BP events on changes. All visual intelligence (hull clipping via Global Distance Fields,
 * caustics, refraction) lives in the assigned Material — this component does not decide
 * what the water looks like.
 *
 * Art designer workflow:
 *  - Assign `WaterMaterial` on the ASubmarineBase (propagated to all compartments).
 *  - Or override per-instance by setting `WaterMaterial` directly on this component.
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

	/** Mesh used as the water surface. Default: engine's BasicShapes/Plane (1m x 1m). Scaled by PlaneWorldSizeCm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Visual")
	TObjectPtr<UStaticMesh> PlaneMesh = nullptr;

	/** Material applied to the plane. The material is responsible for hull clipping (Global DF) and look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Visual")
	TObjectPtr<UMaterialInterface> WaterMaterial = nullptr;

	/** World-space size of the plane (X=Y). 80m default covers a large sub with padding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Visual", meta = (ClampMin = "100.0"))
	float PlaneWorldSizeCm = 8000.f;

	/** Plane is hidden when the compartment's WaterLevel01 is below this threshold. Avoids Z-fighting at dry floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Visual", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VisibilityThreshold01 = 0.02f;

	/** Minimum absolute change in WaterLevel01 between ticks before BP_OnWaterLevelChanged fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Water|Events", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LevelChangeEventThreshold01 = 0.005f;

	/** Fired when water level changes by more than LevelChangeEventThreshold01. Designer hook for bulles / VFX / sfx. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Flood|Water|Events")
	void BP_OnWaterLevelChanged(float NewLevel01, float NewHeightCm);

	/** Fired when the plane toggles visible/hidden. Designer hook for water loop sfx start/stop. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Flood|Water|Events")
	void BP_OnVisibilityChanged(bool bNowVisible);

	/** Manual refresh for BP triggering (editor preview, debug). */
	UFUNCTION(BlueprintCallable, Category = "Flood|Water")
	void RefreshFromFlood();

	/** Access the child mesh component (for advanced BP customization of render settings). */
	UFUNCTION(BlueprintPure, Category = "Flood|Water")
	UStaticMeshComponent* GetPlaneMeshComponent() const { return PlaneMeshComponent; }

	/** The last-applied water level. For BP queries. */
	UFUNCTION(BlueprintPure, Category = "Flood|Water")
	float GetCurrentWaterLevel01() const { return LastLevel01; }

private:
	/** Creates the PlaneMeshComponent child and assigns the mesh/material. */
	void EnsurePlaneMesh();

	/** Applies Z + visibility from the source volume. */
	void ApplyWaterState(float NewLevel01, float NewHeightCm);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PlaneMeshComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialMID = nullptr;

	float LastLevel01 = -1.f;
	bool bLastVisible = false;
};
