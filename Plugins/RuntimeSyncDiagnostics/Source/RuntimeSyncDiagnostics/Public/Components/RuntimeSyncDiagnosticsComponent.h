#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RuntimeSyncSample.h"
#include "Interfaces/RuntimeSyncExpectedTransformProvider.h"
#include "Subsystems/RuntimeSyncRecorderSubsystem.h"
#include "RuntimeSyncDiagnosticsComponent.generated.h"

/**
 * Core component to track network/movement coherence state of its owner.
 */
UCLASS(ClassGroup=(Diagnostics), meta=(BlueprintSpawnableComponent))
class RUNTIMESYNCDIAGNOSTICS_API URuntimeSyncDiagnosticsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	URuntimeSyncDiagnosticsComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Starts tracking diagnostics. */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	void StartDiagnostics();

	/** Stops tracking diagnostics. */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	void StopDiagnostics();

	/** Notifies the diagnostics system that a snap occurred (e.g. from network correction or teleport). */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	void MarkSnap(float SnapDistanceCm);

	/** Notifies the diagnostics system that an authoritative correction occurred. */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	void MarkCorrection(float CorrectionDistanceCm);

	/** Adds a custom event to the event timeline. */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	void MarkCustomEvent(FName EventType, const FString& Context, float NumericValue = 0.f);

	/** Master switch for this component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics")
	bool bEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics")
	bool bRecordSamples;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics")
	bool bDrawDebug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics")
	bool bLogEvents;

	/** How many times per second to sample. 0 means disable sampling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics")
	float SampleRateHz;

	/** Distance beyond which an error is considered significant enough to trigger a warning/event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics")
	float ErrorWarningThresholdCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics")
	bool bTrackMovementBase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics")
	bool bTrackFloorState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Diagnostics")
	bool bTrackBaseDeltas;

	/** The strategy used to calculate where this actor *should* be. */
	UPROPERTY(EditAnywhere, Instanced, Category="Diagnostics")
	URuntimeSyncExpectedTransformProvider* ExpectedTransformProvider;

protected:

	void SampleNow(float DeltaTime);
	void UpdateMovementBaseTracking();
	void UpdateFloorTracking();
	void UpdateParentFrameTracking();
	bool BuildSample(FRuntimeSyncSample& OutSample) const;

	UPROPERTY()
	URuntimeSyncRecorderSubsystem* CachedRecorder;

	float TimeSinceLastSample;

	// Cached previous states for delta/event detection
	TWeakObjectPtr<UPrimitiveComponent> LastMovementBase;
	FString LastFloorStateString;

	FTransform LastBaseTransform;
	float LastBaseDeltaCm;
	
	FString CurrentTickGroupName;
	
	FTransform LastParentTransform;
	float LastParentDeltaCm = 0.f;
	bool bTrackParentFrame = false;

	int32 SnapCount;
	int32 CorrectionCount;
	int32 CapturedFrames;
};
