#include "Utils/RuntimeSyncCsvWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString URuntimeSyncCsvWriter::VectorToString(const FVector& Vec)
{
	return FString::Printf(TEXT("%.3f,%.3f,%.3f"), Vec.X, Vec.Y, Vec.Z);
}

FString URuntimeSyncCsvWriter::RotatorToString(const FRotator& Rot)
{
	return FString::Printf(TEXT("%.3f,%.3f,%.3f"), Rot.Pitch, Rot.Yaw, Rot.Roll);
}

bool URuntimeSyncCsvWriter::WriteSamplesCsv(const FString& AbsolutePath, const TArray<FRuntimeSyncSample>& Samples)
{
	if (Samples.IsEmpty())
	{
		return false;
	}

	FString CsvContents;
	// Header
	CsvContents += TEXT("Timestamp,FrameNumber,Actor,Role,TickGroup,HasExpected,ErrorDistanceCm,MovementBase,FloorState,ExpectedLocX,ExpectedLocY,ExpectedLocZ,ExpectedRotP,ExpectedRotY,ExpectedRotR,ActualLocX,ActualLocY,ActualLocZ,ActualRotP,ActualRotY,ActualRotR,BaseLocX,BaseLocY,BaseLocZ,BaseDeltaCm\n");

	for (const FRuntimeSyncSample& Sample : Samples)
	{
		CsvContents += FString::Printf(TEXT("%.4f,%d,%s,%s,%s,%d,%.4f,%s,%s,%s,%s,%s,%s,%s,%.4f\n"),
			Sample.Timestamp,
			Sample.FrameNumber,
			*Sample.ActorName,
			*Sample.RoleContext,
			*Sample.TickGroup,
			Sample.bHasExpectedTransform ? 1 : 0,
			Sample.ErrorDistanceCm,
			*Sample.MovementBaseName,
			*Sample.FloorStateContext,
			*VectorToString(Sample.ExpectedTransform.GetLocation()),
			*RotatorToString(Sample.ExpectedTransform.Rotator()),
			*VectorToString(Sample.ActualTransform.GetLocation()),
			*RotatorToString(Sample.ActualTransform.Rotator()),
			*VectorToString(Sample.MovementBaseTransform.GetLocation()),
			Sample.MovementBaseDeltaCm
		);
	}

	return FFileHelper::SaveStringToFile(CsvContents, *AbsolutePath);
}

bool URuntimeSyncCsvWriter::WriteEventsCsv(const FString& AbsolutePath, const TArray<FRuntimeSyncEvent>& Events)
{
	if (Events.IsEmpty())
	{
		return false;
	}

	FString CsvContents;
	// Header
	CsvContents += TEXT("Timestamp,EventType,Context,NumericValue\n");

	for (const FRuntimeSyncEvent& Event : Events)
	{
		CsvContents += FString::Printf(TEXT("%.4f,%s,%s,%.4f\n"),
			Event.Timestamp,
			*Event.EventType.ToString(),
			*Event.Context,
			Event.NumericValue
		);
	}

	return FFileHelper::SaveStringToFile(CsvContents, *AbsolutePath);
}

bool URuntimeSyncCsvWriter::WriteSummariesCsv(const FString& AbsolutePath, const TArray<FRuntimeSyncSummary>& Summaries)
{
	if (Summaries.IsEmpty())
	{
		return false;
	}

	FString CsvContents;
	// Header
	CsvContents += TEXT("ActorName,TotalSamples,MaxErrorCm,AvgErrorCm,TotalSnaps,TotalCorrections,TotalBaseChanges\n");

	for (const FRuntimeSyncSummary& Summary : Summaries)
	{
		CsvContents += FString::Printf(TEXT("%s,%d,%.4f,%.4f,%d,%d,%d\n"),
			*Summary.ActorName,
			Summary.TotalSamples,
			Summary.MaxErrorDistanceCm,
			Summary.AverageErrorDistanceCm,
			Summary.TotalSnaps,
			Summary.TotalCorrections,
			Summary.TotalBaseChanges
		);
	}

	return FFileHelper::SaveStringToFile(CsvContents, *AbsolutePath);
}
