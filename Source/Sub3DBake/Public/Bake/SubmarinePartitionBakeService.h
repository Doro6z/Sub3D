#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DOpeningTypes.h"
#include "Types/Sub3DPartitionTypes.h"

namespace Sub3DWave7
{
class SUB3DBAKE_API FSubmarinePartitionBakeService
{
public:
    static bool BakePartitions(
        const TArray<FCompiledOpeningData>& CompiledOpenings,
        const TArray<FPressureBulkheadDef>& PressureBulkheads,
        const TArray<FInternalWallDef>& InternalWalls,
        TArray<FCompiledPartitionData>& OutCompiledPartitions,
        TArray<FString>& OutErrors);
};
} // namespace Sub3DWave7