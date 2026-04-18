#include "Bake/SubmarineRingSequenceBuilder.h"

namespace Sub3DWave2
{
namespace
{
struct FSortedControlRing
{
    int32 OriginalIndex = INDEX_NONE;
    float PositionX = 0.0f;
    FControlRingDef Ring;
};

static float SafeRatio(const float HalfWidthCm, const float HalfHeightCm)
{
    const float SafeHeight = FMath::Max(HalfHeightCm, KINDA_SMALL_NUMBER);
    return HalfWidthCm / SafeHeight;
}

static FGeneratedRingData BuildFromRing(const FControlRingDef& Ring, const float SpineAlpha)
{
    FGeneratedRingData Out;
    Out.SpineAlpha = SpineAlpha;
    Out.RadiusCm = FMath::Max(Ring.HalfHeightCm, KINDA_SMALL_NUMBER);
    Out.WidthToHeightRatio = SafeRatio(Ring.HalfWidthCm, Ring.HalfHeightCm);
    Out.Roundness = FMath::Clamp(Ring.SectionRoundness, 0.0f, 1.0f);
    Out.WallThicknessCm = FMath::Clamp(Ring.WallThicknessCm, 0.0f, Out.RadiusCm - 1.0f);
    Out.Profile = Ring.SectionProfile;
    return Out;
}

static FGeneratedRingData BuildDefaultRing(const FSubmarineHullDef& Hull, const float SpineAlpha)
{
    FGeneratedRingData Out;
    Out.SpineAlpha = SpineAlpha;
    Out.RadiusCm = FMath::Max(Hull.DefaultHalfHeightCm, KINDA_SMALL_NUMBER);
    Out.WidthToHeightRatio = SafeRatio(Hull.DefaultHalfWidthCm, Hull.DefaultHalfHeightCm);
    Out.Roundness = FMath::Clamp(Hull.DefaultSectionRoundness, 0.0f, 1.0f);
    Out.WallThicknessCm = FMath::Clamp(Hull.DefaultWallThicknessCm, 0.0f, Out.RadiusCm - 1.0f);
    Out.Profile = Hull.DefaultSectionProfile;
    return Out;
}

static FGeneratedRingData LerpRings(const FGeneratedRingData& A, const FGeneratedRingData& B, const float Alpha)
{
    FGeneratedRingData Out;
    Out.SpineAlpha = FMath::Lerp(A.SpineAlpha, B.SpineAlpha, Alpha);
    Out.RadiusCm = FMath::Lerp(A.RadiusCm, B.RadiusCm, Alpha);
    Out.WidthToHeightRatio = FMath::Lerp(A.WidthToHeightRatio, B.WidthToHeightRatio, Alpha);
    Out.Roundness = FMath::Lerp(A.Roundness, B.Roundness, Alpha);
    Out.WallThicknessCm = FMath::Lerp(A.WallThicknessCm, B.WallThicknessCm, Alpha);
    Out.Profile = (Alpha < 0.5f) ? A.Profile : B.Profile;
    return Out;
}

static int32 FindNearestSequenceIndex(const TArray<FGeneratedRingData>& Sequence, const float TargetAlpha)
{
    if (Sequence.IsEmpty())
    {
        return INDEX_NONE;
    }

    int32 BestIndex = 0;
    float BestDistance = FLT_MAX;
    for (int32 Index = 0; Index < Sequence.Num(); ++Index)
    {
        const float Distance = FMath::Abs(Sequence[Index].SpineAlpha - TargetAlpha);
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            BestIndex = Index;
        }
    }

    return BestIndex;
}
} // namespace

bool FSubmarineRingSequenceBuilder::BuildRingSequence(
    const FSubmarineHullDef& Hull,
    const TArray<FControlRingDef>& ControlRings,
    const int32 TargetRingCount,
    TArray<FGeneratedRingData>& OutSequence,
    TArray<FString>& OutErrors,
    const TArray<FFrameRingDef>& FrameRings)
{
    OutSequence.Reset();
    OutErrors.Reset();

    if (Hull.LengthCm <= KINDA_SMALL_NUMBER)
    {
        OutErrors.Add(TEXT("Hull.LengthCm must be greater than zero."));
        return false;
    }

    if (TargetRingCount < 2)
    {
        OutErrors.Add(TEXT("TargetRingCount must be >= 2."));
        return false;
    }

    TArray<FSortedControlRing> SortedControlRings;
    SortedControlRings.Reserve(ControlRings.Num());
    for (int32 Index = 0; Index < ControlRings.Num(); ++Index)
    {
        const FControlRingDef& Ring = ControlRings[Index];
        FSortedControlRing& Entry = SortedControlRings.AddDefaulted_GetRef();
        Entry.OriginalIndex = Index;
        Entry.PositionX = FMath::Clamp(Ring.PositionX, 0.0f, Hull.LengthCm);
        Entry.Ring = Ring;
        Entry.Ring.PositionX = Entry.PositionX;
    }

    SortedControlRings.StableSort([](const FSortedControlRing& A, const FSortedControlRing& B)
    {
        return A.PositionX < B.PositionX;
    });

    OutSequence.Reserve(TargetRingCount);
    for (int32 RingIndex = 0; RingIndex < TargetRingCount; ++RingIndex)
    {
        const float Alpha = (TargetRingCount > 1) ? static_cast<float>(RingIndex) / static_cast<float>(TargetRingCount - 1) : 0.0f;
        const float X = Alpha * Hull.LengthCm;

        if (SortedControlRings.IsEmpty())
        {
            OutSequence.Add(BuildDefaultRing(Hull, Alpha));
            continue;
        }

        if (SortedControlRings.Num() == 1)
        {
            OutSequence.Add(BuildFromRing(SortedControlRings[0].Ring, Alpha));
            continue;
        }

        if (X <= SortedControlRings[0].PositionX)
        {
            OutSequence.Add(BuildFromRing(SortedControlRings[0].Ring, Alpha));
            continue;
        }

        const int32 LastControl = SortedControlRings.Num() - 1;
        if (X >= SortedControlRings[LastControl].PositionX)
        {
            OutSequence.Add(BuildFromRing(SortedControlRings[LastControl].Ring, Alpha));
            continue;
        }

        bool bAdded = false;
        for (int32 ControlIndex = 0; ControlIndex < LastControl; ++ControlIndex)
        {
            const FSortedControlRing& Left = SortedControlRings[ControlIndex];
            const FSortedControlRing& Right = SortedControlRings[ControlIndex + 1];
            if (X <= Right.PositionX)
            {
                const float Denominator = FMath::Max(Right.PositionX - Left.PositionX, KINDA_SMALL_NUMBER);
                const float LocalAlpha = FMath::Clamp((X - Left.PositionX) / Denominator, 0.0f, 1.0f);
                FGeneratedRingData L = BuildFromRing(Left.Ring, Alpha);
                FGeneratedRingData R = BuildFromRing(Right.Ring, Alpha);
                FGeneratedRingData Out = LerpRings(L, R, LocalAlpha);
                Out.SpineAlpha = Alpha;
                OutSequence.Add(Out);
                bAdded = true;
                break;
            }
        }

        if (!bAdded)
        {
            OutSequence.Add(BuildDefaultRing(Hull, Alpha));
        }
    }

    if (!SortedControlRings.IsEmpty() && SortedControlRings.Num() <= OutSequence.Num())
    {
        int32 MinAllowedIndex = 0;
        for (int32 ControlIndex = 0; ControlIndex < SortedControlRings.Num(); ++ControlIndex)
        {
            const FSortedControlRing& Control = SortedControlRings[ControlIndex];
            const float ControlAlpha = FMath::Clamp(Control.PositionX / Hull.LengthCm, 0.0f, 1.0f);
            const int32 RemainingControls = SortedControlRings.Num() - (ControlIndex + 1);
            const int32 MaxAllowedIndex = (OutSequence.Num() - 1) - RemainingControls;

            int32 PreferredIndex = FMath::RoundToInt(ControlAlpha * static_cast<float>(OutSequence.Num() - 1));
            PreferredIndex = FMath::Clamp(PreferredIndex, MinAllowedIndex, MaxAllowedIndex);

            FGeneratedRingData Forced = BuildFromRing(Control.Ring, ControlAlpha);
            Forced.bIsControlRing = true;
            Forced.SourceControlRingIndex = Control.OriginalIndex;
            OutSequence[PreferredIndex] = Forced;

            MinAllowedIndex = PreferredIndex + 1;
        }
    }

    for (int32 FrameIndex = 0; FrameIndex < FrameRings.Num(); ++FrameIndex)
    {
        const FFrameRingDef& FrameRing = FrameRings[FrameIndex];
        const float FrameAlpha = FMath::Clamp(FrameRing.SpineAlpha, 0.0f, 1.0f);
        const int32 SequenceIndex = FindNearestSequenceIndex(OutSequence, FrameAlpha);
        if (SequenceIndex == INDEX_NONE)
        {
            continue;
        }

        OutSequence[SequenceIndex].bIsFrameRing = true;
        OutSequence[SequenceIndex].SourceFrameRingIndex = FrameIndex;
    }

    for (int32 Index = 1; Index < OutSequence.Num(); ++Index)
    {
        if (OutSequence[Index].SpineAlpha <= OutSequence[Index - 1].SpineAlpha)
        {
            OutErrors.Add(TEXT("Generated ring sequence is not strictly increasing by SpineAlpha."));
            return false;
        }
    }

    return true;
}

bool FSubmarineRingSequenceBuilder::ValidateRingContinuity(
    const TArray<FGeneratedRingData>& Sequence,
    const float MaxRadiusJumpCm,
    TArray<FString>& OutErrors)
{
    OutErrors.Reset();

    if (Sequence.Num() < 2)
    {
        OutErrors.Add(TEXT("Sequence must contain at least two generated rings."));
        return false;
    }

    if (MaxRadiusJumpCm <= 0.0f)
    {
        OutErrors.Add(TEXT("MaxRadiusJumpCm must be greater than zero."));
        return false;
    }

    bool bValid = true;
    for (int32 Index = 1; Index < Sequence.Num(); ++Index)
    {
        if (Sequence[Index].SpineAlpha <= Sequence[Index - 1].SpineAlpha)
        {
            OutErrors.Add(FString::Printf(TEXT("SpineAlpha not increasing at index %d."), Index));
            bValid = false;
        }

        const float RadiusJump = FMath::Abs(Sequence[Index].RadiusCm - Sequence[Index - 1].RadiusCm);
        if (RadiusJump > MaxRadiusJumpCm)
        {
            OutErrors.Add(FString::Printf(
                TEXT("Radius jump too high between index %d and %d (%.3f cm)."),
                Index - 1,
                Index,
                RadiusJump));
            bValid = false;
        }
    }

    return bValid;
}
} // namespace Sub3DWave2