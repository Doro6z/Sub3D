#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DHullTypes.h"

namespace Sub3DWave1
{
class SUB3DBAKE_API FSubmarineHullValidation
{
public:
    static bool ValidateGeometry(
        const FSubmarineHullDef& Hull,
        const TArray<FControlRingDef>& ControlRings,
        TArray<FString>& OutErrors);
};
} // namespace Sub3DWave1
