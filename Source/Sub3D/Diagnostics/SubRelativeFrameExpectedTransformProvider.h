#pragma once

#include "CoreMinimal.h"
#include "Interfaces/RuntimeSyncExpectedTransformProvider.h"
#include "SubRelativeFrameExpectedTransformProvider.generated.h"

class ASubmarineBase;

/**
 * Sub3D-specific provider: computes the expected transform of a monitored character
 * based on its current MovementBase chain back to a submarine actor.
 * Reads the sub's actor transform directly (no InteriorFrame middleman).
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class SUB3D_API USubRelativeFrameExpectedTransformProvider : public URuntimeSyncExpectedTransformProvider
{
	GENERATED_BODY()

public:
	//~ Begin URuntimeSyncExpectedTransformProvider Interface
	virtual bool GetExpectedTransform(const AActor* MonitoredActor, FTransform& OutTransform) const override;
	//~ End URuntimeSyncExpectedTransformProvider Interface

protected:
	virtual ASubmarineBase* FindActiveSubmarine(const AActor* TargetActor) const;

	mutable FTransform LastKnownRelativeTransform;
	mutable bool bHasLastKnownRelative = false;
};
