#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/Sub3DHullTypes.h"
#include "HullProfileEvaluator.generated.h"

/**
 * Pure math functions for evaluating hull profile radius at any longitudinal position.
 * Stateless — all inputs are passed as parameters.
 */
UCLASS()
class SUB3DBUILDER_API UHullProfileEvaluator : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Evaluate hull radius at normalized position T (0..1) using the given profile params.
     *  Returns the half-width in cm at that position. */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Hull|Profile")
    static float EvaluateRadius(
        const FHullProfileParams& ProfileParams,
        const FSubmarineHullDef& Hull,
        float NormalizedT);

    /** Myring body-of-revolution.
     *  Nose:    r(t) = MaxR * (1 - (1 - t/tn)^n)^(1/n)
     *  Midbody: r = MaxR
     *  Tail:    r(t) = MaxR - (MaxR - MaxR*cos(theta)) * ((t-ts)/(1-ts))^2 */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Hull|Profile")
    static float EvaluateMyring(
        float NormalizedT,
        float MaxRadius,
        float NoseExponent,
        float NoseFraction,
        float TailFraction,
        float TailAngleDeg,
        float ParallelMidbodyFraction);

    /** Series 58 (DTMB) axisymmetric polynomial body. */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Hull|Profile")
    static float EvaluateSeries58(
        float NormalizedT,
        float MaxRadius,
        float Fineness,
        float ParallelMidbodyFraction);

    /** Superellipse longitudinal: r(t) = MaxR * (1 - |2t-1|^n)^(1/n). */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Hull|Profile")
    static float EvaluateSuperellipse(
        float NormalizedT,
        float MaxRadius,
        float Exponent,
        float ParallelMidbodyFraction);

    /** Uniform cylinder with hemispherical end caps. */
    UFUNCTION(BlueprintCallable, Category="Sub3D|Hull|Profile")
    static float EvaluateUniform(
        float NormalizedT,
        float MaxRadius,
        float ParallelMidbodyFraction);
};
