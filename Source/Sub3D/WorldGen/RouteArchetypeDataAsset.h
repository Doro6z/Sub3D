#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WorldGenTypes.h"
#include "RouteArchetypeDataAsset.generated.h"

class UBranchProfileSetDataAsset;
class URouteMotifDataAsset;

USTRUCT(BlueprintType)
struct FRouteFlowAuthoringSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="6", ClampMax="18")) int32 MainSpineNodeCount = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="6")) int32 MaxSplitAnchors = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="2", ClampMax="4")) int32 MaxBranchFanout = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0")) float RejoinChance = 0.55f;
};

USTRUCT(BlueprintType)
struct FRouteCheckpointAuthoringSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) ECheckpointSpaceShape StartShape = ECheckpointSpaceShape::Pocket;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ECheckpointSpaceShape EndShape = ECheckpointSpaceShape::Pocket;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1500.0")) float StartRadiusCm = 5200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1500.0")) float EndRadiusCm = 5200.f;
};

USTRUCT(BlueprintType)
struct FRouteConnectionAuthoringSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="500.0")) float StartDockLengthCm = 3800.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="500.0")) float EndDockLengthCm = 3800.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.25", ClampMax="1.5")) float StartDockRadiusScale = 0.78f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.25", ClampMax="1.5")) float EndDockRadiusScale = 0.78f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0")) float PreferredCampaignOverlapCm = 1800.f;
};

USTRUCT(BlueprintType)
struct FRouteComplexityAuthoringSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="4")) int32 MaxHubCount = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="8")) int32 MaxOptionalSideBranches = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="[Unused] Max Decorative Disconnected Cavities", ClampMin="0", ClampMax="4")) int32 MaxDecorativeDisconnectedCavities = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="2")) int32 MaxPocketDepth = 2;
};

USTRUCT(BlueprintType)
struct FRouteVolumeAuthoringSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1000.0")) float TrunkRadiusCm = 3200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="Branch A Radius Cm (L)", ClampMin="1000.0")) float BranchARadiusCm = 2800.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="Branch B Radius Cm (L)", ClampMin="1000.0")) float BranchBRadiusCm = 3600.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="Branch C Radius Cm (L)", ClampMin="1000.0")) float BranchCRadiusCm = 3200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1000.0")) float MergeRadiusCm = 3800.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1000.0")) float HubRadiusCm = 5200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1000.0")) float PocketRadiusCm = 3200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1000.0")) float DecorativeCavityRadiusCm = 4200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0")) float JunctionTransitionStartT = 0.82f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01", ClampMax="1.0")) float JunctionTransitionSpanT = 0.18f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.5", ClampMax="1.5")) float JunctionThroatScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FRouteSpatialShapeAuthoringSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1000.0")) float BranchSeparationCm = 20000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0")) float VerticalOffsetCm = 5000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0")) float VerticalityBias = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0")) float SegmentCurvatureCm = 3500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0")) float IntermediatePointJitterCm = 2000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="2.0")) float HubApproachCurvatureScale = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="2.0")) float OptionalBranchCurvatureScale = 1.2f;
};

USTRUCT(BlueprintType)
struct FRouteRhythmAuthoringSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0")) float BranchDensity = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0")) float HubChance = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0")) float PocketChance = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="[Unused] Decorative Cavity Chance", ClampMin="0.0", ClampMax="1.0")) float DecorativeCavityChance = 0.f;
};

// Designer-editable route archetype. One DA per route type.
// Create: Content Browser → Miscellaneous → Data Asset → URouteArchetypeDataAsset
UCLASS(BlueprintType)
class SUB3D_API URouteArchetypeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route") FName                    ArchetypeID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route") ETraversalRouteArchetype ArchetypeType   = ETraversalRouteArchetype::MainTransit;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route") FNavigationEnvelopeSpec  EnvelopeSpec;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route") float                    RouteLengthMeters  = 5000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Authoring") bool           bUseGroupedConstrainedAuthoring = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Authoring") ETraversalComplexityTier DefaultComplexityTier = ETraversalComplexityTier::Moderate;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Authoring") TSoftObjectPtr<UBranchProfileSetDataAsset> BranchProfileSet;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Authoring") TArray<TSoftObjectPtr<URouteMotifDataAsset>> AllowedMotifs;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Authoring") FRouteCheckpointAuthoringSettings Checkpoints;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Authoring") FRouteConnectionAuthoringSettings Connections;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Authoring") FRouteFlowAuthoringSettings Flow;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Authoring") FRouteComplexityAuthoringSettings Complexity;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Authoring") FRouteVolumeAuthoringSettings Volumes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Authoring") FRouteSpatialShapeAuthoringSettings SpatialShape;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|Authoring") FRouteRhythmAuthoringSettings Rhythm;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph") bool    bUseConstrainedGraphPattern = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="6", ClampMax="18")) int32 MainSpineNodeCount = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="1", ClampMax="6")) int32 MaxSplitAnchors = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="2", ClampMax="4")) int32 MaxBranchFanout = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="0", ClampMax="4")) int32 MaxHubCount = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="0", ClampMax="8")) int32 MaxOptionalSideBranches = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(DisplayName="Max Decorative Disconnected Cavities (L)", EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="0", ClampMax="4")) int32 MaxDecorativeDisconnectedCavities = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="1", ClampMax="2")) int32 MaxPocketDepth = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="1000.0")) float ConstrainedBranchSeparationCm = 20000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="0.0")) float ConstrainedVerticalOffsetCm = 5000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="1000.0")) float HubRadiusCm = 5200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="1000.0")) float PocketRadiusCm = 3200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="1000.0")) float DecorativeCavityRadiusCm = 4200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="0.0", ClampMax="1.0")) float HubChance = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="0.0", ClampMax="1.0")) float RejoinChance = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="0.0", ClampMax="1.0")) float PocketChance = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|ConstrainedGraph", meta=(DisplayName="Decorative Cavity Chance (L)", EditCondition="!bUseGroupedConstrainedAuthoring", EditConditionHides, ClampMin="0.0", ClampMax="1.0")) float DecorativeCavityChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|MultiStage") bool          bUseMultiStageSplitPattern = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|MultiStage", meta=(ClampMin="3", ClampMax="3")) int32 FirstSplitCount = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|MultiStage", meta=(ClampMin="3", ClampMax="3")) int32 SecondSplitCount = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|MultiStage", meta=(ClampMin="2", ClampMax="2")) int32 MidMergeTargetCount = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|MultiStage", meta=(ClampMin="1000.0")) float FirstSplitSeparationCm = 22000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|MultiStage", meta=(ClampMin="1000.0")) float SecondSplitSeparationCm = 18000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|MultiStage", meta=(ClampMin="1000.0")) float MidMergeRadiusCm = 4200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|MultiStage", meta=(ClampMin="1000.0")) float SecondSplitRadiusCm = 4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|SplitMerge") bool          bUseSplitMergePattern = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|SplitMerge", meta=(ClampMin="2", ClampMax="3")) int32 SplitCount = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|SplitMerge", meta=(ClampMin="1000.0")) float BranchSeparationCm = 18000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|SplitMerge", meta=(ClampMin="0.0")) float BranchVerticalOffsetCm = 3000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|SplitMerge", meta=(ClampMin="0.0", ClampMax="0.2")) float BranchLengthVariancePct = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|SplitMerge", meta=(ClampMin="1000.0")) float MergeChamberRadiusCm = 4200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|SplitMerge", meta=(ClampMin="1000.0")) float TrunkRadiusCm = 3200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|SplitMerge", meta=(DisplayName="Branch A Radius Cm (L)", ClampMin="1000.0")) float BranchARadiusCm = 2800.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|SplitMerge", meta=(DisplayName="Branch B Radius Cm (L)", ClampMin="1000.0")) float BranchBRadiusCm = 3600.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|SplitMerge", meta=(DisplayName="Branch C Radius Cm (L)", ClampMin="1000.0")) float BranchCRadiusCm = 3200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route|SplitMerge", meta=(ClampMin="1000.0")) float MergeRadiusCm = 3800.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route", meta=(DisplayName="Branch Density (L)")) float                    BranchDensity      = 0.15f;  // 0=no branches, 1=max branches
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route", meta=(DisplayName="Verticality Bias (L)")) float                    VerticalityBias    = 0.5f;   // 0=flat, 1=lots of vertical
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route", meta=(DisplayName="Main Path Node Count (L)")) int32                    MainPathNodeCount  = 16;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route") int32                    MaxRetryCount      = 8;

private:
	void MigrateGroupedAuthoringIfNeeded();

	UPROPERTY()
	int32 GroupedAuthoringSchemaVersion = 0;
};
