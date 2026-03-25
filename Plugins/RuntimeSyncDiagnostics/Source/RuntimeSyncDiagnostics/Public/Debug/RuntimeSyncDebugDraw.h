#pragma once

#include "CoreMinimal.h"
#include "Data/RuntimeSyncSample.h"

/**
 * Utility for drawing diagnostic data into the world.
 */
class RUNTIMESYNCDIAGNOSTICS_API FRuntimeSyncDebugDraw
{
public:

	/** Draws the expected and actual transforms of a sample, plus an error line if applicable. */
	static void DrawActorSample(UWorld* World, const FRuntimeSyncSample& Sample, float Duration = 0.f);

	/** Draws a reference frame representation at a specific transform. */
	static void DrawParentFrame(UWorld* World, const FVector& Origin, const FRotator& Rotation, float AxisLength = 50.f, float Duration = 0.f);
};
