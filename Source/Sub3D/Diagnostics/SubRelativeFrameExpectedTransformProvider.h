#pragma once

#include "CoreMinimal.h"
#include "Interfaces/RuntimeSyncExpectedTransformProvider.h"
#include "SubRelativeFrameExpectedTransformProvider.generated.h"

class USubInteriorFrameComponent;

/**
 * Sub3D-specific provider: computes expected transform based on the player's
 * pure relative position inside the USubInteriorFrameComponent of the submarine.
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
	virtual USubInteriorFrameComponent* FindActiveInteriorFrame(const AActor* TargetActor) const;
	
	mutable FTransform LastKnownRelativeTransform;
	mutable bool bHasLastKnownRelative = false;
};
