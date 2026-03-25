#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorldGenTypes.h"
#include "OrganicDeformationGenerator.generated.h"

class UBiomeFieldProfileDataAsset;

// C7 — Applies KdotJPG dual-noise to RenderField to add organic secondary caves.
// INVARIANT: organic deformation can only ENLARGE the navigable space, never close guaranteed brushes.
// density_final = max(density_guaranteed, noise_cave) ensures this mathematically.
UCLASS()
class SUB3D_API UOrganicDeformationGenerator : public UObject
{
	GENERATED_BODY()

public:
	bool ApplyDeformation(const FRouteGenSpec& Spec,
	                      const FRouteSeedCascade& Seeds,
	                      const UBiomeFieldProfileDataAsset* Biome,
	                      FRouteFieldModel& InOutField) const;

private:
	// 3D value noise, returns [-1, 1]
	static float ValueNoise3D(const FVector& P, int32 Seed);

	// KdotJPG cave evaluation: Threshold - (N1² + N2²)
	// > 0 → secondary cave; <= 0 → rock
	static float EvalNoiseCave(const FVector& P, float Freq1, float Freq2,
	                            const FVector& Offset1, const FVector& Offset2,
	                            float Threshold);
};
