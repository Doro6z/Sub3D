#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldGenTypes.h"
#include "SonarFieldComponent.generated.h"

// Proto 04 STUB — stores sonar occupancy data but does not yet perform queries.
// Proto 05: implement SampleOcclusionAlongRay() for gameplay sonar.
UCLASS(ClassGroup=(WorldGen), meta=(BlueprintSpawnableComponent))
class SUB3D_API USonarFieldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USonarFieldComponent();

	void InitializeFromField(const FRouteFieldModel& Field);

	// STUB: always returns false in Proto 04.
	UFUNCTION(BlueprintCallable)
	bool SampleOcclusionAlongRay(const FVector& Start, const FVector& End, float& OutBlockage) const;

	// Returns coarse water/edge points around a center for Sonar V2 topological seeding.
	UFUNCTION(BlueprintCallable)
	void CollectCoarseWaterSurfacePoints(const FVector& Center, float RadiusCm, int32 MaxPoints, TArray<FVector>& OutPoints) const;

	// Debug: how many sonar voxels are loaded
	UFUNCTION(BlueprintCallable)
	int32 GetSonarVoxelCount() const;

private:
	TMap<FFieldChunkCoord, FRouteFieldChunkData> CachedSonarField;
};
