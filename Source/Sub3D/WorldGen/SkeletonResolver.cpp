#include "SkeletonResolver.h"

namespace
{
void BuildSegmentFrame(const FVector& From, const FVector& To, FVector& OutAlong, FVector& OutSide, FVector& OutUp)
{
	const FVector Axis = To - From;
	const float AxisLength = Axis.Size();
	OutAlong = (AxisLength > KINDA_SMALL_NUMBER) ? Axis / AxisLength : FVector::ForwardVector;
	OutUp = FVector::UpVector;
	OutSide = FVector::CrossProduct(OutAlong, OutUp);
	if (OutSide.IsNearlyZero())
	{
		OutSide = FVector::RightVector;
	}
	OutSide = OutSide.GetSafeNormal();
	OutUp = FVector::CrossProduct(OutSide, OutAlong).GetSafeNormal();
}

float CurvatureScaleForNode(const FTraversalTopologyNode& Node)
{
	switch (Node.NodeType)
	{
	case ETopologyNodeType::StartCheckpointSpace:
	case ETopologyNodeType::StartAnchor:
	case ETopologyNodeType::EntryBuffer:
	case ETopologyNodeType::ExitApproach:
	case ETopologyNodeType::EndCheckpointSpace:
	case ETopologyNodeType::ExitAnchor:
		return 0.35f;
	case ETopologyNodeType::HubChamber:
	case ETopologyNodeType::MergeAnchor:
		return 0.6f;
	default:
		return 1.f;
	}
}
}

FVector USkeletonResolver::EvalBezier(const FTraversalSkeletonSegment& Seg, float T)
{
	const float U  = 1.f - T;
	const float U2 = U  * U;
	const float U3 = U2 * U;
	const float T2 = T  * T;
	const float T3 = T2 * T;
	return U3*Seg.P0 + 3.f*U2*T*Seg.P1 + 3.f*U*T2*Seg.P2 + T3*Seg.P3;
}

FVector USkeletonResolver::EvalBezierTangent(const FTraversalSkeletonSegment& Seg, float T)
{
	const float U  = 1.f - T;
	return 3.f*(U*U*(Seg.P1-Seg.P0) + 2.f*U*T*(Seg.P2-Seg.P1) + T*T*(Seg.P3-Seg.P2));
}

float USkeletonResolver::EvalRadius(const FTraversalSkeletonSegment& Seg, float T)
{
	const float ClampedT = FMath::Clamp(T, 0.f, 1.f);
	const float StartT = FMath::Clamp(Seg.RadiusTransitionStartT, 0.f, 1.f);
	const float EndT = FMath::Clamp(FMath::Max(Seg.RadiusTransitionEndT, StartT + KINDA_SMALL_NUMBER), 0.f, 1.f);

	if (ClampedT <= StartT)
	{
		return Seg.StartRadius;
	}
	if (ClampedT >= EndT)
	{
		return Seg.EndRadius;
	}

	const float LocalT = (ClampedT - StartT) / (EndT - StartT);
	const float SmoothT = LocalT * LocalT * (3.f - 2.f * LocalT);
	const float LerpRadius = FMath::Lerp(Seg.StartRadius, Seg.EndRadius, SmoothT);
	const float MinRadius = FMath::Min(Seg.StartRadius, Seg.EndRadius) * Seg.JunctionThroatScale;
	return FMath::Max(LerpRadius, MinRadius);
}

FTraversalSkeletonSegment USkeletonResolver::BuildSegment(const FTraversalTopologyNode& A,
                                                           const FTraversalTopologyNode& B) const
{
	FTraversalSkeletonSegment Seg;
	Seg.StartNodeID    = A.NodeID;
	Seg.EndNodeID      = B.NodeID;
	Seg.P0             = A.WorldPosition;
	Seg.P3             = B.WorldPosition;
	Seg.StartRadius    = A.PreferredRadius;
	Seg.EndRadius      = B.PreferredRadius;
	Seg.RadiusTransitionStartT = 0.f;
	Seg.RadiusTransitionEndT = 1.f;
	Seg.JunctionThroatScale = 1.f;
	Seg.BranchStage    = (A.BranchStage != 0) ? A.BranchStage : B.BranchStage;
	Seg.BranchIndex    = (A.BranchIndex != INDEX_NONE) ? A.BranchIndex : B.BranchIndex;
	Seg.BranchProfileID = !A.BranchProfileID.IsNone() ? A.BranchProfileID : B.BranchProfileID;
	Seg.BranchIntent = (A.BranchIntent != EBranchProfileIntent::CanonicalBypass || A.BranchProfileID != NAME_None)
		? A.BranchIntent
		: B.BranchIntent;
	Seg.LogicalDepth   = FMath::Max(A.LogicalDepth, B.LogicalDepth);
	Seg.LogicalRole    = (A.LogicalRole != ELogicalRouteNodeRole::MainSpine) ? A.LogicalRole : B.LogicalRole;
	Seg.CheckpointSpaceShape = (A.LogicalRole == ELogicalRouteNodeRole::StartCheckpoint || A.LogicalRole == ELogicalRouteNodeRole::EndCheckpoint)
		? A.CheckpointSpaceShape
		: B.CheckpointSpaceShape;
	Seg.bGuaranteedPath = A.bIsCanonicalPath && B.bIsCanonicalPath;
	Seg.bIsOptionalSideContent = A.bIsOptionalSideContent || B.bIsOptionalSideContent;
	Seg.bIsDecorativeDisconnected = A.bIsDecorativeDisconnected || B.bIsDecorativeDisconnected;

	const bool bEndsAtJunction = B.NodeType == ETopologyNodeType::HubChamber || B.NodeType == ETopologyNodeType::MergeAnchor || B.NodeType == ETopologyNodeType::SplitAnchor || B.NodeType == ETopologyNodeType::StartCheckpointSpace || B.NodeType == ETopologyNodeType::EndCheckpointSpace;
	const bool bStartsAtJunction = A.NodeType == ETopologyNodeType::HubChamber || A.NodeType == ETopologyNodeType::MergeAnchor || A.NodeType == ETopologyNodeType::SplitAnchor || A.NodeType == ETopologyNodeType::StartCheckpointSpace || A.NodeType == ETopologyNodeType::EndCheckpointSpace;
	if (bEndsAtJunction)
	{
		Seg.RadiusTransitionStartT = JunctionTransitionStartT;
		Seg.RadiusTransitionEndT = FMath::Clamp(JunctionTransitionStartT + JunctionTransitionSpanT, JunctionTransitionStartT + 0.01f, 1.f);
		Seg.JunctionThroatScale = JunctionThroatScale;
	}
	else if (bStartsAtJunction)
	{
		Seg.RadiusTransitionStartT = 0.f;
		Seg.RadiusTransitionEndT = FMath::Clamp(JunctionTransitionSpanT, 0.01f, 1.f);
		Seg.JunctionThroatScale = JunctionThroatScale;
	}

	// Control points: 1/3 of the chord length along the AB direction
	const FVector Dir    = (B.WorldPosition - A.WorldPosition);
	const float   Chord  = Dir.Size();
	const FVector DirN   = (Chord > KINDA_SMALL_NUMBER) ? Dir / Chord : FVector::ForwardVector;
	const float   Handle = Chord * 0.35f;
	const bool bUseCurvature = SegmentCurvatureCm > KINDA_SMALL_NUMBER || IntermediatePointJitterCm > KINDA_SMALL_NUMBER;
	if (!bUseCurvature || Chord <= KINDA_SMALL_NUMBER)
	{
		Seg.P1 = Seg.P0 + DirN * Handle;
		Seg.P2 = Seg.P3 - DirN * Handle;
		Seg.SupportPoint = FMath::Lerp(Seg.P0, Seg.P3, 0.5f);
		return Seg;
	}

	FVector Along;
	FVector Side;
	FVector Up;
	BuildSegmentFrame(Seg.P0, Seg.P3, Along, Side, Up);

	FRandomStream Rng(HashCombine(::GetTypeHash(A.NodeID), ::GetTypeHash(B.NodeID)));
	float RoleScale = FMath::Min(CurvatureScaleForNode(A), CurvatureScaleForNode(B));
	if (A.LogicalRole == ELogicalRouteNodeRole::Hub || B.LogicalRole == ELogicalRouteNodeRole::Hub)
	{
		RoleScale *= HubApproachCurvatureScale;
	}
	if (A.bIsOptionalSideContent || B.bIsOptionalSideContent)
	{
		RoleScale *= OptionalBranchCurvatureScale;
	}

	const float ChordScale = FMath::Clamp(Chord / 15000.f, 0.35f, 1.f);
	const float CurvatureMagnitude = SegmentCurvatureCm * RoleScale * ChordScale;
	const float JitterMagnitude = IntermediatePointJitterCm * RoleScale * ChordScale;
	const float SideOffset = Rng.FRandRange(-CurvatureMagnitude, CurvatureMagnitude);
	const float UpOffset = Rng.FRandRange(-JitterMagnitude, JitterMagnitude);
	const float SupportT = Rng.FRandRange(0.42f, 0.58f);
	Seg.SupportPoint = FMath::Lerp(Seg.P0, Seg.P3, SupportT) + Side * SideOffset + Up * UpOffset;
	Seg.P1 = FMath::Lerp(Seg.P0, Seg.SupportPoint, 0.55f);
	Seg.P2 = FMath::Lerp(Seg.P3, Seg.SupportPoint, 0.55f);

	return Seg;
}

bool USkeletonResolver::ResolveSkeleton(const TArray<FTraversalTopologyNode>& Nodes,
                                        TArray<FTraversalSkeletonSegment>& OutSegments) const
{
	OutSegments.Reset();
	if (Nodes.Num() < 2) return false;

	// Build a lookup: NodeID → Node
	TMap<int32, const FTraversalTopologyNode*> NodeMap;
	for (const FTraversalTopologyNode& N : Nodes)
		NodeMap.Add(N.NodeID, &N);

	// Walk all nodes and build a segment for each connection
	for (const FTraversalTopologyNode& NodeA : Nodes)
	{
		for (int32 NextID : NodeA.NextNodeIDs)
		{
			const FTraversalTopologyNode* NodeB = NodeMap.FindRef(NextID);
			if (NodeB)
				OutSegments.Add(BuildSegment(NodeA, *NodeB));
		}
	}

	return OutSegments.Num() > 0;
}
