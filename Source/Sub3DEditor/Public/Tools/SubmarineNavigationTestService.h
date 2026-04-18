#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace Sub3DWave6
{
class SUB3DEDITOR_API FSubmarineNavigationTestService
{
public:
    static bool RunPIENavigationSmoke(UWorld* InEditorWorld);
};
} // namespace Sub3DWave6