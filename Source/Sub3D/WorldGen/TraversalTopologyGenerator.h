#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorldGenTypes.h"
#include "TraversalTopologyGenerator.generated.h"

class URouteArchetypeDataAsset;
class UBiomeFieldProfileDataAsset;
class UBranchProfileDataAsset;
class UBranchProfileSetDataAsset;
class URouteMotifDataAsset;

UCLASS()
class SUB3D_API UTraversalTopologyGenerator : public UObject
{
	GENERATED_BODY()

public:
	bool GenerateTopology(const FRouteGenSpec& Spec,
	                      const FRouteSeedCascade& Seeds,
	                      URouteArchetypeDataAsset* Archetype,
	                      TArray<FTraversalTopologyNode>& OutNodes) const;

private:
	ETopologyNodeType SelectNodeType(float NormalizedDist,
	                                 int32 DifficultyTier,
	                                 float BranchDensity,
	                                 FRandomStream& Rng) const;

	FVector ComputeNodePosition(int32 NodeIndex,
	                            int32 TotalNodes,
	                            const FRouteGenSpec& Spec,
	                            float VerticalityBias,
	                            FRandomStream& Rng) const;

	FVector ComputeNodePositionAtT(float T,
	                               const FRouteGenSpec& Spec,
	                               float VerticalityBias,
	                               FRandomStream& Rng) const;

	void AddBranchNodes(const FRouteGenSpec& Spec,
	                    float BranchDensity,
	                    FRandomStream& Rng,
	                    TArray<FTraversalTopologyNode>& InOutNodes) const;

	bool BuildConstrainedGraphTopology(const FRouteGenSpec& Spec,
	                                   const FRouteSeedCascade& Seeds,
	                                   const URouteArchetypeDataAsset* Archetype,
	                                   TArray<FTraversalTopologyNode>& OutNodes) const;

	bool BuildSplitMergeTopology(const FRouteGenSpec& Spec,
	                             const FRouteSeedCascade& Seeds,
	                             const URouteArchetypeDataAsset* Archetype,
	                             TArray<FTraversalTopologyNode>& OutNodes) const;

	bool BuildMultiStageSplitTopology(const FRouteGenSpec& Spec,
	                                  const FRouteSeedCascade& Seeds,
	                                  const URouteArchetypeDataAsset* Archetype,
	                                  TArray<FTraversalTopologyNode>& OutNodes) const;

	ETraversalComplexityTier ResolveComplexityTier(const FRouteGenSpec& Spec,
	                                               const URouteArchetypeDataAsset* Archetype) const;

	FTraversalComplexityBudget ResolveComplexityBudget(ETraversalComplexityTier ComplexityTier,
	                                                   const URouteArchetypeDataAsset* Archetype) const;
};
