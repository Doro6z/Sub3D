#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DBayTypes.h"
#include "Types/Sub3DCompiledTypes.h"
#include "Types/Sub3DFloorTypes.h"
#include "Types/Sub3DHullTypes.h"

namespace Sub3DWave4
{
class SUB3DBAKE_API FSubmarineFloorBakeService
{
public:
    static bool BakeDeckLevels(
        const FSub3DCompiledHullData& HullData,
        const TArray<FCompiledBayData>& StructuralBays,
        const TArray<FDeckLevelDef>& DeckLevels,
        TArray<FCompiledDeckData>& OutCompiledDecks,
        TArray<FString>& OutErrors);

    static bool ValidateFloorRegionWalkability(
        const FSub3DCompiledHullData& HullData,
        const TArray<FCompiledBayData>& StructuralBays,
        const TArray<FCompiledDeckData>& CompiledDecks,
        const TArray<FFloorRegionDef>& FloorRegions,
        TArray<FString>& OutErrors);

    static bool BakeFloorRegions(
        const FSub3DCompiledHullData& HullData,
        const TArray<FCompiledBayData>& StructuralBays,
        const TArray<FCompiledDeckData>& CompiledDecks,
        const TArray<FFloorRegionDef>& FloorRegions,
        TArray<FCompiledFloorRegionData>& OutCompiledFloorRegions,
        TArray<FString>& OutErrors);
};
} // namespace Sub3DWave4