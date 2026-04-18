#include "Generation/HullProfileEvaluator.h"

float UHullProfileEvaluator::EvaluateRadius(
    const FHullProfileParams& ProfileParams,
    const FSubmarineHullDef& Hull,
    float NormalizedT)
{
    const float MaxR = Hull.DefaultHalfWidthCm;
    NormalizedT = FMath::Clamp(NormalizedT, 0.0f, 1.0f);

    switch (ProfileParams.Profile)
    {
    case ESub3DHullLongitudinalProfile::Myring:
        return EvaluateMyring(
            NormalizedT, MaxR,
            ProfileParams.MyringNoseExponent,
            ProfileParams.MyringNoseFraction,
            ProfileParams.MyringTailFraction,
            ProfileParams.MyringTailAngleDeg,
            ProfileParams.ParallelMidbodyFraction);

    case ESub3DHullLongitudinalProfile::Series58:
        return EvaluateSeries58(
            NormalizedT, MaxR,
            ProfileParams.Series58Fineness,
            ProfileParams.ParallelMidbodyFraction);

    case ESub3DHullLongitudinalProfile::SuperellipseLongitudinal:
        return EvaluateSuperellipse(
            NormalizedT, MaxR,
            ProfileParams.LongitudinalExponent,
            ProfileParams.ParallelMidbodyFraction);

    case ESub3DHullLongitudinalProfile::Uniform:
        return EvaluateUniform(
            NormalizedT, MaxR,
            ProfileParams.ParallelMidbodyFraction);

    case ESub3DHullLongitudinalProfile::Manual:
    default:
        // Manual mode: constant radius (rings override this)
        return MaxR;
    }
}

float UHullProfileEvaluator::EvaluateMyring(
    float T,
    float MaxRadius,
    float NoseExponent,
    float NoseFraction,
    float TailFraction,
    float TailAngleDeg,
    float ParallelMidbodyFraction)
{
    T = FMath::Clamp(T, 0.0f, 1.0f);

    // Zone boundaries
    const float NoseEnd = NoseFraction;
    const float TailStart = 1.0f - TailFraction;

    if (T <= 0.0f || T >= 1.0f)
    {
        // Endpoints: zero radius
        return 0.0f;
    }

    if (T < NoseEnd)
    {
        // Nose zone: power-law profile
        // r(t) = MaxR * (1 - (1 - t/tn)^n)^(1/n)
        const float TLocal = T / NoseEnd;
        const float N = FMath::Max(NoseExponent, 0.5f);
        const float Inner = FMath::Pow(1.0f - TLocal, N);
        return MaxRadius * FMath::Pow(FMath::Max(1.0f - Inner, 0.0f), 1.0f / N);
    }

    if (T > TailStart)
    {
        // Tail zone: cosine-based taper
        const float TLocal = (T - TailStart) / (1.0f - TailStart);
        const float ThetaRad = FMath::DegreesToRadians(FMath::Clamp(TailAngleDeg, 5.0f, 60.0f));
        // Smooth quadratic-cosine blend
        const float Taper = 1.0f - TLocal * TLocal;
        const float CosFactor = FMath::Cos(ThetaRad * TLocal);
        return MaxRadius * Taper * CosFactor;
    }

    // Midbody: full radius
    return MaxRadius;
}

float UHullProfileEvaluator::EvaluateSeries58(
    float T,
    float MaxRadius,
    float Fineness,
    float ParallelMidbodyFraction)
{
    T = FMath::Clamp(T, 0.0f, 1.0f);

    if (T <= 0.0f || T >= 1.0f) return 0.0f;

    // Parallel midbody zone
    const float HalfMidbody = ParallelMidbodyFraction * 0.5f;
    const float MidStart = 0.5f - HalfMidbody;
    const float MidEnd = 0.5f + HalfMidbody;

    if (T >= MidStart && T <= MidEnd)
    {
        return MaxRadius;
    }

    // Series 58 polynomial: r/R = sqrt(1 - (2x/L - 1)^2) approximately
    // More precisely: use 4th order polynomial fit for Series 58
    // r(x) = R * (a1*xi + a2*xi^2 + a3*xi^3 + a4*xi^4)^0.5
    // where xi is the normalized distance from the nose or tail

    float TLocal;
    if (T < MidStart)
    {
        // Nose side: remap 0..MidStart to 0..1
        TLocal = T / MidStart;
    }
    else
    {
        // Tail side: remap MidEnd..1 to 1..0
        TLocal = (1.0f - T) / (1.0f - MidEnd);
    }

    // Simplified Series 58 fit: r = R * sqrt(1 - (1-t)^2.5)
    // This captures the characteristic blunt nose / gradual taper
    const float Factor = 1.0f - FMath::Pow(1.0f - TLocal, 2.5f);
    return MaxRadius * FMath::Sqrt(FMath::Max(Factor, 0.0f));
}

float UHullProfileEvaluator::EvaluateSuperellipse(
    float T,
    float MaxRadius,
    float Exponent,
    float ParallelMidbodyFraction)
{
    T = FMath::Clamp(T, 0.0f, 1.0f);

    if (T <= 0.0f || T >= 1.0f) return 0.0f;

    const float HalfMidbody = ParallelMidbodyFraction * 0.5f;
    const float MidStart = 0.5f - HalfMidbody;
    const float MidEnd = 0.5f + HalfMidbody;

    if (T >= MidStart && T <= MidEnd)
    {
        return MaxRadius;
    }

    // Remap to 0..1 for each cap
    float TLocal;
    if (T < MidStart)
    {
        TLocal = T / MidStart;
    }
    else
    {
        TLocal = (1.0f - T) / (1.0f - MidEnd);
    }

    // Superellipse: r = R * (1 - |1-t|^n)^(1/n)
    const float N = FMath::Max(Exponent, 1.2f);
    const float U = 1.0f - TLocal; // distance from max-radius point
    const float Inner = FMath::Pow(FMath::Abs(U), N);
    return MaxRadius * FMath::Pow(FMath::Max(1.0f - Inner, 0.0f), 1.0f / N);
}

float UHullProfileEvaluator::EvaluateUniform(
    float T,
    float MaxRadius,
    float ParallelMidbodyFraction)
{
    T = FMath::Clamp(T, 0.0f, 1.0f);

    if (T <= 0.0f || T >= 1.0f) return 0.0f;

    const float HalfMidbody = ParallelMidbodyFraction * 0.5f;
    const float MidStart = 0.5f - HalfMidbody;
    const float MidEnd = 0.5f + HalfMidbody;

    if (T >= MidStart && T <= MidEnd)
    {
        return MaxRadius;
    }

    // Hemispherical caps: r = R * sqrt(1 - u^2)
    float TLocal;
    if (T < MidStart)
    {
        TLocal = T / MidStart;
    }
    else
    {
        TLocal = (1.0f - T) / (1.0f - MidEnd);
    }

    const float U = 1.0f - TLocal;
    return MaxRadius * FMath::Sqrt(FMath::Max(1.0f - U * U, 0.0f));
}
