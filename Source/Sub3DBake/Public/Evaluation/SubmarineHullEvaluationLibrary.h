#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DHullTypes.h"

namespace Sub3DWave1
{
class SUB3DBAKE_API USubmarineHullEvaluationLibrary
{
public:
    struct FSectionParams
    {
        float HalfWidthCm = 0.0f;
        float HalfHeightCm = 0.0f;
        float SectionRoundness = 0.5f;
        ESub3DSectionProfile SectionProfile = ESub3DSectionProfile::Ellipse;
        float WallThicknessCm = 12.0f;
    };

    static FSectionParams EvaluateSectionParams(
        const FSubmarineHullDef& Hull,
        const TArray<FControlRingDef>& ControlRings,
        float X);

    static FVector EvaluateSectionPoint(
        const FSubmarineHullDef& Hull,
        const TArray<FControlRingDef>& ControlRings,
        float X,
        float ArcAlpha);

    static float EvaluateWallThicknessAtX(
        const FSubmarineHullDef& Hull,
        const TArray<FControlRingDef>& ControlRings,
        float X);
};
} // namespace Sub3DWave1
