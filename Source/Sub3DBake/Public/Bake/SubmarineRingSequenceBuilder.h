#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DEnvelopeTypes.h"
#include "Types/Sub3DHullTypes.h"

namespace Sub3DWave2
{
struct SUB3DBAKE_API FGeneratedRingData
{
    float SpineAlpha = 0.0f;
    float RadiusCm = 0.0f;
    float WidthToHeightRatio = 1.0f;
    float Roundness = 0.5f;
    float WallThicknessCm = 12.0f;
    ESub3DSectionProfile Profile = ESub3DSectionProfile::Ellipse;
    bool bIsControlRing = false;
    int32 SourceControlRingIndex = INDEX_NONE;
    bool bIsFrameRing = false;
    int32 SourceFrameRingIndex = INDEX_NONE;
};

class SUB3DBAKE_API FSubmarineRingSequenceBuilder
{
public:
    static bool BuildRingSequence(
        const FSubmarineHullDef& Hull,
        const TArray<FControlRingDef>& ControlRings,
        int32 TargetRingCount,
        TArray<FGeneratedRingData>& OutSequence,
        TArray<FString>& OutErrors,
        const TArray<FFrameRingDef>& FrameRings = TArray<FFrameRingDef>());

    static bool ValidateRingContinuity(
        const TArray<FGeneratedRingData>& Sequence,
        float MaxRadiusJumpCm,
        TArray<FString>& OutErrors);
};
} // namespace Sub3DWave2