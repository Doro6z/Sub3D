#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DCompiledTypes.h"
#include "Bake/SubmarineRingSequenceBuilder.h"

namespace Sub3DWave2
{
struct SUB3DBAKE_API FHullBakeSettings
{
    int32 RadialSegments = 32;
    bool bBakeCollision = true;
    int32 CollisionRadialSegments = 12;
};

class SUB3DBAKE_API FSubmarineHullBakeService
{
public:
    static bool BakeExteriorHull(
        const FSubmarineHullDef& Hull,
        const TArray<FGeneratedRingData>& RingSequence,
        const FHullBakeSettings& Settings,
        FCompiledMeshSection& OutSection);

    static bool BakeInteriorHull(
        const FSubmarineHullDef& Hull,
        const TArray<FGeneratedRingData>& RingSequence,
        const FHullBakeSettings& Settings,
        FCompiledMeshSection& OutSection);

    static bool BakeHullCollisionProxy(
        const FSubmarineHullDef& Hull,
        const TArray<FGeneratedRingData>& RingSequence,
        const FHullBakeSettings& Settings,
        FCompiledMeshSection& OutCollision);

    static bool BuildHullOwnership(
        const TArray<FGeneratedRingData>& RingSequence,
        const FCompiledMeshSection& HullSection,
        FCompiledHullOwnership& OutOwnership);
};
} // namespace Sub3DWave2
