#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DHullTypes.h"

namespace Sub3DWave2
{
class SUB3DBAKE_API FSubmarineHullProfileService
{
public:
    static bool GenerateControlRingsFromProfile(
        const FSubmarineHullDef& Hull,
        TArray<FControlRingDef>& OutControlRings,
        TArray<FString>& OutErrors);

    static float EvaluateRadiusFraction(
        ESub3DHullLongitudinalProfile Profile,
        const FHullProfileParams& Params,
        float NormalizedX);
};
} // namespace Sub3DWave2
