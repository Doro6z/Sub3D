#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WorldGenTypes.h"
#include "TunnelNavDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FTunnelNavBuildSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="100.0"))
	float SampleSpacingCm = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="4", ClampMax="64"))
	int32 RadialSampleCount = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="1000.0"))
	float CrossSectionProbeMaxCm = 30000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="4", ClampMax="32"))
	int32 BinarySearchIterations = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="4", ClampMax="64"))
	int32 CoarseProbeSteps = 24;
};

USTRUCT(BlueprintType)
struct FTunnelNavEndpointSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FTransform RouteStartTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly) bool bHasRouteStartTransform = false;
	UPROPERTY(BlueprintReadOnly) float RouteStartRadiusCm = 0.f;

	UPROPERTY(BlueprintReadOnly) FTransform RouteStartDockTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly) bool bHasRouteStartDockTransform = false;
	UPROPERTY(BlueprintReadOnly) float RouteStartDockRadiusCm = 0.f;

	UPROPERTY(BlueprintReadOnly) FTransform RouteEndTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly) bool bHasRouteEndTransform = false;
	UPROPERTY(BlueprintReadOnly) float RouteEndRadiusCm = 0.f;

	UPROPERTY(BlueprintReadOnly) FTransform RouteEndDockTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly) bool bHasRouteEndDockTransform = false;
	UPROPERTY(BlueprintReadOnly) float RouteEndDockRadiusCm = 0.f;
};

USTRUCT(BlueprintType)
struct FTunnelNavNodeRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 NodeID = 0;
	UPROPERTY(BlueprintReadOnly) ETopologyNodeType NodeType = ETopologyNodeType::WideTransit;
	UPROPERTY(BlueprintReadOnly) FVector WorldPosition = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float NormalizedDistance = 0.f;
	UPROPERTY(BlueprintReadOnly) float PreferredRadiusCm = 0.f;
	UPROPERTY(BlueprintReadOnly) ECheckpointSpaceShape CheckpointSpaceShape = ECheckpointSpaceShape::Goulot;
	UPROPERTY(BlueprintReadOnly) int32 BranchStage = 0;
	UPROPERTY(BlueprintReadOnly) int32 BranchIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FName BranchProfileID;
	UPROPERTY(BlueprintReadOnly) EBranchProfileIntent BranchIntent = EBranchProfileIntent::OptionalResourceDetour;
	UPROPERTY(BlueprintReadOnly) int32 LogicalDepth = 0;
	UPROPERTY(BlueprintReadOnly) ELogicalRouteNodeRole LogicalRole = ELogicalRouteNodeRole::MainSpine;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer SemanticTags;
	UPROPERTY(BlueprintReadOnly) TArray<int32> NextNodeIDs;
	UPROPERTY(BlueprintReadOnly) bool bIsCanonicalPath = true;
	UPROPERTY(BlueprintReadOnly) bool bIsOptionalSideContent = false;
	UPROPERTY(BlueprintReadOnly) bool bIsDecorativeDisconnected = false;
};

USTRUCT(BlueprintType)
struct FTunnelNavEdgeRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 SegmentIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) int32 StartNodeID = 0;
	UPROPERTY(BlueprintReadOnly) int32 EndNodeID = 0;
	UPROPERTY(BlueprintReadOnly) float ApproxLengthCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float ApproxStartDistanceCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float ApproxEndDistanceCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float ApproxStartNormalizedDistance = 0.f;
	UPROPERTY(BlueprintReadOnly) float ApproxEndNormalizedDistance = 0.f;
	UPROPERTY(BlueprintReadOnly) float StartRadiusCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float EndRadiusCm = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 BranchStage = 0;
	UPROPERTY(BlueprintReadOnly) int32 BranchIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FName BranchProfileID;
	UPROPERTY(BlueprintReadOnly) EBranchProfileIntent BranchIntent = EBranchProfileIntent::OptionalResourceDetour;
	UPROPERTY(BlueprintReadOnly) int32 LogicalDepth = 0;
	UPROPERTY(BlueprintReadOnly) ELogicalRouteNodeRole LogicalRole = ELogicalRouteNodeRole::MainSpine;
	UPROPERTY(BlueprintReadOnly) ECheckpointSpaceShape CheckpointSpaceShape = ECheckpointSpaceShape::Goulot;
	UPROPERTY(BlueprintReadOnly) bool bGuaranteedTraversal = true;
	UPROPERTY(BlueprintReadOnly) bool bIsOptionalSideContent = false;
	UPROPERTY(BlueprintReadOnly) bool bIsDecorativeDisconnected = false;
	UPROPERTY(BlueprintReadOnly) int32 FirstSampleIndex = 0;
	UPROPERTY(BlueprintReadOnly) int32 SampleCount = 0;
	UPROPERTY(BlueprintReadOnly) float MinCrossSectionClearanceCm = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bBelowRequiredClearance = false;
	UPROPERTY(BlueprintReadOnly) bool bValidationPass = false;
};

USTRUCT(BlueprintType)
struct FTunnelNavSampleRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 SampleID = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) int32 SegmentIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) int32 LocalSampleIndex = 0;
	UPROPERTY(BlueprintReadOnly) float LocalT = 0.f;
	UPROPERTY(BlueprintReadOnly) float DistanceAlongSegmentCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float ApproxDistanceCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float ApproxNormalizedDistance = 0.f;
	UPROPERTY(BlueprintReadOnly) FVector WorldPosition = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector Forward = FVector::ForwardVector;
	UPROPERTY(BlueprintReadOnly) FVector Right = FVector::RightVector;
	UPROPERTY(BlueprintReadOnly) FVector Up = FVector::UpVector;
	UPROPERTY(BlueprintReadOnly) float SkeletonRadiusCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float ClearanceRightCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float ClearanceUpCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float ClearanceLeftCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float ClearanceDownCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float MinCrossSectionClearanceCm = 0.f;
	UPROPERTY(BlueprintReadOnly) float MaxCrossSectionClearanceCm = 0.f;
	UPROPERTY(BlueprintReadOnly) TArray<float> RadialClearanceCm;
	UPROPERTY(BlueprintReadOnly) int32 BranchStage = 0;
	UPROPERTY(BlueprintReadOnly) int32 BranchIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FName BranchProfileID;
	UPROPERTY(BlueprintReadOnly) EBranchProfileIntent BranchIntent = EBranchProfileIntent::OptionalResourceDetour;
	UPROPERTY(BlueprintReadOnly) int32 LogicalDepth = 0;
	UPROPERTY(BlueprintReadOnly) ELogicalRouteNodeRole LogicalRole = ELogicalRouteNodeRole::MainSpine;
	UPROPERTY(BlueprintReadOnly) ECheckpointSpaceShape CheckpointSpaceShape = ECheckpointSpaceShape::Goulot;
	UPROPERTY(BlueprintReadOnly) bool bGuaranteedTraversal = true;
	UPROPERTY(BlueprintReadOnly) bool bIsOptionalSideContent = false;
	UPROPERTY(BlueprintReadOnly) bool bIsDecorativeDisconnected = false;
	UPROPERTY(BlueprintReadOnly) bool bBelowRequiredClearance = false;
	UPROPERTY(BlueprintReadOnly) bool bBranchValidationPass = false;
};

UCLASS(BlueprintType)
class SUB3D_API UTunnelNavDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	void ResetData();

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 SchemaVersion = 1;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 BuildHash = 0;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FRouteGenSpec GenSpec;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FRouteSeedCascade Seeds;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FTunnelNavBuildSettings BuildSettings;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FTunnelNavEndpointSnapshot Endpoints;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	TArray<FTunnelNavNodeRecord> Nodes;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	TArray<FTunnelNavEdgeRecord> Edges;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	TArray<FTunnelNavSampleRecord> Samples;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav|Validation")
	float RequiredClearanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav|Validation")
	bool bValidationPass = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav|Validation")
	FRouteValidationReport ValidationReport;
};

