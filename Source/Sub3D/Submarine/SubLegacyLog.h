#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// Dedicated category for Phase 7A runtime markers on legacy fallbacks.
// Allows PIE filtering via `LogSubLegacy` without mixing with LogTemp.
// Remove with Phase 7B once the legacy fallbacks are deleted.
SUB3D_API DECLARE_LOG_CATEGORY_EXTERN(LogSubLegacy, Log, All);
