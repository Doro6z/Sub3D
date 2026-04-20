#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"
#include "TunnelNavDataAsset.h"
#include "TunnelNavigationRuntimeComponent.generated.h"

class ATraversalRouteActor;

UENUM(BlueprintType)
enum class ETunnelNavSubClass : uint8
{
	ClassS UMETA(DisplayName="S"),
	ClassM UMETA(DisplayName="M"),
	ClassL UMETA(DisplayName="L"),
	ClassXL UMETA(DisplayName="XL")
};

UENUM(BlueprintType)
enum class ETunnelNavRestrictionType : uint8
{
	Clearance,
	BranchValidation,
	RuntimeObstacle,
	StoppingDistance,
	CommitmentNoTurn,
	OptionalBranchRisk
};

UENUM(BlueprintType)
enum class ETunnelNavKnowledgeSource : uint8
{
	StaticMapped,
	RuntimeObserved
};

UENUM(BlueprintType)
enum class ETunnelNavSpaceMode : uint8
{
	Corridor,
	Transition,
	CavernHub
};

USTRUCT(BlueprintType)
struct FSubmarineClassNavigationSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	ETunnelNavSubClass SubClass = ETunnelNavSubClass::ClassM;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="100.0"))
	float RequiredClearanceCm = 2800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="100.0"))
	float MinTurnaroundRadiusCm = 4200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="10.0"))
	float ServiceDecelerationCmS2 = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="10.0"))
	float EmergencyDecelerationCmS2 = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="1000.0"))
	float CommitmentLookaheadCm = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="0.0", ClampMax="180.0"))
	float DriftWarningAngleDeg = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="0.0"))
	float LateralSpeedWarningCmS = 90.f;
};

USTRUCT(BlueprintType)
struct FSubmarineNavigationProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	ETunnelNavSubClass ClassId = ETunnelNavSubClass::ClassM;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="100.0"))
	float HullLengthCm = 2700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="100.0"))
	float HullBeamCm = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="100.0"))
	float HullHeightCm = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="50.0"))
	float HardClearanceCm = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="50.0"))
	float PreferredClearanceCm = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="100.0"))
	float MinTurningBasinDiameterCm = 8400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	FRuntimeFloatCurve CrashStopDistanceCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	FRuntimeFloatCurve SafeSpeedByClearanceCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="10.0"))
	float ServiceDecelerationCmS2 = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="10.0"))
	float EmergencyDecelerationCmS2 = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="0.1"))
	float YawInertiaScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="0.1"))
	float LateralDriftScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="0.1"))
	float PitchResponseScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="1000.0"))
	float CommitmentLookaheadCm = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="0.0", ClampMax="180.0"))
	float DriftWarningAngleDeg = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="0.0"))
	float LateralSpeedWarningCmS = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	bool bCanRollForClearance = false;
};

UCLASS(BlueprintType)
class SUB3D_API UTunnelNavigationClassProfileDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TunnelNav")
	TArray<FSubmarineClassNavigationSpec> ClassSpecs;
};

USTRUCT(BlueprintType)
struct FRuntimeObservedObstacle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="10.0"))
	float RadiusCm = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="0.01"))
	float ExpireAtWorldTimeS = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	FName ObstacleTag = NAME_None;
};

USTRUCT(BlueprintType)
struct FTunnelNavProjectionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bProjected = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 SampleIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 EdgeIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float RouteDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float DistanceAlongEdgeCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float LocalOffsetRightCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float LocalOffsetUpCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float DistanceFromCenterCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FVector ClosestPointWorld = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FVector FrameForward = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FVector FrameRight = FVector::RightVector;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FVector FrameUp = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float AlignmentDot = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	ETunnelNavSpaceMode SpaceMode = ETunnelNavSpaceMode::Corridor;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bHubLike = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bOnGuaranteedPath = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bOptionalBranch = false;
};

USTRUCT(BlueprintType)
struct FTunnelNavCrossSectionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 SampleIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FVector Forward = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FVector Right = FVector::RightVector;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FVector Up = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float ClearanceRightCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float ClearanceLeftCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float ClearanceUpCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float ClearanceDownCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float MinClearanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float RequiredClearanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float ClearanceMarginCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	TArray<float> RadialSamplesCm;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float SubProjectedOffsetRightCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float SubProjectedOffsetUpCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	ETunnelNavSpaceMode SpaceMode = ETunnelNavSpaceMode::Corridor;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bNearWallWarning = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bHardClearanceViolation = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bRestrictedForClass = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bTurnaroundPossible = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bRuntimeObstacleInsideSection = false;
};

USTRUCT(BlueprintType)
struct FTunnelNavAnticipationPoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 SampleIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float DistanceAheadCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float RouteDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float TunnelHalfWidthLeftCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float TunnelHalfWidthRightCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float TunnelHalfHeightUpCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float TunnelHalfHeightDownCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float MinClearanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float RequiredClearanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float CurvatureDegPer100m = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float GradeDegPer100m = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	ETunnelNavSpaceMode SpaceMode = ETunnelNavSpaceMode::Corridor;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bHubTransition = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bBranchApproach = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bBelowRequiredClearance = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bRuntimeObstacle = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bNoTurnZone = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bOptionalBranch = false;
};

USTRUCT(BlueprintType)
struct FTunnelNavForwardProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	ETunnelNavSpaceMode SpaceMode = ETunnelNavSpaceMode::Corridor;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float StartDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float LookaheadDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float MinClearanceAheadCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float RecommendedMaxSpeedCmS = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float StoppingDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float DistanceToFirstRestrictionCm = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float FirstCriticalObstacleDistanceCm = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float DistanceToFirstRuntimeObstacleCm = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bCommitmentZone = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bNoTurnaroundBeforeNextHub = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bCrashStopAlreadyLate = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	TArray<FTunnelNavAnticipationPoint> Points;
};

USTRUCT(BlueprintType)
struct FTunnelNavGraphNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 NodeID = 0;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FVector WorldPosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	ETopologyNodeType NodeType = ETopologyNodeType::WideTransit;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	ELogicalRouteNodeRole LogicalRole = ELogicalRouteNodeRole::MainSpine;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 BranchIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bOptional = false;
};

USTRUCT(BlueprintType)
struct FTunnelNavGraphEdge
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 EdgeIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 FromNodeID = 0;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 ToNodeID = 0;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float ApproxLengthCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	ELogicalRouteNodeRole LogicalRole = ELogicalRouteNodeRole::MainSpine;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 BranchIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bOptional = false;
};

USTRUCT(BlueprintType)
struct FTunnelNavGraphWindow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	ETunnelNavSpaceMode SpaceMode = ETunnelNavSpaceMode::Corridor;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 AnchorNodeID = 0;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 CurrentEdgeIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 CurrentBranchIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float DistanceToNextHubCm = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float DistanceToNextSplitCm = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float DistanceToNextMergeCm = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bMultipleExitsNearby = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	TArray<FTunnelNavGraphNode> Nodes;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	TArray<FTunnelNavGraphEdge> Edges;
};

USTRUCT(BlueprintType)
struct FTunnelNavRestriction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	ETunnelNavRestrictionType RestrictionType = ETunnelNavRestrictionType::Clearance;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	ETunnelNavKnowledgeSource KnowledgeSource = ETunnelNavKnowledgeSource::StaticMapped;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 SampleIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	int32 RelatedEdgeIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float DistanceAheadCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float Severity01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bHardBlock = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bClassSpecific = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FText Message;
};

USTRUCT(BlueprintType)
struct FTunnelNavStoppingDistanceWarning
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bWarning = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float CurrentForwardSpeedCmS = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float RequiredStopDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float AvailableDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float MarginCm = 0.f;
};

USTRUCT(BlueprintType)
struct FTunnelNavCommitmentWarning
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bWarning = false;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float DistanceToCommitmentCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float MaxClearanceAheadCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float RequiredTurnaroundCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bNoTurnaroundBeforeNextHub = false;
};

USTRUCT(BlueprintType)
struct FTunnelNavHeadingVsVelocityState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FVector HeadingWorld = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	FVector VelocityWorld = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float HeadingYawDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float VelocityYawDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float DriftAngleDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float ForwardSpeedCmS = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float LateralSpeedCmS = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float VerticalSpeedCmS = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	float ProjectedStopDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	TArray<FTransform> ProjectedGhostTransforms;

	UPROPERTY(BlueprintReadOnly, Category="TunnelNav")
	bool bDriftingSignificantly = false;
};

UCLASS(ClassGroup=(Submarine), meta=(BlueprintSpawnableComponent))
class SUB3D_API UTunnelNavigationRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTunnelNavigationRuntimeComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	bool bAutoResolveRouteActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	bool bAutoBindTunnelNavAssetFromRoute = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	ETunnelNavSubClass ActiveSubClass = ETunnelNavSubClass::ClassM;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	TObjectPtr<UTunnelNavigationClassProfileDataAsset> ClassProfileAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="500.0"))
	float ProjectionSearchRadiusCm = 60000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="1", ClampMax="128"))
	int32 ProjectionCacheSampleWindow = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="500.0"))
	float DefaultLookaheadDistanceCm = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="50.0"))
	float AnticipationSamplingStepCm = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="0.05"))
	float RouteResolveRetryPeriodS = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav", meta=(ClampMin="0.05"))
	float RuntimeObstacleDefaultLifetimeS = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav")
	bool bIncludeOptionalBranchesInLookahead = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TunnelNav|Debug", meta=(ClampMin="1000.0"))
	float DebugLookaheadCm = 10000.f;

	UFUNCTION(BlueprintCallable, Category="TunnelNav")
	void SetRouteActor(ATraversalRouteActor* InRouteActor);

	UFUNCTION(BlueprintCallable, Category="TunnelNav")
	void SetTunnelNavData(UTunnelNavDataAsset* InTunnelNavData);

	UFUNCTION(BlueprintPure, Category="TunnelNav")
	ATraversalRouteActor* GetRouteActor() const;

	UFUNCTION(BlueprintPure, Category="TunnelNav")
	UTunnelNavDataAsset* GetTunnelNavData() const;

	UFUNCTION(BlueprintCallable, Category="TunnelNav|Query")
	bool BuildNavigationProfileForSubClass(ETunnelNavSubClass SubClass, FSubmarineNavigationProfile& OutProfile) const;

	UFUNCTION(BlueprintCallable, Category="TunnelNav|Query")
	bool ProjectSubmarineToRoute(FTunnelNavProjectionResult& OutProjection) const;

	bool ProjectSubmarineToRoute(const FTransform& SubTransform, FTunnelNavProjectionResult& OutProjection) const;

	UFUNCTION(BlueprintCallable, Category="TunnelNav|Query")
	bool ProjectWorldLocationToRoute(const FVector& WorldLocation, FTunnelNavProjectionResult& OutProjection) const;

	UFUNCTION(BlueprintCallable, Category="TunnelNav|Query")
	bool GetLocalCrossSection(const FTunnelNavProjectionResult& Projection, ETunnelNavSubClass SubClass, FTunnelNavCrossSectionResult& OutResult) const;

	bool GetLocalCrossSection(const FTunnelNavProjectionResult& Projection, const FSubmarineNavigationProfile& NavProfile, FTunnelNavCrossSectionResult& OutResult) const;

	UFUNCTION(BlueprintCallable, Category="TunnelNav|Query")
	bool GetForwardAnticipationProfile(const FTunnelNavProjectionResult& Projection, ETunnelNavSubClass SubClass, float LookaheadDistanceCm, FTunnelNavForwardProfile& OutProfile) const;

	bool GetForwardAnticipationProfile(const FTunnelNavProjectionResult& Projection, const FSubmarineNavigationProfile& NavProfile, float LookaheadDistanceCm, FTunnelNavForwardProfile& OutProfile) const;

	UFUNCTION(BlueprintCallable, Category="TunnelNav|Query")
	bool GetLocalGraphWindow(const FTunnelNavProjectionResult& Projection, int32 GraphDepth, FTunnelNavGraphWindow& OutWindow) const;

	bool GetLocalGraphWindow(const FTunnelNavProjectionResult& Projection, float RadiusCm, FTunnelNavGraphWindow& OutWindow) const;

	UFUNCTION(BlueprintCallable, Category="TunnelNav|Query")
	void GetActiveRestrictionsForSubClass(const FTunnelNavProjectionResult& Projection, ETunnelNavSubClass SubClass, float LookaheadDistanceCm, TArray<FTunnelNavRestriction>& OutRestrictions) const;

	void GetActiveRestrictionsForSubClass(const FTunnelNavProjectionResult& Projection, const FSubmarineNavigationProfile& NavProfile, float LookaheadDistanceCm, TArray<FTunnelNavRestriction>& OutRestrictions) const;

	UFUNCTION(BlueprintCallable, Category="TunnelNav|Query")
	bool GetStoppingDistanceWarning(const FTunnelNavProjectionResult& Projection, ETunnelNavSubClass SubClass, FTunnelNavStoppingDistanceWarning& OutWarning) const;

	bool GetStoppingDistanceWarning(const FTunnelNavForwardProfile& Profile, const FSubmarineNavigationProfile& NavProfile, float CurrentForwardSpeedCmS, FTunnelNavStoppingDistanceWarning& OutWarning) const;

	UFUNCTION(BlueprintCallable, Category="TunnelNav|Query")
	bool GetCommitmentWarning(const FTunnelNavProjectionResult& Projection, ETunnelNavSubClass SubClass, FTunnelNavCommitmentWarning& OutWarning) const;

	bool GetCommitmentWarning(const FTunnelNavProjectionResult& Projection, const FSubmarineNavigationProfile& NavProfile, FTunnelNavCommitmentWarning& OutWarning) const;

	UFUNCTION(BlueprintCallable, Category="TunnelNav|Query")
	FTunnelNavHeadingVsVelocityState GetHeadingVsVelocityState(ETunnelNavSubClass SubClass) const;

	bool GetHeadingVsVelocityState(const FTransform& SubTransform, const FVector& LinearVelocity, const FSubmarineNavigationProfile& NavProfile, FTunnelNavHeadingVsVelocityState& OutState) const;

	UFUNCTION(BlueprintCallable, Category="TunnelNav|RuntimeObstacles")
	void AddRuntimeObservedObstacle(const FVector& WorldLocation, float RadiusCm, float LifetimeSeconds = 0.f, FName ObstacleTag = NAME_None);

	UFUNCTION(BlueprintCallable, Category="TunnelNav|RuntimeObstacles")
	void ClearRuntimeObservedObstacles();

	UFUNCTION(BlueprintPure, Category="TunnelNav|RuntimeObstacles")
	const TArray<FRuntimeObservedObstacle>& GetRuntimeObservedObstacles() const
	{
		return RuntimeObservedObstacles;
	}

	UFUNCTION(CallInEditor, BlueprintCallable, Category="TunnelNav|Debug")
	void LogCurrentProjection() const;

	UFUNCTION(CallInEditor, BlueprintCallable, Category="TunnelNav|Debug")
	void LogCurrentRestrictions() const;

private:
	bool EnsureNavigationDataAvailable() const;
	void ResolveRouteActorFromWorld();
	void CleanupExpiredRuntimeObstacles();
	void DrawDebugOverlay() const;

	const FSubmarineClassNavigationSpec* ResolveClassSpec(ETunnelNavSubClass SubClass) const;
	bool ResolveNavigationProfile(ETunnelNavSubClass SubClass, FSubmarineNavigationProfile& OutProfile) const;
	bool ProjectWorldLocationToRouteInternal(const FVector& WorldLocation, const FVector& ForwardHint, FTunnelNavProjectionResult& OutProjection) const;
	bool FindProjectedSampleIndex(const FVector& WorldLocation, const FVector& ForwardHint, int32& OutSampleIndex, float* OutDistanceCm = nullptr, float* OutAlignmentDot = nullptr) const;
	void AppendEdgeSampleIndices(int32 EdgeIndex, TSet<int32>& OutCandidateIndices) const;
	void AppendNeighborEdgeSampleIndices(int32 EdgeIndex, TSet<int32>& OutCandidateIndices) const;
	bool IsSampleOptionalForLookahead(const FTunnelNavSampleRecord& Sample) const;
	bool IsSampleBlockedByRuntimeObstacle(const FTunnelNavSampleRecord& Sample, float RequiredClearanceCm) const;
	float ComputeRestrictionSeverity(float MinClearanceCm, float RequiredClearanceCm) const;
	float ComputeCurvatureDegPer100m(int32 PreviousSampleIndex, int32 NextSampleIndex) const;
	float ComputeGradeDegPer100m(int32 PreviousSampleIndex, int32 NextSampleIndex) const;
	float ComputeDistanceToFirstRestrictionCm(const FTunnelNavProjectionResult& Projection, const FSubmarineNavigationProfile& NavProfile, float LookaheadDistanceCm) const;
	float ComputeForwardSpeedCmS() const;
	float ComputeLateralSpeedCmS() const;
	float ComputeVerticalSpeedCmS() const;
	float ComputeStoppingDistanceCm(const FSubmarineNavigationProfile& NavProfile, float ForwardSpeedCmS) const;
	float ComputeRecommendedMaxSpeedCmS(const FTunnelNavForwardProfile& Profile, const FSubmarineNavigationProfile& NavProfile, float CurrentForwardSpeedCmS) const;
	int32 ResolveAnchorNodeId(const FTunnelNavProjectionResult& Projection) const;
	ETunnelNavSpaceMode DetermineSpaceMode(const FTunnelNavSampleRecord& Sample) const;
	void UpdateProjectionCache(const FTunnelNavProjectionResult& Projection) const;
	void CollectProjectedGhostTransforms(const FTransform& SubTransform, const FVector& LinearVelocity, float ForwardSpeedCmS, TArray<FTransform>& OutGhosts) const;

	static bool CompareSampleDistance(const FTunnelNavAnticipationPoint& A, const FTunnelNavAnticipationPoint& B)
	{
		return A.DistanceAheadCm < B.DistanceAheadCm;
	}

	UPROPERTY(Transient)
	TObjectPtr<ATraversalRouteActor> CachedRouteActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTunnelNavDataAsset> CachedTunnelNavData = nullptr;

	UPROPERTY(Transient)
	mutable float TimeSinceLastResolveAttemptS = 0.f;

	UPROPERTY(Transient)
	TArray<FRuntimeObservedObstacle> RuntimeObservedObstacles;

	UPROPERTY(Transient)
	TArray<FSubmarineClassNavigationSpec> DefaultClassSpecs;

	UPROPERTY(Transient)
	mutable int32 LastProjectedSampleIndex = INDEX_NONE;

	UPROPERTY(Transient)
	mutable int32 LastProjectedEdgeIndex = INDEX_NONE;
};
