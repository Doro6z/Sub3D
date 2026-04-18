#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DBayTypes.h"
#include "Types/Sub3DEnvelopeTypes.h"
#include "Types/Sub3DHullTypes.h"

namespace Sub3DWave3
{
class SUB3DBAKE_API FSubmarineBaySolveService
{
public:
    static bool SolveStructuralBays(
        const FSubmarineHullDef& Hull,
        const TArray<FFrameRingDef>& FrameRings,
        const TArray<FStructuralBayDef>& StructuralBays,
        TArray<FCompiledBayData>& OutCompiledBays,
        TArray<FString>& OutErrors);
};
} // namespace Sub3DWave3