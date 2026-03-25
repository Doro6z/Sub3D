#pragma once

#include "CoreMinimal.h"
#include "Data/RuntimeSyncSample.h"
#include "Data/RuntimeSyncEvent.h"
#include "Data/RuntimeSyncSummary.h"

/**
 * Utility class for serializing diagnostic data to CSV format.
 */
class RUNTIMESYNCDIAGNOSTICS_API URuntimeSyncCsvWriter
{
public:

	/** Writes sample data to the specified absolute path. */
	static bool WriteSamplesCsv(const FString& AbsolutePath, const TArray<FRuntimeSyncSample>& Samples);

	/** Writes event data to the specified absolute path. */
	static bool WriteEventsCsv(const FString& AbsolutePath, const TArray<FRuntimeSyncEvent>& Events);

	/** Writes summary data to the specified absolute path. */
	static bool WriteSummariesCsv(const FString& AbsolutePath, const TArray<FRuntimeSyncSummary>& Summaries);

protected:
	static FString VectorToString(const FVector& Vec);
	static FString RotatorToString(const FRotator& Rot);
};
