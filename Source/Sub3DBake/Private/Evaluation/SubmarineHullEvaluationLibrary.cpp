#include "Evaluation/SubmarineHullEvaluationLibrary.h"

namespace Sub3DWave1
{
namespace
{
struct FInterpolatedRing
{
    float HalfWidthCm = 0.0f;
    float HalfHeightCm = 0.0f;
    float WallThicknessCm = 0.0f;
    float SectionRoundness = 0.5f;
    ESub3DSectionProfile SectionProfile = ESub3DSectionProfile::Ellipse;
};

static float ClampNonZero(const float Value)
{
    return FMath::Max(Value, KINDA_SMALL_NUMBER);
}

static float ToExponent(const float SectionRoundness)
{
    return FMath::Lerp(1.2f, 6.0f, FMath::Clamp(SectionRoundness, 0.0f, 1.0f));
}

static FInterpolatedRing BuildDefaultRing(const FSubmarineHullDef& Hull)
{
    FInterpolatedRing Ring;
    Ring.HalfWidthCm = ClampNonZero(Hull.DefaultHalfWidthCm);
    Ring.HalfHeightCm = ClampNonZero(Hull.DefaultHalfHeightCm);
    Ring.WallThicknessCm = FMath::Max(0.0f, Hull.DefaultWallThicknessCm);
    Ring.SectionRoundness = FMath::Clamp(Hull.DefaultSectionRoundness, 0.0f, 1.0f);
    Ring.SectionProfile = Hull.DefaultSectionProfile;
    return Ring;
}

static FInterpolatedRing ConvertRing(const FControlRingDef& InRing)
{
    FInterpolatedRing Ring;
    Ring.HalfWidthCm = ClampNonZero(InRing.HalfWidthCm);
    Ring.HalfHeightCm = ClampNonZero(InRing.HalfHeightCm);
    Ring.WallThicknessCm = FMath::Max(0.0f, InRing.WallThicknessCm);
    Ring.SectionRoundness = FMath::Clamp(InRing.SectionRoundness, 0.0f, 1.0f);
    Ring.SectionProfile = InRing.SectionProfile;
    return Ring;
}

static FInterpolatedRing LerpRing(const FInterpolatedRing& A, const FInterpolatedRing& B, const float Alpha)
{
    FInterpolatedRing Ring;
    Ring.HalfWidthCm = FMath::Lerp(A.HalfWidthCm, B.HalfWidthCm, Alpha);
    Ring.HalfHeightCm = FMath::Lerp(A.HalfHeightCm, B.HalfHeightCm, Alpha);
    Ring.WallThicknessCm = FMath::Lerp(A.WallThicknessCm, B.WallThicknessCm, Alpha);
    Ring.SectionRoundness = FMath::Lerp(A.SectionRoundness, B.SectionRoundness, Alpha);
    Ring.SectionProfile = (A.SectionProfile == B.SectionProfile) ? A.SectionProfile : ESub3DSectionProfile::Ellipse;
    return Ring;
}

static FInterpolatedRing InterpolateRingAtX(
    const FSubmarineHullDef& Hull,
    const TArray<FControlRingDef>& ControlRings,
    const float X)
{
    if (ControlRings.IsEmpty())
    {
        return BuildDefaultRing(Hull);
    }

    TArray<const FControlRingDef*> SortedRings;
    SortedRings.Reserve(ControlRings.Num());
    for (const FControlRingDef& Ring : ControlRings)
    {
        SortedRings.Add(&Ring);
    }

    SortedRings.StableSort([](const FControlRingDef& A, const FControlRingDef& B)
    {
        return A.PositionX < B.PositionX;
    });

    if (SortedRings.Num() == 1)
    {
        return ConvertRing(*SortedRings[0]);
    }

    if (X <= SortedRings[0]->PositionX)
    {
        return ConvertRing(*SortedRings[0]);
    }

    const int32 LastIndex = SortedRings.Num() - 1;
    if (X >= SortedRings[LastIndex]->PositionX)
    {
        return ConvertRing(*SortedRings[LastIndex]);
    }

    for (int32 Index = 0; Index < LastIndex; ++Index)
    {
        const FControlRingDef& Left = *SortedRings[Index];
        const FControlRingDef& Right = *SortedRings[Index + 1];
        if (X <= Right.PositionX)
        {
            const float Denominator = Right.PositionX - Left.PositionX;
            if (FMath::IsNearlyZero(Denominator))
            {
                return ConvertRing(Right);
            }

            const float Alpha = FMath::Clamp((X - Left.PositionX) / Denominator, 0.0f, 1.0f);
            return LerpRing(ConvertRing(Left), ConvertRing(Right), Alpha);
        }
    }

    return BuildDefaultRing(Hull);
}
} // namespace

USubmarineHullEvaluationLibrary::FSectionParams USubmarineHullEvaluationLibrary::EvaluateSectionParams(
    const FSubmarineHullDef& Hull,
    const TArray<FControlRingDef>& ControlRings,
    const float X)
{
    const FInterpolatedRing Ring = InterpolateRingAtX(Hull, ControlRings, X);
    
    FSectionParams Params;
    Params.HalfWidthCm = Ring.HalfWidthCm;
    Params.HalfHeightCm = Ring.HalfHeightCm;
    Params.SectionRoundness = Ring.SectionRoundness;
    Params.SectionProfile = Ring.SectionProfile;
    Params.WallThicknessCm = Ring.WallThicknessCm;
    
    return Params;
}

FVector USubmarineHullEvaluationLibrary::EvaluateSectionPoint(
    const FSubmarineHullDef& Hull,
    const TArray<FControlRingDef>& ControlRings,
    const float X,
    const float ArcAlpha)
{
    const FInterpolatedRing Ring = InterpolateRingAtX(Hull, ControlRings, X);
    const float Arc = FMath::Clamp(ArcAlpha, 0.0f, 1.0f) * UE_TWO_PI;

    float Y = 0.0f;
    float Z = 0.0f;

    switch (Ring.SectionProfile)
    {
    case ESub3DSectionProfile::Circle:
        {
            const float Radius = FMath::Min(Ring.HalfWidthCm, Ring.HalfHeightCm);
            Y = Radius * FMath::Cos(Arc);
            Z = Radius * FMath::Sin(Arc);
        }
        break;
    case ESub3DSectionProfile::Superellipse:
        {
            const float Exponent = ToExponent(Ring.SectionRoundness);
            const float CosValue = FMath::Cos(Arc);
            const float SinValue = FMath::Sin(Arc);
            const float CosPow = FMath::Sign(CosValue) * FMath::Pow(FMath::Abs(CosValue), 2.0f / Exponent);
            const float SinPow = FMath::Sign(SinValue) * FMath::Pow(FMath::Abs(SinValue), 2.0f / Exponent);
            Y = Ring.HalfWidthCm * CosPow;
            Z = Ring.HalfHeightCm * SinPow;
        }
        break;
    case ESub3DSectionProfile::Ellipse:
    default:
        Y = Ring.HalfWidthCm * FMath::Cos(Arc);
        Z = Ring.HalfHeightCm * FMath::Sin(Arc);
        break;
    }

    return FVector(X, Y, Z);
}

float USubmarineHullEvaluationLibrary::EvaluateWallThicknessAtX(
    const FSubmarineHullDef& Hull,
    const TArray<FControlRingDef>& ControlRings,
    const float X)
{
    const FInterpolatedRing Ring = InterpolateRingAtX(Hull, ControlRings, X);
    const float MaxThickness = FMath::Max(0.0f, FMath::Min(Ring.HalfWidthCm, Ring.HalfHeightCm) - 1.0f);
    return FMath::Clamp(Ring.WallThicknessCm, 0.0f, MaxThickness);
}
} // namespace Sub3DWave1
