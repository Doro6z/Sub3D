#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TunnelNavigationRuntimeComponent.h"
#include "HelmNavigationDisplayComponent.generated.h"

class UTunnelNavigationRuntimeComponent;

USTRUCT(BlueprintType)
struct FHelmInstrumentStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bBound = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bRuntimeReady = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bStale = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bProjectionSuspect = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	ETunnelNavSpaceMode SpaceMode = ETunnelNavSpaceMode::Corridor;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float RefreshPeriodMs = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float UpdateAgeMs = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float ReconstructionTimeMs = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float Confidence01 = 0.f;
};

USTRUCT(BlueprintType)
struct FHelmCrossSectionViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	ETunnelNavSpaceMode SpaceMode = ETunnelNavSpaceMode::Corridor;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float RouteDistanceMeters = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float OffsetRightCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float OffsetUpCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float SafeLeftCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float SafeRightCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float SafeAboveCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float SafeBelowCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float SafeAbove01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float SafeBelow01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float ClearanceMarginCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	FVector2D SubOffsetNormalized = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	FVector2D HeadingVectorNormalized = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	FVector2D VelocityVectorNormalized = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	TArray<FVector2D> ContourPointsNormalized;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bNearWallWarning = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bHardClearanceViolation = false;
};

USTRUCT(BlueprintType)
struct FHelmForwardAnticipationSampleViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float DistanceAheadMeters = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float Distance01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float Ceiling01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float Floor01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float Width01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float CurvatureSeverity01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float LeftWall01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float RightWall01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float TurnSigned01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float SlopeSigned01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float CenterShiftSigned01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bNarrowing = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bCritical = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bNoTurnZone = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bHubTransition = false;
};

USTRUCT(BlueprintType)
struct FHelmForwardAnticipationViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	ETunnelNavSpaceMode SpaceMode = ETunnelNavSpaceMode::Corridor;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float LookaheadMeters = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float RecommendedMaxSpeedKmh = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float StoppingDistanceMeters = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float FirstCriticalObstacleMeters = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float DistanceToFirstRuntimeObstacleMeters = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bCommitmentZone = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bCrashStopAlreadyLate = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bNoTurnaroundBeforeNextHub = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	TArray<FHelmForwardAnticipationSampleViewData> Samples;
};

USTRUCT(BlueprintType)
struct FHelmReconstructionViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	FHelmInstrumentStatus InstrumentStatus;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	FHelmCrossSectionViewData CrossSection;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	FHelmForwardAnticipationViewData Forward;
};

USTRUCT(BlueprintType)
struct FHelmTacticalGraphNodeViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	int32 NodeID = 0;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	FVector2D PositionNormalized = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bCurrent = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bHub = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bSplitLike = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bOptional = false;
};

USTRUCT(BlueprintType)
struct FHelmTacticalGraphEdgeViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	int32 EdgeIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	int32 FromNodeID = 0;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	int32 ToNodeID = 0;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bCurrent = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bOptional = false;
};

USTRUCT(BlueprintType)
struct FHelmTacticalGraphViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	ETunnelNavSpaceMode SpaceMode = ETunnelNavSpaceMode::Corridor;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	int32 AnchorNodeID = 0;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	int32 CurrentEdgeIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float DistanceToNextHubMeters = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float DistanceToNextSplitMeters = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	float DistanceToNextMergeMeters = -1.f;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	bool bMultipleExitsNearby = false;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	TArray<FHelmTacticalGraphNodeViewData> Nodes;

	UPROPERTY(BlueprintReadOnly, Category="HelmNav")
	TArray<FHelmTacticalGraphEdgeViewData> Edges;
};

UCLASS(ClassGroup=(Submarine), meta=(BlueprintSpawnableComponent))
class SUB3D_API UHelmNavigationDisplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHelmNavigationDisplayComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav")
	bool bAutoBindTunnelNavigationRuntime = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav", meta=(ClampMin="0.02"))
	float RefreshPeriodS = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav", meta=(ClampMin="1000.0"))
	float ForwardLookaheadCm = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav", meta=(ClampMin="1000.0"))
	float TacticalGraphRadiusCm = 18000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav")
	FSubmarineNavigationProfile CanonicalNavigationProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HelmNav|Debug")
	bool bEnableDebugLogs = false;

	UFUNCTION(BlueprintCallable, Category="HelmNav")
	void SetTunnelNavigationRuntime(UTunnelNavigationRuntimeComponent* InRuntime);

	UFUNCTION(BlueprintPure, Category="HelmNav")
	UTunnelNavigationRuntimeComponent* GetTunnelNavigationRuntime() const;

	UFUNCTION(BlueprintCallable, Category="HelmNav")
	bool RefreshViewData();

	UFUNCTION(BlueprintPure, Category="HelmNav")
	bool HasValidNavigationData() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FTunnelNavProjectionResult GetProjectionResult() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FHelmCrossSectionViewData GetCrossSectionViewData() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FHelmForwardAnticipationViewData GetForwardAnticipationViewData() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FHelmTacticalGraphViewData GetTacticalGraphViewData() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FHelmReconstructionViewData GetReconstructionViewData() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FHelmInstrumentStatus GetInstrumentStatus() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	float GetForwardLookaheadCm() const
	{
		return ForwardLookaheadCm;
	}

	UFUNCTION(BlueprintPure, Category="HelmNav")
	float GetTacticalGraphRadiusCm() const
	{
		return TacticalGraphRadiusCm;
	}

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FTunnelNavHeadingVsVelocityState GetHeadingVsVelocityState() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FTunnelNavStoppingDistanceWarning GetStoppingDistanceWarning() const;

	UFUNCTION(BlueprintPure, Category="HelmNav")
	FTunnelNavCommitmentWarning GetCommitmentWarning() const;

private:
	void ResolveRuntimeComponent();
	void ResetCachedViewData();
	bool BuildCrossSectionView(const FTunnelNavCrossSectionResult& CrossSection, const FTunnelNavHeadingVsVelocityState& HeadingState, const FTunnelNavProjectionResult& Projection, FHelmCrossSectionViewData& OutView) const;
	bool BuildForwardView(const FTunnelNavForwardProfile& Profile, FHelmForwardAnticipationViewData& OutView) const;
	bool BuildGraphView(const FTunnelNavGraphWindow& GraphWindow, const FTunnelNavProjectionResult& Projection, FHelmTacticalGraphViewData& OutView) const;
	void BuildReconstructionView();
	void UpdateInstrumentStatus(bool bProjectionSuspect, float RefreshDurationMs);
	float ComputeInstrumentConfidence(bool bProjectionSuspect) const;

	UPROPERTY(Transient)
	TObjectPtr<UTunnelNavigationRuntimeComponent> CachedTunnelRuntime = nullptr;

	UPROPERTY(Transient)
	float TimeSinceLastRefreshS = 0.f;

	UPROPERTY(Transient)
	bool bHasValidData = false;

	UPROPERTY(Transient)
	FTunnelNavProjectionResult CachedProjection;

	UPROPERTY(Transient)
	FTunnelNavHeadingVsVelocityState CachedHeadingState;

	UPROPERTY(Transient)
	FTunnelNavStoppingDistanceWarning CachedStopWarning;

	UPROPERTY(Transient)
	FTunnelNavCommitmentWarning CachedCommitmentWarning;

	UPROPERTY(Transient)
	FHelmInstrumentStatus CachedInstrumentStatus;

	UPROPERTY(Transient)
	FHelmCrossSectionViewData CachedCrossSectionView;

	UPROPERTY(Transient)
	FHelmForwardAnticipationViewData CachedForwardView;

	UPROPERTY(Transient)
	FHelmReconstructionViewData CachedReconstructionView;

	UPROPERTY(Transient)
	FHelmTacticalGraphViewData CachedGraphView;

	UPROPERTY(Transient)
	float TimeSinceLastSuccessfulRefreshS = 0.f;

	bool bLoggedMissingRuntime = false;
};
