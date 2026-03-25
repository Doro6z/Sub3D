#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WorldGenTypes.h"
#include "BiomeFieldProfileDataAsset.generated.h"

class UBranchProfileSetDataAsset;

// Biome noise + material profile. One DA per biome.
// Create: Content Browser → Miscellaneous → Data Asset → UBiomeFieldProfileDataAsset
UCLASS(BlueprintType)
class SUB3D_API UBiomeFieldProfileDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Biome")  FName   BiomeID;
	// Organic deformation: KdotJPG dual-noise frequencies (in 1/cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Organic") float  NoiseFreq1          = 1.f / 4800.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Organic") float  NoiseFreq2          = 1.f / 3200.f;
	// Threshold: N1²+N2² < Threshold → secondary cave opens
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Organic") float  NoiseCaveThreshold  = 0.04f;
	// Scale factor on organic contribution (0=no organic, 1=full)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Organic") float  OrganicAmplitude    = 1.0f;
	// Large-scale warp applied to render field (additive SDF displacement in cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Organic") float  LargeScaleWarpAmplitude = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Organic") float  MediumNoiseAmplitude    = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Material") TSoftObjectPtr<UMaterialInterface> PrimaryMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route") TSoftObjectPtr<UBranchProfileSetDataAsset> PreferredBranchProfileSet;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Route") ETraversalComplexityTier DefaultTraversalComplexity = ETraversalComplexityTier::Moderate;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Biome")    FGameplayTagContainer BiomeTags;
};
