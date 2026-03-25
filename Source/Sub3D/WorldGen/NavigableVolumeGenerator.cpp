#include "NavigableVolumeGenerator.h"
#include "RouteArchetypeDataAsset.h"
#include "SkeletonResolver.h"
#include "WorldGenTypes.h"

namespace
{
FLinearColor DebugColorForRole(ELogicalRouteNodeRole LogicalRole, int32 BranchIndex)
{
	switch (LogicalRole)
	{
	case ELogicalRouteNodeRole::StartCheckpoint:
		return FLinearColor(0.3f, 0.85f, 1.f, 1.f);
	case ELogicalRouteNodeRole::EndCheckpoint:
		return FLinearColor(1.f, 1.f, 1.f, 1.f);
	case ELogicalRouteNodeRole::Hub:
		return FLinearColor(1.f, 0.85f, 0.15f, 1.f);
	case ELogicalRouteNodeRole::CanonicalBypass:
		switch (BranchIndex)
		{
		case 0: return FLinearColor(0.95f, 0.35f, 0.35f, 1.f);
		case 1: return FLinearColor(0.35f, 0.95f, 0.45f, 1.f);
		case 2: return FLinearColor(0.55f, 0.45f, 1.f, 1.f);
		default: return FLinearColor(0.95f, 0.55f, 0.35f, 1.f);
		}
	case ELogicalRouteNodeRole::OptionalSideBranch:
		return FLinearColor(1.f, 0.55f, 0.15f, 1.f);
	case ELogicalRouteNodeRole::PocketDeadEnd:
		return FLinearColor(1.f, 0.45f, 0.8f, 1.f);
	case ELogicalRouteNodeRole::DecorativeDisconnected:
		return FLinearColor(0.5f, 0.5f, 0.55f, 1.f);
	case ELogicalRouteNodeRole::MainSpine:
	default:
		return FLinearColor(0.2f, 0.7f, 1.f, 1.f);
	}
}

float CheckpointShapeRadiusScale(ECheckpointSpaceShape Shape)
{
	switch (Shape)
	{
	case ECheckpointSpaceShape::Goulot:
		return 0.85f;
	case ECheckpointSpaceShape::LargeCavity:
		return 1.35f;
	case ECheckpointSpaceShape::InterfaceOnly:
		return 1.0f;
	case ECheckpointSpaceShape::Pocket:
	default:
		return 1.0f;
	}
}

bool TryGetCheckpointDockBrush(
	const URouteArchetypeDataAsset* Archetype,
	const FTraversalTopologyNode& Node,
	const TArray<FTraversalSkeletonSegment>& Skeleton,
	FVolumeBrushDef& OutBrush)
{
	for (const FTraversalSkeletonSegment& Seg : Skeleton)
	{
		const bool bMatchesStart = Node.NodeType == ETopologyNodeType::StartCheckpointSpace && Seg.StartNodeID == Node.NodeID;
		const bool bMatchesEnd = Node.NodeType == ETopologyNodeType::EndCheckpointSpace && Seg.EndNodeID == Node.NodeID;
		if (!bMatchesStart && !bMatchesEnd)
		{
			continue;
		}

		const FVector InnerForward = bMatchesStart
			? USkeletonResolver::EvalBezier(Seg, 0.05f) - Node.WorldPosition
			: Node.WorldPosition - USkeletonResolver::EvalBezier(Seg, 0.95f);
		const FVector SafeInnerForward = InnerForward.IsNearlyZero()
			? (bMatchesStart ? FVector::ForwardVector : FVector::BackwardVector)
			: InnerForward.GetSafeNormal();
		const FVector DockForward = bMatchesStart ? -SafeInnerForward : SafeInnerForward;
		const float ThroatLength = Archetype
			? (bMatchesStart ? Archetype->Connections.StartDockLengthCm : Archetype->Connections.EndDockLengthCm)
			: 3800.f;
		const float RadiusScale = Archetype
			? (bMatchesStart ? Archetype->Connections.StartDockRadiusScale : Archetype->Connections.EndDockRadiusScale)
			: 0.78f;
		const float ThroatRadius = FMath::Max(1200.f, Node.PreferredRadius * RadiusScale);

		OutBrush.BrushType = EVolumeBrushType::CapsuleCorridor;
		OutBrush.CenterA = Node.WorldPosition;
		OutBrush.CenterB = Node.WorldPosition + DockForward * ThroatLength;
		OutBrush.Radius = ThroatRadius;
		OutBrush.Smoothness = 220.f;
		OutBrush.bGuaranteedTraversal = true;
		OutBrush.bDecorativeOnly = false;
		OutBrush.bAffectsRenderField = true;
		OutBrush.bAffectsSonarField = true;
		OutBrush.LogicalRole = Node.LogicalRole;
		OutBrush.BranchIndex = Node.BranchIndex;
		OutBrush.DebugColor = DebugColorForRole(Node.LogicalRole, Node.BranchIndex);
		return true;
	}

	return false;
}
}

float UNavigableVolumeGenerator::SmoothMin(float A, float B, float K)
{
	if (K < KINDA_SMALL_NUMBER) return FMath::Min(A, B);
	const float H = FMath::Clamp(0.5f + 0.5f * (B - A) / K, 0.f, 1.f);
	return FMath::Lerp(B, A, H) - K * H * (1.f - H);
}

float UNavigableVolumeGenerator::CapsuleSDF(const FVector& P, const FVector& A, const FVector& B, float Radius)
{
	const FVector AB = B - A;
	const float   Len2 = AB.SizeSquared();
	const float   T   = (Len2 > KINDA_SMALL_NUMBER)
	                    ? FMath::Clamp(FVector::DotProduct(P - A, AB) / Len2, 0.f, 1.f)
	                    : 0.f;
	return (P - (A + T * AB)).Size() - Radius;
}

float UNavigableVolumeGenerator::SphereSDF(const FVector& P, const FVector& Center, float Radius)
{
	return (P - Center).Size() - Radius;
}

float UNavigableVolumeGenerator::EvalGuaranteedDensity(const FVector& P, const TArray<FVolumeBrushDef>& Brushes)
{
	// density = -SDF_min_smooth over all brushes
	// density >= 0 → water (navigable) ; density < 0 → rock
	if (Brushes.Num() == 0) return -1.f; // all rock

	float BestSDF = 1e9f; // start far outside

	for (const FVolumeBrushDef& Brush : Brushes)
	{
		float SDF = 1e9f;
		switch (Brush.BrushType)
		{
		case EVolumeBrushType::CapsuleCorridor:
			SDF = CapsuleSDF(P, Brush.CenterA, Brush.CenterB, Brush.Radius);
			break;
		case EVolumeBrushType::SpherePocket:
			SDF = SphereSDF(P, Brush.CenterA, Brush.Radius);
			break;
		case EVolumeBrushType::EllipsoidChamber:
			// Approximate ellipsoid as sphere scaled by average half extent
			SDF = SphereSDF(P, Brush.CenterA, Brush.HalfExtents.GetMax());
			break;
		}
		BestSDF = SmoothMin(BestSDF, SDF, Brush.Smoothness);
	}

	return -BestSDF; // negate: inside brush (SDF < 0) → density > 0 = water
}

void UNavigableVolumeGenerator::GenerateBrushes(const URouteArchetypeDataAsset* Archetype,
                                                const TArray<FTraversalTopologyNode>& Nodes,
                                                const TArray<FTraversalSkeletonSegment>& Skeleton,
                                                TArray<FVolumeBrushDef>& OutGuaranteedBrushes,
                                                TArray<FVolumeBrushDef>& OutRenderOnlyBrushes) const
{
	OutGuaranteedBrushes.Reset();
	OutRenderOnlyBrushes.Reset();
	TMap<int32, int32> IncomingDegree;
	TMap<int32, int32> OutgoingDegree;
	TMap<int32, FVector> NodePositions;
	TMap<int32, float> NodeRadii;

	for (const FTraversalSkeletonSegment& Seg : Skeleton)
	{
		OutgoingDegree.FindOrAdd(Seg.StartNodeID)++;
		IncomingDegree.FindOrAdd(Seg.EndNodeID)++;
		NodePositions.FindOrAdd(Seg.StartNodeID) = Seg.P0;
		NodePositions.FindOrAdd(Seg.EndNodeID) = Seg.P3;
		NodeRadii.FindOrAdd(Seg.StartNodeID) = FMath::Max(NodeRadii.FindRef(Seg.StartNodeID), Seg.StartRadius);
		NodeRadii.FindOrAdd(Seg.EndNodeID) = FMath::Max(NodeRadii.FindRef(Seg.EndNodeID), Seg.EndRadius);

		// Sample the Bezier at intervals to approximate with capsules
		const int32 Steps = 8;
		FVector PrevPos   = USkeletonResolver::EvalBezier(Seg, 0.f);
		float   PrevRadius = USkeletonResolver::EvalRadius(Seg, 0.f);

		for (int32 s = 1; s <= Steps; s++)
		{
			const float    T      = (float)s / (float)Steps;
			const FVector  CurPos = USkeletonResolver::EvalBezier(Seg, T);
			const float    CurR   = USkeletonResolver::EvalRadius(Seg, T);
			const float    MaxR   = FMath::Max(PrevRadius, CurR);

			FVolumeBrushDef Brush;
			Brush.BrushType  = EVolumeBrushType::CapsuleCorridor;
			Brush.CenterA    = PrevPos;
			Brush.CenterB    = CurPos;
			Brush.Radius     = MaxR;
			Brush.Smoothness = 200.f; // SDF blend radius. Gradient normals handle visual smoothness — keep this tight.
			Brush.bGuaranteedTraversal = Seg.bGuaranteedPath;
			Brush.bDecorativeOnly = Seg.bIsDecorativeDisconnected;
			Brush.bAffectsRenderField = true;
			Brush.bAffectsSonarField = !Seg.bIsDecorativeDisconnected;
			Brush.BranchIndex = Seg.BranchIndex;
			Brush.LogicalRole = Seg.LogicalRole;
			Brush.DebugColor = DebugColorForRole(Seg.LogicalRole, Seg.BranchIndex);

			if (Seg.bGuaranteedPath)
			{
				OutGuaranteedBrushes.Add(Brush);
			}
			else
			{
				OutRenderOnlyBrushes.Add(Brush);
			}

			PrevPos    = CurPos;
			PrevRadius = CurR;
		}
	}

	for (const auto& NodePair : NodePositions)
	{
		const int32 NodeID = NodePair.Key;
		const int32 InDegree = IncomingDegree.FindRef(NodeID);
		const int32 OutDegree = OutgoingDegree.FindRef(NodeID);
		const bool bIsSplitOrMerge = InDegree + OutDegree > 2 || InDegree > 1 || OutDegree > 1;
		if (!bIsSplitOrMerge)
		{
			continue;
		}

		FVolumeBrushDef JunctionBrush;
		JunctionBrush.BrushType = EVolumeBrushType::SpherePocket;
		JunctionBrush.CenterA = NodePair.Value;
		JunctionBrush.CenterB = NodePair.Value;
		JunctionBrush.Radius = NodeRadii.FindRef(NodeID) * 1.1f;
		JunctionBrush.Smoothness = 260.f;
		JunctionBrush.bGuaranteedTraversal = true;
		JunctionBrush.LogicalRole = ELogicalRouteNodeRole::Hub;
		JunctionBrush.DebugColor = DebugColorForRole(ELogicalRouteNodeRole::Hub, INDEX_NONE);
		OutGuaranteedBrushes.Add(JunctionBrush);
	}

	for (const FTraversalTopologyNode& Node : Nodes)
	{
		if (Node.NodeType == ETopologyNodeType::StartCheckpointSpace || Node.NodeType == ETopologyNodeType::EndCheckpointSpace)
		{
			if (Node.CheckpointSpaceShape != ECheckpointSpaceShape::InterfaceOnly)
			{
				FVolumeBrushDef CheckpointBrush;
				CheckpointBrush.BrushType = EVolumeBrushType::SpherePocket;
				CheckpointBrush.CenterA = Node.WorldPosition;
				CheckpointBrush.CenterB = Node.WorldPosition;
				CheckpointBrush.Radius = Node.PreferredRadius * CheckpointShapeRadiusScale(Node.CheckpointSpaceShape);
				CheckpointBrush.Smoothness = 260.f;
				CheckpointBrush.bGuaranteedTraversal = true;
				CheckpointBrush.bDecorativeOnly = false;
				CheckpointBrush.bAffectsRenderField = true;
				CheckpointBrush.bAffectsSonarField = true;
				CheckpointBrush.LogicalRole = Node.LogicalRole;
				CheckpointBrush.BranchIndex = Node.BranchIndex;
				CheckpointBrush.DebugColor = DebugColorForRole(Node.LogicalRole, Node.BranchIndex);
				OutGuaranteedBrushes.Add(CheckpointBrush);
			}

			FVolumeBrushDef DockBrush;
			if (TryGetCheckpointDockBrush(Archetype, Node, Skeleton, DockBrush))
			{
				OutGuaranteedBrushes.Add(DockBrush);
			}
			continue;
		}

		if (!Node.bIsCanonicalPath && Node.NodeType == ETopologyNodeType::HubChamber)
		{
			FVolumeBrushDef HubBrush;
			HubBrush.BrushType = EVolumeBrushType::SpherePocket;
			HubBrush.CenterA = Node.WorldPosition;
			HubBrush.CenterB = Node.WorldPosition;
			HubBrush.Radius = Node.PreferredRadius;
			HubBrush.Smoothness = 260.f;
			HubBrush.bGuaranteedTraversal = false;
			HubBrush.bDecorativeOnly = false;
			HubBrush.bAffectsRenderField = true;
			HubBrush.bAffectsSonarField = true;
			HubBrush.LogicalRole = ELogicalRouteNodeRole::Hub;
			HubBrush.BranchIndex = Node.BranchIndex;
			HubBrush.DebugColor = DebugColorForRole(ELogicalRouteNodeRole::Hub, Node.BranchIndex);
			OutRenderOnlyBrushes.Add(HubBrush);
		}

		if (Node.bIsOptionalSideContent && (Node.NodeType == ETopologyNodeType::ResourcePocket || Node.NodeType == ETopologyNodeType::WreckPocket))
		{
			FVolumeBrushDef PocketBrush;
			PocketBrush.BrushType = EVolumeBrushType::SpherePocket;
			PocketBrush.CenterA = Node.WorldPosition;
			PocketBrush.CenterB = Node.WorldPosition;
			PocketBrush.Radius = Node.PreferredRadius;
			PocketBrush.Smoothness = 220.f;
			PocketBrush.bGuaranteedTraversal = false;
			PocketBrush.bDecorativeOnly = false;
			PocketBrush.bAffectsRenderField = true;
			PocketBrush.bAffectsSonarField = true;
			PocketBrush.LogicalRole = Node.LogicalRole;
			PocketBrush.BranchIndex = Node.BranchIndex;
			PocketBrush.DebugColor = DebugColorForRole(Node.LogicalRole, Node.BranchIndex);
			OutRenderOnlyBrushes.Add(PocketBrush);
		}
	}
}

FBox UNavigableVolumeGenerator::ComputeSkeletonBounds(const TArray<FTraversalSkeletonSegment>& Skeleton, float Padding)
{
	FBox Bounds(EForceInit::ForceInit);
	for (const FTraversalSkeletonSegment& Seg : Skeleton)
	{
		const float MaxR = FMath::Max(Seg.StartRadius, Seg.EndRadius) + Padding;
		Bounds += Seg.P0; Bounds += Seg.P1; Bounds += Seg.P2; Bounds += Seg.P3;
		Bounds = Bounds.ExpandBy(MaxR);
	}
	return Bounds;
}

void UNavigableVolumeGenerator::RasterizeField(const TArray<FVolumeBrushDef>& Brushes,
                                                const FBox& WorldBounds, float VoxelSize,
                                                int32 SamplesPerChunkAxis,
                                                TMap<FFieldChunkCoord, FRouteFieldChunkData>& OutField) const
{
	const float ChunkSize = VoxelSize * (float)SamplesPerChunkAxis;
	const int32 S = SamplesPerChunkAxis;

	// Sparse: collect only the chunk coords touched by at least one brush.
	// Avoids rasterizing the vast empty volume of the full bounding box.
	TSet<FFieldChunkCoord> ActiveCoords;
	for (const FVolumeBrushDef& Brush : Brushes)
	{
		// AABB of this brush + smoothness padding.
		// 6× smoothness ensures the smooth-union contribution at the border of the AABB
		// is negligible (<0.25% of amplitude), preventing brush-list divergence between adjacent chunks.
		const float Pad = Brush.Radius + Brush.Smoothness * 6.f;
		FBox BrushBox(EForceInit::ForceInit);
		BrushBox += Brush.CenterA;
		BrushBox += Brush.CenterB;
		BrushBox = BrushBox.ExpandBy(Pad);

		const int32 cx0 = FMath::FloorToInt(BrushBox.Min.X / ChunkSize);
		const int32 cy0 = FMath::FloorToInt(BrushBox.Min.Y / ChunkSize);
		const int32 cz0 = FMath::FloorToInt(BrushBox.Min.Z / ChunkSize);
		const int32 cx1 = FMath::CeilToInt (BrushBox.Max.X / ChunkSize);
		const int32 cy1 = FMath::CeilToInt (BrushBox.Max.Y / ChunkSize);
		const int32 cz1 = FMath::CeilToInt (BrushBox.Max.Z / ChunkSize);

		for (int32 cx = cx0; cx <= cx1; cx++)
		for (int32 cy = cy0; cy <= cy1; cy++)
		for (int32 cz = cz0; cz <= cz1; cz++)
		{
			FFieldChunkCoord Coord; Coord.X = cx; Coord.Y = cy; Coord.Z = cz;
			ActiveCoords.Add(Coord);
		}
	}

	for (const FFieldChunkCoord& Coord : ActiveCoords)
	{
		FRouteFieldChunkData& Chunk = OutField.Add(Coord);

		Chunk.ChunkOrigin    = FVector(Coord.X, Coord.Y, Coord.Z) * ChunkSize;
		Chunk.VoxelSize      = VoxelSize;
		Chunk.SamplesPerAxis = S;

		// P1 seam fix: store (S+1)³ samples — the extra border row matches sample[0]
		// of the adjacent chunk (same world position → same SDF → seamless isosurface).
		const int32 S1 = S + 1;
		Chunk.DensitySamples.SetNumUninitialized(S1 * S1 * S1);
		Chunk.OccupancySamples.SetNumZeroed(S1 * S1 * S1);

		// P2 brush culling: only evaluate brushes that touch this chunk's AABB.
		// Pad must match the activation pad above (6× smoothness) so adjacent chunks
		// always include the same brushes for their shared border samples.
		const FBox ChunkBox(Chunk.ChunkOrigin, Chunk.ChunkOrigin + FVector(ChunkSize));
		TArray<FVolumeBrushDef> LocalBrushes;
		LocalBrushes.Reserve(16);
		for (const FVolumeBrushDef& Brush : Brushes)
		{
			const float Pad = Brush.Radius + Brush.Smoothness * 6.f;
			FBox BrushBox(EForceInit::ForceInit);
			BrushBox += Brush.CenterA;
			BrushBox += Brush.CenterB;
			BrushBox = BrushBox.ExpandBy(Pad);
			if (ChunkBox.Intersect(BrushBox))
				LocalBrushes.Add(Brush);
		}

		// Log chunks with 0 local brushes — these will be all-rock and may border guaranteed chunks,
		// creating visible holes where the isosurface expects a water neighbour.
		if (LocalBrushes.Num() == 0)
		{
			UE_LOG(LogRouteGen, Verbose,
			       TEXT("[C6] Chunk (%d,%d,%d) has 0 local brushes — will be all-rock."),
			       Coord.X, Coord.Y, Coord.Z);
		}

		bool bAnyWater = false;
		for (int32 iz = 0; iz <= S; iz++)
		for (int32 iy = 0; iy <= S; iy++)
		for (int32 ix = 0; ix <= S; ix++)
		{
			const FVector P = Chunk.ChunkOrigin + FVector(ix, iy, iz) * VoxelSize;
			const float D   = EvalGuaranteedDensity(P, LocalBrushes);
			const int32 Idx = iz * S1 * S1 + iy * S1 + ix;
			Chunk.DensitySamples[Idx]   = D;
			Chunk.OccupancySamples[Idx] = (D >= 0.f) ? 0 : 255;
			if (D >= 0.f) bAnyWater = true;
		}
		Chunk.bContainsGuaranteedPath = bAnyWater;
	}

	// Summary: how many active chunks got zero local brushes
	{
		int32 ZeroBrushChunks = 0;
		for (const auto& P : OutField)
			if (!P.Value.bContainsGuaranteedPath) ZeroBrushChunks++;
		UE_LOG(LogRouteGen, Log,
		       TEXT("[C6] RasterizeField done. ActiveChunks=%d AllRockChunks=%d VoxelSize=%.0f S=%d"),
		       OutField.Num(), ZeroBrushChunks, VoxelSize, S);
	}
}

bool UNavigableVolumeGenerator::BuildGuaranteedVolume(const FRouteGenSpec& Spec,
                                                      const URouteArchetypeDataAsset* Archetype,
                                                      const TArray<FTraversalTopologyNode>& Nodes,
                                                      const TArray<FTraversalSkeletonSegment>& Skeleton,
                                                      const TArray<FVolumeBrushDef>* ExternalGuaranteedBrushes,
                                                      FRouteFieldModel& OutField) const
{
	OutField = FRouteFieldModel();
	if (Skeleton.Num() == 0) return false;

	// 1. Generate brushes from skeleton
	GenerateBrushes(Archetype, Nodes, Skeleton, OutField.GuaranteedBrushes, OutField.RenderOnlyBrushes);
	if (ExternalGuaranteedBrushes)
	{
		OutField.GuaranteedBrushes.Append(*ExternalGuaranteedBrushes);
	}

	// 2. Compute bounds + padding
	const float MaxRadius = Spec.Envelope.PreferredClearance + Spec.Envelope.MinTurnRadius;
	FBox Bounds = ComputeSkeletonBounds(Skeleton, MaxRadius + 400.f);
	TArray<FVolumeBrushDef> RenderBrushes = OutField.GuaranteedBrushes;
	RenderBrushes.Append(OutField.RenderOnlyBrushes);
	for (const FVolumeBrushDef& Brush : RenderBrushes)
	{
		FBox BrushBounds(EForceInit::ForceInit);
		BrushBounds += Brush.CenterA;
		BrushBounds += Brush.CenterB;
		Bounds += BrushBounds.ExpandBy(Brush.Radius + Brush.Smoothness * 2.f);
	}

	// 3. Rasterize render field (200cm voxels, chunks of 16 samples)
	RasterizeField(RenderBrushes, Bounds, 200.f, 16, OutField.RenderField);

	// 4. Rasterize sonar field (500cm voxels, chunks of 8 samples)
	RasterizeField(OutField.GuaranteedBrushes, Bounds, 500.f, 8,  OutField.SonarField);

	return true;
}
