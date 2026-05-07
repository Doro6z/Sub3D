#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SubmarineWaterBakerLibrary.generated.h"

class USubmarineDefinition;
class UCompartmentWaterBake;
class AActor;

/**
 * Bake parameters — exposed to the editor utility (or BP caller).
 * Defaults match the proto's validated values.
 */
USTRUCT(BlueprintType)
struct SUB3DWATERBAKE_API FSubmarineWaterBakeParams
{
	GENERATED_BODY()

	/** Number of horizontal Z slices to bake per compartment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "2", ClampMax = "64"))
	int32 NumSlices = 12;

	/** Voxel cell size in cm. Smaller = more precise but slower bake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "5.0", ClampMax = "200.0"))
	float CellSizeCm = 25.0f;

	/** Number of resampled polygon points (also = ring vertex count). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "16", ClampMax = "256"))
	int32 PolygonResampleN = 64;

	/** Concentric rings inside cap mesh (0 = simple fan, 3 = good for wave propagation viz). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "8"))
	int32 RingsCount = 3;

	/** Polygon inward inset in cm (cosmetic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "20.0"))
	float CapInsetCm = 2.0f;

	/** Where to save the baked asset, relative to /Game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PackagePathRoot = TEXT("/Game/Submarines/Craniata/Water/");
};

/**
 * Editor-only bake pipeline for submarine compartments.
 *
 * Reads compartment topology from USubmarineDefinition + finds matching
 * UCompartmentVolumeComponents on a hull source actor, voxelises their union, runs Marching
 * Squares + tessellation rings, saves a UCompartmentWaterBake asset.
 *
 * STUB at step 2c — bodies log and return 0/nullptr. Real logic lands in step 2d.
 */
UCLASS()
class SUB3DWATERBAKE_API USubmarineWaterBakerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Bake one compartment. Returns the produced bake asset or nullptr on failure.
	 * Appends a one-line human-readable report to OutReport (same content as the LogWaterBake
	 * `Baked %s: ...` line, plus a leading "OK"/"FAIL" status tag for the panel).
	 */
	UFUNCTION(BlueprintCallable, Category = "Sub3D|Water Bake")
	static UCompartmentWaterBake* BakeCompartment(
		USubmarineDefinition* Definition,
		FName CompartmentId,
		AActor* HullSourceActor,
		const FSubmarineWaterBakeParams& Params,
		UPARAM(ref) FString& OutReport);

	/**
	 * Bake every compartment in the DA. Returns the count of successful bakes.
	 * OutReport contains a header + one line per compartment + a footer summary.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sub3D|Water Bake")
	static int32 BakeAllCompartments(
		USubmarineDefinition* Definition,
		AActor* HullSourceActor,
		const FSubmarineWaterBakeParams& Params,
		UPARAM(ref) FString& OutReport);
};
