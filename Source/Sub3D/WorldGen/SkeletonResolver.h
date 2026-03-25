#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorldGenTypes.h"
#include "SkeletonResolver.generated.h"

// C5 — Converts discrete topology nodes into continuous cubic Bezier skeleton segments.
UCLASS()
class SUB3D_API USkeletonResolver : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY() float SegmentCurvatureCm = 0.f;
	UPROPERTY() float IntermediatePointJitterCm = 0.f;
	UPROPERTY() float HubApproachCurvatureScale = 0.45f;
	UPROPERTY() float OptionalBranchCurvatureScale = 1.2f;
	UPROPERTY() float JunctionTransitionStartT = 0.82f;
	UPROPERTY() float JunctionTransitionSpanT = 0.18f;
	UPROPERTY() float JunctionThroatScale = 1.0f;

	bool ResolveSkeleton(const TArray<FTraversalTopologyNode>& Nodes,
	                     TArray<FTraversalSkeletonSegment>& OutSegments) const;

	// Evaluate cubic Bezier at t ∈ [0,1]
	static FVector EvalBezier(const FTraversalSkeletonSegment& Seg, float T);

	// Evaluate interpolated radius at t ∈ [0,1]
	static float EvalRadius(const FTraversalSkeletonSegment& Seg, float T);

	// Compute tangent direction at t
	static FVector EvalBezierTangent(const FTraversalSkeletonSegment& Seg, float T);

private:
	FTraversalSkeletonSegment BuildSegment(const FTraversalTopologyNode& A,
	                                       const FTraversalTopologyNode& B) const;
};
