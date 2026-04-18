#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DAppendageTypes.h"
#include "Types/Sub3DHullTypes.h"

struct FCompiledMeshSection;
namespace Sub3DWave2 { struct FGeneratedRingData; }

namespace Sub3DWave9
{
/**
 * Generates analytical preview meshes for hull appendages:
 * sail (fin/kiosque), bow sonar dome, stern fairing/cone.
 *
 * These are low-detail hulls suitable for the viewport preview only.
 * They are not part of the full bake pipeline (no collision, no UV).
 */
class SUB3DBAKE_API FSubmarineAppendageBakeService
{
public:
    /** Trapezoidal box extruded from the hull surface at sail SpineAlpha. */
    static bool BakeSail(
        const FSubmarineHullDef& Hull,
        const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
        const FSailDef& Sail,
        FCompiledMeshSection& OutSection);

    /** Half-ellipsoid cap attached at X=0 (bow). */
    static bool BakeBowDome(
        const FSubmarineHullDef& Hull,
        const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
        const FBowSectionDef& BowSection,
        FCompiledMeshSection& OutSection);

    /** Truncated cone / fairing from stern end (X=LengthCm) to propulsor tip. */
    static bool BakeSternFairing(
        const FSubmarineHullDef& Hull,
        const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
        const FSternSectionDef& SternSection,
        FCompiledMeshSection& OutSection);

private:
    /** Sample hull surface radius at a given spine X by linearly interpolating
     *  the ring sequence. Returns DefaultHalfHeightCm if no rings found. */
    static float SampleHullRadiusAtX(
        const FSubmarineHullDef& Hull,
        const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
        float WorldX);
};
} // namespace Sub3DWave9
