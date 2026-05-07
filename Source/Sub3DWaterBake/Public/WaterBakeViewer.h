#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterBakeViewer.generated.h"

class UCompartmentWaterBake;
class UBillboardComponent;

/**
 * Editor-only debug actor. Place in level, pick a UCompartmentWaterBake asset, and the actor
 * draws the bake's bounds + slice contours + SDF cells + cap mesh wireframe at its OWN
 * world transform — so you can offset it (move, rotate, scale = 1) anywhere to inspect the
 * data outside the submarine geometry.
 *
 * Ticks in editor viewports (no PIE needed) via ShouldTickIfViewportsOnly. Stripped from
 * cooked builds via bIsEditorOnlyActor.
 */
UCLASS()
class SUB3DWATERBAKE_API AWaterBakeViewer : public AActor
{
	GENERATED_BODY()

public:
	AWaterBakeViewer();

	/** Bake asset to visualize. Pick any CWB_*.uasset produced by the bake pipeline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer")
	TObjectPtr<UCompartmentWaterBake> BakeToView = nullptr;

	// ── Bounds box ───────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer|Draw")
	bool bDrawBounds = true;

	// ── Slice contour polygons (post-chain) ──────────────────────────────
	/** Draw the FINAL chained+resampled polygon for each slice (= what the cap mesh is built
	 *  from). If chain fails, you'll see long diagonal jumps closing the loop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer|Draw")
	bool bDrawSliceContours = true;

	/** -1 = draw all slices (color-graded blue→yellow by Z). >=0 = draw only that slice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer|Draw",
		meta = (ClampMin = "-1", ClampMax = "63"))
	int32 SliceContourIndex = -1;

	// ── Raw MS segments (pre-chain) ──────────────────────────────────────
	/** Draw the raw Marching Squares segment soup BEFORE chaining. Each segment is one line.
	 *  If raw segments form a clean wall outline but post-chain contour is broken → bug is in
	 *  ChainSegmentsIntoPolygon. If raw segments are already chaotic → bug is in MS / SDF. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer|Draw")
	bool bDrawRawMSSegments = false;

	/** -1 = draw raw segments for all slices. >=0 = draw only that slice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer|Draw",
		meta = (ClampMin = "-1", ClampMax = "63"))
	int32 RawMSSegmentsSliceIndex = -1;

	// ── SDF cells ────────────────────────────────────────────────────────
	/** Per-cell SDF visualization at one slice. Green = inside, Red = outside.
	 *  Brightness scales with proximity to wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer|Draw")
	bool bDrawSDFCells = false;

	/** Slice index for SDF cells viz. -1 = ALL slices stacked at their respective Z (heavy:
	 *  use SDFSubsampleStep ≥ 4 to keep it perf-safe). Defaults to middle slice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer|Draw",
		meta = (ClampMin = "-1", ClampMax = "63"))
	int32 SDFCellsSliceIndex = 6;

	/** Sample one cell every N. 1 = all (heavy), 4 = quarter, 8 = eighth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer|Draw",
		meta = (ClampMin = "1", ClampMax = "16"))
	int32 SDFSubsampleStep = 4;

	// ── Polygon endpoints (open-chain diagnostic) ────────────────────────
	/** Draw bright spheres at the FIRST (green) and LAST (red) vertex of each slice's chained
	 *  polygon. If the chain is closed, the two markers overlap. If open, they show WHERE the
	 *  path breaks — usually a door, hatch, or AABB boundary issue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer|Draw")
	bool bDrawPolygonEndpoints = false;

	// ── Cap mesh wireframe ───────────────────────────────────────────────
	/** Draw the pre-generated cap mesh as wireframe triangles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer|Draw")
	bool bDrawCapMesh = false;

	/** Slice index for cap mesh viz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bake Viewer|Draw",
		meta = (ClampMin = "0", ClampMax = "63"))
	int32 CapMeshSliceIndex = 6;

	// ── Engine overrides ─────────────────────────────────────────────────
	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

private:
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Billboard = nullptr;
};
