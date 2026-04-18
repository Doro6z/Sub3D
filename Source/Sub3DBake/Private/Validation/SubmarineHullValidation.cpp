#include "Validation/SubmarineHullValidation.h"

namespace Sub3DWave1
{
namespace
{
static bool ValidatePositiveDimension(const FString& Label, const float Value, TArray<FString>& OutErrors)
{
    if (Value <= KINDA_SMALL_NUMBER)
    {
        OutErrors.Add(FString::Printf(TEXT("%s must be greater than zero."), *Label));
        return false;
    }

    return true;
}
} // namespace

bool FSubmarineHullValidation::ValidateGeometry(
    const FSubmarineHullDef& Hull,
    const TArray<FControlRingDef>& ControlRings,
    TArray<FString>& OutErrors)
{
    OutErrors.Reset();
    bool bIsValid = true;

    bIsValid &= ValidatePositiveDimension(TEXT("Hull.LengthCm"), Hull.LengthCm, OutErrors);
    bIsValid &= ValidatePositiveDimension(TEXT("Hull.DefaultHalfWidthCm"), Hull.DefaultHalfWidthCm, OutErrors);
    bIsValid &= ValidatePositiveDimension(TEXT("Hull.DefaultHalfHeightCm"), Hull.DefaultHalfHeightCm, OutErrors);

    const float DefaultMinHalfExtent = FMath::Min(Hull.DefaultHalfWidthCm, Hull.DefaultHalfHeightCm);
    if (Hull.DefaultWallThicknessCm < 0.0f || Hull.DefaultWallThicknessCm >= DefaultMinHalfExtent)
    {
        OutErrors.Add(TEXT("Hull.DefaultWallThicknessCm must be >= 0 and smaller than the smallest hull half extent."));
        bIsValid = false;
    }

    float PreviousX = -FLT_MAX;
    for (int32 Index = 0; Index < ControlRings.Num(); ++Index)
    {
        const FControlRingDef& Ring = ControlRings[Index];
        const FString RingLabel = FString::Printf(TEXT("ControlRing[%d]"), Index);

        if (Ring.PositionX < 0.0f || Ring.PositionX > Hull.LengthCm)
        {
            OutErrors.Add(FString::Printf(TEXT("%s.PositionX must be in [0, Hull.LengthCm]."), *RingLabel));
            bIsValid = false;
        }

        if (!ValidatePositiveDimension(FString::Printf(TEXT("%s.HalfWidthCm"), *RingLabel), Ring.HalfWidthCm, OutErrors))
        {
            bIsValid = false;
        }

        if (!ValidatePositiveDimension(FString::Printf(TEXT("%s.HalfHeightCm"), *RingLabel), Ring.HalfHeightCm, OutErrors))
        {
            bIsValid = false;
        }

        const float RingMinHalfExtent = FMath::Min(Ring.HalfWidthCm, Ring.HalfHeightCm);
        if (Ring.WallThicknessCm < 0.0f || Ring.WallThicknessCm >= RingMinHalfExtent)
        {
            OutErrors.Add(FString::Printf(
                TEXT("%s.WallThicknessCm must be >= 0 and smaller than the smallest ring half extent."),
                *RingLabel));
            bIsValid = false;
        }

        if (Index > 0 && Ring.PositionX <= PreviousX)
        {
            OutErrors.Add(TEXT("Control rings must be strictly increasing by PositionX for stable interpolation."));
            bIsValid = false;
        }

        PreviousX = Ring.PositionX;
    }

    return bIsValid;
}
} // namespace Sub3DWave1
