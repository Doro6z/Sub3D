#pragma once

#include "CoreMinimal.h"
#include "RuntimeSyncSummary.generated.h"

/**
 * A synthesized summary of a diagnostic session for an actor.
 */
USTRUCT(BlueprintType)
struct RUNTIMESYNCDIAGNOSTICS_API FRuntimeSyncSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FString ActorName;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	int32 TotalSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	float MaxErrorDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	float AverageErrorDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	int32 TotalSnaps = 0;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	int32 TotalCorrections = 0;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	int32 TotalBaseChanges = 0;
};
