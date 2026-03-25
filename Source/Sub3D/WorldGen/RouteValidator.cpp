#include "RouteValidator.h"
#include "NavigableVolumeGenerator.h"
#include "SkeletonResolver.h"

namespace
{
FIntVector WorldToSampleKey(const FVector& P, float VoxelSize)
{
	return FIntVector(
		FMath::RoundToInt(P.X / VoxelSize),
		FMath::RoundToInt(P.Y / VoxelSize),
		FMath::RoundToInt(P.Z / VoxelSize));
}

FIntVector ChunkSampleToGlobalKey(const FFieldChunkCoord& Coord, int32 SamplesPerAxis, int32 X, int32 Y, int32 Z)
{
	return FIntVector(
		Coord.X * SamplesPerAxis + X,
		Coord.Y * SamplesPerAxis + Y,
		Coord.Z * SamplesPerAxis + Z);
}

bool FindNearestWaterKey(const TSet<FIntVector>& WaterSamples, const FIntVector& SeedKey, int32 MaxRadius, FIntVector& OutKey)
{
	if (WaterSamples.Contains(SeedKey))
	{
		OutKey = SeedKey;
		return true;
	}

	for (int32 Radius = 1; Radius <= MaxRadius; Radius++)
	{
		for (int32 Z = -Radius; Z <= Radius; Z++)
		for (int32 Y = -Radius; Y <= Radius; Y++)
		for (int32 X = -Radius; X <= Radius; X++)
		{
			const FIntVector Candidate = SeedKey + FIntVector(X, Y, Z);
			if (WaterSamples.Contains(Candidate))
			{
				OutKey = Candidate;
				return true;
			}
		}
	}

	return false;
}

bool HasPathBetweenNodes(const TArray<FTraversalSkeletonSegment>& Skeleton, int32 StartNodeID, int32 EndNodeID, int32 BranchIndexFilter)
{
	TMap<int32, TArray<int32>> Adjacency;
	for (const FTraversalSkeletonSegment& Seg : Skeleton)
	{
		if (!Seg.bGuaranteedPath)
		{
			continue;
		}
		if (BranchIndexFilter != INDEX_NONE && Seg.BranchIndex != BranchIndexFilter)
		{
			continue;
		}
		Adjacency.FindOrAdd(Seg.StartNodeID).Add(Seg.EndNodeID);
	}

	TSet<int32> Visited;
	TArray<int32> Queue;
	Queue.Add(StartNodeID);
	Visited.Add(StartNodeID);

	for (int32 Head = 0; Head < Queue.Num(); Head++)
	{
		const int32 Current = Queue[Head];
		if (Current == EndNodeID)
		{
			return true;
		}

		const TArray<int32>* NextNodes = Adjacency.Find(Current);
		if (!NextNodes)
		{
			continue;
		}

		for (const int32 NextNodeID : *NextNodes)
		{
			if (Visited.Contains(NextNodeID))
			{
				continue;
			}
			Visited.Add(NextNodeID);
			Queue.Add(NextNodeID);
		}
	}

	return false;
}
}

float URouteValidator::SampleClearanceAtPoint(const FVector& P, const TArray<FVolumeBrushDef>& Brushes) const
{
	float BestSDF = 1e9f;
	for (const FVolumeBrushDef& Brush : Brushes)
	{
		float SDF = 0.f;
		switch (Brush.BrushType)
		{
		case EVolumeBrushType::CapsuleCorridor:
			SDF = UNavigableVolumeGenerator::CapsuleSDF(P, Brush.CenterA, Brush.CenterB, Brush.Radius);
			break;
		case EVolumeBrushType::SpherePocket:
		case EVolumeBrushType::EllipsoidChamber:
			SDF = UNavigableVolumeGenerator::SphereSDF(P, Brush.CenterA, Brush.Radius);
			break;
		}

		BestSDF = UNavigableVolumeGenerator::SmoothMin(BestSDF, SDF, Brush.Smoothness);
	}

	return -BestSDF;
}

float URouteValidator::MeasureMinClearance(const TArray<FTraversalSkeletonSegment>& Skeleton,
                                           const TArray<FVolumeBrushDef>& Brushes) const
{
	TArray<const FTraversalSkeletonSegment*> GuaranteedSegments;
	for (const FTraversalSkeletonSegment& Seg : Skeleton)
	{
		if (Seg.bGuaranteedPath)
		{
			GuaranteedSegments.Add(&Seg);
		}
	}

	return MeasureMinClearanceForSegments(GuaranteedSegments, Brushes);
}

float URouteValidator::MeasureMinClearanceForSegments(const TArray<const FTraversalSkeletonSegment*>& Segments,
                                                      const TArray<FVolumeBrushDef>& Brushes) const
{
	float MinClearance = 1e9f;

	for (const FTraversalSkeletonSegment* Seg : Segments)
	{
		if (!Seg)
		{
			continue;
		}

		const int32 Steps = 20;
		for (int32 Step = 0; Step <= Steps; Step++)
		{
			const float T = (float)Step / (float)Steps;
			const FVector P = USkeletonResolver::EvalBezier(*Seg, T);
			const float Clearance = SampleClearanceAtPoint(P, Brushes);
			MinClearance = FMath::Min(MinClearance, Clearance);
		}
	}

	return (MinClearance > 0.9e9f) ? 0.f : MinClearance;
}

bool URouteValidator::CheckSonarConnectivity(const FRouteFieldModel& Field,
                                             const FVector& StartWorld,
                                             const FVector& ExitWorld) const
{
	if (Field.SonarField.Num() == 0)
	{
		return false;
	}

	float SonarVoxelSize = 0.f;
	int32 SamplesPerAxis = 0;
	TSet<FIntVector> WaterSamples;

	for (const auto& Pair : Field.SonarField)
	{
		const FRouteFieldChunkData& Chunk = Pair.Value;
		if (SonarVoxelSize <= 0.f)
		{
			SonarVoxelSize = Chunk.VoxelSize;
			SamplesPerAxis = Chunk.SamplesPerAxis;
		}

		const int32 S = Chunk.SamplesPerAxis;
		const int32 S1 = S + 1;
		for (int32 Z = 0; Z <= S; Z++)
		for (int32 Y = 0; Y <= S; Y++)
		for (int32 X = 0; X <= S; X++)
		{
			const int32 Idx = Z * S1 * S1 + Y * S1 + X;
			if (Chunk.OccupancySamples.IsValidIndex(Idx) && Chunk.OccupancySamples[Idx] == 0)
			{
				WaterSamples.Add(ChunkSampleToGlobalKey(Pair.Key, S, X, Y, Z));
			}
		}
	}

	if (WaterSamples.Num() == 0 || SonarVoxelSize <= 0.f || SamplesPerAxis <= 0)
	{
		return false;
	}

	FIntVector StartKey;
	FIntVector ExitKey;
	if (!FindNearestWaterKey(WaterSamples, WorldToSampleKey(StartWorld, SonarVoxelSize), 3, StartKey))
	{
		return false;
	}
	if (!FindNearestWaterKey(WaterSamples, WorldToSampleKey(ExitWorld, SonarVoxelSize), 3, ExitKey))
	{
		return false;
	}

	TSet<FIntVector> Visited;
	TArray<FIntVector> Queue;
	Queue.Add(StartKey);
	Visited.Add(StartKey);

	for (int32 Head = 0; Head < Queue.Num(); Head++)
	{
		const FIntVector Current = Queue[Head];
		if (Current == ExitKey)
		{
			return true;
		}

		static const FIntVector Neighbors[6] =
		{
			FIntVector( 1, 0, 0), FIntVector(-1, 0, 0),
			FIntVector( 0, 1, 0), FIntVector( 0,-1, 0),
			FIntVector( 0, 0, 1), FIntVector( 0, 0,-1)
		};

		for (const FIntVector& Offset : Neighbors)
		{
			const FIntVector Next = Current + Offset;
			if (!WaterSamples.Contains(Next) || Visited.Contains(Next))
			{
				continue;
			}

			Visited.Add(Next);
			Queue.Add(Next);
		}
	}

	return false;
}

FRouteValidationReport URouteValidator::Validate(const FRouteGenSpec& Spec,
                                                 const TArray<FTraversalTopologyNode>& Nodes,
                                                 const TArray<FTraversalSkeletonSegment>& Skeleton,
                                                 const FRouteFieldModel& Field,
                                                 const FRouteSemanticModel& Semantic) const
{
	FRouteValidationReport Report;
	const float RequiredClearance = Spec.Envelope.MinTurnRadius;

	Report.MinObservedClearance = MeasureMinClearance(Skeleton, Field.GuaranteedBrushes);
	if (Report.MinObservedClearance < RequiredClearance)
	{
		Report.FailReasons.Add(FString::Printf(TEXT("Clearance %.0f cm < required %.0f cm"),
			Report.MinObservedClearance, RequiredClearance));
	}

	TMap<int32, int32> InDegree;
	TMap<int32, int32> OutDegree;
	TMap<int32, FVector> NodePositions;
	TMap<int32, TArray<const FTraversalSkeletonSegment*>> BranchSegmentsByIndex;
	TSet<FName> SelectedBranchProfileIDs;
	int32 GuaranteedSegments = 0;
	bool bUsesMultiStageBranches = false;
	bool bUsesConstrainedGraph = false;
	int32 DecorativeCavityCount = 0;
	int32 HubNodeCount = 0;
	int32 CanonicalBypassCount = 0;
	int32 OptionalPocketCount = 0;

	for (const FTraversalSkeletonSegment& Seg : Skeleton)
	{
		if (!Seg.bGuaranteedPath)
		{
			continue;
		}

		GuaranteedSegments++;
		OutDegree.FindOrAdd(Seg.StartNodeID)++;
		InDegree.FindOrAdd(Seg.EndNodeID)++;
		NodePositions.FindOrAdd(Seg.StartNodeID) = Seg.P0;
		NodePositions.FindOrAdd(Seg.EndNodeID) = Seg.P3;

		if (Seg.BranchIndex != INDEX_NONE)
		{
			BranchSegmentsByIndex.FindOrAdd(Seg.BranchIndex).Add(&Seg);
		}
		if (!Seg.BranchProfileID.IsNone())
		{
			SelectedBranchProfileIDs.Add(Seg.BranchProfileID);
		}
		if (Seg.BranchStage > 1)
		{
			bUsesMultiStageBranches = true;
		}
		if (Seg.bIsOptionalSideContent || Seg.LogicalRole != ELogicalRouteNodeRole::MainSpine)
		{
			bUsesConstrainedGraph = true;
		}
	}

	const FTraversalTopologyNode* CanonicalStartNode = nullptr;
	const FTraversalTopologyNode* CanonicalExitNode = nullptr;
	TSet<int32> OptionalBranchIndices;
	for (const FTraversalTopologyNode& Node : Nodes)
	{
		if (Node.bIsDecorativeDisconnected)
		{
			DecorativeCavityCount++;
			bUsesConstrainedGraph = true;
		}
		if (Node.LogicalRole == ELogicalRouteNodeRole::Hub && !Node.bIsDecorativeDisconnected)
		{
			HubNodeCount++;
			bUsesConstrainedGraph = true;
		}
		if (Node.bIsOptionalSideContent)
		{
			bUsesConstrainedGraph = true;
			if (Node.BranchIndex != INDEX_NONE)
			{
				OptionalBranchIndices.Add(Node.BranchIndex);
			}
		}
		if (!Node.BranchProfileID.IsNone())
		{
			SelectedBranchProfileIDs.Add(Node.BranchProfileID);
		}
		if (Node.LogicalRole == ELogicalRouteNodeRole::CanonicalBypass)
		{
			CanonicalBypassCount++;
		}
		if (Node.LogicalRole == ELogicalRouteNodeRole::PocketDeadEnd)
		{
			OptionalPocketCount++;
		}
		if (Node.bIsCanonicalPath && (Node.NodeType == ETopologyNodeType::StartAnchor || Node.NodeType == ETopologyNodeType::EntryBuffer))
		{
			if (!CanonicalStartNode || Node.NormalizedDistance < CanonicalStartNode->NormalizedDistance)
			{
				CanonicalStartNode = &Node;
			}
		}
		if (Node.bIsCanonicalPath && (Node.NodeType == ETopologyNodeType::ExitApproach || Node.NodeType == ETopologyNodeType::ExitAnchor))
		{
			if (!CanonicalExitNode || Node.NormalizedDistance > CanonicalExitNode->NormalizedDistance)
			{
				CanonicalExitNode = &Node;
			}
		}
	}

	if (GuaranteedSegments < 2)
	{
		Report.FailReasons.Add(TEXT("Fewer than 2 guaranteed skeleton segments"));
	}

	TArray<int32> StartNodes;
	TArray<int32> SplitNodes;
	TArray<int32> MergeNodes;
	TArray<int32> ExitNodes;
	for (const auto& NodePair : NodePositions)
	{
		const int32 NodeID = NodePair.Key;
		const int32 In = InDegree.FindRef(NodeID);
		const int32 Out = OutDegree.FindRef(NodeID);

		if (In == 0 && Out > 0)
		{
			StartNodes.Add(NodeID);
		}
		if (Out >= 2)
		{
			SplitNodes.Add(NodeID);
		}
		if (In >= 2)
		{
			MergeNodes.Add(NodeID);
		}
		if (Out == 0 && In > 0)
		{
			ExitNodes.Add(NodeID);
		}
	}

	const int32 ExpectedBranchCount = SplitNodes.Num() == 1 ? OutDegree.FindRef(SplitNodes[0]) : BranchSegmentsByIndex.Num();
	Report.SplitCount = SplitNodes.Num();
	Report.OptionalBranchCount = OptionalBranchIndices.Num();
	Report.DecorativeCavityCount = DecorativeCavityCount;
	Report.HubCount = HubNodeCount;
	Report.CanonicalBypassCount = CanonicalBypassCount;
	Report.OptionalPocketCount = OptionalPocketCount;
	Report.SelectedProfileCount = SelectedBranchProfileIDs.Num();

	if (bUsesConstrainedGraph && CanonicalStartNode && CanonicalExitNode)
	{
		Report.bMergeConnected = HasPathBetweenNodes(Skeleton, CanonicalStartNode->NodeID, CanonicalExitNode->NodeID, INDEX_NONE);
		if (!Report.bMergeConnected)
		{
			Report.FailReasons.Add(TEXT("Canonical main spine is not connected from start to exit"));
		}

		if (!CheckSonarConnectivity(Field, CanonicalStartNode->WorldPosition, CanonicalExitNode->WorldPosition))
		{
			Report.FailReasons.Add(TEXT("Sonar field connectivity check failed"));
		}

		Report.TraversableBranches = OptionalBranchIndices.Num();
		Report.MainPathCoverageRatio = Report.bMergeConnected ? 1.f : 0.f;
		Report.DeadEndCount = 0;
		Report.bPass = Report.FailReasons.Num() == 0;
		return Report;
	}

	if (bUsesMultiStageBranches)
	{
		if (StartNodes.Num() != 1)
		{
			Report.FailReasons.Add(TEXT("Expected exactly 1 start node in multi-stage topology"));
		}
		if (ExitNodes.Num() != 1)
		{
			Report.FailReasons.Add(TEXT("Expected exactly 1 exit node in multi-stage topology"));
		}
		if (SplitNodes.Num() < 2)
		{
			Report.FailReasons.Add(TEXT("Expected at least 2 split anchors in multi-stage topology"));
		}
		if (MergeNodes.Num() < 2)
		{
			Report.FailReasons.Add(TEXT("Expected at least 2 merge anchors in multi-stage topology"));
			Report.MergeFailures++;
		}

		Report.TraversableBranches = BranchSegmentsByIndex.Num();
		if (StartNodes.Num() == 1 && ExitNodes.Num() == 1)
		{
			Report.bMergeConnected = HasPathBetweenNodes(Skeleton, StartNodes[0], ExitNodes[0], INDEX_NONE);
			if (!Report.bMergeConnected)
			{
				Report.FailReasons.Add(TEXT("Multi-stage route is not connected from start to exit"));
				Report.MergeFailures++;
			}

			if (!CheckSonarConnectivity(Field, NodePositions[StartNodes[0]], NodePositions[ExitNodes[0]]))
			{
				Report.FailReasons.Add(TEXT("Sonar field connectivity check failed"));
			}
		}

		Report.MainPathCoverageRatio = BranchSegmentsByIndex.Num() > 0 ? 1.f : 0.f;
		Report.DeadEndCount = 0;
		Report.bPass = Report.FailReasons.Num() == 0;
		return Report;
	}

	const bool bLooksLikeSplitMerge = SplitNodes.Num() > 0 || MergeNodes.Num() > 0 || BranchSegmentsByIndex.Num() > 0;
	if (!bLooksLikeSplitMerge)
	{
		FVector StartWorld = FVector::ZeroVector;
		FVector ExitWorld = FVector::ZeroVector;
		int32 MinLinearNode = MAX_int32;
		int32 MaxLinearNode = MIN_int32;
		for (const FTraversalSkeletonSegment& Seg : Skeleton)
		{
			if (!Seg.bGuaranteedPath)
			{
				continue;
			}
			if (Seg.EndNodeID != Seg.StartNodeID + 1)
			{
				continue;
			}

			if (Seg.StartNodeID < MinLinearNode)
			{
				MinLinearNode = Seg.StartNodeID;
				StartWorld = Seg.P0;
			}
			if (Seg.EndNodeID > MaxLinearNode)
			{
				MaxLinearNode = Seg.EndNodeID;
				ExitWorld = Seg.P3;
			}
		}

		if (MinLinearNode == MAX_int32 || MaxLinearNode == MIN_int32)
		{
			Report.FailReasons.Add(TEXT("Unable to derive main-path start/exit for connectivity validation"));
		}
		else if (!CheckSonarConnectivity(Field, StartWorld, ExitWorld))
		{
			Report.FailReasons.Add(TEXT("Sonar field connectivity check failed"));
		}

		Report.MainPathCoverageRatio = (GuaranteedSegments > 0) ? 1.f : 0.f;
		Report.DeadEndCount = ExitNodes.Num() > 0 ? ExitNodes.Num() - 1 : 0;
		Report.bPass = Report.FailReasons.Num() == 0;
		return Report;
	}

	if (StartNodes.Num() != 1)
	{
		Report.FailReasons.Add(TEXT("Expected exactly 1 start node in split/merge topology"));
	}
	if (SplitNodes.Num() != 1)
	{
		Report.FailReasons.Add(TEXT("Expected exactly 1 split node in split/merge topology"));
	}
	if (MergeNodes.Num() != 1)
	{
		Report.FailReasons.Add(TEXT("Expected exactly 1 merge node in split/merge topology"));
		Report.MergeFailures++;
	}
	if (ExitNodes.Num() != 1)
	{
		Report.FailReasons.Add(TEXT("Expected exactly 1 exit node in split/merge topology"));
	}

	if (SplitNodes.Num() == 1 && MergeNodes.Num() == 1)
	{
		const int32 SplitNodeID = SplitNodes[0];
		const int32 MergeNodeID = MergeNodes[0];
		for (int32 BranchIndex = 0; BranchIndex < ExpectedBranchCount; BranchIndex++)
		{
			const TArray<const FTraversalSkeletonSegment*>* BranchSegments = BranchSegmentsByIndex.Find(BranchIndex);
			const int32 SegmentCount = BranchSegments ? BranchSegments->Num() : 0;
			const float BranchClearance = BranchSegments
				? MeasureMinClearanceForSegments(*BranchSegments, Field.GuaranteedBrushes)
				: 0.f;
			const bool bBranchPass = BranchSegments
				&& SegmentCount >= 2
				&& BranchClearance >= RequiredClearance
				&& HasPathBetweenNodes(Skeleton, SplitNodeID, MergeNodeID, BranchIndex);

			switch (BranchIndex)
			{
			case 0:
				Report.MinClearanceBranchA = BranchClearance;
				Report.bBranchA_Pass = bBranchPass;
				break;
			case 1:
				Report.MinClearanceBranchB = BranchClearance;
				Report.bBranchB_Pass = bBranchPass;
				break;
			case 2:
				Report.MinClearanceBranchC = BranchClearance;
				Report.bBranchC_Pass = bBranchPass;
				break;
			default:
				break;
			}

			if (SegmentCount < 2)
			{
				Report.FailReasons.Add(FString::Printf(TEXT("Branch %d does not contain enough guaranteed segments"), BranchIndex));
			}
			else if (!bBranchPass)
			{
				Report.FailReasons.Add(FString::Printf(TEXT("Branch %d failed traversability validation"), BranchIndex));
			}

			Report.TraversableBranches += bBranchPass ? 1 : 0;
		}

		if (ExitNodes.Num() == 1)
		{
			Report.bMergeConnected = HasPathBetweenNodes(Skeleton, MergeNodeID, ExitNodes[0], INDEX_NONE);
			if (!Report.bMergeConnected)
			{
				Report.FailReasons.Add(TEXT("Merge node is not connected to exit"));
				Report.MergeFailures++;
			}
		}

		if (StartNodes.Num() == 1 && ExitNodes.Num() == 1)
		{
			if (!CheckSonarConnectivity(Field, NodePositions[StartNodes[0]], NodePositions[ExitNodes[0]]))
			{
				Report.FailReasons.Add(TEXT("Sonar field connectivity check failed"));
			}
		}
	}

	Report.MainPathCoverageRatio = (ExpectedBranchCount > 0)
		? (float)Report.TraversableBranches / (float)ExpectedBranchCount
		: 0.f;
	Report.DeadEndCount = 0;
	Report.bPass = Report.FailReasons.Num() == 0;
	return Report;
}
