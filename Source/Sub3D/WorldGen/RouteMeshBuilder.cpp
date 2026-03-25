#include "RouteMeshBuilder.h"
#include "MarchingCubesTables.h"
#include "NavigableVolumeGenerator.h"
#include "WorldGenTypes.h"
#include "Math/NumericLimits.h"

namespace
{
bool IsChunkInDebugWindow(const FFieldChunkCoord& Coord, const FRouteTunnelDebugOptions* DebugOptions)
{
	if (!DebugOptions || !DebugOptions->bEnableTunnelDebug || !DebugOptions->bFilterToChunkWindow)
	{
		return true;
	}

	return FMath::Abs(Coord.X - DebugOptions->DebugChunkCoord.X) <= DebugOptions->DebugChunkRadius
		&& FMath::Abs(Coord.Y - DebugOptions->DebugChunkCoord.Y) <= DebugOptions->DebugChunkRadius
		&& FMath::Abs(Coord.Z - DebugOptions->DebugChunkCoord.Z) <= DebugOptions->DebugChunkRadius;
}

float BrushSdfAtPoint(const FVolumeBrushDef& Brush, const FVector& Position)
{
	switch (Brush.BrushType)
	{
	case EVolumeBrushType::CapsuleCorridor:
		return UNavigableVolumeGenerator::CapsuleSDF(Position, Brush.CenterA, Brush.CenterB, Brush.Radius);
	case EVolumeBrushType::SpherePocket:
	case EVolumeBrushType::EllipsoidChamber:
	default:
		return UNavigableVolumeGenerator::SphereSDF(Position, Brush.CenterA, Brush.Radius);
	}
}

FBox BuildBrushBounds(const FVolumeBrushDef& Brush)
{
	FBox BrushBox(EForceInit::ForceInit);
	BrushBox += Brush.CenterA;
	BrushBox += Brush.CenterB;
	return BrushBox.ExpandBy(Brush.Radius + Brush.Smoothness * 6.f);
}

FLinearColor ResolveDebugColorAtPoint(const FVector& Position, const TArray<FVolumeBrushDef>& Brushes)
{
	float BestSdf = TNumericLimits<float>::Max();
	FLinearColor BestColor = FLinearColor::White;
	for (const FVolumeBrushDef& Brush : Brushes)
	{
		const float Sdf = BrushSdfAtPoint(Brush, Position);
		if (Sdf < BestSdf)
		{
			BestSdf = Sdf;
			BestColor = Brush.DebugColor;
		}
	}
	return BestColor;
}

FIntVector QuantizePositionKey(const FVector& Position)
{
	return FIntVector(
		FMath::RoundToInt(Position.X),
		FMath::RoundToInt(Position.Y),
		FMath::RoundToInt(Position.Z));
}
}

FVector URouteMeshBuilder::InterpolateVert(const FVector& PosA, const FVector& PosB, float DensA, float DensB)
{
	const float Denom = DensB - DensA;
	if (FMath::Abs(Denom) < KINDA_SMALL_NUMBER)
	{
		return (PosA + PosB) * 0.5f;
	}

	const float T = (0.f - DensA) / Denom;
	return FMath::Lerp(PosA, PosB, FMath::Clamp(T, 0.f, 1.f));
}

void URouteMeshBuilder::RecalculateNormals(FRouteMeshChunkData& Mesh, bool bWeightedByArea)
{
	Mesh.Normals.SetNumZeroed(Mesh.Vertices.Num());
	TMap<FIntVector, FVector> SharedNormals;

	for (int32 i = 0; i < Mesh.Triangles.Num(); i += 3)
	{
		const FVector& A = Mesh.Vertices[Mesh.Triangles[i]];
		const FVector& B = Mesh.Vertices[Mesh.Triangles[i + 1]];
		const FVector& C = Mesh.Vertices[Mesh.Triangles[i + 2]];
		const FVector FaceCross = FVector::CrossProduct(B - A, C - A);
		const FVector Normal = FaceCross.GetSafeNormal();
		const FVector Weighted = bWeightedByArea ? FaceCross : Normal;
		Mesh.Normals[Mesh.Triangles[i]] += Normal;
		Mesh.Normals[Mesh.Triangles[i + 1]] += Normal;
		Mesh.Normals[Mesh.Triangles[i + 2]] += Normal;
		SharedNormals.FindOrAdd(QuantizePositionKey(A)) += Weighted;
		SharedNormals.FindOrAdd(QuantizePositionKey(B)) += Weighted;
		SharedNormals.FindOrAdd(QuantizePositionKey(C)) += Weighted;
	}

	for (int32 VertexIndex = 0; VertexIndex < Mesh.Normals.Num(); VertexIndex++)
	{
		if (const FVector* Shared = SharedNormals.Find(QuantizePositionKey(Mesh.Vertices[VertexIndex])))
		{
			Mesh.Normals[VertexIndex] = Shared->GetSafeNormal();
		}
		else
		{
			Mesh.Normals[VertexIndex] = Mesh.Normals[VertexIndex].GetSafeNormal();
		}
	}
}

void URouteMeshBuilder::GenerateTriplanarUVs(FRouteMeshChunkData& Mesh, float VoxelSize)
{
	const float Scale = 1.f / (VoxelSize * 8.f);
	Mesh.UVs.SetNumUninitialized(Mesh.Vertices.Num());
	for (int32 i = 0; i < Mesh.Vertices.Num(); i++)
	{
		Mesh.UVs[i] = FVector2D(Mesh.Vertices[i].X * Scale, Mesh.Vertices[i].Y * Scale);
	}
}

void URouteMeshBuilder::ApplySurfaceSmoothing(FRouteMeshChunkData& Mesh,
	int32 Iterations,
	float Strength,
	float MaxDisplacementCm,
	const FBox& ChunkBounds,
	float BoundaryPaddingCm)
{
	if (Iterations <= 0 || Strength <= KINDA_SMALL_NUMBER || Mesh.Vertices.Num() == 0 || Mesh.Triangles.Num() == 0)
	{
		return;
	}

	TMap<FIntVector, TArray<int32>> Groups;
	for (int32 VertexIndex = 0; VertexIndex < Mesh.Vertices.Num(); VertexIndex++)
	{
		Groups.FindOrAdd(QuantizePositionKey(Mesh.Vertices[VertexIndex])).Add(VertexIndex);
	}

	TMap<FIntVector, TSet<FIntVector>> Adjacency;
	TMap<FIntVector, FVector> GroupPositions;
	TSet<FIntVector> PinnedGroups;
	for (const TPair<FIntVector, TArray<int32>>& Pair : Groups)
	{
		FVector Average = FVector::ZeroVector;
		for (int32 VertexIndex : Pair.Value)
		{
			Average += Mesh.Vertices[VertexIndex];
		}
		const FVector GroupPosition = Average / FMath::Max(Pair.Value.Num(), 1);
		GroupPositions.Add(Pair.Key, GroupPosition);

		const bool bNearBoundary =
			FMath::Abs(GroupPosition.X - ChunkBounds.Min.X) <= BoundaryPaddingCm ||
			FMath::Abs(GroupPosition.X - ChunkBounds.Max.X) <= BoundaryPaddingCm ||
			FMath::Abs(GroupPosition.Y - ChunkBounds.Min.Y) <= BoundaryPaddingCm ||
			FMath::Abs(GroupPosition.Y - ChunkBounds.Max.Y) <= BoundaryPaddingCm ||
			FMath::Abs(GroupPosition.Z - ChunkBounds.Min.Z) <= BoundaryPaddingCm ||
			FMath::Abs(GroupPosition.Z - ChunkBounds.Max.Z) <= BoundaryPaddingCm;
		if (bNearBoundary)
		{
			PinnedGroups.Add(Pair.Key);
		}
	}

	for (int32 TriIndex = 0; TriIndex < Mesh.Triangles.Num(); TriIndex += 3)
	{
		const FIntVector AKey = QuantizePositionKey(Mesh.Vertices[Mesh.Triangles[TriIndex]]);
		const FIntVector BKey = QuantizePositionKey(Mesh.Vertices[Mesh.Triangles[TriIndex + 1]]);
		const FIntVector CKey = QuantizePositionKey(Mesh.Vertices[Mesh.Triangles[TriIndex + 2]]);
		Adjacency.FindOrAdd(AKey).Add(BKey);
		Adjacency.FindOrAdd(AKey).Add(CKey);
		Adjacency.FindOrAdd(BKey).Add(AKey);
		Adjacency.FindOrAdd(BKey).Add(CKey);
		Adjacency.FindOrAdd(CKey).Add(AKey);
		Adjacency.FindOrAdd(CKey).Add(BKey);
	}

	for (int32 Iteration = 0; Iteration < Iterations; Iteration++)
	{
		TMap<FIntVector, FVector> UpdatedPositions = GroupPositions;
		for (const TPair<FIntVector, FVector>& Pair : GroupPositions)
		{
			if (PinnedGroups.Contains(Pair.Key))
			{
				continue;
			}

			const TSet<FIntVector>* Neighbors = Adjacency.Find(Pair.Key);
			if (!Neighbors || Neighbors->Num() == 0)
			{
				continue;
			}

			FVector NeighborAverage = FVector::ZeroVector;
			int32 NeighborCount = 0;
			for (const FIntVector& NeighborKey : *Neighbors)
			{
				if (const FVector* NeighborPosition = GroupPositions.Find(NeighborKey))
				{
					NeighborAverage += *NeighborPosition;
					NeighborCount++;
				}
			}
			if (NeighborCount <= 0)
			{
				continue;
			}

			NeighborAverage /= NeighborCount;
			const FVector Delta = (NeighborAverage - Pair.Value) * Strength;
			UpdatedPositions[Pair.Key] = Pair.Value + Delta.GetClampedToMaxSize(MaxDisplacementCm);
		}
		GroupPositions = MoveTemp(UpdatedPositions);
	}

	for (const TPair<FIntVector, TArray<int32>>& Pair : Groups)
	{
		const FVector* NewPosition = GroupPositions.Find(Pair.Key);
		if (!NewPosition)
		{
			continue;
		}
		for (int32 VertexIndex : Pair.Value)
		{
			Mesh.Vertices[VertexIndex] = *NewPosition;
		}
	}
}

void URouteMeshBuilder::ProcessChunk(const FFieldChunkCoord& Coord,
                                     const FRouteFieldChunkData& ChunkData,
                                     const TArray<FVolumeBrushDef>& LocalBrushes,
                                     const FRouteSurfaceBuildSettings* SurfaceSettings,
                                     FRouteMeshChunkData& OutMesh,
                                     FChunkBuildDebugStats* DebugStats) const
{
	OutMesh.Coord = Coord;
	OutMesh.Bounds = FBox(EForceInit::ForceInit);

	const int32 S = ChunkData.SamplesPerAxis;
	const int32 S1 = S + 1;
	const float VS = ChunkData.VoxelSize;
	const FVector Origin = ChunkData.ChunkOrigin;
	const FBox ChunkBounds(Origin, Origin + FVector(S * VS));

	if (DebugStats)
	{
		*DebugStats = FChunkBuildDebugStats();
		DebugStats->MinDensity = TNumericLimits<float>::Max();
		DebugStats->MaxDensity = TNumericLimits<float>::Lowest();
	}

	if (S < 2 || ChunkData.DensitySamples.Num() < S1 * S1 * S1)
	{
		return;
	}

	auto SampleDensity = [&](int32 ix, int32 iy, int32 iz) -> float
	{
		ix = FMath::Clamp(ix, 0, S);
		iy = FMath::Clamp(iy, 0, S);
		iz = FMath::Clamp(iz, 0, S);
		return ChunkData.DensitySamples[iz * S1 * S1 + iy * S1 + ix];
	};

	for (int32 iz = 0; iz < S; iz++)
	for (int32 iy = 0; iy < S; iy++)
	for (int32 ix = 0; ix < S; ix++)
	{
		float Density[8];
		FVector Pos[8];

		for (int32 c = 0; c < 8; c++)
		{
			const int32 CX = ix + MC_CORNER_OFFSET_X[c];
			const int32 CY = iy + MC_CORNER_OFFSET_Y[c];
			const int32 CZ = iz + MC_CORNER_OFFSET_Z[c];
			Density[c] = SampleDensity(CX, CY, CZ);
			Pos[c] = Origin + FVector(CX, CY, CZ) * VS;

			if (DebugStats)
			{
				DebugStats->MinDensity = FMath::Min(DebugStats->MinDensity, Density[c]);
				DebugStats->MaxDensity = FMath::Max(DebugStats->MaxDensity, Density[c]);
			}
		}

		int32 CubeIndex = 0;
		for (int32 c = 0; c < 8; c++)
		{
			if (Density[c] < 0.f)
			{
				CubeIndex |= (1 << c);
			}
		}

		if (CubeIndex == 0 || CubeIndex == 255)
		{
			continue;
		}

		if (DebugStats)
		{
			DebugStats->NonTrivialCubes++;
		}

		for (int32 e = 0; e < 15; e += 3)
		{
			const int8 E0 = MC_TRIANGULATION[CubeIndex][e];
			if (E0 == -1)
			{
				break;
			}

			const int8 E1 = MC_TRIANGULATION[CubeIndex][e + 1];
			const int8 E2 = MC_TRIANGULATION[CubeIndex][e + 2];

			auto MakeVertWithNormal = [&](int8 Edge, FVector& OutNormal) -> FVector
			{
				const int32 CA = MC_EDGE_CORNER_A[Edge];
				const int32 CB = MC_EDGE_CORNER_B[Edge];

				const int32 AX = ix + MC_CORNER_OFFSET_X[CA];
				const int32 AY = iy + MC_CORNER_OFFSET_Y[CA];
				const int32 AZ = iz + MC_CORNER_OFFSET_Z[CA];
				const int32 BX = ix + MC_CORNER_OFFSET_X[CB];
				const int32 BY = iy + MC_CORNER_OFFSET_Y[CB];
				const int32 BZ = iz + MC_CORNER_OFFSET_Z[CB];

				const float Denom = Density[CB] - Density[CA];
				const float T = FMath::Abs(Denom) < KINDA_SMALL_NUMBER
					? 0.5f
					: FMath::Clamp(-Density[CA] / Denom, 0.f, 1.f);

				const FVector GradA(
					SampleDensity(AX + 1, AY, AZ) - SampleDensity(AX - 1, AY, AZ),
					SampleDensity(AX, AY + 1, AZ) - SampleDensity(AX, AY - 1, AZ),
					SampleDensity(AX, AY, AZ + 1) - SampleDensity(AX, AY, AZ - 1));
				const FVector GradB(
					SampleDensity(BX + 1, BY, BZ) - SampleDensity(BX - 1, BY, BZ),
					SampleDensity(BX, BY + 1, BZ) - SampleDensity(BX, BY - 1, BZ),
					SampleDensity(BX, BY, BZ + 1) - SampleDensity(BX, BY, BZ - 1));

				const FVector Grad = FMath::Lerp(GradA, GradB, T);
				OutNormal = Grad.IsNearlyZero() ? FVector::UpVector : Grad.GetSafeNormal();

				return FMath::Lerp(Pos[CA], Pos[CB], T);
			};

			FVector N0;
			FVector N1;
			FVector N2;
			const FVector V0 = MakeVertWithNormal(E0, N0);
			const FVector V1 = MakeVertWithNormal(E1, N1);
			const FVector V2 = MakeVertWithNormal(E2, N2);

			const FVector E01 = V1 - V0;
			const FVector E02 = V2 - V0;
			const FVector E12 = V2 - V1;
			if (E01.SizeSquared() < 1.f || E02.SizeSquared() < 1.f || E12.SizeSquared() < 1.f)
			{
				if (DebugStats)
				{
					DebugStats->RejectedTriangles++;
				}
				continue;
			}
			if (FVector::CrossProduct(E01, E02).SizeSquared() < 1.f)
			{
				if (DebugStats)
				{
					DebugStats->RejectedTriangles++;
				}
				continue;
			}

			const int32 Base = OutMesh.Vertices.Num();
			OutMesh.Vertices.Add(V0);
			OutMesh.Vertices.Add(V1);
			OutMesh.Vertices.Add(V2);
			OutMesh.Normals.Add(N0);
			OutMesh.Normals.Add(N1);
			OutMesh.Normals.Add(N2);
			OutMesh.VertexColors.Add(ResolveDebugColorAtPoint(V0, LocalBrushes));
			OutMesh.VertexColors.Add(ResolveDebugColorAtPoint(V1, LocalBrushes));
			OutMesh.VertexColors.Add(ResolveDebugColorAtPoint(V2, LocalBrushes));
			OutMesh.Triangles.Add(Base);
			OutMesh.Triangles.Add(Base + 1);
			OutMesh.Triangles.Add(Base + 2);
			OutMesh.Bounds += V0;
			OutMesh.Bounds += V1;
			OutMesh.Bounds += V2;

			if (DebugStats)
			{
				DebugStats->TrianglesGenerated++;
			}
		}
	}

	if (OutMesh.Vertices.Num() > 0)
	{
		if (SurfaceSettings && SurfaceSettings->bEnableVisualSurfaceSmoothing)
		{
			ApplySurfaceSmoothing(
				OutMesh,
				SurfaceSettings->SurfaceSmoothingIterations,
				SurfaceSettings->SurfaceSmoothingStrength,
				SurfaceSettings->SurfaceSmoothingMaxDisplacementCm,
				ChunkBounds,
				VS * 1.5f);
		}
		RecalculateNormals(OutMesh, !SurfaceSettings || SurfaceSettings->bUseWeightedVertexNormals);
		GenerateTriplanarUVs(OutMesh, VS);
	}

	if (DebugStats && DebugStats->MinDensity == TNumericLimits<float>::Max())
	{
		DebugStats->MinDensity = 0.f;
		DebugStats->MaxDensity = 0.f;
	}
}

bool URouteMeshBuilder::BuildMeshChunks(const FRouteGenSpec& Spec,
                                        const FRouteFieldModel& Field,
                                        const UBiomeFieldProfileDataAsset* Biome,
                                        TArray<FRouteMeshChunkData>& OutChunks,
                                        const FRouteSurfaceBuildSettings* SurfaceSettings,
                                        const FRouteTunnelDebugOptions* DebugOptions) const
{
	OutChunks.Reset();

	const bool bDebugEnabled = DebugOptions && DebugOptions->bEnableTunnelDebug;

	int32 UniformAllRockOrWater = 0;
	int32 GuaranteedButUniform = 0;
	int32 CandidateChunks = 0;
	int32 GuaranteedCoords = 0;
	int32 SignChangingZeroVerts = 0;
	int32 RejectedTriangleChunks = 0;
	int32 TotalRejectedTriangles = 0;
	TArray<FVolumeBrushDef> AllRenderBrushes = Field.GuaranteedBrushes;
	AllRenderBrushes.Append(Field.RenderOnlyBrushes);

	TArray<FRouteMeshChunkData> CandidateMeshes;
	TSet<FFieldChunkCoord> GuaranteedChunkCoords;
	// Built during sign-change scan below; reused by BFS to avoid re-scanning DensitySamples.
	TSet<FFieldChunkCoord> WaterChunkCoords;

	for (const auto& Pair : Field.RenderField)
	{
		if (Pair.Value.bContainsGuaranteedPath)
		{
			GuaranteedChunkCoords.Add(Pair.Key);
			GuaranteedCoords++;
		}
	}

	if (bDebugEnabled && DebugOptions->bLogChunkSelectionSummary)
	{
		UE_LOG(LogRouteGen, Log,
			TEXT("[TunnelDebug] Enabled. FilterToChunkWindow=%d Chunk=(%d,%d,%d) Radius=%d RejectThreshold=%d"),
			DebugOptions->bFilterToChunkWindow,
			DebugOptions->DebugChunkCoord.X, DebugOptions->DebugChunkCoord.Y, DebugOptions->DebugChunkCoord.Z,
			DebugOptions->DebugChunkRadius,
			DebugOptions->RejectedTriangleLogThreshold);
	}

	for (const auto& Pair : Field.RenderField)
	{
		bool bHasWater = false;
		bool bHasRock = false;
		for (const float D : Pair.Value.DensitySamples)
		{
			bHasWater |= (D >= 0.f);
			bHasRock |= (D < 0.f);
			if (bHasWater && bHasRock)
			{
				break;
			}
		}

		if (bHasWater)
		{
			WaterChunkCoords.Add(Pair.Key);
		}

		if (!(bHasWater && bHasRock))
		{
			UniformAllRockOrWater++;
			if (Pair.Value.bContainsGuaranteedPath)
			{
				GuaranteedButUniform++;
				if (bDebugEnabled && DebugOptions->bLogGuaranteedUniformChunks && IsChunkInDebugWindow(Pair.Key, DebugOptions))
				{
					UE_LOG(LogRouteGen, Warning,
						TEXT("[TunnelDebug] GuaranteedUniform Chunk=(%d,%d,%d) Samples=%d"),
						Pair.Key.X, Pair.Key.Y, Pair.Key.Z,
						Pair.Value.DensitySamples.Num());
				}
			}
			continue;
		}

		FRouteMeshChunkData MeshChunk;
		FChunkBuildDebugStats ChunkStats;
		TArray<FVolumeBrushDef> LocalBrushes;
		LocalBrushes.Reserve(32);
		const float ChunkSize = Pair.Value.VoxelSize * Pair.Value.SamplesPerAxis;
		const FBox ChunkBounds(Pair.Value.ChunkOrigin, Pair.Value.ChunkOrigin + FVector(ChunkSize));
		for (const FVolumeBrushDef& Brush : AllRenderBrushes)
		{
			if (ChunkBounds.Intersect(BuildBrushBounds(Brush)))
			{
				LocalBrushes.Add(Brush);
			}
		}
		ProcessChunk(Pair.Key, Pair.Value, LocalBrushes, SurfaceSettings, MeshChunk, bDebugEnabled ? &ChunkStats : nullptr);

		if (bDebugEnabled)
		{
			TotalRejectedTriangles += ChunkStats.RejectedTriangles;
			if (ChunkStats.RejectedTriangles >= DebugOptions->RejectedTriangleLogThreshold)
			{
				RejectedTriangleChunks++;
				if (DebugOptions->bLogRejectedTriangleChunks && IsChunkInDebugWindow(Pair.Key, DebugOptions))
				{
					UE_LOG(LogRouteGen, Warning,
						TEXT("[TunnelDebug] HighReject Chunk=(%d,%d,%d) Guaranteed=%d NonTrivialCubes=%d GeneratedTris=%d RejectedTris=%d Density=[%.3f, %.3f]"),
						Pair.Key.X, Pair.Key.Y, Pair.Key.Z,
						Pair.Value.bContainsGuaranteedPath,
						ChunkStats.NonTrivialCubes,
						ChunkStats.TrianglesGenerated,
						ChunkStats.RejectedTriangles,
						ChunkStats.MinDensity, ChunkStats.MaxDensity);
				}
			}
		}

		if (MeshChunk.Vertices.Num() > 0)
		{
			MeshChunk.bServerCollision = Pair.Value.bContainsGuaranteedPath;
			CandidateMeshes.Add(MoveTemp(MeshChunk));
			CandidateChunks++;
		}
		else
		{
			SignChangingZeroVerts++;
			if (bDebugEnabled && DebugOptions->bLogZeroVertChunks && IsChunkInDebugWindow(Pair.Key, DebugOptions))
			{
				UE_LOG(LogRouteGen, Error,
					TEXT("[TunnelDebug] ZeroVerts Chunk=(%d,%d,%d) Guaranteed=%d NonTrivialCubes=%d RejectedTris=%d Density=[%.3f, %.3f] Samples=%d"),
					Pair.Key.X, Pair.Key.Y, Pair.Key.Z,
					Pair.Value.bContainsGuaranteedPath,
					ChunkStats.NonTrivialCubes,
					ChunkStats.RejectedTriangles,
					ChunkStats.MinDensity, ChunkStats.MaxDensity,
					Pair.Value.DensitySamples.Num());
			}
		}
	}

	if (CandidateMeshes.Num() == 0)
	{
		UE_LOG(LogRouteGen, Warning,
			TEXT("[MeshBuilder] No candidate chunks after sign-change filtering. UniformChunks=%d GuaranteedUniform=%d"),
			UniformAllRockOrWater, GuaranteedButUniform);
		return false;
	}

	// BFS flood fill from guaranteed seeds through all water-containing chunks.
	// Replaces the 1-hop shell check which dropped organic cave chunks (DisconnectedDropped)
	// that were spatially connected to the navigable volume but not adjacent to guaranteed coords.
	TSet<FFieldChunkCoord> ConnectedCoords;
	{
		TArray<FFieldChunkCoord> BFSQueue;
		// Reserve ~8× seeds: each chunk has 26 3D neighbors but organic shells
		// rarely expand more than a few hops, so this is a rough upper bound.
		BFSQueue.Reserve(GuaranteedChunkCoords.Num() * 8);

		for (const FFieldChunkCoord& GC : GuaranteedChunkCoords)
		{
			ConnectedCoords.Add(GC);
			BFSQueue.Add(GC);
		}

		for (int32 BFSHead = 0; BFSHead < BFSQueue.Num(); BFSHead++)
		{
			const FFieldChunkCoord Current = BFSQueue[BFSHead];

			for (int32 dx = -1; dx <= 1; dx++)
			for (int32 dy = -1; dy <= 1; dy++)
			for (int32 dz = -1; dz <= 1; dz++)
			{
				if (dx == 0 && dy == 0 && dz == 0) continue;

				FFieldChunkCoord Neighbor;
				Neighbor.X = Current.X + dx;
				Neighbor.Y = Current.Y + dy;
				Neighbor.Z = Current.Z + dz;

				if (ConnectedCoords.Contains(Neighbor)) continue;

				const FRouteFieldChunkData* ND = Field.RenderField.Find(Neighbor);
				if (!ND) continue;

				// Expand through any chunk with water — includes organic caves
				bool bHasWater = false;
				for (float D : ND->DensitySamples)
				{
					if (D >= 0.f) { bHasWater = true; break; }
				}

				if (bHasWater)
				{
					ConnectedCoords.Add(Neighbor);
					BFSQueue.Add(Neighbor);
				}
			}
		}
	}

	int32 ShellKept = 0;
	for (int32 Index = 0; Index < CandidateMeshes.Num(); Index++)
	{
		if (!ConnectedCoords.Contains(CandidateMeshes[Index].Coord))
		{
			continue;
		}

		const bool bGuaranteed = CandidateMeshes[Index].bServerCollision;
		if (!bGuaranteed)
		{
			ShellKept++;
			if (bDebugEnabled && DebugOptions->bLogKeptShellChunks && IsChunkInDebugWindow(CandidateMeshes[Index].Coord, DebugOptions))
			{
				UE_LOG(LogRouteGen, Log,
					TEXT("[TunnelDebug] KeptShell Chunk=(%d,%d,%d)"),
					CandidateMeshes[Index].Coord.X,
					CandidateMeshes[Index].Coord.Y,
					CandidateMeshes[Index].Coord.Z);
			}
		}

		OutChunks.Add(MoveTemp(CandidateMeshes[Index]));
	}

	if (OutChunks.Num() == 0)
	{
		for (FRouteMeshChunkData& MeshChunk : CandidateMeshes)
		{
			OutChunks.Add(MoveTemp(MeshChunk));
		}
	}

	UE_LOG(LogRouteGen, Log,
		TEXT("[MeshBuilder] Done. Kept=%d Candidates=%d GuaranteedCoords=%d Uniform=%d GuaranteedUniform=%d ShellKept=%d DisconnectedDropped=%d"),
		OutChunks.Num(), CandidateChunks, GuaranteedCoords,
		UniformAllRockOrWater, GuaranteedButUniform,
		ShellKept,
		CandidateChunks - OutChunks.Num());

	if (bDebugEnabled && DebugOptions->bLogChunkSelectionSummary)
	{
		UE_LOG(LogRouteGen, Log,
			TEXT("[TunnelDebug] Summary ZeroVerts=%d RejectedTriangleChunks=%d TotalRejectedTriangles=%d"),
			SignChangingZeroVerts,
			RejectedTriangleChunks,
			TotalRejectedTriangles);
	}

	return OutChunks.Num() > 0;
}
