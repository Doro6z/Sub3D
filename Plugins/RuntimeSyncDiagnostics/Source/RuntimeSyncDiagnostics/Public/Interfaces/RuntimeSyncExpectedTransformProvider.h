#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RuntimeSyncExpectedTransformProvider.generated.h"

/**
 * Base abstract provider to determine what the 'expected' transform
 * for a monitored actor should be at runtime.
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class RUNTIMESYNCDIAGNOSTICS_API URuntimeSyncExpectedTransformProvider : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Calculates the expected world transform for the given monitored actor.
	 * @param MonitoredActor The actor whose expected transform is being queried.
	 * @param OutTransform   The resulting expected world transform.
	 * @return true if a valid expected transform could be produced.
	 */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	virtual bool GetExpectedTransform(const AActor* MonitoredActor, FTransform& OutTransform) const PURE_VIRTUAL(URuntimeSyncExpectedTransformProvider::GetExpectedTransform, return false;);
};
