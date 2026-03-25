#pragma once

#include "CoreMinimal.h"
#include "Interfaces/RuntimeSyncExpectedTransformProvider.h"
#include "RelativeFrameExpectedTransformProvider.generated.h"

/**
 * A provider that calculates expected world transform based on a predefined 
 * relative transform against a moving parent frame component.
 */
UCLASS()
class RUNTIMESYNCDIAGNOSTICS_API URelativeFrameExpectedTransformProvider : public URuntimeSyncExpectedTransformProvider
{
	GENERATED_BODY()

public:

	URelativeFrameExpectedTransformProvider();

	/** The moving frame of reference. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics")
	TWeakObjectPtr<USceneComponent> FrameComponent;

	/** The expected relative transform in coordinates of the FrameComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics")
	FTransform RelativeExpectedTransform;

	virtual bool GetExpectedTransform(const AActor* MonitoredActor, FTransform& OutTransform) const override;
};
