#include "TunnelNavigationRuntimeComponent.h"

#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Math/NumericLimits.h"
#include "Math/RotationMatrix.h"
#include "TraversalRouteActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogTunnelNavRuntime, Log, All);

namespace
{
const FTunnelNavNodeRecord* FindNavNodeById(const TArray<FTunnelNavNodeRecord>& Nodes, int32 NodeID)
{
	return Nodes.FindByPredicate([NodeID](const FTunnelNavNodeRecord& Node)
	{
		return Node.NodeID == NodeID;
	});
}

bool IsHubLikeNode(const FTunnelNavNodeRecord& Node)
{
	return Node.LogicalRole == ELogicalRouteNodeRole::Hub
		|| Node.NodeType == ETopologyNodeType::HubChamber
		|| Node.CheckpointSpaceShape == ECheckpointSpaceShape::LargeCavity;
}

bool IsSplitLikeNode(const FTunnelNavNodeRecord& Node)
{
	return Node.NodeType == ETopologyNodeType::SplitAnchor || Node.NextNodeIDs.Num() > 1;
}

bool IsMergeLikeNode(const FTunnelNavNodeRecord& Node)
{
	return Node.NodeType == ETopologyNodeType::MergeAnchor;
}
}

UTunnelNavigationRuntimeComponent::UTunnelNavigationRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	DefaultClassSpecs.Reserve(4);

	FSubmarineClassNavigationSpec ClassS;
	ClassS.SubClass = ETunnelNavSubClass::ClassS;
	ClassS.RequiredClearanceCm = 2200.f;
	ClassS.MinTurnaroundRadiusCm = 3200.f;
	ClassS.ServiceDecelerationCmS2 = 180.f;
	ClassS.EmergencyDecelerationCmS2 = 280.f;
	ClassS.CommitmentLookaheadCm = 9000.f;
	ClassS.DriftWarningAngleDeg = 20.f;
	ClassS.LateralSpeedWarningCmS = 100.f;
	DefaultClassSpecs.Add(ClassS);

	FSubmarineClassNavigationSpec ClassM;
	ClassM.SubClass = ETunnelNavSubClass::ClassM;
	ClassM.RequiredClearanceCm = 2800.f;
	ClassM.MinTurnaroundRadiusCm = 4200.f;
	ClassM.ServiceDecelerationCmS2 = 160.f;
	ClassM.EmergencyDecelerationCmS2 = 260.f;
	ClassM.CommitmentLookaheadCm = 12000.f;
	ClassM.DriftWarningAngleDeg = 18.f;
	ClassM.LateralSpeedWarningCmS = 90.f;
	DefaultClassSpecs.Add(ClassM);

	FSubmarineClassNavigationSpec ClassL;
	ClassL.SubClass = ETunnelNavSubClass::ClassL;
	ClassL.RequiredClearanceCm = 3400.f;
	ClassL.MinTurnaroundRadiusCm = 5400.f;
	ClassL.ServiceDecelerationCmS2 = 130.f;
	ClassL.EmergencyDecelerationCmS2 = 220.f;
	ClassL.CommitmentLookaheadCm = 15000.f;
	ClassL.DriftWarningAngleDeg = 16.f;
	ClassL.LateralSpeedWarningCmS = 80.f;
	DefaultClassSpecs.Add(ClassL);

	FSubmarineClassNavigationSpec ClassXL;
	ClassXL.SubClass = ETunnelNavSubClass::ClassXL;
	ClassXL.RequiredClearanceCm = 4000.f;
	ClassXL.MinTurnaroundRadiusCm = 6800.f;
	ClassXL.ServiceDecelerationCmS2 = 110.f;
	ClassXL.EmergencyDecelerationCmS2 = 190.f;
	ClassXL.CommitmentLookaheadCm = 18000.f;
	ClassXL.DriftWarningAngleDeg = 14.f;
	ClassXL.LateralSpeedWarningCmS = 70.f;
	DefaultClassSpecs.Add(ClassXL);
}

void UTunnelNavigationRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoResolveRouteActor)
	{
		ResolveRouteActorFromWorld();
	}
}

void UTunnelNavigationRuntimeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	CleanupExpiredRuntimeObstacles();

	if (bAutoResolveRouteActor && (!IsValid(CachedRouteActor) || (bAutoBindTunnelNavAssetFromRoute && !IsValid(CachedTunnelNavData))))
	{
		TimeSinceLastResolveAttemptS += DeltaTime;
		if (TimeSinceLastResolveAttemptS >= FMath::Max(0.05f, RouteResolveRetryPeriodS))
		{
			TimeSinceLastResolveAttemptS = 0.f;
			ResolveRouteActorFromWorld();
		}
	}

	if (bEnableDebugDraw)
	{
		DrawDebugOverlay();
	}
}

void UTunnelNavigationRuntimeComponent::SetRouteActor(ATraversalRouteActor* InRouteActor)
{
	CachedRouteActor = InRouteActor;
	if (bAutoBindTunnelNavAssetFromRoute && IsValid(InRouteActor))
	{
		CachedTunnelNavData = InRouteActor->GetTunnelNavData();
	}
}

void UTunnelNavigationRuntimeComponent::SetTunnelNavData(UTunnelNavDataAsset* InTunnelNavData)
{
	CachedTunnelNavData = InTunnelNavData;
}

ATraversalRouteActor* UTunnelNavigationRuntimeComponent::GetRouteActor() const
{
	return CachedRouteActor;
}

UTunnelNavDataAsset* UTunnelNavigationRuntimeComponent::GetTunnelNavData() const
{
	return CachedTunnelNavData;
}

bool UTunnelNavigationRuntimeComponent::BuildNavigationProfileForSubClass(ETunnelNavSubClass SubClass, FSubmarineNavigationProfile& OutProfile) const
{
	return ResolveNavigationProfile(SubClass, OutProfile);
}

bool UTunnelNavigationRuntimeComponent::ProjectSubmarineToRoute(FTunnelNavProjectionResult& OutProjection) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		OutProjection = FTunnelNavProjectionResult();
		return false;
	}

	return ProjectSubmarineToRoute(OwnerActor->GetActorTransform(), OutProjection);
}

bool UTunnelNavigationRuntimeComponent::ProjectSubmarineToRoute(const FTransform& SubTransform, FTunnelNavProjectionResult& OutProjection) const
{
	return ProjectWorldLocationToRouteInternal(SubTransform.GetLocation(), SubTransform.GetRotation().GetForwardVector(), OutProjection);
}

bool UTunnelNavigationRuntimeComponent::ProjectWorldLocationToRoute(const FVector& WorldLocation, FTunnelNavProjectionResult& OutProjection) const
{
	const AActor* OwnerActor = GetOwner();
	const FVector ForwardHint = OwnerActor ? OwnerActor->GetActorForwardVector() : FVector::ZeroVector;
	return ProjectWorldLocationToRouteInternal(WorldLocation, ForwardHint, OutProjection);
}

bool UTunnelNavigationRuntimeComponent::ProjectWorldLocationToRouteInternal(
	const FVector& WorldLocation,
	const FVector& ForwardHint,
	FTunnelNavProjectionResult& OutProjection) const
{
	OutProjection = FTunnelNavProjectionResult();
	if (!EnsureNavigationDataAvailable())
	{
		return false;
	}

	float DistanceFromCenterCm = 0.f;
	float AlignmentDot = 0.f;
	int32 BestSampleIndex = INDEX_NONE;
	if (!FindProjectedSampleIndex(WorldLocation, ForwardHint, BestSampleIndex, &DistanceFromCenterCm, &AlignmentDot))
	{
		return false;
	}
	if (!CachedTunnelNavData->Samples.IsValidIndex(BestSampleIndex))
	{
		return false;
	}

	const FTunnelNavSampleRecord& Sample = CachedTunnelNavData->Samples[BestSampleIndex];
	const FVector Delta = WorldLocation - Sample.WorldPosition;

	OutProjection.bProjected = true;
	OutProjection.SampleIndex = BestSampleIndex;
	OutProjection.EdgeIndex = Sample.SegmentIndex;
	OutProjection.RouteDistanceCm = Sample.ApproxDistanceCm;
	OutProjection.DistanceAlongEdgeCm = Sample.DistanceAlongSegmentCm;
	OutProjection.LocalOffsetRightCm = FVector::DotProduct(Delta, Sample.Right);
	OutProjection.LocalOffsetUpCm = FVector::DotProduct(Delta, Sample.Up);
	OutProjection.DistanceFromCenterCm = DistanceFromCenterCm;
	OutProjection.ClosestPointWorld = Sample.WorldPosition;
	OutProjection.FrameForward = Sample.Forward;
	OutProjection.FrameRight = Sample.Right;
	OutProjection.FrameUp = Sample.Up;
	OutProjection.AlignmentDot = AlignmentDot;
	OutProjection.SpaceMode = DetermineSpaceMode(Sample);
	OutProjection.bHubLike = OutProjection.SpaceMode == ETunnelNavSpaceMode::CavernHub;
	OutProjection.bOnGuaranteedPath = Sample.bGuaranteedTraversal;
	OutProjection.bOptionalBranch = Sample.bIsOptionalSideContent;

	UpdateProjectionCache(OutProjection);
	return true;
}

bool UTunnelNavigationRuntimeComponent::GetLocalCrossSection(
	const FTunnelNavProjectionResult& Projection,
	ETunnelNavSubClass SubClass,
	FTunnelNavCrossSectionResult& OutResult) const
{
	FSubmarineNavigationProfile NavProfile;
	if (!ResolveNavigationProfile(SubClass, NavProfile))
	{
		OutResult = FTunnelNavCrossSectionResult();
		return false;
	}

	return GetLocalCrossSection(Projection, NavProfile, OutResult);
}

bool UTunnelNavigationRuntimeComponent::GetLocalCrossSection(
	const FTunnelNavProjectionResult& Projection,
	const FSubmarineNavigationProfile& NavProfile,
	FTunnelNavCrossSectionResult& OutResult) const
{
	OutResult = FTunnelNavCrossSectionResult();
	if (!Projection.bProjected || !EnsureNavigationDataAvailable() || !CachedTunnelNavData->Samples.IsValidIndex(Projection.SampleIndex))
	{
		return false;
	}

	const FTunnelNavSampleRecord& Sample = CachedTunnelNavData->Samples[Projection.SampleIndex];
	const float RightRoomCm = Sample.ClearanceRightCm - FMath::Max(0.f, Projection.LocalOffsetRightCm);
	const float LeftRoomCm = Sample.ClearanceLeftCm - FMath::Max(0.f, -Projection.LocalOffsetRightCm);
	const float UpRoomCm = Sample.ClearanceUpCm - FMath::Max(0.f, Projection.LocalOffsetUpCm);
	const float DownRoomCm = Sample.ClearanceDownCm - FMath::Max(0.f, -Projection.LocalOffsetUpCm);
	const float LocalMinRoomCm = FMath::Min(FMath::Min(RightRoomCm, LeftRoomCm), FMath::Min(UpRoomCm, DownRoomCm));

	OutResult.bValid = true;
	OutResult.SampleIndex = Projection.SampleIndex;
	OutResult.Center = Sample.WorldPosition;
	OutResult.Forward = Sample.Forward;
	OutResult.Right = Sample.Right;
	OutResult.Up = Sample.Up;
	OutResult.ClearanceRightCm = Sample.ClearanceRightCm;
	OutResult.ClearanceLeftCm = Sample.ClearanceLeftCm;
	OutResult.ClearanceUpCm = Sample.ClearanceUpCm;
	OutResult.ClearanceDownCm = Sample.ClearanceDownCm;
	OutResult.MinClearanceCm = LocalMinRoomCm;
	OutResult.RequiredClearanceCm = NavProfile.HardClearanceCm;
	OutResult.ClearanceMarginCm = LocalMinRoomCm - NavProfile.HardClearanceCm;
	OutResult.RadialSamplesCm = Sample.RadialClearanceCm;
	OutResult.SubProjectedOffsetRightCm = Projection.LocalOffsetRightCm;
	OutResult.SubProjectedOffsetUpCm = Projection.LocalOffsetUpCm;
	OutResult.SpaceMode = DetermineSpaceMode(Sample);
	OutResult.bNearWallWarning = LocalMinRoomCm < NavProfile.PreferredClearanceCm;
	OutResult.bHardClearanceViolation = LocalMinRoomCm < NavProfile.HardClearanceCm;
	OutResult.bRestrictedForClass = OutResult.bHardClearanceViolation || !Sample.bBranchValidationPass;
	OutResult.bTurnaroundPossible = Sample.MinCrossSectionClearanceCm >= (0.5f * NavProfile.MinTurningBasinDiameterCm);
	OutResult.bRuntimeObstacleInsideSection = IsSampleBlockedByRuntimeObstacle(Sample, NavProfile.HardClearanceCm);
	return true;
}

bool UTunnelNavigationRuntimeComponent::GetForwardAnticipationProfile(
	const FTunnelNavProjectionResult& Projection,
	ETunnelNavSubClass SubClass,
	float LookaheadDistanceCm,
	FTunnelNavForwardProfile& OutProfile) const
{
	FSubmarineNavigationProfile NavProfile;
	if (!ResolveNavigationProfile(SubClass, NavProfile))
	{
		OutProfile = FTunnelNavForwardProfile();
		return false;
	}

	return GetForwardAnticipationProfile(Projection, NavProfile, LookaheadDistanceCm, OutProfile);
}

bool UTunnelNavigationRuntimeComponent::GetForwardAnticipationProfile(
	const FTunnelNavProjectionResult& Projection,
	const FSubmarineNavigationProfile& NavProfile,
	float LookaheadDistanceCm,
	FTunnelNavForwardProfile& OutProfile) const
{
	OutProfile = FTunnelNavForwardProfile();
	if (!Projection.bProjected || !EnsureNavigationDataAvailable())
	{
		return false;
	}

	const float EffectiveLookaheadCm = (LookaheadDistanceCm > 0.f)
		? LookaheadDistanceCm
		: FMath::Max(DefaultLookaheadDistanceCm, NavProfile.CommitmentLookaheadCm);
	const float SamplingStepCm = FMath::Max(50.f, AnticipationSamplingStepCm);
	const float StartDistanceCm = Projection.RouteDistanceCm;
	const float EndDistanceCm = StartDistanceCm + EffectiveLookaheadCm;
	const int32 CurrentBranchIndex = CachedTunnelNavData->Samples.IsValidIndex(Projection.SampleIndex)
		? CachedTunnelNavData->Samples[Projection.SampleIndex].BranchIndex
		: INDEX_NONE;

	TArray<FTunnelNavAnticipationPoint> GatheredPoints;
	GatheredPoints.Reserve(64);

	for (int32 SampleIndex = 0; SampleIndex < CachedTunnelNavData->Samples.Num(); ++SampleIndex)
	{
		const FTunnelNavSampleRecord& Sample = CachedTunnelNavData->Samples[SampleIndex];
		if (Sample.ApproxDistanceCm < StartDistanceCm || Sample.ApproxDistanceCm > EndDistanceCm)
		{
			continue;
		}
		if (!IsSampleOptionalForLookahead(Sample))
		{
			continue;
		}

		FTunnelNavAnticipationPoint Point;
		Point.SampleIndex = SampleIndex;
		Point.DistanceAheadCm = FMath::Max(0.f, Sample.ApproxDistanceCm - StartDistanceCm);
		Point.RouteDistanceCm = Sample.ApproxDistanceCm;
		Point.TunnelHalfWidthLeftCm = Sample.ClearanceLeftCm;
		Point.TunnelHalfWidthRightCm = Sample.ClearanceRightCm;
		Point.TunnelHalfHeightUpCm = Sample.ClearanceUpCm;
		Point.TunnelHalfHeightDownCm = Sample.ClearanceDownCm;
		Point.MinClearanceCm = Sample.MinCrossSectionClearanceCm;
		Point.RequiredClearanceCm = NavProfile.HardClearanceCm;
		Point.SpaceMode = DetermineSpaceMode(Sample);
		Point.bHubTransition = (Point.SpaceMode != Projection.SpaceMode);
		Point.bBranchApproach = (Sample.BranchIndex != INDEX_NONE && Sample.BranchIndex != CurrentBranchIndex) || Sample.LogicalRole == ELogicalRouteNodeRole::OptionalSideBranch;
		Point.bBelowRequiredClearance = Sample.MinCrossSectionClearanceCm < NavProfile.HardClearanceCm;
		Point.bRuntimeObstacle = IsSampleBlockedByRuntimeObstacle(Sample, NavProfile.HardClearanceCm);
		Point.bNoTurnZone = Sample.MinCrossSectionClearanceCm < (0.5f * NavProfile.MinTurningBasinDiameterCm);
		Point.bOptionalBranch = Sample.bIsOptionalSideContent;
		GatheredPoints.Add(Point);
	}

	GatheredPoints.Sort(&UTunnelNavigationRuntimeComponent::CompareSampleDistance);
	if (GatheredPoints.Num() == 0)
	{
		return false;
	}

	float NextAcceptedDistanceCm = 0.f;
	float FirstRestrictionDistanceCm = -1.f;
	float FirstRuntimeObstacleDistanceCm = -1.f;
	float MinClearanceAheadCm = TNumericLimits<float>::Max();
	bool bCommitmentZone = false;
	bool bNoTurnBeforeHub = false;
	bool bEncounteredHub = false;

	for (int32 Index = 0; Index < GatheredPoints.Num(); ++Index)
	{
		FTunnelNavAnticipationPoint& Point = GatheredPoints[Index];
		if (Point.DistanceAheadCm + KINDA_SMALL_NUMBER < NextAcceptedDistanceCm)
		{
			continue;
		}

		const int32 PrevIndex = (Index > 0) ? GatheredPoints[Index - 1].SampleIndex : Point.SampleIndex;
		const int32 NextIndex = (Index + 1 < GatheredPoints.Num()) ? GatheredPoints[Index + 1].SampleIndex : Point.SampleIndex;
		Point.CurvatureDegPer100m = ComputeCurvatureDegPer100m(PrevIndex, NextIndex);
		Point.GradeDegPer100m = ComputeGradeDegPer100m(PrevIndex, NextIndex);
		OutProfile.Points.Add(Point);
		NextAcceptedDistanceCm = Point.DistanceAheadCm + SamplingStepCm;

		MinClearanceAheadCm = FMath::Min(MinClearanceAheadCm, Point.MinClearanceCm);
		if (FirstRestrictionDistanceCm < 0.f && (Point.bBelowRequiredClearance || Point.bRuntimeObstacle))
		{
			FirstRestrictionDistanceCm = Point.DistanceAheadCm;
		}
		if (FirstRuntimeObstacleDistanceCm < 0.f && Point.bRuntimeObstacle)
		{
			FirstRuntimeObstacleDistanceCm = Point.DistanceAheadCm;
		}

		if (!bEncounteredHub && Point.SpaceMode == ETunnelNavSpaceMode::CavernHub)
		{
			bEncounteredHub = true;
		}
		if (!bEncounteredHub && Point.bNoTurnZone)
		{
			bNoTurnBeforeHub = true;
		}
		bCommitmentZone |= Point.bNoTurnZone;
	}

	const float ForwardSpeedCmS = FMath::Abs(ComputeForwardSpeedCmS());
	OutProfile.bValid = OutProfile.Points.Num() > 0;
	OutProfile.SpaceMode = Projection.SpaceMode;
	OutProfile.StartDistanceCm = StartDistanceCm;
	OutProfile.LookaheadDistanceCm = EffectiveLookaheadCm;
	OutProfile.MinClearanceAheadCm = (MinClearanceAheadCm == TNumericLimits<float>::Max()) ? 0.f : MinClearanceAheadCm;
	OutProfile.StoppingDistanceCm = ComputeStoppingDistanceCm(NavProfile, ForwardSpeedCmS);
	OutProfile.DistanceToFirstRestrictionCm = FirstRestrictionDistanceCm;
	OutProfile.FirstCriticalObstacleDistanceCm = FirstRestrictionDistanceCm;
	OutProfile.DistanceToFirstRuntimeObstacleCm = FirstRuntimeObstacleDistanceCm;
	OutProfile.bCommitmentZone = bCommitmentZone;
	OutProfile.bNoTurnaroundBeforeNextHub = bNoTurnBeforeHub;
	OutProfile.bCrashStopAlreadyLate = FirstRestrictionDistanceCm >= 0.f && OutProfile.StoppingDistanceCm > FirstRestrictionDistanceCm;
	OutProfile.RecommendedMaxSpeedCmS = ComputeRecommendedMaxSpeedCmS(OutProfile, NavProfile, ForwardSpeedCmS);
	return OutProfile.bValid;
}

bool UTunnelNavigationRuntimeComponent::GetLocalGraphWindow(
	const FTunnelNavProjectionResult& Projection,
	int32 GraphDepth,
	FTunnelNavGraphWindow& OutWindow) const
{
	const float RadiusCm = FMath::Max(1, GraphDepth) * 15000.f;
	return GetLocalGraphWindow(Projection, RadiusCm, OutWindow);
}

bool UTunnelNavigationRuntimeComponent::GetLocalGraphWindow(
	const FTunnelNavProjectionResult& Projection,
	float RadiusCm,
	FTunnelNavGraphWindow& OutWindow) const
{
	OutWindow = FTunnelNavGraphWindow();
	if (!Projection.bProjected || !EnsureNavigationDataAvailable())
	{
		return false;
	}

	const int32 AnchorNodeId = ResolveAnchorNodeId(Projection);
	const FTunnelNavNodeRecord* AnchorNode = FindNavNodeById(CachedTunnelNavData->Nodes, AnchorNodeId);
	if (!AnchorNode)
	{
		return false;
	}

	const FVector AnchorPosition = AnchorNode->WorldPosition;
	const float RadiusSq = FMath::Square(FMath::Max(1000.f, RadiusCm));
	TSet<int32> IncludedNodeIds;

	for (const FTunnelNavNodeRecord& Node : CachedTunnelNavData->Nodes)
	{
		if (Node.NodeID == AnchorNodeId || FVector::DistSquared(Node.WorldPosition, AnchorPosition) <= RadiusSq)
		{
			IncludedNodeIds.Add(Node.NodeID);

			FTunnelNavGraphNode GraphNode;
			GraphNode.NodeID = Node.NodeID;
			GraphNode.WorldPosition = Node.WorldPosition;
			GraphNode.NodeType = Node.NodeType;
			GraphNode.LogicalRole = Node.LogicalRole;
			GraphNode.BranchIndex = Node.BranchIndex;
			GraphNode.bOptional = Node.bIsOptionalSideContent;
			OutWindow.Nodes.Add(GraphNode);
		}
	}

	for (int32 EdgeIndex = 0; EdgeIndex < CachedTunnelNavData->Edges.Num(); ++EdgeIndex)
	{
		const FTunnelNavEdgeRecord& Edge = CachedTunnelNavData->Edges[EdgeIndex];
		if (!IncludedNodeIds.Contains(Edge.StartNodeID) || !IncludedNodeIds.Contains(Edge.EndNodeID))
		{
			continue;
		}

		FTunnelNavGraphEdge GraphEdge;
		GraphEdge.EdgeIndex = EdgeIndex;
		GraphEdge.FromNodeID = Edge.StartNodeID;
		GraphEdge.ToNodeID = Edge.EndNodeID;
		GraphEdge.ApproxLengthCm = Edge.ApproxLengthCm;
		GraphEdge.LogicalRole = Edge.LogicalRole;
		GraphEdge.BranchIndex = Edge.BranchIndex;
		GraphEdge.bOptional = Edge.bIsOptionalSideContent;
		OutWindow.Edges.Add(GraphEdge);
	}

	const float RouteLengthCm = FMath::Max(1.f, CachedTunnelNavData->GenSpec.RouteLengthMeters * 100.f);
	for (const FTunnelNavNodeRecord& Node : CachedTunnelNavData->Nodes)
	{
		const float NodeRouteDistanceCm = Node.NormalizedDistance * RouteLengthCm;
		const float DistanceAheadCm = NodeRouteDistanceCm - Projection.RouteDistanceCm;
		if (DistanceAheadCm < 0.f)
		{
			continue;
		}

		if (OutWindow.DistanceToNextHubCm < 0.f && IsHubLikeNode(Node))
		{
			OutWindow.DistanceToNextHubCm = DistanceAheadCm;
		}
		if (OutWindow.DistanceToNextSplitCm < 0.f && IsSplitLikeNode(Node))
		{
			OutWindow.DistanceToNextSplitCm = DistanceAheadCm;
		}
		if (OutWindow.DistanceToNextMergeCm < 0.f && IsMergeLikeNode(Node))
		{
			OutWindow.DistanceToNextMergeCm = DistanceAheadCm;
		}
	}

	OutWindow.bValid = OutWindow.Nodes.Num() > 0;
	OutWindow.SpaceMode = Projection.SpaceMode;
	OutWindow.AnchorNodeID = AnchorNodeId;
	OutWindow.CurrentEdgeIndex = Projection.EdgeIndex;
	OutWindow.CurrentBranchIndex = CachedTunnelNavData->Samples.IsValidIndex(Projection.SampleIndex)
		? CachedTunnelNavData->Samples[Projection.SampleIndex].BranchIndex
		: INDEX_NONE;
	OutWindow.bMultipleExitsNearby = AnchorNode->NextNodeIDs.Num() > 1;
	return OutWindow.bValid;
}

void UTunnelNavigationRuntimeComponent::GetActiveRestrictionsForSubClass(
	const FTunnelNavProjectionResult& Projection,
	ETunnelNavSubClass SubClass,
	float LookaheadDistanceCm,
	TArray<FTunnelNavRestriction>& OutRestrictions) const
{
	FSubmarineNavigationProfile NavProfile;
	if (!ResolveNavigationProfile(SubClass, NavProfile))
	{
		OutRestrictions.Reset();
		return;
	}

	GetActiveRestrictionsForSubClass(Projection, NavProfile, LookaheadDistanceCm, OutRestrictions);
}

void UTunnelNavigationRuntimeComponent::GetActiveRestrictionsForSubClass(
	const FTunnelNavProjectionResult& Projection,
	const FSubmarineNavigationProfile& NavProfile,
	float LookaheadDistanceCm,
	TArray<FTunnelNavRestriction>& OutRestrictions) const
{
	OutRestrictions.Reset();
	if (!Projection.bProjected || !EnsureNavigationDataAvailable())
	{
		return;
	}

	FTunnelNavForwardProfile Profile;
	if (!GetForwardAnticipationProfile(Projection, NavProfile, LookaheadDistanceCm, Profile))
	{
		return;
	}

	for (const FTunnelNavAnticipationPoint& Point : Profile.Points)
	{
		const FTunnelNavSampleRecord* Sample = CachedTunnelNavData->Samples.IsValidIndex(Point.SampleIndex)
			? &CachedTunnelNavData->Samples[Point.SampleIndex]
			: nullptr;
		if (!Sample)
		{
			continue;
		}

		if (Point.bBelowRequiredClearance)
		{
			FTunnelNavRestriction Restriction;
			Restriction.RestrictionType = ETunnelNavRestrictionType::Clearance;
			Restriction.KnowledgeSource = ETunnelNavKnowledgeSource::StaticMapped;
			Restriction.SampleIndex = Point.SampleIndex;
			Restriction.RelatedEdgeIndex = Sample->SegmentIndex;
			Restriction.DistanceAheadCm = Point.DistanceAheadCm;
			Restriction.Severity01 = ComputeRestrictionSeverity(Point.MinClearanceCm, NavProfile.HardClearanceCm);
			Restriction.bHardBlock = Point.MinClearanceCm < NavProfile.HardClearanceCm;
			Restriction.bClassSpecific = true;
			Restriction.Message = FText::FromString(TEXT("Clearance below hard class envelope"));
			OutRestrictions.Add(Restriction);
		}

		if (!Sample->bBranchValidationPass)
		{
			FTunnelNavRestriction Restriction;
			Restriction.RestrictionType = ETunnelNavRestrictionType::BranchValidation;
			Restriction.KnowledgeSource = ETunnelNavKnowledgeSource::StaticMapped;
			Restriction.SampleIndex = Point.SampleIndex;
			Restriction.RelatedEdgeIndex = Sample->SegmentIndex;
			Restriction.DistanceAheadCm = Point.DistanceAheadCm;
			Restriction.Severity01 = 1.f;
			Restriction.bHardBlock = true;
			Restriction.bClassSpecific = false;
			Restriction.Message = FText::FromString(TEXT("Validation failed on branch segment"));
			OutRestrictions.Add(Restriction);
		}

		if (Point.bRuntimeObstacle)
		{
			FTunnelNavRestriction Restriction;
			Restriction.RestrictionType = ETunnelNavRestrictionType::RuntimeObstacle;
			Restriction.KnowledgeSource = ETunnelNavKnowledgeSource::RuntimeObserved;
			Restriction.SampleIndex = Point.SampleIndex;
			Restriction.RelatedEdgeIndex = Sample->SegmentIndex;
			Restriction.DistanceAheadCm = Point.DistanceAheadCm;
			Restriction.Severity01 = 1.f;
			Restriction.bHardBlock = true;
			Restriction.bClassSpecific = false;
			Restriction.Message = FText::FromString(TEXT("Runtime obstacle inside corridor envelope"));
			OutRestrictions.Add(Restriction);
		}

		if (Point.bNoTurnZone)
		{
			FTunnelNavRestriction Restriction;
			Restriction.RestrictionType = ETunnelNavRestrictionType::CommitmentNoTurn;
			Restriction.KnowledgeSource = ETunnelNavKnowledgeSource::StaticMapped;
			Restriction.SampleIndex = Point.SampleIndex;
			Restriction.RelatedEdgeIndex = Sample->SegmentIndex;
			Restriction.DistanceAheadCm = Point.DistanceAheadCm;
			Restriction.Severity01 = 0.8f;
			Restriction.bHardBlock = false;
			Restriction.bClassSpecific = true;
			Restriction.Message = FText::FromString(TEXT("No-turn commitment zone for active class"));
			OutRestrictions.Add(Restriction);
		}

		if (Point.bOptionalBranch)
		{
			FTunnelNavRestriction Restriction;
			Restriction.RestrictionType = ETunnelNavRestrictionType::OptionalBranchRisk;
			Restriction.KnowledgeSource = ETunnelNavKnowledgeSource::StaticMapped;
			Restriction.SampleIndex = Point.SampleIndex;
			Restriction.RelatedEdgeIndex = Sample->SegmentIndex;
			Restriction.DistanceAheadCm = Point.DistanceAheadCm;
			Restriction.Severity01 = 0.25f;
			Restriction.bHardBlock = false;
			Restriction.bClassSpecific = false;
			Restriction.Message = FText::FromString(TEXT("Optional branch in local lookahead"));
			OutRestrictions.Add(Restriction);
		}
	}

	FTunnelNavStoppingDistanceWarning StopWarning;
	if (GetStoppingDistanceWarning(Profile, NavProfile, FMath::Abs(ComputeForwardSpeedCmS()), StopWarning) && StopWarning.bWarning)
	{
		FTunnelNavRestriction Restriction;
		Restriction.RestrictionType = ETunnelNavRestrictionType::StoppingDistance;
		Restriction.KnowledgeSource = ETunnelNavKnowledgeSource::StaticMapped;
		Restriction.SampleIndex = INDEX_NONE;
		Restriction.RelatedEdgeIndex = Projection.EdgeIndex;
		Restriction.DistanceAheadCm = StopWarning.AvailableDistanceCm;
		Restriction.Severity01 = FMath::Clamp(-StopWarning.MarginCm / FMath::Max(1.f, StopWarning.RequiredStopDistanceCm), 0.f, 1.f);
		Restriction.bHardBlock = true;
		Restriction.bClassSpecific = true;
		Restriction.Message = FText::FromString(TEXT("Crash stop already exceeds safe remaining distance"));
		OutRestrictions.Add(Restriction);
	}

	FTunnelNavCommitmentWarning CommitmentWarning;
	if (GetCommitmentWarning(Projection, NavProfile, CommitmentWarning) && CommitmentWarning.bWarning)
	{
		FTunnelNavRestriction Restriction;
		Restriction.RestrictionType = ETunnelNavRestrictionType::CommitmentNoTurn;
		Restriction.KnowledgeSource = ETunnelNavKnowledgeSource::StaticMapped;
		Restriction.SampleIndex = INDEX_NONE;
		Restriction.RelatedEdgeIndex = Projection.EdgeIndex;
		Restriction.DistanceAheadCm = CommitmentWarning.DistanceToCommitmentCm;
		Restriction.Severity01 = 0.85f;
		Restriction.bHardBlock = false;
		Restriction.bClassSpecific = true;
		Restriction.Message = FText::FromString(TEXT("Commitment zone before next turnaround opportunity"));
		OutRestrictions.Add(Restriction);
	}
}

bool UTunnelNavigationRuntimeComponent::GetStoppingDistanceWarning(
	const FTunnelNavProjectionResult& Projection,
	ETunnelNavSubClass SubClass,
	FTunnelNavStoppingDistanceWarning& OutWarning) const
{
	FSubmarineNavigationProfile NavProfile;
	if (!ResolveNavigationProfile(SubClass, NavProfile))
	{
		OutWarning = FTunnelNavStoppingDistanceWarning();
		return false;
	}

	FTunnelNavForwardProfile Profile;
	if (!GetForwardAnticipationProfile(Projection, NavProfile, NavProfile.CommitmentLookaheadCm, Profile))
	{
		OutWarning = FTunnelNavStoppingDistanceWarning();
		return false;
	}

	return GetStoppingDistanceWarning(Profile, NavProfile, FMath::Abs(ComputeForwardSpeedCmS()), OutWarning);
}

bool UTunnelNavigationRuntimeComponent::GetStoppingDistanceWarning(
	const FTunnelNavForwardProfile& Profile,
	const FSubmarineNavigationProfile& NavProfile,
	float CurrentForwardSpeedCmS,
	FTunnelNavStoppingDistanceWarning& OutWarning) const
{
	OutWarning = FTunnelNavStoppingDistanceWarning();
	if (!Profile.bValid)
	{
		return false;
	}

	const float StopDistanceCm = ComputeStoppingDistanceCm(NavProfile, FMath::Abs(CurrentForwardSpeedCmS));
	const float AvailableDistanceCm = (Profile.FirstCriticalObstacleDistanceCm >= 0.f)
		? Profile.FirstCriticalObstacleDistanceCm
		: Profile.LookaheadDistanceCm;

	OutWarning.CurrentForwardSpeedCmS = FMath::Abs(CurrentForwardSpeedCmS);
	OutWarning.RequiredStopDistanceCm = StopDistanceCm;
	OutWarning.AvailableDistanceCm = AvailableDistanceCm;
	OutWarning.MarginCm = AvailableDistanceCm - StopDistanceCm;
	OutWarning.bWarning = (Profile.FirstCriticalObstacleDistanceCm >= 0.f) && (StopDistanceCm > AvailableDistanceCm);
	return true;
}

bool UTunnelNavigationRuntimeComponent::GetCommitmentWarning(
	const FTunnelNavProjectionResult& Projection,
	ETunnelNavSubClass SubClass,
	FTunnelNavCommitmentWarning& OutWarning) const
{
	FSubmarineNavigationProfile NavProfile;
	if (!ResolveNavigationProfile(SubClass, NavProfile))
	{
		OutWarning = FTunnelNavCommitmentWarning();
		return false;
	}

	return GetCommitmentWarning(Projection, NavProfile, OutWarning);
}

bool UTunnelNavigationRuntimeComponent::GetCommitmentWarning(
	const FTunnelNavProjectionResult& Projection,
	const FSubmarineNavigationProfile& NavProfile,
	FTunnelNavCommitmentWarning& OutWarning) const
{
	OutWarning = FTunnelNavCommitmentWarning();
	if (!Projection.bProjected || !EnsureNavigationDataAvailable())
	{
		return false;
	}

	FTunnelNavForwardProfile Profile;
	if (!GetForwardAnticipationProfile(Projection, NavProfile, NavProfile.CommitmentLookaheadCm, Profile))
	{
		return false;
	}

	float FirstNoTurnDistanceCm = -1.f;
	float MaxClearanceAheadCm = 0.f;
	bool bReachedHub = false;
	bool bNoTurnBeforeHub = false;

	for (const FTunnelNavAnticipationPoint& Point : Profile.Points)
	{
		MaxClearanceAheadCm = FMath::Max(MaxClearanceAheadCm, Point.MinClearanceCm);

		if (!bReachedHub && Point.SpaceMode == ETunnelNavSpaceMode::CavernHub)
		{
			bReachedHub = true;
		}
		if (!bReachedHub && Point.bNoTurnZone)
		{
			if (FirstNoTurnDistanceCm < 0.f)
			{
				FirstNoTurnDistanceCm = Point.DistanceAheadCm;
			}
			bNoTurnBeforeHub = true;
		}
	}

	OutWarning.DistanceToCommitmentCm = FirstNoTurnDistanceCm;
	OutWarning.MaxClearanceAheadCm = MaxClearanceAheadCm;
	OutWarning.RequiredTurnaroundCm = 0.5f * NavProfile.MinTurningBasinDiameterCm;
	OutWarning.bNoTurnaroundBeforeNextHub = bNoTurnBeforeHub;
	OutWarning.bWarning = FirstNoTurnDistanceCm >= 0.f && bNoTurnBeforeHub;
	return true;
}

FTunnelNavHeadingVsVelocityState UTunnelNavigationRuntimeComponent::GetHeadingVsVelocityState(ETunnelNavSubClass SubClass) const
{
	FTunnelNavHeadingVsVelocityState Result;

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return Result;
	}

	FSubmarineNavigationProfile NavProfile;
	if (!ResolveNavigationProfile(SubClass, NavProfile))
	{
		return Result;
	}

	GetHeadingVsVelocityState(OwnerActor->GetActorTransform(), OwnerActor->GetVelocity(), NavProfile, Result);
	return Result;
}

bool UTunnelNavigationRuntimeComponent::GetHeadingVsVelocityState(
	const FTransform& SubTransform,
	const FVector& LinearVelocity,
	const FSubmarineNavigationProfile& NavProfile,
	FTunnelNavHeadingVsVelocityState& OutState) const
{
	OutState = FTunnelNavHeadingVsVelocityState();

	const FVector Heading = SubTransform.GetRotation().GetForwardVector().GetSafeNormal();
	const FVector VelocityDir = LinearVelocity.IsNearlyZero() ? Heading : LinearVelocity.GetSafeNormal();
	const FVector Forward2D = FVector(Heading.X, Heading.Y, 0.f).GetSafeNormal();
	const FVector Velocity2D = FVector(LinearVelocity.X, LinearVelocity.Y, 0.f);
	const FVector VelocityDir2D = Velocity2D.IsNearlyZero() ? Forward2D : Velocity2D.GetSafeNormal();
	const FVector Right = SubTransform.GetRotation().GetRightVector();
	const FVector Up = SubTransform.GetRotation().GetUpVector();

	OutState.HeadingWorld = Heading;
	OutState.VelocityWorld = VelocityDir;
	OutState.HeadingYawDeg = Forward2D.IsNearlyZero() ? 0.f : Forward2D.Rotation().Yaw;
	OutState.VelocityYawDeg = VelocityDir2D.IsNearlyZero() ? OutState.HeadingYawDeg : VelocityDir2D.Rotation().Yaw;
	OutState.DriftAngleDeg = FMath::Abs(FMath::FindDeltaAngleDegrees(OutState.HeadingYawDeg, OutState.VelocityYawDeg));
	OutState.ForwardSpeedCmS = FVector::DotProduct(LinearVelocity, Heading);
	OutState.LateralSpeedCmS = FVector::DotProduct(LinearVelocity, Right) * NavProfile.LateralDriftScale;
	OutState.VerticalSpeedCmS = FVector::DotProduct(LinearVelocity, Up);
	OutState.ProjectedStopDistanceCm = ComputeStoppingDistanceCm(NavProfile, FMath::Abs(OutState.ForwardSpeedCmS));
	CollectProjectedGhostTransforms(SubTransform, LinearVelocity, OutState.ForwardSpeedCmS, OutState.ProjectedGhostTransforms);
	OutState.bDriftingSignificantly =
		(OutState.DriftAngleDeg >= NavProfile.DriftWarningAngleDeg)
		|| (FMath::Abs(OutState.LateralSpeedCmS) >= NavProfile.LateralSpeedWarningCmS);
	return true;
}

void UTunnelNavigationRuntimeComponent::AddRuntimeObservedObstacle(
	const FVector& WorldLocation,
	float RadiusCm,
	float LifetimeSeconds,
	FName ObstacleTag)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FRuntimeObservedObstacle Obstacle;
	Obstacle.WorldLocation = WorldLocation;
	Obstacle.RadiusCm = FMath::Max(10.f, RadiusCm);
	const float EffectiveLifetime = (LifetimeSeconds > 0.f) ? LifetimeSeconds : RuntimeObstacleDefaultLifetimeS;
	Obstacle.ExpireAtWorldTimeS = World->GetTimeSeconds() + FMath::Max(0.01f, EffectiveLifetime);
	Obstacle.ObstacleTag = ObstacleTag;
	RuntimeObservedObstacles.Add(Obstacle);
}

void UTunnelNavigationRuntimeComponent::ClearRuntimeObservedObstacles()
{
	RuntimeObservedObstacles.Reset();
}

void UTunnelNavigationRuntimeComponent::LogCurrentProjection() const
{
	FTunnelNavProjectionResult Projection;
	if (!ProjectSubmarineToRoute(Projection))
	{
		UE_LOG(LogTunnelNavRuntime, Warning, TEXT("LogCurrentProjection | projection unavailable"));
		return;
	}

	FTunnelNavHeadingVsVelocityState HeadingState = GetHeadingVsVelocityState(ActiveSubClass);
	UE_LOG(
		LogTunnelNavRuntime,
		Log,
		TEXT("Projection | Sample=%d Edge=%d Route=%.0fcm EdgeDist=%.0fcm OffsetR=%.0fcm OffsetU=%.0fcm Align=%.2f Mode=%d Guaranteed=%d Drift=%.1fdeg"),
		Projection.SampleIndex,
		Projection.EdgeIndex,
		Projection.RouteDistanceCm,
		Projection.DistanceAlongEdgeCm,
		Projection.LocalOffsetRightCm,
		Projection.LocalOffsetUpCm,
		Projection.AlignmentDot,
		static_cast<int32>(Projection.SpaceMode),
		Projection.bOnGuaranteedPath ? 1 : 0,
		HeadingState.DriftAngleDeg);
}

void UTunnelNavigationRuntimeComponent::LogCurrentRestrictions() const
{
	FTunnelNavProjectionResult Projection;
	if (!ProjectSubmarineToRoute(Projection))
	{
		UE_LOG(LogTunnelNavRuntime, Warning, TEXT("LogCurrentRestrictions | projection unavailable"));
		return;
	}

	TArray<FTunnelNavRestriction> Restrictions;
	GetActiveRestrictionsForSubClass(Projection, ActiveSubClass, DebugLookaheadCm, Restrictions);
	UE_LOG(LogTunnelNavRuntime, Log, TEXT("Restrictions | Count=%d Lookahead=%.0fcm"), Restrictions.Num(), DebugLookaheadCm);

	for (const FTunnelNavRestriction& Restriction : Restrictions)
	{
		UE_LOG(
			LogTunnelNavRuntime,
			Log,
			TEXT("Restriction | Type=%d Edge=%d Sample=%d Distance=%.0fcm Severity=%.2f Hard=%d ClassSpecific=%d Msg=%s"),
			static_cast<int32>(Restriction.RestrictionType),
			Restriction.RelatedEdgeIndex,
			Restriction.SampleIndex,
			Restriction.DistanceAheadCm,
			Restriction.Severity01,
			Restriction.bHardBlock ? 1 : 0,
			Restriction.bClassSpecific ? 1 : 0,
			*Restriction.Message.ToString());
	}
}

bool UTunnelNavigationRuntimeComponent::EnsureNavigationDataAvailable() const
{
	if ((!IsValid(CachedTunnelNavData) || CachedTunnelNavData->Samples.Num() == 0) && bAutoResolveRouteActor)
	{
		const_cast<UTunnelNavigationRuntimeComponent*>(this)->ResolveRouteActorFromWorld();
	}

	return IsValid(CachedTunnelNavData) && CachedTunnelNavData->Samples.Num() > 0;
}

void UTunnelNavigationRuntimeComponent::ResolveRouteActorFromWorld()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ATraversalRouteActor* FirstRouteActor = nullptr;
	ATraversalRouteActor* BestRouteActor = nullptr;
	UTunnelNavDataAsset* BestTunnelNavData = nullptr;

	for (TActorIterator<ATraversalRouteActor> It(World); It; ++It)
	{
		ATraversalRouteActor* CandidateRoute = *It;
		if (!FirstRouteActor)
		{
			FirstRouteActor = CandidateRoute;
		}

		if (!CandidateRoute)
		{
			continue;
		}

		UTunnelNavDataAsset* CandidateData = CandidateRoute->GetTunnelNavData();
		if (IsValid(CandidateData) && CandidateData->Samples.Num() > 0)
		{
			BestRouteActor = CandidateRoute;
			BestTunnelNavData = CandidateData;
			break;
		}
	}

	if (BestRouteActor)
	{
		CachedRouteActor = BestRouteActor;
		CachedTunnelNavData = BestTunnelNavData;
	}
	else if (IsValid(FirstRouteActor))
	{
		CachedRouteActor = FirstRouteActor;
		if (bAutoBindTunnelNavAssetFromRoute)
		{
			CachedTunnelNavData = FirstRouteActor->GetTunnelNavData();
		}
	}
	else
	{
		CachedRouteActor = nullptr;
		CachedTunnelNavData = nullptr;
	}

	if (bEnableDebugLogs)
	{
		const int32 SampleCount = IsValid(CachedTunnelNavData) ? CachedTunnelNavData->Samples.Num() : 0;
		UE_LOG(
			LogTunnelNavRuntime,
			Log,
			TEXT("[%s] ResolveRouteActorFromWorld | Route=%s | TunnelNav=%s | Samples=%d"),
			*GetName(),
			*GetNameSafe(CachedRouteActor),
			*GetNameSafe(CachedTunnelNavData),
			SampleCount);
	}
}

void UTunnelNavigationRuntimeComponent::CleanupExpiredRuntimeObstacles()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		RuntimeObservedObstacles.Reset();
		return;
	}

	const float Now = World->GetTimeSeconds();
	RuntimeObservedObstacles.RemoveAll([Now](const FRuntimeObservedObstacle& Obstacle)
	{
		return Obstacle.ExpireAtWorldTimeS <= Now;
	});
}

void UTunnelNavigationRuntimeComponent::DrawDebugOverlay() const
{
	UWorld* World = GetWorld();
	const AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor || !EnsureNavigationDataAvailable())
	{
		return;
	}

	FTunnelNavProjectionResult Projection;
	if (!ProjectSubmarineToRoute(Projection))
	{
		return;
	}

	FSubmarineNavigationProfile NavProfile;
	if (!ResolveNavigationProfile(ActiveSubClass, NavProfile))
	{
		return;
	}

	DrawDebugLine(World, OwnerActor->GetActorLocation(), Projection.ClosestPointWorld, FColor::Magenta, false, 0.f, 0, 1.1f);
	DrawDebugSphere(World, Projection.ClosestPointWorld, 65.f, 10, FColor::Yellow, false, 0.f, 0, 1.6f);

	FTunnelNavCrossSectionResult CrossSection;
	if (GetLocalCrossSection(Projection, NavProfile, CrossSection) && CrossSection.bValid)
	{
		const FVector Center = CrossSection.Center;
		DrawDebugLine(World, Center, Center + CrossSection.Right * CrossSection.ClearanceRightCm, FColor::Green, false, 0.f, 0, 1.3f);
		DrawDebugLine(World, Center, Center - CrossSection.Right * CrossSection.ClearanceLeftCm, FColor::Green, false, 0.f, 0, 1.3f);
		DrawDebugLine(World, Center, Center + CrossSection.Up * CrossSection.ClearanceUpCm, FColor::Cyan, false, 0.f, 0, 1.2f);
		DrawDebugLine(World, Center, Center - CrossSection.Up * CrossSection.ClearanceDownCm, FColor::Cyan, false, 0.f, 0, 1.2f);

		const FVector ProjectedSubPosition =
			Center
			+ CrossSection.Right * CrossSection.SubProjectedOffsetRightCm
			+ CrossSection.Up * CrossSection.SubProjectedOffsetUpCm;
		const FColor SubColor = CrossSection.bHardClearanceViolation ? FColor::Red : FColor::White;
		DrawDebugSphere(World, ProjectedSubPosition, 45.f, 10, SubColor, false, 0.f, 0, 1.5f);
	}

	FTunnelNavForwardProfile Profile;
	if (GetForwardAnticipationProfile(Projection, NavProfile, DebugLookaheadCm, Profile))
	{
		for (int32 Index = 1; Index < Profile.Points.Num(); ++Index)
		{
			const FTunnelNavSampleRecord& PrevSample = CachedTunnelNavData->Samples[Profile.Points[Index - 1].SampleIndex];
			const FTunnelNavSampleRecord& CurrSample = CachedTunnelNavData->Samples[Profile.Points[Index].SampleIndex];
			const bool bRestricted = Profile.Points[Index].bBelowRequiredClearance || Profile.Points[Index].bRuntimeObstacle;
			const FColor Color =
				(Profile.Points[Index].SpaceMode == ETunnelNavSpaceMode::CavernHub) ? FColor::Silver :
				(bRestricted ? FColor::Red : FColor::Blue);
			DrawDebugLine(World, PrevSample.WorldPosition, CurrSample.WorldPosition, Color, false, 0.f, 0, 1.8f);
		}

		if (Profile.FirstCriticalObstacleDistanceCm >= 0.f)
		{
			DrawDebugString(
				World,
				OwnerActor->GetActorLocation() + FVector(0.f, 0.f, 350.f),
				FString::Printf(TEXT("Stop %.0f / FirstCritical %.0f"), Profile.StoppingDistanceCm, Profile.FirstCriticalObstacleDistanceCm),
				nullptr,
				FColor::White,
				0.f,
				true);
		}
	}

	FTunnelNavHeadingVsVelocityState HeadingState;
	if (GetHeadingVsVelocityState(OwnerActor->GetActorTransform(), OwnerActor->GetVelocity(), NavProfile, HeadingState))
	{
		const FVector Origin = OwnerActor->GetActorLocation();
		DrawDebugLine(World, Origin, Origin + HeadingState.HeadingWorld * 1200.f, FColor::White, false, 0.f, 0, 1.6f);
		DrawDebugLine(World, Origin, Origin + HeadingState.VelocityWorld * 1200.f, FColor::Orange, false, 0.f, 0, 1.8f);

		for (const FTransform& GhostTransform : HeadingState.ProjectedGhostTransforms)
		{
			DrawDebugSphere(World, GhostTransform.GetLocation(), 35.f, 8, FColor::Purple, false, 0.f, 0, 1.f);
		}
	}

	for (const FRuntimeObservedObstacle& Obstacle : RuntimeObservedObstacles)
	{
		DrawDebugSphere(World, Obstacle.WorldLocation, Obstacle.RadiusCm, 12, FColor::Orange, false, 0.f, 0, 1.5f);
	}
}

const FSubmarineClassNavigationSpec* UTunnelNavigationRuntimeComponent::ResolveClassSpec(ETunnelNavSubClass SubClass) const
{
	if (ClassProfileAsset)
	{
		const FSubmarineClassNavigationSpec* Found = ClassProfileAsset->ClassSpecs.FindByPredicate([SubClass](const FSubmarineClassNavigationSpec& Spec)
		{
			return Spec.SubClass == SubClass;
		});
		if (Found)
		{
			return Found;
		}
	}

	return DefaultClassSpecs.FindByPredicate([SubClass](const FSubmarineClassNavigationSpec& Spec)
	{
		return Spec.SubClass == SubClass;
	});
}

bool UTunnelNavigationRuntimeComponent::ResolveNavigationProfile(ETunnelNavSubClass SubClass, FSubmarineNavigationProfile& OutProfile) const
{
	const FSubmarineClassNavigationSpec* ClassSpec = ResolveClassSpec(SubClass);
	if (!ClassSpec)
	{
		OutProfile = FSubmarineNavigationProfile();
		return false;
	}

	OutProfile = FSubmarineNavigationProfile();
	OutProfile.ClassId = SubClass;
	OutProfile.HardClearanceCm = ClassSpec->RequiredClearanceCm;
	OutProfile.PreferredClearanceCm = ClassSpec->RequiredClearanceCm * 1.2f;
	OutProfile.MinTurningBasinDiameterCm = ClassSpec->MinTurnaroundRadiusCm * 2.f;
	OutProfile.ServiceDecelerationCmS2 = ClassSpec->ServiceDecelerationCmS2;
	OutProfile.EmergencyDecelerationCmS2 = ClassSpec->EmergencyDecelerationCmS2;
	OutProfile.CommitmentLookaheadCm = ClassSpec->CommitmentLookaheadCm;
	OutProfile.DriftWarningAngleDeg = ClassSpec->DriftWarningAngleDeg;
	OutProfile.LateralSpeedWarningCmS = ClassSpec->LateralSpeedWarningCmS;

	switch (SubClass)
	{
	case ETunnelNavSubClass::ClassS:
		OutProfile.HullLengthCm = 1800.f;
		OutProfile.HullBeamCm = 320.f;
		OutProfile.HullHeightCm = 320.f;
		OutProfile.YawInertiaScale = 0.85f;
		OutProfile.LateralDriftScale = 0.9f;
		OutProfile.PitchResponseScale = 1.1f;
		OutProfile.bCanRollForClearance = true;
		break;
	case ETunnelNavSubClass::ClassL:
		OutProfile.HullLengthCm = 3600.f;
		OutProfile.HullBeamCm = 620.f;
		OutProfile.HullHeightCm = 620.f;
		OutProfile.YawInertiaScale = 1.2f;
		OutProfile.LateralDriftScale = 1.1f;
		OutProfile.PitchResponseScale = 0.9f;
		break;
	case ETunnelNavSubClass::ClassXL:
		OutProfile.HullLengthCm = 5200.f;
		OutProfile.HullBeamCm = 860.f;
		OutProfile.HullHeightCm = 860.f;
		OutProfile.YawInertiaScale = 1.35f;
		OutProfile.LateralDriftScale = 1.15f;
		OutProfile.PitchResponseScale = 0.8f;
		break;
	case ETunnelNavSubClass::ClassM:
	default:
		OutProfile.HullLengthCm = 2700.f;
		OutProfile.HullBeamCm = 450.f;
		OutProfile.HullHeightCm = 450.f;
		OutProfile.YawInertiaScale = 1.f;
		OutProfile.LateralDriftScale = 1.f;
		OutProfile.PitchResponseScale = 1.f;
		break;
	}

	return true;
}

bool UTunnelNavigationRuntimeComponent::FindProjectedSampleIndex(
	const FVector& WorldLocation,
	const FVector& ForwardHint,
	int32& OutSampleIndex,
	float* OutDistanceCm,
	float* OutAlignmentDot) const
{
	OutSampleIndex = INDEX_NONE;
	if (OutDistanceCm)
	{
		*OutDistanceCm = 0.f;
	}
	if (OutAlignmentDot)
	{
		*OutAlignmentDot = 0.f;
	}
	if (!EnsureNavigationDataAvailable())
	{
		return false;
	}

	float BestScore = TNumericLimits<float>::Max();
	float BestDistSq = TNumericLimits<float>::Max();
	float BestAlignment = 0.f;
	bool bFoundInsideSectionCandidate = false;
	const float MaxRadiusSq = ProjectionSearchRadiusCm * ProjectionSearchRadiusCm;
	const FVector SafeForwardHint = ForwardHint.GetSafeNormal();

	auto ConsiderSample = [&](int32 SampleIndex)
	{
		if (!CachedTunnelNavData->Samples.IsValidIndex(SampleIndex))
		{
			return;
		}

		const FTunnelNavSampleRecord& Sample = CachedTunnelNavData->Samples[SampleIndex];
		const float DistSq = FVector::DistSquared(Sample.WorldPosition, WorldLocation);
		if (DistSq > MaxRadiusSq)
		{
			return;
		}

		const FVector Delta = WorldLocation - Sample.WorldPosition;
		const float LocalRight = FVector::DotProduct(Delta, Sample.Right);
		const float LocalUp = FVector::DotProduct(Delta, Sample.Up);
		const float AllowedRight = (LocalRight >= 0.f) ? Sample.ClearanceRightCm : Sample.ClearanceLeftCm;
		const float AllowedUp = (LocalUp >= 0.f) ? Sample.ClearanceUpCm : Sample.ClearanceDownCm;
		const float RightOverflowCm = FMath::Max(0.f, FMath::Abs(LocalRight) - AllowedRight);
		const float UpOverflowCm = FMath::Max(0.f, FMath::Abs(LocalUp) - AllowedUp);
		const float SectionOverflowCm = FMath::Max(RightOverflowCm, UpOverflowCm);
		const bool bInsideSection = SectionOverflowCm <= 250.f;

		const float Alignment = SafeForwardHint.IsNearlyZero()
			? 0.f
			: FVector::DotProduct(SafeForwardHint, Sample.Forward.GetSafeNormal());

		float Penalty = 1.f + 0.2f * (1.f - FMath::Abs(Alignment));
		if (!bInsideSection)
		{
			const float OverflowPenalty = 1.f + (SectionOverflowCm / FMath::Max(100.f, Sample.MinCrossSectionClearanceCm));
			Penalty *= 5.f * OverflowPenalty;
		}

		if (bFoundInsideSectionCandidate && !bInsideSection)
		{
			Penalty *= 4.f;
		}

		const float Score = DistSq * Penalty;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestDistSq = DistSq;
			BestAlignment = Alignment;
			OutSampleIndex = SampleIndex;
			bFoundInsideSectionCandidate |= bInsideSection;
		}
	};

	TSet<int32> CandidateIndices;

	if (LastProjectedSampleIndex != INDEX_NONE)
	{
		const int32 SafeWindow = FMath::Clamp(ProjectionCacheSampleWindow, 1, 128);
		for (int32 SampleIndex = LastProjectedSampleIndex - SafeWindow; SampleIndex <= LastProjectedSampleIndex + SafeWindow; ++SampleIndex)
		{
			if (CachedTunnelNavData->Samples.IsValidIndex(SampleIndex))
			{
				CandidateIndices.Add(SampleIndex);
			}
		}
	}

	if (LastProjectedEdgeIndex != INDEX_NONE)
	{
		AppendEdgeSampleIndices(LastProjectedEdgeIndex, CandidateIndices);
		AppendNeighborEdgeSampleIndices(LastProjectedEdgeIndex, CandidateIndices);
	}

	for (int32 SampleIndex : CandidateIndices)
	{
		ConsiderSample(SampleIndex);
	}

	if (OutSampleIndex == INDEX_NONE)
	{
		for (int32 SampleIndex = 0; SampleIndex < CachedTunnelNavData->Samples.Num(); ++SampleIndex)
		{
			ConsiderSample(SampleIndex);
		}
	}

	if (OutSampleIndex == INDEX_NONE)
	{
		return false;
	}

	if (OutDistanceCm)
	{
		*OutDistanceCm = FMath::Sqrt(BestDistSq);
	}
	if (OutAlignmentDot)
	{
		*OutAlignmentDot = BestAlignment;
	}
	return true;
}

void UTunnelNavigationRuntimeComponent::AppendEdgeSampleIndices(int32 EdgeIndex, TSet<int32>& OutCandidateIndices) const
{
	if (!EnsureNavigationDataAvailable() || !CachedTunnelNavData->Edges.IsValidIndex(EdgeIndex))
	{
		return;
	}

	const FTunnelNavEdgeRecord& Edge = CachedTunnelNavData->Edges[EdgeIndex];
	for (int32 Offset = 0; Offset < Edge.SampleCount; ++Offset)
	{
		const int32 SampleIndex = Edge.FirstSampleIndex + Offset;
		if (CachedTunnelNavData->Samples.IsValidIndex(SampleIndex))
		{
			OutCandidateIndices.Add(SampleIndex);
		}
	}
}

void UTunnelNavigationRuntimeComponent::AppendNeighborEdgeSampleIndices(int32 EdgeIndex, TSet<int32>& OutCandidateIndices) const
{
	if (!EnsureNavigationDataAvailable() || !CachedTunnelNavData->Edges.IsValidIndex(EdgeIndex))
	{
		return;
	}

	const FTunnelNavEdgeRecord& Edge = CachedTunnelNavData->Edges[EdgeIndex];
	for (int32 CandidateEdgeIndex = 0; CandidateEdgeIndex < CachedTunnelNavData->Edges.Num(); ++CandidateEdgeIndex)
	{
		if (CandidateEdgeIndex == EdgeIndex)
		{
			continue;
		}

		const FTunnelNavEdgeRecord& CandidateEdge = CachedTunnelNavData->Edges[CandidateEdgeIndex];
		const bool bNeighbor =
			CandidateEdge.StartNodeID == Edge.StartNodeID
			|| CandidateEdge.StartNodeID == Edge.EndNodeID
			|| CandidateEdge.EndNodeID == Edge.StartNodeID
			|| CandidateEdge.EndNodeID == Edge.EndNodeID;
		if (bNeighbor)
		{
			AppendEdgeSampleIndices(CandidateEdgeIndex, OutCandidateIndices);
		}
	}
}

bool UTunnelNavigationRuntimeComponent::IsSampleOptionalForLookahead(const FTunnelNavSampleRecord& Sample) const
{
	return bIncludeOptionalBranchesInLookahead || !Sample.bIsOptionalSideContent;
}

bool UTunnelNavigationRuntimeComponent::IsSampleBlockedByRuntimeObstacle(const FTunnelNavSampleRecord& Sample, float RequiredClearanceCm) const
{
	const float NeededRadius = FMath::Max(0.f, RequiredClearanceCm);
	for (const FRuntimeObservedObstacle& Obstacle : RuntimeObservedObstacles)
	{
		const float Reach = Obstacle.RadiusCm + NeededRadius;
		if (FVector::DistSquared(Sample.WorldPosition, Obstacle.WorldLocation) <= Reach * Reach)
		{
			return true;
		}
	}
	return false;
}

float UTunnelNavigationRuntimeComponent::ComputeRestrictionSeverity(float MinClearanceCm, float RequiredClearanceCm) const
{
	if (RequiredClearanceCm <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	const float Ratio = FMath::Clamp(MinClearanceCm / RequiredClearanceCm, 0.f, 1.f);
	return 1.f - Ratio;
}

float UTunnelNavigationRuntimeComponent::ComputeCurvatureDegPer100m(int32 PreviousSampleIndex, int32 NextSampleIndex) const
{
	if (!EnsureNavigationDataAvailable()
		|| !CachedTunnelNavData->Samples.IsValidIndex(PreviousSampleIndex)
		|| !CachedTunnelNavData->Samples.IsValidIndex(NextSampleIndex)
		|| PreviousSampleIndex == NextSampleIndex)
	{
		return 0.f;
	}

	const FTunnelNavSampleRecord& Prev = CachedTunnelNavData->Samples[PreviousSampleIndex];
	const FTunnelNavSampleRecord& Next = CachedTunnelNavData->Samples[NextSampleIndex];
	const FVector PrevForward = Prev.Forward.GetSafeNormal();
	const FVector NextForward = Next.Forward.GetSafeNormal();
	const float DeltaAngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(PrevForward, NextForward), -1.f, 1.f)));
	const float DeltaDistanceCm = FMath::Max(1.f, FMath::Abs(Next.ApproxDistanceCm - Prev.ApproxDistanceCm));
	return DeltaAngleDeg * (10000.f / DeltaDistanceCm);
}

float UTunnelNavigationRuntimeComponent::ComputeGradeDegPer100m(int32 PreviousSampleIndex, int32 NextSampleIndex) const
{
	if (!EnsureNavigationDataAvailable()
		|| !CachedTunnelNavData->Samples.IsValidIndex(PreviousSampleIndex)
		|| !CachedTunnelNavData->Samples.IsValidIndex(NextSampleIndex)
		|| PreviousSampleIndex == NextSampleIndex)
	{
		return 0.f;
	}

	const FTunnelNavSampleRecord& Prev = CachedTunnelNavData->Samples[PreviousSampleIndex];
	const FTunnelNavSampleRecord& Next = CachedTunnelNavData->Samples[NextSampleIndex];
	const float DeltaDistanceCm = FMath::Max(1.f, FMath::Abs(Next.ApproxDistanceCm - Prev.ApproxDistanceCm));
	const float DeltaZCm = Next.WorldPosition.Z - Prev.WorldPosition.Z;
	const float GradeAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DeltaZCm, DeltaDistanceCm));
	return GradeAngleDeg * (10000.f / DeltaDistanceCm);
}

float UTunnelNavigationRuntimeComponent::ComputeDistanceToFirstRestrictionCm(
	const FTunnelNavProjectionResult& Projection,
	const FSubmarineNavigationProfile& NavProfile,
	float LookaheadDistanceCm) const
{
	FTunnelNavForwardProfile Profile;
	if (!GetForwardAnticipationProfile(Projection, NavProfile, LookaheadDistanceCm, Profile))
	{
		return -1.f;
	}

	return Profile.FirstCriticalObstacleDistanceCm;
}

float UTunnelNavigationRuntimeComponent::ComputeForwardSpeedCmS() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return 0.f;
	}

	const FVector Forward = OwnerActor->GetActorForwardVector();
	return FVector::DotProduct(OwnerActor->GetVelocity(), Forward);
}

float UTunnelNavigationRuntimeComponent::ComputeLateralSpeedCmS() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return 0.f;
	}

	const FVector Right = OwnerActor->GetActorRightVector();
	return FVector::DotProduct(OwnerActor->GetVelocity(), Right);
}

float UTunnelNavigationRuntimeComponent::ComputeVerticalSpeedCmS() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return 0.f;
	}

	const FVector Up = OwnerActor->GetActorUpVector();
	return FVector::DotProduct(OwnerActor->GetVelocity(), Up);
}

float UTunnelNavigationRuntimeComponent::ComputeStoppingDistanceCm(const FSubmarineNavigationProfile& NavProfile, float ForwardSpeedCmS) const
{
	const FRichCurve* StopCurve = NavProfile.CrashStopDistanceCurve.GetRichCurveConst();
	if (StopCurve && StopCurve->GetNumKeys() > 0)
	{
		return FMath::Max(0.f, StopCurve->Eval(FMath::Abs(ForwardSpeedCmS)));
	}

	const float SafeDecel = FMath::Max(10.f, NavProfile.ServiceDecelerationCmS2);
	return (ForwardSpeedCmS * ForwardSpeedCmS) / (2.f * SafeDecel);
}

float UTunnelNavigationRuntimeComponent::ComputeRecommendedMaxSpeedCmS(
	const FTunnelNavForwardProfile& Profile,
	const FSubmarineNavigationProfile& NavProfile,
	float CurrentForwardSpeedCmS) const
{
	const FRichCurve* SafeSpeedCurve = NavProfile.SafeSpeedByClearanceCurve.GetRichCurveConst();
	float RecommendedMaxSpeedCmS = 0.f;

	if (SafeSpeedCurve && SafeSpeedCurve->GetNumKeys() > 0)
	{
		RecommendedMaxSpeedCmS = FMath::Max(0.f, SafeSpeedCurve->Eval(Profile.MinClearanceAheadCm));
	}
	else
	{
		const float ClearanceRatio = Profile.MinClearanceAheadCm / FMath::Max(1.f, NavProfile.PreferredClearanceCm);
		RecommendedMaxSpeedCmS = 700.f * FMath::Clamp(ClearanceRatio, 0.5f, 3.f);
	}

	if (Profile.FirstCriticalObstacleDistanceCm >= 0.f)
	{
		const float BrakingLimitedSpeed = FMath::Sqrt(2.f * FMath::Max(10.f, NavProfile.ServiceDecelerationCmS2) * FMath::Max(Profile.FirstCriticalObstacleDistanceCm * 0.85f, 0.f));
		RecommendedMaxSpeedCmS = (RecommendedMaxSpeedCmS > 0.f)
			? FMath::Min(RecommendedMaxSpeedCmS, BrakingLimitedSpeed)
			: BrakingLimitedSpeed;
	}

	if (RecommendedMaxSpeedCmS <= 0.f)
	{
		RecommendedMaxSpeedCmS = FMath::Max(CurrentForwardSpeedCmS, 300.f);
	}

	return RecommendedMaxSpeedCmS / FMath::Max(0.5f, NavProfile.YawInertiaScale);
}

int32 UTunnelNavigationRuntimeComponent::ResolveAnchorNodeId(const FTunnelNavProjectionResult& Projection) const
{
	if (!EnsureNavigationDataAvailable() || !CachedTunnelNavData->Edges.IsValidIndex(Projection.EdgeIndex))
	{
		return INDEX_NONE;
	}

	const FTunnelNavEdgeRecord& Edge = CachedTunnelNavData->Edges[Projection.EdgeIndex];
	if (!CachedTunnelNavData->Samples.IsValidIndex(Projection.SampleIndex))
	{
		return Edge.StartNodeID;
	}

	const FTunnelNavSampleRecord& Sample = CachedTunnelNavData->Samples[Projection.SampleIndex];
	return (Sample.LocalT < 0.5f) ? Edge.StartNodeID : Edge.EndNodeID;
}

ETunnelNavSpaceMode UTunnelNavigationRuntimeComponent::DetermineSpaceMode(const FTunnelNavSampleRecord& Sample) const
{
	if (Sample.LogicalRole == ELogicalRouteNodeRole::Hub || Sample.CheckpointSpaceShape == ECheckpointSpaceShape::LargeCavity)
	{
		return ETunnelNavSpaceMode::CavernHub;
	}

	const float ClearanceRatio = Sample.MaxCrossSectionClearanceCm / FMath::Max(1.f, Sample.MinCrossSectionClearanceCm);
	const bool bWideVariance = ClearanceRatio >= 1.7f;
	const bool bPocketLike = Sample.CheckpointSpaceShape == ECheckpointSpaceShape::Pocket;
	const bool bVeryWide = Sample.MinCrossSectionClearanceCm >= Sample.SkeletonRadiusCm * 1.75f;
	if (bPocketLike || bWideVariance || bVeryWide)
	{
		return ETunnelNavSpaceMode::Transition;
	}

	return ETunnelNavSpaceMode::Corridor;
}

void UTunnelNavigationRuntimeComponent::UpdateProjectionCache(const FTunnelNavProjectionResult& Projection) const
{
	LastProjectedSampleIndex = Projection.SampleIndex;
	LastProjectedEdgeIndex = Projection.EdgeIndex;
}

void UTunnelNavigationRuntimeComponent::CollectProjectedGhostTransforms(
	const FTransform& SubTransform,
	const FVector& LinearVelocity,
	float ForwardSpeedCmS,
	TArray<FTransform>& OutGhosts) const
{
	OutGhosts.Reset();
	const FVector Heading = SubTransform.GetRotation().GetForwardVector().GetSafeNormal();
	const FVector GhostDirection = LinearVelocity.IsNearlyZero() ? Heading : LinearVelocity.GetSafeNormal();
	const float SpeedCmS = FMath::Max(FMath::Abs(ForwardSpeedCmS), LinearVelocity.Size());
	const float TimeSteps[] = { 0.5f, 1.f, 1.5f };

	for (float TimeStepS : TimeSteps)
	{
		const FVector GhostLocation = SubTransform.GetLocation() + GhostDirection * SpeedCmS * TimeStepS;
		OutGhosts.Add(FTransform(SubTransform.GetRotation(), GhostLocation, SubTransform.GetScale3D()));
	}
}
