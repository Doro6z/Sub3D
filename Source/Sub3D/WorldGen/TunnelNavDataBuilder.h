#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TunnelNavDataAsset.h"
#include "TunnelNavDataBuilder.generated.h"

UCLASS()
class SUB3D_API UTunnelNavDataBuilder : public UObject
{
	GENERATED_BODY()

public:
	void InitializeFromC5(
		UTunnelNavDataAsset* Asset,
		const FRouteGenSpec& Spec,
		const FRouteSeedCascade& Seeds,
		int32 BuildHash,
		const TArray<FTraversalTopologyNode>& Nodes,
		const TArray<FTraversalSkeletonSegment>& Skeleton,
		const FTunnelNavBuildSettings& Settings,
		const FTunnelNavEndpointSnapshot& Endpoints) const;

	void PopulateFromC6(
		UTunnelNavDataAsset* Asset,
		const FRouteFieldModel& Field) const;

	void StampFromC9(
		UTunnelNavDataAsset* Asset,
		const FRouteGenSpec& Spec,
		const FRouteValidationReport& ValidationReport) const;

private:
	float ApproximateSegmentLengthCm(const FTraversalSkeletonSegment& Segment) const;
	void BuildFrame(const FVector& Forward, FVector& OutRight, FVector& OutUp) const;
	float EvaluateDensityAtPoint(const FVector& Position, const TArray<FVolumeBrushDef>& Brushes) const;
	float FindDirectionalClearanceCm(
		const FVector& Origin,
		const FVector& Direction,
		const TArray<FVolumeBrushDef>& Brushes,
		float MaxDistanceCm,
		int32 CoarseSteps,
		int32 BinaryIterations) const;
	bool ResolveBranchValidationPass(const FRouteValidationReport& ValidationReport, int32 BranchIndex) const;
};

