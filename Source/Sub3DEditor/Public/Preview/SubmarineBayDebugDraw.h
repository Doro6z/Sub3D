#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DBayTypes.h"
#include "Types/Sub3DHullTypes.h"

class UWorld;

namespace Sub3DWave3
{
class SUB3DEDITOR_API FSubmarineBayDebugDraw
{
public:
    static void DrawStructuralBays(
        UWorld* World,
        const FSubmarineHullDef& Hull,
        const TArray<FCompiledBayData>& StructuralBays,
        bool bPersistentLines = false,
        float LifeTime = 0.0f);
};
} // namespace Sub3DWave3