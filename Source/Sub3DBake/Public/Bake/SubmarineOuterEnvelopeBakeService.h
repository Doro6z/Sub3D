#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DEnvelopeTypes.h"
#include "Types/Sub3DHullTypes.h"

struct FCompiledMeshSection;

namespace Sub3DWave2
{
struct FGeneratedRingData;
}

namespace Sub3DWave3
{
class SUB3DBAKE_API FSubmarineOuterEnvelopeBakeService
{
public:
    static bool BakeOuterEnvelope(
        const FSubmarineHullDef& Hull,
        const TArray<Sub3DWave2::FGeneratedRingData>& RingSequence,
        const FOuterEnvelopeDef& OuterEnvelope,
        FCompiledMeshSection& OutEnvelopeSection,
        FCompiledMeshSection& OutCollisionSection,
        TArray<FString>& OutErrors);
};
} // namespace Sub3DWave3