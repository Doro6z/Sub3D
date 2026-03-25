#pragma once

#include "CoreMinimal.h"

/**
 * Utility class to retrieve clean strings for Unreal Network Roles.
 */
class RUNTIMESYNCDIAGNOSTICS_API FRuntimeSyncRoleUtils
{
public:

	/** Returns a human-readable string for an ENetRole. */
	static FString GetRoleString(ENetRole Role);
};
