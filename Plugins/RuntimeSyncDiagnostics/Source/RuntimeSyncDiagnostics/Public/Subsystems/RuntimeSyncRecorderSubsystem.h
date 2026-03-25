#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/RuntimeSyncSample.h"
#include "Data/RuntimeSyncEvent.h"
#include "Data/RuntimeSyncSummary.h"
#include "RuntimeSyncRecorderSubsystem.generated.h"

/**
 * Subsystem responsible for recording and exporting diagnostic sessions.
 */
UCLASS()
class RUNTIMESYNCDIAGNOSTICS_API URuntimeSyncRecorderSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	URuntimeSyncRecorderSubsystem();

	// UWorldSubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	/** Starts recording diagnostic data. */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	void StartCapture(const FString& InScenarioName = TEXT("DefaultScenario"));

	/** Stops recording diagnostic data. */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	void StopCapture();

	/** Clears all recorded data in memory. */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	void ClearCapture();

	/** Returns true if a capture is currently running. */
	UFUNCTION(BlueprintPure, Category="Diagnostics")
	bool IsCapturing() const;

	/** Records a single diagnostic sample. */
	void RecordSample(const FRuntimeSyncSample& Sample);

	/** Records a single diagnostic event. */
	void RecordEvent(const FRuntimeSyncEvent& Event);

	/** Exports all recorded data to CSV files in the specified directory. */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	bool ExportCsv(const FString& Directory);

	/** Exports all recorded data to JSON format in the specified directory. (V1 Stub) */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	bool ExportJson(const FString& Directory);

	/** Gets a read-only view of recorded samples. */
	const TArray<FRuntimeSyncSample>& GetSamples() const { return Samples; }

	/** Gets a read-only view of recorded events. */
	const TArray<FRuntimeSyncEvent>& GetEvents() const { return Events; }

	/** Synthesizes detailed summaries for all tracked actors. */
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	TArray<FRuntimeSyncSummary> BuildSummaries() const;

protected:
	bool bIsCapturing;
	FString SessionId;
	FString ScenarioName;
	FDateTime StartTime;

	TArray<FRuntimeSyncSample> Samples;
	TArray<FRuntimeSyncEvent> Events;

	// Optimization to prevent infinite memory growth
	int32 MaxSamplesToKeep;
	int32 MaxEventsToKeep;
};
