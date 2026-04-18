#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DBayTypes.h"
#include "Types/Sub3DClosureTypes.h"
#include "Types/Sub3DConnectorTypes.h"
#include "Types/Sub3DFloodTypes.h"
#include "Types/Sub3DFloorTypes.h"

namespace Sub3DWave8
{
class SUB3DBAKE_API FSubmarineFloodGraphBuilder
{
public:
    static bool BuildFloodGraph(
        const TArray<FCompiledBayData>& StructuralBays,
        const TArray<FCompiledDeckData>& Decks,
        const TArray<FCompiledConnectorData>& Connectors,
        const TArray<FCompiledClosureData>& Closures,
        FCompiledFloodGraph& OutFloodGraph,
        TArray<FString>& OutErrors);
};
} // namespace Sub3DWave8
