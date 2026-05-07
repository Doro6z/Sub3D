#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CompartmentWaterBake.generated.h"

/**
 * One horizontal Z slice of a compartment, sampled at SliceZ_Local.
 *
 * Stores a 2D Signed Distance Field (one float per cell) rather than a binary mask:
 *   - negative = inside the compartment, magnitude = distance in cm to nearest wall
 *   - positive = outside, magnitude = distance in cm to nearest wall
 *   - 0       = on the wall
 *
 * The SDF lets Marching Squares place the contour at the interpolated zero-crossing
 * (sub-cell precision) instead of cell-center stair-stepping.
 */
USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompartmentBakeSlice
{
	GENERATED_BODY()

	/** Z height in submarine-local space at which this slice was sampled. */
	UPROPERTY(VisibleAnywhere)
	float SliceZ_Local = 0.0f;

	UPROPERTY(VisibleAnywhere)
	int32 GridWidth = 0;

	UPROPERTY(VisibleAnywhere)
	int32 GridHeight = 0;

	/** Signed distance field. Size = GridWidth * GridHeight. Convention: negative = inside. */
	UPROPERTY(VisibleAnywhere)
	TArray<float> SignedDistance;

	/** Final ordered polygon, post-chaining/inset/resample/align. 2D in sub-local XY.
	 *  This is what the cap mesh is triangulated from. */
	UPROPERTY(VisibleAnywhere)
	TArray<FVector2D> ContourPolygon;

	/** Raw Marching Squares output BEFORE chaining: pairs (i, i+1) form a segment.
	 *  Stored for debug viz — lets you see if MS is producing correct segments independently
	 *  of whether chain succeeded in joining them into a closed polygon. */
	UPROPERTY(VisibleAnywhere)
	TArray<FVector2D> RawContourSegments;

	/** Number of MS saddle cases (5 or 10) hit on this slice. Diagnostic for the saddle-pairing
	 *  fix. If 0, saddle-pairing is irrelevant for this slice — open chain comes from elsewhere. */
	UPROPERTY(VisibleAnywhere)
	int32 SaddleCount = 0;
};

/**
 * Pre-computed procedural mesh for one slice. Vertices are stored at Z=0;
 * the runtime translates via SetRelativeLocation to position the cap mesh at the current
 * water height.
 */
USTRUCT(BlueprintType)
struct SUB3DCORE_API FCachedBakeMesh
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TArray<FVector> Vertices;

	UPROPERTY(VisibleAnywhere)
	TArray<int32> Triangles;

	UPROPERTY(VisibleAnywhere)
	TArray<FVector> Normals;

	UPROPERTY(VisibleAnywhere)
	TArray<FVector2D> UV0;
};

/**
 * Bake result for one compartment: stack of horizontal silhouettes by height + pre-generated
 * cap meshes (one per slice).
 *
 * Created and populated by Sub3DWaterBake (editor-only). Read at runtime by Sub3D's
 * UFloodWaterPlaneComponent to drive the per-compartment water visual.
 */
UCLASS(BlueprintType)
class SUB3DCORE_API UCompartmentWaterBake : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Compartment identifier (matches USubmarineDefinition::Compartments[i].CompartmentId). */
	UPROPERTY(VisibleAnywhere, Category = "Bake Source")
	FName CompartmentId;

	/** Sub-local bounds of the AUTHORED BP volume (= what the user defined in the BP).
	 *  This is what debug viewers should draw as the "compartment AABB", and what cap meshes
	 *  fit. Cell positions in Slices[i].SignedDistance use an INTERNALLY PADDED grid of 1 cell
	 *  on each X/Y side — convention: cell (x, y) sub-local centre =
	 *      LocalBoundsMin.X - CellSizeCm + (x + 0.5) * CellSizeCm
	 *      LocalBoundsMin.Y - CellSizeCm + (y + 0.5) * CellSizeCm
	 *  The padding cells (cell index 0, GridWidth-1, etc.) are forced "outside" by the bake to
	 *  guarantee MS edge-iteration captures every transition without inflating the bake region. */
	UPROPERTY(VisibleAnywhere, Category = "Bake Source")
	FVector LocalBoundsMin = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Bake Source")
	FVector LocalBoundsMax = FVector::ZeroVector;

	/** Voxel cell size used at bake time (cm). Same value applies to both X and Y. */
	UPROPERTY(VisibleAnywhere, Category = "Bake Source")
	float CellSizeCm = 25.0f;

	/** Z-stacked slices. */
	UPROPERTY(VisibleAnywhere, Category = "Slices")
	TArray<FCompartmentBakeSlice> Slices;

	/** Cap mesh per slice, indexed in lockstep with Slices. */
	UPROPERTY(VisibleAnywhere, Category = "Cached Meshes")
	TArray<FCachedBakeMesh> CapMeshesPerSlice;
};
