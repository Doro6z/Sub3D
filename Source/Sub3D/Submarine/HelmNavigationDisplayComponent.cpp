#include "HelmNavigationDisplayComponent.h"

#include "HAL/PlatformTime.h"
#include "SubmarineBase.h"
#include "TraversalRouteActor.h"

namespace
{
FString SpaceModeToString(ETunnelNavSpaceMode SpaceMode)
{
	switch (SpaceMode)
	{
	case ETunnelNavSpaceMode::CavernHub:
		return TEXT("Hub");
	case ETunnelNavSpaceMode::Transition:
		return TEXT("Transition");
	case ETunnelNavSpaceMode::Corridor:
	default:
		return TEXT("Corridor");
	}
}
}

UHelmNavigationDisplayComponent::UHelmNavigationDisplayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UHelmNavigationDisplayComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveRuntimeComponent();
	RefreshViewData();
}

void UHelmNavigationDisplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceLastRefreshS += DeltaTime;
	TimeSinceLastSuccessfulRefreshS += DeltaTime;
	if (TimeSinceLastRefreshS >= FMath::Max(0.02f, RefreshPeriodS))
	{
		TimeSinceLastRefreshS = 0.f;
		RefreshViewData();
	}
}

void UHelmNavigationDisplayComponent::SetTunnelNavigationRuntime(UTunnelNavigationRuntimeComponent* InRuntime)
{
	CachedTunnelRuntime = InRuntime;
	bLoggedMissingRuntime = false;
}

UTunnelNavigationRuntimeComponent* UHelmNavigationDisplayComponent::GetTunnelNavigationRuntime() const
{
	return CachedTunnelRuntime;
}

bool UHelmNavigationDisplayComponent::RefreshViewData()
{
	const double RefreshStartS = FPlatformTime::Seconds();
	ResolveRuntimeComponent();
	if (!IsValid(CachedTunnelRuntime))
	{
		ResetCachedViewData();
		UpdateInstrumentStatus(false, static_cast<float>((FPlatformTime::Seconds() - RefreshStartS) * 1000.0));
		if (bEnableDebugLogs && !bLoggedMissingRuntime)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] HelmNavigationDisplay: missing TunnelNavigationRuntime."), *GetName());
			bLoggedMissingRuntime = true;
		}
		return false;
	}

	bLoggedMissingRuntime = false;
	CachedProjection = FTunnelNavProjectionResult();
	CachedHeadingState = FTunnelNavHeadingVsVelocityState();
	CachedStopWarning = FTunnelNavStoppingDistanceWarning();
	CachedCommitmentWarning = FTunnelNavCommitmentWarning();
	CachedInstrumentStatus = FHelmInstrumentStatus();
	CachedCrossSectionView = FHelmCrossSectionViewData();
	CachedForwardView = FHelmForwardAnticipationViewData();
	CachedReconstructionView = FHelmReconstructionViewData();
	CachedGraphView = FHelmTacticalGraphViewData();
	bHasValidData = false;

	if (!CachedTunnelRuntime->ProjectSubmarineToRoute(CachedProjection))
	{
		if (bEnableDebugLogs)
		{
			UTunnelNavigationRuntimeComponent* Runtime = CachedTunnelRuntime.Get();
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[%s] HelmNavigationDisplay projection failed | Runtime=%s | Route=%s | TunnelNav=%s | Samples=%d"),
				*GetName(),
				*GetNameSafe(Runtime),
				*GetNameSafe(Runtime ? Runtime->GetRouteActor() : nullptr),
				*GetNameSafe(Runtime ? Runtime->GetTunnelNavData() : nullptr),
				(Runtime && Runtime->GetTunnelNavData()) ? Runtime->GetTunnelNavData()->Samples.Num() : 0);
		}
		UpdateInstrumentStatus(false, static_cast<float>((FPlatformTime::Seconds() - RefreshStartS) * 1000.0));
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		UpdateInstrumentStatus(false, static_cast<float>((FPlatformTime::Seconds() - RefreshStartS) * 1000.0));
		return false;
	}

	CachedTunnelRuntime->GetHeadingVsVelocityState(
		OwnerActor->GetActorTransform(),
		OwnerActor->GetVelocity(),
		CanonicalNavigationProfile,
		CachedHeadingState);

	bool bProjectionSuspect = false;
	FTunnelNavCrossSectionResult CrossSection;
	if (CachedTunnelRuntime->GetLocalCrossSection(CachedProjection, CanonicalNavigationProfile, CrossSection))
	{
		BuildCrossSectionView(CrossSection, CachedHeadingState, CachedProjection, CachedCrossSectionView);

		if (CrossSection.bValid)
		{
			const float RightOverflowCm = FMath::Max(0.f, FMath::Abs(CrossSection.SubProjectedOffsetRightCm) - ((CrossSection.SubProjectedOffsetRightCm >= 0.f) ? CrossSection.ClearanceRightCm : CrossSection.ClearanceLeftCm));
			const float UpOverflowCm = FMath::Max(0.f, FMath::Abs(CrossSection.SubProjectedOffsetUpCm) - ((CrossSection.SubProjectedOffsetUpCm >= 0.f) ? CrossSection.ClearanceUpCm : CrossSection.ClearanceDownCm));
			const float WorstOverflowCm = FMath::Max(RightOverflowCm, UpOverflowCm);
			bProjectionSuspect = WorstOverflowCm > 250.f;
			if (bEnableDebugLogs && bProjectionSuspect)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[%s] HelmNavigationDisplay suspect projection | Sample=%d | Route=%.1fm | OffsetR=%.0f | OffsetU=%.0f | CLR L/R/U/D = %.0f/%.0f/%.0f/%.0f"),
					*GetName(),
					CrossSection.SampleIndex,
					CachedProjection.RouteDistanceCm / 100.f,
					CrossSection.SubProjectedOffsetRightCm,
					CrossSection.SubProjectedOffsetUpCm,
					CrossSection.ClearanceLeftCm,
					CrossSection.ClearanceRightCm,
					CrossSection.ClearanceUpCm,
					CrossSection.ClearanceDownCm);
			}
		}
	}

	FTunnelNavForwardProfile ForwardProfile;
	if (CachedTunnelRuntime->GetForwardAnticipationProfile(CachedProjection, CanonicalNavigationProfile, ForwardLookaheadCm, ForwardProfile))
	{
		BuildForwardView(ForwardProfile, CachedForwardView);
		CachedTunnelRuntime->GetStoppingDistanceWarning(
			ForwardProfile,
			CanonicalNavigationProfile,
			FMath::Abs(CachedHeadingState.ForwardSpeedCmS),
			CachedStopWarning);
	}

	CachedTunnelRuntime->GetCommitmentWarning(CachedProjection, CanonicalNavigationProfile, CachedCommitmentWarning);

	FTunnelNavGraphWindow GraphWindow;
	if (CachedTunnelRuntime->GetLocalGraphWindow(CachedProjection, TacticalGraphRadiusCm, GraphWindow))
	{
		BuildGraphView(GraphWindow, CachedProjection, CachedGraphView);
	}
	else if (bEnableDebugLogs)
	{
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("[%s] HelmNavigationDisplay graph unavailable | Projected=%d | Sample=%d | Edge=%d | Radius=%.0f"),
			*GetName(),
			CachedProjection.bProjected ? 1 : 0,
			CachedProjection.SampleIndex,
			CachedProjection.EdgeIndex,
			TacticalGraphRadiusCm);
	}

	bHasValidData = CachedCrossSectionView.bValid || CachedForwardView.bValid || CachedGraphView.bValid;
	if (bHasValidData)
	{
		TimeSinceLastSuccessfulRefreshS = 0.f;
	}
	UpdateInstrumentStatus(
		bProjectionSuspect,
		static_cast<float>((FPlatformTime::Seconds() - RefreshStartS) * 1000.0));
	BuildReconstructionView();

	if (bEnableDebugLogs && bHasValidData)
	{
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("[%s] HelmNavigationDisplay refreshed | Route=%.1fm | Space=%s | Cross=%d | Forward=%d | Graph=%d"),
			*GetName(),
			CachedProjection.RouteDistanceCm / 100.f,
			*SpaceModeToString(CachedProjection.SpaceMode),
			CachedCrossSectionView.bValid ? 1 : 0,
			CachedForwardView.bValid ? 1 : 0,
			CachedGraphView.bValid ? 1 : 0);
	}

	return bHasValidData;
}

bool UHelmNavigationDisplayComponent::HasValidNavigationData() const
{
	return bHasValidData;
}

FTunnelNavProjectionResult UHelmNavigationDisplayComponent::GetProjectionResult() const
{
	return CachedProjection;
}

FHelmCrossSectionViewData UHelmNavigationDisplayComponent::GetCrossSectionViewData() const
{
	return CachedCrossSectionView;
}

FHelmForwardAnticipationViewData UHelmNavigationDisplayComponent::GetForwardAnticipationViewData() const
{
	return CachedForwardView;
}

FHelmTacticalGraphViewData UHelmNavigationDisplayComponent::GetTacticalGraphViewData() const
{
	return CachedGraphView;
}

FHelmReconstructionViewData UHelmNavigationDisplayComponent::GetReconstructionViewData() const
{
	return CachedReconstructionView;
}

FHelmInstrumentStatus UHelmNavigationDisplayComponent::GetInstrumentStatus() const
{
	return CachedInstrumentStatus;
}

FTunnelNavHeadingVsVelocityState UHelmNavigationDisplayComponent::GetHeadingVsVelocityState() const
{
	return CachedHeadingState;
}

FTunnelNavStoppingDistanceWarning UHelmNavigationDisplayComponent::GetStoppingDistanceWarning() const
{
	return CachedStopWarning;
}

FTunnelNavCommitmentWarning UHelmNavigationDisplayComponent::GetCommitmentWarning() const
{
	return CachedCommitmentWarning;
}

void UHelmNavigationDisplayComponent::ResolveRuntimeComponent()
{
	if (IsValid(CachedTunnelRuntime))
	{
		return;
	}

	if (!bAutoBindTunnelNavigationRuntime)
	{
		return;
	}

	if (const ASubmarineBase* Submarine = Cast<ASubmarineBase>(GetOwner()))
	{
		CachedTunnelRuntime = Submarine->TunnelNavigationRuntime;
		return;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		CachedTunnelRuntime = OwnerActor->FindComponentByClass<UTunnelNavigationRuntimeComponent>();
	}
}

void UHelmNavigationDisplayComponent::ResetCachedViewData()
{
	bHasValidData = false;
	CachedProjection = FTunnelNavProjectionResult();
	CachedHeadingState = FTunnelNavHeadingVsVelocityState();
	CachedStopWarning = FTunnelNavStoppingDistanceWarning();
	CachedCommitmentWarning = FTunnelNavCommitmentWarning();
	CachedInstrumentStatus = FHelmInstrumentStatus();
	CachedCrossSectionView = FHelmCrossSectionViewData();
	CachedForwardView = FHelmForwardAnticipationViewData();
	CachedReconstructionView = FHelmReconstructionViewData();
	CachedGraphView = FHelmTacticalGraphViewData();
}

bool UHelmNavigationDisplayComponent::BuildCrossSectionView(
	const FTunnelNavCrossSectionResult& CrossSection,
	const FTunnelNavHeadingVsVelocityState& HeadingState,
	const FTunnelNavProjectionResult& Projection,
	FHelmCrossSectionViewData& OutView) const
{
	OutView = FHelmCrossSectionViewData();
	if (!CrossSection.bValid)
	{
		return false;
	}

	const float ScaleCm = FMath::Max3(
		FMath::Max(CrossSection.ClearanceLeftCm, CrossSection.ClearanceRightCm),
		FMath::Max(CrossSection.ClearanceUpCm, CrossSection.ClearanceDownCm),
		100.f);
	const float SafeLeftCm = FMath::Max(0.f, CrossSection.ClearanceLeftCm - FMath::Max(0.f, -CrossSection.SubProjectedOffsetRightCm));
	const float SafeRightCm = FMath::Max(0.f, CrossSection.ClearanceRightCm - FMath::Max(0.f, CrossSection.SubProjectedOffsetRightCm));
	const float SafeAboveCm = FMath::Max(0.f, CrossSection.ClearanceUpCm - FMath::Max(0.f, CrossSection.SubProjectedOffsetUpCm));
	const float SafeBelowCm = FMath::Max(0.f, CrossSection.ClearanceDownCm - FMath::Max(0.f, -CrossSection.SubProjectedOffsetUpCm));
	const FVector2D Heading2D(
		FVector::DotProduct(HeadingState.HeadingWorld, CrossSection.Right),
		FVector::DotProduct(HeadingState.HeadingWorld, CrossSection.Up));
	const FVector2D Velocity2D(
		FVector::DotProduct(HeadingState.VelocityWorld, CrossSection.Right),
		FVector::DotProduct(HeadingState.VelocityWorld, CrossSection.Up));

	OutView.bValid = true;
	OutView.SpaceMode = CrossSection.SpaceMode;
	OutView.RouteDistanceMeters = Projection.RouteDistanceCm / 100.f;
	OutView.OffsetRightCm = CrossSection.SubProjectedOffsetRightCm;
	OutView.OffsetUpCm = CrossSection.SubProjectedOffsetUpCm;
	OutView.SafeLeftCm = SafeLeftCm;
	OutView.SafeRightCm = SafeRightCm;
	OutView.SafeAboveCm = SafeAboveCm;
	OutView.SafeBelowCm = SafeBelowCm;
	OutView.SafeAbove01 = FMath::Clamp(SafeAboveCm / FMath::Max(1.f, CrossSection.ClearanceUpCm), 0.f, 1.f);
	OutView.SafeBelow01 = FMath::Clamp(SafeBelowCm / FMath::Max(1.f, CrossSection.ClearanceDownCm), 0.f, 1.f);
	OutView.ClearanceMarginCm = CrossSection.ClearanceMarginCm;
	OutView.SubOffsetNormalized = FVector2D(
		FMath::Clamp(CrossSection.SubProjectedOffsetRightCm / ScaleCm, -1.f, 1.f),
		FMath::Clamp(-CrossSection.SubProjectedOffsetUpCm / ScaleCm, -1.f, 1.f));
	OutView.HeadingVectorNormalized = Heading2D.GetClampedToMaxSize(1.f) * 0.35f;
	OutView.VelocityVectorNormalized = Velocity2D.GetClampedToMaxSize(1.f) * 0.35f;
	OutView.bNearWallWarning = CrossSection.bNearWallWarning;
	OutView.bHardClearanceViolation = CrossSection.bHardClearanceViolation;

	if (CrossSection.RadialSamplesCm.Num() >= 3)
	{
		OutView.ContourPointsNormalized.Reserve(CrossSection.RadialSamplesCm.Num() + 1);
		const int32 NumSamples = CrossSection.RadialSamplesCm.Num();
		for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
		{
			const float Angle = (2.f * PI * static_cast<float>(SampleIndex)) / static_cast<float>(NumSamples);
			const float Radius01 = FMath::Clamp(CrossSection.RadialSamplesCm[SampleIndex] / ScaleCm, 0.f, 1.25f);
			OutView.ContourPointsNormalized.Add(FVector2D(FMath::Cos(Angle) * Radius01, -FMath::Sin(Angle) * Radius01));
		}
		if (OutView.ContourPointsNormalized.Num() > 0)
		{
			const FVector2D FirstPoint = OutView.ContourPointsNormalized[0];
			OutView.ContourPointsNormalized.Add(FirstPoint);
		}
	}
	else
	{
		const float Left01 = FMath::Clamp(CrossSection.ClearanceLeftCm / ScaleCm, 0.f, 1.f);
		const float Right01 = FMath::Clamp(CrossSection.ClearanceRightCm / ScaleCm, 0.f, 1.f);
		const float Up01 = FMath::Clamp(CrossSection.ClearanceUpCm / ScaleCm, 0.f, 1.f);
		const float Down01 = FMath::Clamp(CrossSection.ClearanceDownCm / ScaleCm, 0.f, 1.f);
		OutView.ContourPointsNormalized = {
			FVector2D(-Left01, -Up01),
			FVector2D(Right01, -Up01),
			FVector2D(Right01, Down01),
			FVector2D(-Left01, Down01),
			FVector2D(-Left01, -Up01)
		};
	}

	return true;
}

bool UHelmNavigationDisplayComponent::BuildForwardView(const FTunnelNavForwardProfile& Profile, FHelmForwardAnticipationViewData& OutView) const
{
	OutView = FHelmForwardAnticipationViewData();
	if (!Profile.bValid || Profile.Points.Num() == 0)
	{
		return false;
	}

	float MaxHeightCm = 100.f;
	float MaxWidthCm = 100.f;
	float MaxLeftCm = 100.f;
	float MaxRightCm = 100.f;
	for (const FTunnelNavAnticipationPoint& Point : Profile.Points)
	{
		MaxHeightCm = FMath::Max(MaxHeightCm, FMath::Max(Point.TunnelHalfHeightUpCm, Point.TunnelHalfHeightDownCm));
		MaxWidthCm = FMath::Max(MaxWidthCm, FMath::Min(Point.TunnelHalfWidthLeftCm, Point.TunnelHalfWidthRightCm));
		MaxLeftCm = FMath::Max(MaxLeftCm, Point.TunnelHalfWidthLeftCm);
		MaxRightCm = FMath::Max(MaxRightCm, Point.TunnelHalfWidthRightCm);
	}

	OutView.bValid = true;
	OutView.SpaceMode = Profile.SpaceMode;
	OutView.LookaheadMeters = Profile.LookaheadDistanceCm / 100.f;
	OutView.RecommendedMaxSpeedKmh = Profile.RecommendedMaxSpeedCmS * 0.036f;
	OutView.StoppingDistanceMeters = Profile.StoppingDistanceCm / 100.f;
	OutView.FirstCriticalObstacleMeters = (Profile.FirstCriticalObstacleDistanceCm >= 0.f) ? (Profile.FirstCriticalObstacleDistanceCm / 100.f) : -1.f;
	OutView.DistanceToFirstRuntimeObstacleMeters = (Profile.DistanceToFirstRuntimeObstacleCm >= 0.f) ? (Profile.DistanceToFirstRuntimeObstacleCm / 100.f) : -1.f;
	OutView.bCommitmentZone = Profile.bCommitmentZone;
	OutView.bCrashStopAlreadyLate = Profile.bCrashStopAlreadyLate;
	OutView.bNoTurnaroundBeforeNextHub = Profile.bNoTurnaroundBeforeNextHub;
	OutView.Samples.Reserve(Profile.Points.Num());

	for (const FTunnelNavAnticipationPoint& Point : Profile.Points)
	{
		FHelmForwardAnticipationSampleViewData ViewPoint;
		ViewPoint.DistanceAheadMeters = Point.DistanceAheadCm / 100.f;
		ViewPoint.Distance01 = FMath::Clamp(Point.DistanceAheadCm / FMath::Max(Profile.LookaheadDistanceCm, 1.f), 0.f, 1.f);
		ViewPoint.Ceiling01 = FMath::Clamp(Point.TunnelHalfHeightUpCm / MaxHeightCm, 0.f, 1.f);
		ViewPoint.Floor01 = FMath::Clamp(Point.TunnelHalfHeightDownCm / MaxHeightCm, 0.f, 1.f);
		ViewPoint.Width01 = FMath::Clamp(FMath::Min(Point.TunnelHalfWidthLeftCm, Point.TunnelHalfWidthRightCm) / MaxWidthCm, 0.f, 1.f);
		ViewPoint.CurvatureSeverity01 = FMath::Clamp(FMath::Abs(Point.CurvatureDegPer100m) / 35.f, 0.f, 1.f);
		ViewPoint.LeftWall01 = FMath::Clamp(Point.TunnelHalfWidthLeftCm / MaxLeftCm, 0.f, 1.f);
		ViewPoint.RightWall01 = FMath::Clamp(Point.TunnelHalfWidthRightCm / MaxRightCm, 0.f, 1.f);
		ViewPoint.TurnSigned01 = FMath::Clamp(Point.CurvatureDegPer100m / 35.f, -1.f, 1.f);
		ViewPoint.SlopeSigned01 = FMath::Clamp(Point.GradeDegPer100m / 18.f, -1.f, 1.f);
		ViewPoint.CenterShiftSigned01 = FMath::Clamp(
			(Point.TunnelHalfWidthRightCm - Point.TunnelHalfWidthLeftCm) / FMath::Max(Point.TunnelHalfWidthRightCm + Point.TunnelHalfWidthLeftCm, 1.f),
			-1.f,
			1.f);
		ViewPoint.bNarrowing = Point.MinClearanceCm < CanonicalNavigationProfile.PreferredClearanceCm;
		ViewPoint.bCritical = Point.bBelowRequiredClearance || Point.bRuntimeObstacle;
		ViewPoint.bNoTurnZone = Point.bNoTurnZone;
		ViewPoint.bHubTransition = Point.bHubTransition;
		OutView.Samples.Add(ViewPoint);
	}

	return true;
}

void UHelmNavigationDisplayComponent::BuildReconstructionView()
{
	CachedReconstructionView = FHelmReconstructionViewData();
	CachedReconstructionView.bValid = CachedCrossSectionView.bValid || CachedForwardView.bValid;
	CachedReconstructionView.InstrumentStatus = CachedInstrumentStatus;
	CachedReconstructionView.CrossSection = CachedCrossSectionView;
	CachedReconstructionView.Forward = CachedForwardView;
}

void UHelmNavigationDisplayComponent::UpdateInstrumentStatus(bool bProjectionSuspect, float RefreshDurationMs)
{
	CachedInstrumentStatus = FHelmInstrumentStatus();
	CachedInstrumentStatus.bBound = IsValid(CachedTunnelRuntime);
	CachedInstrumentStatus.bRuntimeReady = bHasValidData;
	CachedInstrumentStatus.bProjectionSuspect = bProjectionSuspect;
	CachedInstrumentStatus.SpaceMode = CachedProjection.bProjected ? CachedProjection.SpaceMode : ETunnelNavSpaceMode::Corridor;
	CachedInstrumentStatus.RefreshPeriodMs = FMath::Max(0.02f, RefreshPeriodS) * 1000.f;
	CachedInstrumentStatus.UpdateAgeMs = TimeSinceLastSuccessfulRefreshS * 1000.f;
	CachedInstrumentStatus.ReconstructionTimeMs = RefreshDurationMs;
	CachedInstrumentStatus.Confidence01 = ComputeInstrumentConfidence(bProjectionSuspect);
	CachedInstrumentStatus.bStale = CachedInstrumentStatus.UpdateAgeMs > (CachedInstrumentStatus.RefreshPeriodMs * 2.5f);
	CachedReconstructionView.InstrumentStatus = CachedInstrumentStatus;
}

float UHelmNavigationDisplayComponent::ComputeInstrumentConfidence(bool bProjectionSuspect) const
{
	float Confidence01 = 0.f;
	if (CachedCrossSectionView.bValid)
	{
		Confidence01 += 0.45f;
	}
	if (CachedForwardView.bValid)
	{
		Confidence01 += 0.35f;
	}
	if (CachedGraphView.bValid)
	{
		Confidence01 += 0.20f;
	}

	if (CachedProjection.bProjected)
	{
		Confidence01 *= FMath::GetMappedRangeValueClamped(FVector2D(0.3f, 1.f), FVector2D(0.55f, 1.f), FMath::Abs(CachedProjection.AlignmentDot));
	}

	if (bProjectionSuspect)
	{
		Confidence01 *= 0.45f;
	}

	return FMath::Clamp(Confidence01, 0.f, 1.f);
}

bool UHelmNavigationDisplayComponent::BuildGraphView(
	const FTunnelNavGraphWindow& GraphWindow,
	const FTunnelNavProjectionResult& Projection,
	FHelmTacticalGraphViewData& OutView) const
{
	OutView = FHelmTacticalGraphViewData();
	if (!GraphWindow.bValid || GraphWindow.Nodes.Num() == 0)
	{
		return false;
	}

	const float RadiusCm = FMath::Max(1000.f, TacticalGraphRadiusCm);
	OutView.bValid = true;
	OutView.SpaceMode = GraphWindow.SpaceMode;
	OutView.AnchorNodeID = GraphWindow.AnchorNodeID;
	OutView.CurrentEdgeIndex = GraphWindow.CurrentEdgeIndex;
	OutView.DistanceToNextHubMeters = (GraphWindow.DistanceToNextHubCm >= 0.f) ? (GraphWindow.DistanceToNextHubCm / 100.f) : -1.f;
	OutView.DistanceToNextSplitMeters = (GraphWindow.DistanceToNextSplitCm >= 0.f) ? (GraphWindow.DistanceToNextSplitCm / 100.f) : -1.f;
	OutView.DistanceToNextMergeMeters = (GraphWindow.DistanceToNextMergeCm >= 0.f) ? (GraphWindow.DistanceToNextMergeCm / 100.f) : -1.f;
	OutView.bMultipleExitsNearby = GraphWindow.bMultipleExitsNearby;

	OutView.Nodes.Reserve(GraphWindow.Nodes.Num());
	for (const FTunnelNavGraphNode& Node : GraphWindow.Nodes)
	{
		const FVector Delta = Node.WorldPosition - Projection.ClosestPointWorld;
		FHelmTacticalGraphNodeViewData ViewNode;
		ViewNode.NodeID = Node.NodeID;
		ViewNode.PositionNormalized = FVector2D(
			FMath::Clamp(FVector::DotProduct(Delta, Projection.FrameRight) / RadiusCm, -1.f, 1.f),
			FMath::Clamp(FVector::DotProduct(Delta, Projection.FrameForward) / RadiusCm, -1.f, 1.f));
		ViewNode.bCurrent = (Node.NodeID == GraphWindow.AnchorNodeID);
		ViewNode.bHub = (Node.LogicalRole == ELogicalRouteNodeRole::Hub) || (Node.NodeType == ETopologyNodeType::HubChamber);
		ViewNode.bSplitLike = (Node.NodeType == ETopologyNodeType::SplitAnchor || Node.NodeType == ETopologyNodeType::MergeAnchor);
		ViewNode.bOptional = Node.bOptional;
		OutView.Nodes.Add(ViewNode);
	}

	OutView.Edges.Reserve(GraphWindow.Edges.Num());
	for (const FTunnelNavGraphEdge& Edge : GraphWindow.Edges)
	{
		FHelmTacticalGraphEdgeViewData ViewEdge;
		ViewEdge.EdgeIndex = Edge.EdgeIndex;
		ViewEdge.FromNodeID = Edge.FromNodeID;
		ViewEdge.ToNodeID = Edge.ToNodeID;
		ViewEdge.bCurrent = (Edge.EdgeIndex == GraphWindow.CurrentEdgeIndex);
		ViewEdge.bOptional = Edge.bOptional;
		OutView.Edges.Add(ViewEdge);
	}

	return true;
}
