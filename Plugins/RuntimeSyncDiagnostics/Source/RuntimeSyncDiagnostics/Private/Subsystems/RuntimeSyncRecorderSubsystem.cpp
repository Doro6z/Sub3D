#include "Subsystems/RuntimeSyncRecorderSubsystem.h"
#include "RuntimeSyncDiagnosticsLog.h"
#include "Utils/RuntimeSyncCsvWriter.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"

URuntimeSyncRecorderSubsystem::URuntimeSyncRecorderSubsystem()
{
	bIsCapturing = false;
	MaxSamplesToKeep = 100000;
	MaxEventsToKeep = 50000;
}

void URuntimeSyncRecorderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("URuntimeSyncRecorderSubsystem Initialized"));
}

void URuntimeSyncRecorderSubsystem::Deinitialize()
{
	ClearCapture();
	Super::Deinitialize();
}

void URuntimeSyncRecorderSubsystem::StartCapture(const FString& InScenarioName)
{
	if (bIsCapturing)
	{
		UE_LOG(LogRuntimeSyncDiagnostics, Warning, TEXT("Capture is already running. Call StopCapture first."));
		return;
	}

	ClearCapture();
	bIsCapturing = true;
	ScenarioName = InScenarioName;
	SessionId = FGuid::NewGuid().ToString();
	StartTime = FDateTime::Now();

	UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("Started capture. Session ID: %s, Scenario: %s"), *SessionId, *ScenarioName);
}

void URuntimeSyncRecorderSubsystem::StopCapture()
{
	if (!bIsCapturing)
	{
		return;
	}

	bIsCapturing = false;
	UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("Stopped capture. Collected %d samples and %d events."), Samples.Num(), Events.Num());
}

void URuntimeSyncRecorderSubsystem::ClearCapture()
{
	Samples.Empty();
	Events.Empty();
}

bool URuntimeSyncRecorderSubsystem::IsCapturing() const
{
	return bIsCapturing;
}

void URuntimeSyncRecorderSubsystem::RecordSample(const FRuntimeSyncSample& Sample)
{
	if (!bIsCapturing)
	{
		return;
	}

	// Circular buffer implementation
	if (Samples.Num() >= MaxSamplesToKeep)
	{
		Samples.RemoveAt(0);
	}

	Samples.Add(Sample);
}

void URuntimeSyncRecorderSubsystem::RecordEvent(const FRuntimeSyncEvent& Event)
{
	if (!bIsCapturing)
	{
		return;
	}

	// Circular buffer implementation
	if (Events.Num() >= MaxEventsToKeep)
	{
		Events.RemoveAt(0);
	}

	Events.Add(Event);
}

bool URuntimeSyncRecorderSubsystem::ExportCsv(const FString& Directory)
{
	if (Samples.IsEmpty() && Events.IsEmpty())
	{
		UE_LOG(LogRuntimeSyncDiagnostics, Warning, TEXT("Nothing to export (No samples, no events)."));
		return false;
	}

	// Ensure directory exists
	IFileManager::Get().MakeDirectory(*Directory, true);

	const FString FormattedTime = StartTime.ToString(TEXT("%Y-%m-%d_%H-%M-%S"));
	const FString BasePath = FPaths::Combine(Directory, FString::Printf(TEXT("SyncDiag_%s_%s"), *ScenarioName, *FormattedTime));

	FString SamplesPath = BasePath + TEXT("_Samples.csv");
	FString EventsPath = BasePath + TEXT("_Events.csv");
	FString SummariesPath = BasePath + TEXT("_Summaries.csv");

	bool bSuccess = true;

	if (Samples.Num() > 0)
	{
		if (!URuntimeSyncCsvWriter::WriteSamplesCsv(SamplesPath, Samples))
		{
			UE_LOG(LogRuntimeSyncDiagnostics, Error, TEXT("Failed to write samples to %s"), *SamplesPath);
			bSuccess = false;
		}
	}

	if (Events.Num() > 0)
	{
		if (!URuntimeSyncCsvWriter::WriteEventsCsv(EventsPath, Events))
		{
			UE_LOG(LogRuntimeSyncDiagnostics, Error, TEXT("Failed to write events to %s"), *EventsPath);
			bSuccess = false;
		}
	}

	TArray<FRuntimeSyncSummary> Summaries = BuildSummaries();
	if (Summaries.Num() > 0)
	{
		if (!URuntimeSyncCsvWriter::WriteSummariesCsv(SummariesPath, Summaries))
		{
			UE_LOG(LogRuntimeSyncDiagnostics, Error, TEXT("Failed to write summaries to %s"), *SummariesPath);
			bSuccess = false;
		}
	}

	if (bSuccess)
	{
		UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("Successfully exported CSVs to %s"), *Directory);
	}

	return bSuccess;
}

bool URuntimeSyncRecorderSubsystem::ExportJson(const FString& Directory)
{
	UE_LOG(LogRuntimeSyncDiagnostics, Warning, TEXT("ExportJson is a V1 stub and not implemented yet. Please use ExportCsv."));
	return false;
}

TArray<FRuntimeSyncSummary> URuntimeSyncRecorderSubsystem::BuildSummaries() const
{
	TArray<FRuntimeSyncSummary> ResultSummaries;
	
	if (Samples.IsEmpty())
	{
		return ResultSummaries;
	}

	TMap<FString, FRuntimeSyncSummary> SummaryMap;
	TMap<FString, int32> ErrorSampleCountMap;

	// Aggregate from Samples
	for (const FRuntimeSyncSample& Sample : Samples)
	{
		FRuntimeSyncSummary& Summary = SummaryMap.FindOrAdd(Sample.ActorName);
		Summary.ActorName = Sample.ActorName;
		Summary.TotalSamples++;

		if (Sample.bHasExpectedTransform)
		{
			int32& errorCount = ErrorSampleCountMap.FindOrAdd(Sample.ActorName);
			errorCount++;
			
			Summary.MaxErrorDistanceCm = FMath::Max(Summary.MaxErrorDistanceCm, Sample.ErrorDistanceCm);
			// Running average
			Summary.AverageErrorDistanceCm = (Summary.AverageErrorDistanceCm * (errorCount - 1) + Sample.ErrorDistanceCm) / errorCount;
		}
	}

	// Aggregate from Events
	for (const FRuntimeSyncEvent& Event : Events)
	{
		for (auto& It : SummaryMap)
		{
			// Context filter: Look for ActorName in brackets [ActorName]
			if (Event.Context.Contains(FString::Printf(TEXT("[%s]"), *It.Key)))
			{
				if (Event.EventType == FName("Snap"))
				{
					It.Value.TotalSnaps++;
				}
				else if (Event.EventType == FName("Correction"))
				{
					It.Value.TotalCorrections++;
				}
				else if (Event.EventType == FName("BaseChange"))
				{
					It.Value.TotalBaseChanges++;
				}
			}
		}
	}

	SummaryMap.GenerateValueArray(ResultSummaries);
	return ResultSummaries;
}
