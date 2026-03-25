#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorldGenTypes.h"
#include "RouteMeshBuilder.generated.h"

class UBiomeFieldProfileDataAsset;

// C10 — CPU Marching Cubes extraction from RenderField → FRouteMeshChunkData[].
// isoLevel = 0: density < 0 = rock (solid), density >= 0 = water (empty).
UCLASS()
class SUB3D_API URouteMeshBuilder : public UObject
{
	GENERATED_BODY()

public:
	bool BuildMeshChunks(const FRouteGenSpec& Spec,
	                     const FRouteFieldModel& Field,
	                     const UBiomeFieldProfileDataAsset* Biome,
	                     TArray<FRouteMeshChunkData>& OutChunks,
	                     const FRouteSurfaceBuildSettings* SurfaceSettings = nullptr,
	                     const FRouteTunnelDebugOptions* DebugOptions = nullptr) const;

private:
	struct FChunkBuildDebugStats
	{
		int32 NonTrivialCubes = 0;
		int32 TrianglesGenerated = 0;
		int32 RejectedTriangles = 0;
		float MinDensity = 0.f;
		float MaxDensity = 0.f;
	};

	void ProcessChunk(const FFieldChunkCoord& Coord,
	                  const FRouteFieldChunkData& ChunkData,
	                  const TArray<FVolumeBrushDef>& LocalBrushes,
	                  const FRouteSurfaceBuildSettings* SurfaceSettings,
	                  FRouteMeshChunkData& OutMesh,
	                  FChunkBuildDebugStats* DebugStats = nullptr) const;

	// Interpolate vertex position along edge between A and B at isoLevel = 0
	static FVector InterpolateVert(const FVector& PosA, const FVector& PosB, float DensA, float DensB);

	// Per-triangle normal recalculation
	static void RecalculateNormals(FRouteMeshChunkData& Mesh, bool bWeightedByArea);

	// Triplanar UVs: world XZ / VoxelSize scale
	static void GenerateTriplanarUVs(FRouteMeshChunkData& Mesh, float VoxelSize);

	static void ApplySurfaceSmoothing(FRouteMeshChunkData& Mesh,
		int32 Iterations,
		float Strength,
		float MaxDisplacementCm,
		const FBox& ChunkBounds,
		float BoundaryPaddingCm);
};
