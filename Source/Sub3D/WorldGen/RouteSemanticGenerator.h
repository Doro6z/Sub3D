#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorldGenTypes.h"
#include "RouteSemanticGenerator.generated.h"

// C8 — Derives mission sockets and gameplay zones from the skeleton topology.
UCLASS()
class SUB3D_API URouteSemanticGenerator : public UObject
{
	GENERATED_BODY()

public:
	bool BuildSemantics(const FRouteGenSpec& Spec,
	                    const TArray<FTraversalTopologyNode>& Nodes,
	                    const TArray<FTraversalSkeletonSegment>& Skeleton,
	                    FRouteSemanticModel& OutSemantic) const;

private:
	void BuildZonesFromNodes(const TArray<FTraversalTopologyNode>& Nodes,
	                         TArray<FRouteSemanticZone>& OutZones) const;

	void BuildSocketsFromNodes(const TArray<FTraversalTopologyNode>& Nodes,
	                           TArray<FMissionSocketDef>& OutSockets) const;
};
