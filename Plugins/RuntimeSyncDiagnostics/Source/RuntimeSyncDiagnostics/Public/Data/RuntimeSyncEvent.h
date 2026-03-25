#pragma once

#include "CoreMinimal.h"
#include "RuntimeSyncEvent.generated.h"

/**
 * Represents a discrete diagnostic event (e.g. Snap, Correction, BaseChange).
 */
USTRUCT(BlueprintType)
struct RUNTIMESYNCDIAGNOSTICS_API FRuntimeSyncEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	float Timestamp = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FName EventType;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FString Context;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	float NumericValue = 0.f;
};
