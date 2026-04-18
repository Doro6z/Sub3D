#include "Bake/SubmarineBaySolveService.h"

namespace Sub3DWave3
{
namespace
{
struct FBaySpan
{
    FName BayId = NAME_None;
    float StartAlpha = 0.0f;
    float EndAlpha = 1.0f;
    int32 RequestedDeckLevels = 1;
    TArray<FName> BoundaryFrameRingIds;
};

static int32 ComputeMaxDeckLevels(const FSubmarineHullDef& Hull)
{
    const float HullInteriorHeightCm = FMath::Max((Hull.DefaultHalfHeightCm * 2.0f) - (2.0f * Hull.DefaultWallThicknessCm), 100.0f);
    return FMath::Max(1, FMath::FloorToInt(HullInteriorHeightCm / 220.0f));
}

static bool ValidateSpan(const FBaySpan& Span, TArray<FString>& OutErrors)
{
    bool bValid = true;

    if (Span.StartAlpha < 0.0f || Span.StartAlpha > 1.0f || Span.EndAlpha < 0.0f || Span.EndAlpha > 1.0f)
    {
        OutErrors.Add(FString::Printf(TEXT("Structural Bay '%s' must stay inside [0..1]."), *Span.BayId.ToString()));
        bValid = false;
    }

    if (Span.EndAlpha <= Span.StartAlpha)
    {
        OutErrors.Add(FString::Printf(TEXT("Structural Bay '%s' must have EndAlpha > StartAlpha."), *Span.BayId.ToString()));
        bValid = false;
    }

    return bValid;
}
} // namespace

bool FSubmarineBaySolveService::SolveStructuralBays(
    const FSubmarineHullDef& Hull,
    const TArray<FFrameRingDef>& FrameRings,
    const TArray<FStructuralBayDef>& StructuralBays,
    TArray<FCompiledBayData>& OutCompiledBays,
    TArray<FString>& OutErrors)
{
    OutCompiledBays.Reset();
    OutErrors.Reset();

    if (Hull.LengthCm <= KINDA_SMALL_NUMBER)
    {
        OutErrors.Add(TEXT("Hull.LengthCm must be > 0 for Structural Bay solve."));
        return false;
    }

    TArray<FBaySpan> CandidateSpans;

    if (!StructuralBays.IsEmpty())
    {
        CandidateSpans.Reserve(StructuralBays.Num());
        for (int32 Index = 0; Index < StructuralBays.Num(); ++Index)
        {
            const FStructuralBayDef& Bay = StructuralBays[Index];
            FBaySpan& Span = CandidateSpans.AddDefaulted_GetRef();
            Span.BayId = Bay.BayId.IsNone() ? FName(*FString::Printf(TEXT("StructuralBay_%d"), Index)) : Bay.BayId;
            Span.StartAlpha = Bay.StartAlpha;
            Span.EndAlpha = Bay.EndAlpha;
            Span.RequestedDeckLevels = FMath::Max(1, Bay.RequestedDeckLevels);
        }
    }
    else
    {
        TArray<const FFrameRingDef*> Boundaries;
        for (const FFrameRingDef& FrameRing : FrameRings)
        {
            if (FrameRing.bIsBayBoundary)
            {
                Boundaries.Add(&FrameRing);
            }
        }

        Boundaries.StableSort([](const FFrameRingDef& A, const FFrameRingDef& B)
        {
            return A.SpineAlpha < B.SpineAlpha;
        });

        if (Boundaries.Num() < 2)
        {
            FBaySpan& DefaultSpan = CandidateSpans.AddDefaulted_GetRef();
            DefaultSpan.BayId = TEXT("StructuralBay_0");
            DefaultSpan.StartAlpha = 0.0f;
            DefaultSpan.EndAlpha = 1.0f;
            DefaultSpan.RequestedDeckLevels = 1;
        }
        else
        {
            CandidateSpans.Reserve(Boundaries.Num() - 1);
            for (int32 Index = 0; Index + 1 < Boundaries.Num(); ++Index)
            {
                const FFrameRingDef& Left = *Boundaries[Index];
                const FFrameRingDef& Right = *Boundaries[Index + 1];

                FBaySpan& Span = CandidateSpans.AddDefaulted_GetRef();
                Span.BayId = FName(*FString::Printf(TEXT("StructuralBay_%d"), Index));
                Span.StartAlpha = FMath::Clamp(Left.SpineAlpha, 0.0f, 1.0f);
                Span.EndAlpha = FMath::Clamp(Right.SpineAlpha, 0.0f, 1.0f);
                Span.RequestedDeckLevels = 1;
                Span.BoundaryFrameRingIds.Add(Left.FrameRingId);
                Span.BoundaryFrameRingIds.Add(Right.FrameRingId);
            }
        }
    }

    bool bValid = true;
    for (const FBaySpan& Span : CandidateSpans)
    {
        bValid &= ValidateSpan(Span, OutErrors);
    }

    CandidateSpans.StableSort([](const FBaySpan& A, const FBaySpan& B)
    {
        return A.StartAlpha < B.StartAlpha;
    });

    for (int32 Index = 1; Index < CandidateSpans.Num(); ++Index)
    {
        if (CandidateSpans[Index].StartAlpha < CandidateSpans[Index - 1].EndAlpha)
        {
            OutErrors.Add(FString::Printf(
                TEXT("Structural Bay overlap between '%s' and '%s'."),
                *CandidateSpans[Index - 1].BayId.ToString(),
                *CandidateSpans[Index].BayId.ToString()));
            bValid = false;
        }
    }

    if (!bValid)
    {
        return false;
    }

    const int32 GlobalMaxDeckLevels = ComputeMaxDeckLevels(Hull);
    OutCompiledBays.Reserve(CandidateSpans.Num());

    for (const FBaySpan& Span : CandidateSpans)
    {
        FCompiledBayData& OutBay = OutCompiledBays.AddDefaulted_GetRef();
        OutBay.BayId = Span.BayId;
        OutBay.StartAlpha = Span.StartAlpha;
        OutBay.EndAlpha = Span.EndAlpha;
        OutBay.StartX = Span.StartAlpha * Hull.LengthCm;
        OutBay.EndX = Span.EndAlpha * Hull.LengthCm;
        OutBay.MaxDeckLevels = FMath::Clamp(Span.RequestedDeckLevels, 1, GlobalMaxDeckLevels);
        OutBay.BoundaryFrameRingIds = Span.BoundaryFrameRingIds;
    }

    return true;
}
} // namespace Sub3DWave3