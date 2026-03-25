#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorldGenTypes.h"
#include "NavigableVolumeGenerator.generated.h"

// C6 — Builds guaranteed navigable volume from skeleton segments.
// Produces: GuaranteedBrushes list + rasterized RenderField (200cm) + SonarField (500cm).
UCLASS()
class SUB3D_API UNavigableVolumeGenerator : public UObject
{
	GENERATED_BODY()

public:
	bool BuildGuaranteedVolume(const FRouteGenSpec& Spec,
	                           const class URouteArchetypeDataAsset* Archetype,
	                           const TArray<FTraversalTopologyNode>& Nodes,
	                           const TArray<FTraversalSkeletonSegment>& Skeleton,
	                           const TArray<FVolumeBrushDef>* ExternalGuaranteedBrushes,
	                           FRouteFieldModel& OutField) const;

	// Evaluate guaranteed density at world point P from a brush list.
	// density < 0 = rock ; density >= 0 = water (navigable).
	static float EvalGuaranteedDensity(const FVector& P, const TArray<FVolumeBrushDef>& Brushes);

	// SDF of a capsule: negative inside, positive outside.
	static float CapsuleSDF(const FVector& P, const FVector& A, const FVector& B, float Radius);

	// SDF of a sphere.
	static float SphereSDF(const FVector& P, const FVector& Center, float Radius);

	// Smooth min (Inigo Quilez): smooth union of two SDF values.
	static float SmoothMin(float A, float B, float K);

private:
	void GenerateBrushes(const class URouteArchetypeDataAsset* Archetype,
	                     const TArray<FTraversalTopologyNode>& Nodes,
	                     const TArray<FTraversalSkeletonSegment>& Skeleton,
	                     TArray<FVolumeBrushDef>& OutGuaranteedBrushes,
	                     TArray<FVolumeBrushDef>& OutRenderOnlyBrushes) const;

	void RasterizeField(const TArray<FVolumeBrushDef>& Brushes,
	                    const FBox& WorldBounds, float VoxelSize, int32 SamplesPerChunkAxis,
	                    TMap<FFieldChunkCoord, FRouteFieldChunkData>& OutField) const;

	static FBox ComputeSkeletonBounds(const TArray<FTraversalSkeletonSegment>& Skeleton, float Padding);
};
