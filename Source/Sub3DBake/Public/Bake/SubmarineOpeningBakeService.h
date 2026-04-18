#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DClosureTypes.h"
#include "Types/Sub3DConnectorTypes.h"
#include "Types/Sub3DFloorTypes.h"
#include "Types/Sub3DOpeningTypes.h"

namespace Sub3DWave6
{
class SUB3DBAKE_API FSubmarineOpeningBakeService
{
public:
    static bool BakeOpenings(
        const TArray<FCompiledFloorRegionData>& FloorRegions,
        const TArray<FOpeningDef>& Openings,
        TArray<FCompiledOpeningData>& OutCompiledOpenings,
        TArray<FString>& OutErrors);

    static bool BakeConnectors(
        const TArray<FCompiledDeckData>& Decks,
        const TArray<FCompiledOpeningData>& Openings,
        const TArray<FConnectorDef>& Connectors,
        TArray<FCompiledConnectorData>& OutCompiledConnectors,
        TArray<FString>& OutErrors);

    static bool BakeClosures(
        const TArray<FCompiledOpeningData>& Openings,
        const TArray<FClosureDef>& Closures,
        TArray<FCompiledClosureData>& OutCompiledClosures,
        TArray<FString>& OutErrors);
};
} // namespace Sub3DWave6