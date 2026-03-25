#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorldGenTypes.h"
#include "RouteValidator.generated.h"

UCLASS()
class SUB3D_API URouteValidator : public UObject
{
	GENERATED_BODY()

public:
	FRouteValidationReport Validate(const FRouteGenSpec& Spec,
	                                const TArray<FTraversalTopologyNode>& Nodes,
	                                const TArray<FTraversalSkeletonSegment>& Skeleton,
	                                const FRouteFieldModel& Field,
	                                const FRouteSemanticModel& Semantic) const;

private:
	float SampleClearanceAtPoint(const FVector& P, const TArray<FVolumeBrushDef>& Brushes) const;

	float MeasureMinClearance(const TArray<FTraversalSkeletonSegment>& Skeleton,
	                          const TArray<FVolumeBrushDef>& Brushes) const;

	float MeasureMinClearanceForSegments(const TArray<const FTraversalSkeletonSegment*>& Segments,
	                                     const TArray<FVolumeBrushDef>& Brushes) const;

	bool CheckSonarConnectivity(const FRouteFieldModel& Field,
	                            const FVector& StartWorld,
	                            const FVector& ExitWorld) const;
};
