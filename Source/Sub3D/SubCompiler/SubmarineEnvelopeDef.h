#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "SubCompilerTypes.h"
#include "SubmarineEnvelopeDef.generated.h"

UCLASS(BlueprintType)
class SUB3D_API USubmarineEnvelopeDef : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope", meta = (ClampMin = "500.0"))
	float SpineLengthCm = 1520.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope")
	FRuntimeFloatCurve RadiusProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope", meta = (ClampMin = "50.0"))
	float DefaultRadiusCm = 180.f;

	// Preferred floor depth below the envelope centerline.
	// Higher values lower the floor, increase headroom, and reveal the hull curvature.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope", meta = (ClampMin = "0.0"))
	float FloorDropBiasCm = 0.f;

	// Longitudinal smoothing between solved compartment boundaries for exterior preview/build.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope|Exterior", meta = (ClampMin = "1", ClampMax = "16"))
	int32 ExteriorLongitudinalSubdivisionsPerSpan = 6;

	// Radial tessellation used for the exterior hull preview/build.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope|Exterior", meta = (ClampMin = "12", ClampMax = "64"))
	int32 ExteriorRadialSegments = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope", meta = (ClampMin = "1"))
	int32 MaxCompartments = 6;

	// Superellipse exponent for cross-section shape.
	// 2.0 = circle/ellipse (default), >2 = squarish, <2 = diamond/organic.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope|Section", meta = (ClampMin = "0.5", ClampMax = "8.0"))
	float SectionExponent = 2.f;

	// Width-to-height ratio of the cross-section.
	// 1.0 = symmetric (default), >1 = wider than tall, <1 = taller than wide.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope|Section", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float WidthToHeightRatio = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope|BowStern")
	EBowSternProfile BowProfile = EBowSternProfile::Rounded;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope|BowStern")
	EBowSternProfile SternProfile = EBowSternProfile::Tapered;

	// Length of the bow taper zone as fraction of SpineLength (0.0 = no taper, 0.2 = 20% of spine).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope|BowStern", meta = (ClampMin = "0.0", ClampMax = "0.4"))
	float BowTaperFraction = 0.12f;

	// Length of the stern taper zone as fraction of SpineLength.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Envelope|BowStern", meta = (ClampMin = "0.0", ClampMax = "0.4"))
	float SternTaperFraction = 0.15f;

	UFUNCTION(BlueprintPure, Category = "Envelope")
	float EvaluateRadius(float NormalizedPosition) const;

	// Evaluate the bow/stern profile taper at a given position.
	// Returns a radius multiplier in [0, 1].
	UFUNCTION(BlueprintPure, Category = "Envelope")
	float EvaluateBowSternTaper(float NormalizedPosition) const;
};
