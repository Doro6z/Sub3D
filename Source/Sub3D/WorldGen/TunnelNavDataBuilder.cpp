#include "TunnelNavDataBuilder.h"

#include "NavigableVolumeGenerator.h"
#include "SkeletonResolver.h"
#include "Math/RotationMatrix.h"
#include "Math/NumericLimits.h"

namespace
{
const FTraversalTopologyNode* FindNodeById(const TArray<FTraversalTopologyNode>& Nodes, int32 NodeID)
{
	for (const FTraversalTopologyNode& Node : Nodes)
	{
		if (Node.NodeID == NodeID)
		{
			return &Node;
		}
	}
	return nullptr;
}
}

void UTunnelNavDataBuilder::InitializeFromC5(
	UTunnelNavDataAsset* Asset,
	const FRouteGenSpec& Spec,
	const FRouteSeedCascade& Seeds,
	int32 BuildHash,
	const TArray<FTraversalTopologyNode>& Nodes,
	const TArray<FTraversalSkeletonSegment>& Skeleton,
	const FTunnelNavBuildSettings& Settings,
	const FTunnelNavEndpointSnapshot& Endpoints) const
{
	if (!IsValid(Asset))
	{
		return;
	}

	Asset->ResetData();
	Asset->BuildHash = BuildHash;
	Asset->GenSpec = Spec;
	Asset->Seeds = Seeds;
	Asset->BuildSettings = Settings;
	Asset->Endpoints = Endpoints;
	Asset->RequiredClearanceCm = Spec.Envelope.MinTurnRadius;

	Asset->Nodes.Reserve(Nodes.Num());
	for (const FTraversalTopologyNode& Node : Nodes)
	{
		FTunnelNavNodeRecord Record;
		Record.NodeID = Node.NodeID;
		Record.NodeType = Node.NodeType;
		Record.WorldPosition = Node.WorldPosition;
		Record.NormalizedDistance = Node.NormalizedDistance;
		Record.PreferredRadiusCm = Node.PreferredRadius;
		Record.CheckpointSpaceShape = Node.CheckpointSpaceShape;
		Record.BranchStage = Node.BranchStage;
		Record.BranchIndex = Node.BranchIndex;
		Record.BranchProfileID = Node.BranchProfileID;
		Record.BranchIntent = Node.BranchIntent;
		Record.LogicalDepth = Node.LogicalDepth;
		Record.LogicalRole = Node.LogicalRole;
		Record.SemanticTags = Node.SemanticTags;
		Record.NextNodeIDs = Node.NextNodeIDs;
		Record.bIsCanonicalPath = Node.bIsCanonicalPath;
		Record.bIsOptionalSideContent = Node.bIsOptionalSideContent;
		Record.bIsDecorativeDisconnected = Node.bIsDecorativeDisconnected;
		Asset->Nodes.Add(MoveTemp(Record));
	}

	Asset->Edges.Reserve(Skeleton.Num());
	const float RouteLengthCm = FMath::Max(0.f, Spec.RouteLengthMeters * 100.f);
	int32 NextSampleId = 0;

	for (int32 SegmentIndex = 0; SegmentIndex < Skeleton.Num(); ++SegmentIndex)
	{
		const FTraversalSkeletonSegment& Segment = Skeleton[SegmentIndex];
		const FTraversalTopologyNode* StartNode = FindNodeById(Nodes, Segment.StartNodeID);
		const FTraversalTopologyNode* EndNode = FindNodeById(Nodes, Segment.EndNodeID);
		const float StartNorm = StartNode ? StartNode->NormalizedDistance : 0.f;
		const float EndNorm = EndNode ? EndNode->NormalizedDistance : StartNorm;
		const float SegmentLengthCm = ApproximateSegmentLengthCm(Segment);
		const int32 SampleCount = FMath::Max(2, FMath::CeilToInt(SegmentLengthCm / FMath::Max(Settings.SampleSpacingCm, 100.f)) + 1);

		FTunnelNavEdgeRecord Edge;
		Edge.SegmentIndex = SegmentIndex;
		Edge.StartNodeID = Segment.StartNodeID;
		Edge.EndNodeID = Segment.EndNodeID;
		Edge.ApproxLengthCm = SegmentLengthCm;
		Edge.ApproxStartNormalizedDistance = StartNorm;
		Edge.ApproxEndNormalizedDistance = EndNorm;
		Edge.ApproxStartDistanceCm = StartNorm * RouteLengthCm;
		Edge.ApproxEndDistanceCm = EndNorm * RouteLengthCm;
		Edge.StartRadiusCm = Segment.StartRadius;
		Edge.EndRadiusCm = Segment.EndRadius;
		Edge.BranchStage = Segment.BranchStage;
		Edge.BranchIndex = Segment.BranchIndex;
		Edge.BranchProfileID = Segment.BranchProfileID;
		Edge.BranchIntent = Segment.BranchIntent;
		Edge.LogicalDepth = Segment.LogicalDepth;
		Edge.LogicalRole = Segment.LogicalRole;
		Edge.CheckpointSpaceShape = Segment.CheckpointSpaceShape;
		Edge.bGuaranteedTraversal = Segment.bGuaranteedPath;
		Edge.bIsOptionalSideContent = Segment.bIsOptionalSideContent;
		Edge.bIsDecorativeDisconnected = Segment.bIsDecorativeDisconnected;
		Edge.FirstSampleIndex = Asset->Samples.Num();
		Edge.SampleCount = SampleCount;

		for (int32 LocalSampleIndex = 0; LocalSampleIndex < SampleCount; ++LocalSampleIndex)
		{
			const float T = (SampleCount > 1)
				? (static_cast<float>(LocalSampleIndex) / static_cast<float>(SampleCount - 1))
				: 0.f;
			const FVector Position = USkeletonResolver::EvalBezier(Segment, T);
			const FVector Forward = USkeletonResolver::EvalBezierTangent(Segment, T).GetSafeNormal();
			FVector Right = FVector::RightVector;
			FVector Up = FVector::UpVector;
			BuildFrame(Forward, Right, Up);

			FTunnelNavSampleRecord Sample;
			Sample.SampleID = NextSampleId++;
			Sample.SegmentIndex = SegmentIndex;
			Sample.LocalSampleIndex = LocalSampleIndex;
			Sample.LocalT = T;
			Sample.DistanceAlongSegmentCm = SegmentLengthCm * T;
			Sample.ApproxNormalizedDistance = FMath::Lerp(StartNorm, EndNorm, T);
			Sample.ApproxDistanceCm = Sample.ApproxNormalizedDistance * RouteLengthCm;
			Sample.WorldPosition = Position;
			Sample.Forward = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward;
			Sample.Right = Right;
			Sample.Up = Up;
			Sample.SkeletonRadiusCm = USkeletonResolver::EvalRadius(Segment, T);
			Sample.BranchStage = Segment.BranchStage;
			Sample.BranchIndex = Segment.BranchIndex;
			Sample.BranchProfileID = Segment.BranchProfileID;
			Sample.BranchIntent = Segment.BranchIntent;
			Sample.LogicalDepth = Segment.LogicalDepth;
			Sample.LogicalRole = Segment.LogicalRole;
			Sample.CheckpointSpaceShape = Segment.CheckpointSpaceShape;
			Sample.bGuaranteedTraversal = Segment.bGuaranteedPath;
			Sample.bIsOptionalSideContent = Segment.bIsOptionalSideContent;
			Sample.bIsDecorativeDisconnected = Segment.bIsDecorativeDisconnected;
			Sample.RadialClearanceCm.Init(0.f, FMath::Max(4, Settings.RadialSampleCount));
			Asset->Samples.Add(MoveTemp(Sample));
		}

		Asset->Edges.Add(MoveTemp(Edge));
	}
}

void UTunnelNavDataBuilder::PopulateFromC6(
	UTunnelNavDataAsset* Asset,
	const FRouteFieldModel& Field) const
{
	if (!IsValid(Asset) || Asset->Samples.Num() == 0)
	{
		return;
	}

	const TArray<FVolumeBrushDef>& Brushes = Field.GuaranteedBrushes;
	const int32 RadialSampleCount = FMath::Max(4, Asset->BuildSettings.RadialSampleCount);
	const float ProbeMaxDistanceCm = FMath::Max(Asset->BuildSettings.CrossSectionProbeMaxCm, Asset->RequiredClearanceCm * 2.f);

	for (FTunnelNavSampleRecord& Sample : Asset->Samples)
	{
		Sample.MinCrossSectionClearanceCm = TNumericLimits<float>::Max();
		Sample.MaxCrossSectionClearanceCm = 0.f;
		Sample.RadialClearanceCm.SetNumZeroed(RadialSampleCount);

		for (int32 RadialIndex = 0; RadialIndex < RadialSampleCount; ++RadialIndex)
		{
			const float Angle = (2.f * PI * static_cast<float>(RadialIndex)) / static_cast<float>(RadialSampleCount);
			const FVector Direction = (Sample.Right * FMath::Cos(Angle) + Sample.Up * FMath::Sin(Angle)).GetSafeNormal();
			const float ClearanceCm = FindDirectionalClearanceCm(
				Sample.WorldPosition,
				Direction,
				Brushes,
				FMath::Max(ProbeMaxDistanceCm, Sample.SkeletonRadiusCm * 2.5f),
				Asset->BuildSettings.CoarseProbeSteps,
				Asset->BuildSettings.BinarySearchIterations);

			Sample.RadialClearanceCm[RadialIndex] = ClearanceCm;
			Sample.MinCrossSectionClearanceCm = FMath::Min(Sample.MinCrossSectionClearanceCm, ClearanceCm);
			Sample.MaxCrossSectionClearanceCm = FMath::Max(Sample.MaxCrossSectionClearanceCm, ClearanceCm);
		}

		if (Sample.MinCrossSectionClearanceCm == TNumericLimits<float>::Max())
		{
			Sample.MinCrossSectionClearanceCm = 0.f;
		}

		const int32 QuarterTurn = RadialSampleCount / 4;
		Sample.ClearanceRightCm = Sample.RadialClearanceCm.IsValidIndex(0) ? Sample.RadialClearanceCm[0] : 0.f;
		Sample.ClearanceUpCm = Sample.RadialClearanceCm.IsValidIndex(QuarterTurn) ? Sample.RadialClearanceCm[QuarterTurn] : 0.f;
		Sample.ClearanceLeftCm = Sample.RadialClearanceCm.IsValidIndex(QuarterTurn * 2) ? Sample.RadialClearanceCm[QuarterTurn * 2] : 0.f;
		Sample.ClearanceDownCm = Sample.RadialClearanceCm.IsValidIndex(QuarterTurn * 3) ? Sample.RadialClearanceCm[QuarterTurn * 3] : 0.f;
	}

	for (FTunnelNavEdgeRecord& Edge : Asset->Edges)
	{
		Edge.MinCrossSectionClearanceCm = 0.f;
		if (Edge.SampleCount <= 0)
		{
			continue;
		}

		float MinEdgeClearance = TNumericLimits<float>::Max();
		for (int32 Offset = 0; Offset < Edge.SampleCount; ++Offset)
		{
			const int32 SampleIndex = Edge.FirstSampleIndex + Offset;
			if (!Asset->Samples.IsValidIndex(SampleIndex))
			{
				continue;
			}
			MinEdgeClearance = FMath::Min(MinEdgeClearance, Asset->Samples[SampleIndex].MinCrossSectionClearanceCm);
		}

		Edge.MinCrossSectionClearanceCm = (MinEdgeClearance == TNumericLimits<float>::Max()) ? 0.f : MinEdgeClearance;
	}
}

void UTunnelNavDataBuilder::StampFromC9(
	UTunnelNavDataAsset* Asset,
	const FRouteGenSpec& Spec,
	const FRouteValidationReport& ValidationReport) const
{
	if (!IsValid(Asset))
	{
		return;
	}

	Asset->RequiredClearanceCm = Spec.Envelope.MinTurnRadius;
	Asset->bValidationPass = ValidationReport.bPass;
	Asset->ValidationReport = ValidationReport;

	for (FTunnelNavSampleRecord& Sample : Asset->Samples)
	{
		Sample.bBelowRequiredClearance = Sample.MinCrossSectionClearanceCm > 0.f
			&& Sample.MinCrossSectionClearanceCm < Asset->RequiredClearanceCm;
		Sample.bBranchValidationPass = ResolveBranchValidationPass(ValidationReport, Sample.BranchIndex);
	}

	for (FTunnelNavEdgeRecord& Edge : Asset->Edges)
	{
		Edge.bBelowRequiredClearance = false;
		bool bAnySample = false;
		float MinEdgeClearance = TNumericLimits<float>::Max();
		for (int32 Offset = 0; Offset < Edge.SampleCount; ++Offset)
		{
			const int32 SampleIndex = Edge.FirstSampleIndex + Offset;
			if (!Asset->Samples.IsValidIndex(SampleIndex))
			{
				continue;
			}

			bAnySample = true;
			const FTunnelNavSampleRecord& Sample = Asset->Samples[SampleIndex];
			Edge.bBelowRequiredClearance |= Sample.bBelowRequiredClearance;
			MinEdgeClearance = FMath::Min(MinEdgeClearance, Sample.MinCrossSectionClearanceCm);
		}

		Edge.MinCrossSectionClearanceCm = (bAnySample && MinEdgeClearance != TNumericLimits<float>::Max()) ? MinEdgeClearance : 0.f;
		Edge.bValidationPass = ResolveBranchValidationPass(ValidationReport, Edge.BranchIndex) && !Edge.bBelowRequiredClearance;
	}
}

float UTunnelNavDataBuilder::ApproximateSegmentLengthCm(const FTraversalSkeletonSegment& Segment) const
{
	float LengthCm = 0.f;
	FVector Previous = USkeletonResolver::EvalBezier(Segment, 0.f);
	constexpr int32 Steps = 12;
	for (int32 Step = 1; Step <= Steps; ++Step)
	{
		const float T = static_cast<float>(Step) / static_cast<float>(Steps);
		const FVector Current = USkeletonResolver::EvalBezier(Segment, T);
		LengthCm += FVector::Distance(Previous, Current);
		Previous = Current;
	}
	return LengthCm;
}

void UTunnelNavDataBuilder::BuildFrame(const FVector& Forward, FVector& OutRight, FVector& OutUp) const
{
	const FVector SafeForward = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward.GetSafeNormal();
	FVector UpHint = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(SafeForward, UpHint)) > 0.98f)
	{
		UpHint = FVector::RightVector;
	}

	const FMatrix Frame = FRotationMatrix::MakeFromXZ(SafeForward, UpHint);
	OutRight = Frame.GetUnitAxis(EAxis::Y).GetSafeNormal();
	OutUp = Frame.GetUnitAxis(EAxis::Z).GetSafeNormal();
}

float UTunnelNavDataBuilder::EvaluateDensityAtPoint(const FVector& Position, const TArray<FVolumeBrushDef>& Brushes) const
{
	if (Brushes.Num() == 0)
	{
		return -1.f;
	}

	float BestSdf = TNumericLimits<float>::Max();
	for (const FVolumeBrushDef& Brush : Brushes)
	{
		float Sdf = TNumericLimits<float>::Max();
		switch (Brush.BrushType)
		{
		case EVolumeBrushType::CapsuleCorridor:
			Sdf = UNavigableVolumeGenerator::CapsuleSDF(Position, Brush.CenterA, Brush.CenterB, Brush.Radius);
			break;
		case EVolumeBrushType::SpherePocket:
			Sdf = UNavigableVolumeGenerator::SphereSDF(Position, Brush.CenterA, Brush.Radius);
			break;
		case EVolumeBrushType::EllipsoidChamber:
		default:
			Sdf = UNavigableVolumeGenerator::SphereSDF(Position, Brush.CenterA, Brush.HalfExtents.GetMax());
			break;
		}

		BestSdf = UNavigableVolumeGenerator::SmoothMin(BestSdf, Sdf, Brush.Smoothness);
	}

	return -BestSdf;
}

float UTunnelNavDataBuilder::FindDirectionalClearanceCm(
	const FVector& Origin,
	const FVector& Direction,
	const TArray<FVolumeBrushDef>& Brushes,
	float MaxDistanceCm,
	int32 CoarseSteps,
	int32 BinaryIterations) const
{
	const FVector SafeDirection = Direction.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return 0.f;
	}

	const float OriginDensity = EvaluateDensityAtPoint(Origin, Brushes);
	if (OriginDensity <= 0.f)
	{
		return 0.f;
	}

	float LowDistance = 0.f;
	float HighDistance = FMath::Clamp(FMath::Max(OriginDensity, 100.f), 100.f, MaxDistanceCm);
	bool bFoundBoundary = false;

	for (int32 Step = 0; Step < FMath::Max(CoarseSteps, 4); ++Step)
	{
		const float DensityAtHigh = EvaluateDensityAtPoint(Origin + SafeDirection * HighDistance, Brushes);
		if (DensityAtHigh <= 0.f)
		{
			bFoundBoundary = true;
			break;
		}

		LowDistance = HighDistance;
		if (FMath::IsNearlyEqual(HighDistance, MaxDistanceCm))
		{
			return MaxDistanceCm;
		}
		HighDistance = FMath::Min(HighDistance * 1.5f, MaxDistanceCm);
	}

	if (!bFoundBoundary)
	{
		return HighDistance;
	}

	for (int32 Iteration = 0; Iteration < FMath::Max(BinaryIterations, 1); ++Iteration)
	{
		const float MidDistance = 0.5f * (LowDistance + HighDistance);
		const float DensityAtMid = EvaluateDensityAtPoint(Origin + SafeDirection * MidDistance, Brushes);
		if (DensityAtMid > 0.f)
		{
			LowDistance = MidDistance;
		}
		else
		{
			HighDistance = MidDistance;
		}
	}

	return HighDistance;
}

bool UTunnelNavDataBuilder::ResolveBranchValidationPass(const FRouteValidationReport& ValidationReport, int32 BranchIndex) const
{
	switch (BranchIndex)
	{
	case 0:
		return ValidationReport.bBranchA_Pass;
	case 1:
		return ValidationReport.bBranchB_Pass;
	case 2:
		return ValidationReport.bBranchC_Pass;
	default:
		return true;
	}
}
