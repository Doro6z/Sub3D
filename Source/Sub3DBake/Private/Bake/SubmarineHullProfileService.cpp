#include "Bake/SubmarineHullProfileService.h"

namespace Sub3DWave2
{
namespace
{
static float EvaluateMyring(const FHullProfileParams& Params, const float T)
{
    const float NoseFrac = FMath::Clamp(Params.MyringNoseFraction, 0.05f, 0.5f);
    const float TailFrac = FMath::Clamp(Params.MyringTailFraction, 0.05f, 0.5f);
    const float MidStart = NoseFrac;
    const float MidEnd = 1.0f - TailFrac;

    if (T <= 0.0f || T >= 1.0f)
    {
        return 0.0f;
    }

    if (T < MidStart)
    {
        // Nose: power-law r = (1 - ((x - NoseFrac) / NoseFrac)^2)^(n/2)
        const float Xi = (MidStart - T) / MidStart;
        const float N = FMath::Max(Params.MyringNoseExponent, 0.5f);
        return FMath::Pow(FMath::Max(1.0f - FMath::Pow(Xi, 2.0f), 0.0f), N * 0.5f);
    }

    if (T > MidEnd)
    {
        // Tail: cosine-power taper based on tail angle
        const float Xi = (T - MidEnd) / TailFrac;
        const float TailAngleRad = FMath::DegreesToRadians(FMath::Clamp(Params.MyringTailAngleDeg, 5.0f, 60.0f));
        const float CosVal = FMath::Cos(Xi * UE_HALF_PI);
        const float Exponent = FMath::Max(TailAngleRad / UE_HALF_PI * 3.0f, 0.5f);
        return FMath::Pow(FMath::Max(CosVal, 0.0f), Exponent);
    }

    // Parallel midbody
    return 1.0f;
}

static float EvaluateSeries58(const FHullProfileParams& Params, const float T)
{
    // Series 58: polynomial body of revolution
    // Normalized so that max radius = 1.0 at the widest point
    // Uses a 4th-order polynomial fit: r(x) = a0 + a1*x + a2*x^2 + a3*x^3 + a4*x^4
    // where x is normalized to [-1, 1] from bow to stern
    // Simplified parametric form based on fineness ratio
    if (T <= 0.0f || T >= 1.0f)
    {
        return 0.0f;
    }

    const float MidFrac = FMath::Clamp(Params.ParallelMidbodyFraction, 0.0f, 0.8f);
    const float NoseFrac = (1.0f - MidFrac) * 0.45f;
    const float TailFrac = (1.0f - MidFrac) * 0.55f;
    const float MidStart = NoseFrac;
    const float MidEnd = 1.0f - TailFrac;

    if (T < MidStart)
    {
        // Bow: Series 58 nose uses a polynomial approximation
        // r = 1 - (1 - t/NoseFrac)^2.5 (gives the characteristic Series 58 blunt nose)
        const float Xi = 1.0f - (T / MidStart);
        return 1.0f - FMath::Pow(Xi, 2.5f);
    }

    if (T > MidEnd)
    {
        // Stern: more gradual taper
        const float Xi = (T - MidEnd) / TailFrac;
        return 1.0f - FMath::Pow(Xi, 2.0f);
    }

    return 1.0f;
}

static float EvaluateSuperellipseLongitudinal(const FHullProfileParams& Params, const float T)
{
    if (T <= 0.0f || T >= 1.0f)
    {
        return 0.0f;
    }

    const float MidFrac = FMath::Clamp(Params.ParallelMidbodyFraction, 0.0f, 0.8f);

    if (MidFrac <= KINDA_SMALL_NUMBER)
    {
        // Pure superellipsoid: |2t - 1|^n + |r|^n = 1 => r = (1 - |2t-1|^n)^(1/n)
        const float N = FMath::Max(Params.LongitudinalExponent, 1.2f);
        const float Xi = FMath::Abs(2.0f * T - 1.0f);
        return FMath::Pow(FMath::Max(1.0f - FMath::Pow(Xi, N), 0.0f), 1.0f / N);
    }

    const float CapFrac = (1.0f - MidFrac) * 0.5f;
    const float MidStart = CapFrac;
    const float MidEnd = 1.0f - CapFrac;

    if (T < MidStart)
    {
        const float N = FMath::Max(Params.LongitudinalExponent, 1.2f);
        const float Xi = 1.0f - (T / MidStart);
        return FMath::Pow(FMath::Max(1.0f - FMath::Pow(Xi, N), 0.0f), 1.0f / N);
    }

    if (T > MidEnd)
    {
        const float N = FMath::Max(Params.LongitudinalExponent, 1.2f);
        const float Xi = (T - MidEnd) / CapFrac;
        return FMath::Pow(FMath::Max(1.0f - FMath::Pow(Xi, N), 0.0f), 1.0f / N);
    }

    return 1.0f;
}

static float EvaluateUniform(const FHullProfileParams& Params, const float T)
{
    if (T <= 0.0f || T >= 1.0f)
    {
        return 0.0f;
    }

    const float MidFrac = FMath::Clamp(Params.ParallelMidbodyFraction, 0.0f, 0.8f);
    const float CapFrac = (1.0f - MidFrac) * 0.5f;
    const float MidStart = CapFrac;
    const float MidEnd = 1.0f - CapFrac;

    if (T < MidStart)
    {
        // Hemispherical cap (ellipse profile)
        const float Xi = 1.0f - (T / MidStart);
        return FMath::Sqrt(FMath::Max(1.0f - Xi * Xi, 0.0f));
    }

    if (T > MidEnd)
    {
        const float Xi = (T - MidEnd) / CapFrac;
        return FMath::Sqrt(FMath::Max(1.0f - Xi * Xi, 0.0f));
    }

    return 1.0f;
}
} // namespace

float FSubmarineHullProfileService::EvaluateRadiusFraction(
    const ESub3DHullLongitudinalProfile Profile,
    const FHullProfileParams& Params,
    const float NormalizedX)
{
    switch (Profile)
    {
    case ESub3DHullLongitudinalProfile::Myring:
        return EvaluateMyring(Params, NormalizedX);
    case ESub3DHullLongitudinalProfile::Series58:
        return EvaluateSeries58(Params, NormalizedX);
    case ESub3DHullLongitudinalProfile::SuperellipseLongitudinal:
        return EvaluateSuperellipseLongitudinal(Params, NormalizedX);
    case ESub3DHullLongitudinalProfile::Uniform:
        return EvaluateUniform(Params, NormalizedX);
    case ESub3DHullLongitudinalProfile::Manual:
    default:
        return 1.0f;
    }
}

bool FSubmarineHullProfileService::GenerateControlRingsFromProfile(
    const FSubmarineHullDef& Hull,
    TArray<FControlRingDef>& OutControlRings,
    TArray<FString>& OutErrors)
{
    OutControlRings.Reset();
    OutErrors.Reset();

    const FHullProfileParams& Params = Hull.ProfileParams;

    if (Params.Profile == ESub3DHullLongitudinalProfile::Manual)
    {
        OutErrors.Add(TEXT("Manual profile selected: no rings generated. Place control rings manually."));
        return false;
    }

    if (Hull.LengthCm <= KINDA_SMALL_NUMBER)
    {
        OutErrors.Add(TEXT("Hull length must be > 0."));
        return false;
    }

    // Generate 4 default control rings: bow tip, bow shoulder, stern shoulder, stern tip
    // Plus 2 midbody boundary rings for a total of 6 rings for smooth profiles
    struct FRingSample
    {
        float Alpha;
        FName Id;
    };

    TArray<FRingSample> Samples;

    // Determine key positions based on profile
    float NoseEnd = 0.0f;
    float TailStart = 1.0f;

    switch (Params.Profile)
    {
    case ESub3DHullLongitudinalProfile::Myring:
        NoseEnd = FMath::Clamp(Params.MyringNoseFraction, 0.05f, 0.5f);
        TailStart = 1.0f - FMath::Clamp(Params.MyringTailFraction, 0.05f, 0.5f);
        break;
    case ESub3DHullLongitudinalProfile::Series58:
    {
        const float MidFrac = FMath::Clamp(Params.ParallelMidbodyFraction, 0.0f, 0.8f);
        NoseEnd = (1.0f - MidFrac) * 0.45f;
        TailStart = 1.0f - (1.0f - MidFrac) * 0.55f;
        break;
    }
    case ESub3DHullLongitudinalProfile::SuperellipseLongitudinal:
    case ESub3DHullLongitudinalProfile::Uniform:
    {
        const float MidFrac = FMath::Clamp(Params.ParallelMidbodyFraction, 0.0f, 0.8f);
        const float CapFrac = (1.0f - MidFrac) * 0.5f;
        NoseEnd = CapFrac;
        TailStart = 1.0f - CapFrac;
        break;
    }
    default:
        break;
    }

    // Bow tip
    Samples.Add({0.01f, TEXT("Ring_BowTip")});
    // Bow mid
    Samples.Add({NoseEnd * 0.5f, TEXT("Ring_BowMid")});
    // Bow shoulder (start of midbody)
    Samples.Add({NoseEnd, TEXT("Ring_BowShoulder")});
    // Stern shoulder (end of midbody)
    if (TailStart > NoseEnd + 0.05f)
    {
        Samples.Add({TailStart, TEXT("Ring_SternShoulder")});
    }
    // Stern mid
    Samples.Add({TailStart + (1.0f - TailStart) * 0.5f, TEXT("Ring_SternMid")});
    // Stern tip
    Samples.Add({0.99f, TEXT("Ring_SternTip")});

    OutControlRings.Reserve(Samples.Num());
    for (const FRingSample& Sample : Samples)
    {
        const float RadiusFraction = EvaluateRadiusFraction(Params.Profile, Params, Sample.Alpha);

        FControlRingDef Ring;
        Ring.ControlRingId = Sample.Id;
        Ring.PositionX = Sample.Alpha * Hull.LengthCm;
        Ring.HalfWidthCm = Hull.DefaultHalfWidthCm * RadiusFraction;
        Ring.HalfHeightCm = Hull.DefaultHalfHeightCm * RadiusFraction;
        Ring.SectionProfile = Hull.DefaultSectionProfile;
        Ring.SectionRoundness = Hull.DefaultSectionRoundness;
        Ring.WallThicknessCm = Hull.DefaultWallThicknessCm;

        // Ensure minimum ring size at tips
        Ring.HalfWidthCm = FMath::Max(Ring.HalfWidthCm, 1.0f);
        Ring.HalfHeightCm = FMath::Max(Ring.HalfHeightCm, 1.0f);

        OutControlRings.Add(Ring);
    }

    return true;
}
} // namespace Sub3DWave2
