#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SubCompiler/SubCompilerTypes.h"
#include "SubmarineAuthoringAssets.h"
#include "SubmarineAuthoringPipeline.generated.h"

UCLASS()
class SUB3D_API USubmarineHullEvaluationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	static FVector EvaluateSectionPoint(const FSubmarineHullAuthoring& Hull, float SpineAlpha, float ArcAlpha);

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	static float EvaluateBowSternTaper(const FSubmarineHullAuthoring& Hull, float SpineAlpha);

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	static float GetSectionArcLengthEstimate(const FSubmarineHullAuthoring& Hull, float SpineAlpha, int32 NumSamples);
};

UCLASS()
class SUB3D_API USubmarineAuthoringBakeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Submarine|Authoring")
	static bool ValidateAuthoringAsset(const USubmarineAuthoringAsset* AuthoringAsset, UPARAM(ref) TArray<FLayoutValidationMessage>& OutMessages);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Authoring")
	static bool BakeToCompiledAsset(
		const USubmarineAuthoringAsset* AuthoringAsset,
		UCompiledSubmarineAsset* TargetAsset,
		UPARAM(ref) TArray<FLayoutValidationMessage>& OutMessages);
};
