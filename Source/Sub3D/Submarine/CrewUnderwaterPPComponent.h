#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrewUnderwaterPPComponent.generated.h"

class ASubCrewCharacter;
class UCompartmentVolumeComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPostProcessComponent;

/**
 * Drives the crew's underwater post-process effect. 100% passive C++:
 * compares camera Z to the current compartment's water surface Z (or ocean surface when Outside),
 * blends the PP BlendWeight, and fires BP events for designer hooks (droplets on screen,
 * splash sfx, waterline distortion). All visual intelligence lives in the assigned
 * UnderwaterPostProcessMaterial.
 *
 * Runs only on the owning/autonomous client (underwater is a local camera effect).
 */
UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API UCrewUnderwaterPPComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCrewUnderwaterPPComponent();

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

	/** Fired once when the camera crosses below the water surface (underwater start). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnEnterWater();

	/** Fired once when the camera crosses above the water surface (underwater end). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnExitWater();

	/**
	 * Fired each tick while the camera is within WaterlineCrossThresholdCm of the surface.
	 * Designer hook for waterline glitch mitigation: droplets on screen, chromatic aberration,
	 * splash distortion. DistanceCm is signed: negative = just below surface, positive = just above.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Underwater|Events")
	void BP_OnWaterlineProximity(float DistanceCm);

	/** True while the camera is below the water surface. */
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

private:
	/** Creates the PostProcessComponent sub-object and assigns the material as a blendable. */
	void EnsurePostProcessComponent();

	/** Resolves the water surface Z for the current crew state (compartment interior or ocean). */
	float ResolveWaterSurfaceZ() const;

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
};
