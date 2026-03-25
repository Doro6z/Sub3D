#include "Debug/RuntimeSyncDebugDraw.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "RuntimeSyncDiagnosticsSettings.h"

void FRuntimeSyncDebugDraw::DrawActorSample(UWorld* World, const FRuntimeSyncSample& Sample, float Duration)
{
	if (!World) return;

	const URuntimeSyncDiagnosticsSettings* Settings = GetDefault<URuntimeSyncDiagnosticsSettings>();
	const bool bExceedsWarning = (Sample.ErrorDistanceCm > Settings->ErrorWarningThresholdCm);
	const bool bExceedsCritical = (Sample.ErrorDistanceCm > Settings->ErrorCriticalThresholdCm);

	FColor ErrorColor = FColor::Green;
	if (bExceedsCritical)
	{
		ErrorColor = FColor::Red;
	}
	else if (bExceedsWarning)
	{
		ErrorColor = FColor::Yellow;
	}

	// Draw Actual Transform (Blue)
	DrawDebugCoordinateSystem(World, Sample.ActualTransform.GetLocation(), Sample.ActualTransform.Rotator(), 30.f, false, Duration, 0, 1.f);
	
	// Draw Expected Transform (Ghostly/White)
	DrawDebugCoordinateSystem(World, Sample.ExpectedTransform.GetLocation(), Sample.ExpectedTransform.Rotator(), 30.f, false, Duration, 0, 0.5f);
	DrawDebugSphere(World, Sample.ExpectedTransform.GetLocation(), 10.f, 12, FColor::White, false, Duration, 0, 0.5f);

	// Draw Error Line
	if (Sample.ErrorDistanceCm > KINDA_SMALL_NUMBER)
	{
		DrawDebugLine(World, Sample.ActualTransform.GetLocation(), Sample.ExpectedTransform.GetLocation(), ErrorColor, false, Duration, 0, 2.f);
	}
}

void FRuntimeSyncDebugDraw::DrawParentFrame(UWorld* World, const FVector& Origin, const FRotator& Rotation, float AxisLength, float Duration)
{
	if (!World) return;

	DrawDebugCoordinateSystem(World, Origin, Rotation, AxisLength, false, Duration, 0, 2.f);
	DrawDebugBox(World, Origin, FVector(AxisLength * 0.1f), Rotation.Quaternion(), FColor::Cyan, false, Duration, 0, 1.f);
}
