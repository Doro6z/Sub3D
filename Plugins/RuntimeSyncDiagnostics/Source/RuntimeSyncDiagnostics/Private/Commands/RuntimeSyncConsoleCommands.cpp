#include "Commands/RuntimeSyncConsoleCommands.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Subsystems/RuntimeSyncRecorderSubsystem.h"
#include "RuntimeSyncDiagnosticsSettings.h"
#include "RuntimeSyncDiagnosticsLog.h"

URuntimeSyncConsoleCommands::URuntimeSyncConsoleCommands()
{
	StartCommand = nullptr;
	StopCommand = nullptr;
	ClearCommand = nullptr;
	ExportCommand = nullptr;
	SummaryCommand = nullptr;
}

void URuntimeSyncConsoleCommands::RegisterCommands()
{
	StartCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("rsd.start"),
		TEXT("Starts runtime sync diagnostics capture. Usage: rsd.start [ScenarioName]"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &URuntimeSyncConsoleCommands::CmdStartCapture),
		ECVF_Default
	);

	StopCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("rsd.stop"),
		TEXT("Stops runtime sync diagnostics capture."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &URuntimeSyncConsoleCommands::CmdStopCapture),
		ECVF_Default
	);

	ClearCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("rsd.clear"),
		TEXT("Clears the recorded runtime sync diagnostics data in memory."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &URuntimeSyncConsoleCommands::CmdClearCapture),
		ECVF_Default
	);

	ExportCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("rsd.exportcsv"),
		TEXT("Exports current runtime sync diagnostics data to CSV files."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &URuntimeSyncConsoleCommands::CmdExportCsv),
		ECVF_Default
	);
	
	SummaryCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("rsd.summary"),
		TEXT("Prints a summary of the current runtime sync diagnostics data to the log."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &URuntimeSyncConsoleCommands::CmdSummary),
		ECVF_Default
	);
}

void URuntimeSyncConsoleCommands::UnregisterCommands()
{
	if (StartCommand) IConsoleManager::Get().UnregisterConsoleObject(StartCommand);
	if (StopCommand) IConsoleManager::Get().UnregisterConsoleObject(StopCommand);
	if (ClearCommand) IConsoleManager::Get().UnregisterConsoleObject(ClearCommand);
	if (ExportCommand) IConsoleManager::Get().UnregisterConsoleObject(ExportCommand);
	if (SummaryCommand) IConsoleManager::Get().UnregisterConsoleObject(SummaryCommand);
}

// Helpers
TArray<URuntimeSyncRecorderSubsystem*> GetAllRecorderSubsystems()
{
	TArray<URuntimeSyncRecorderSubsystem*> Recorders;

#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) && Context.World())
			{
				if (URuntimeSyncRecorderSubsystem* Sub = Context.World()->GetSubsystem<URuntimeSyncRecorderSubsystem>())
				{
					Recorders.Add(Sub);
				}
			}
		}
		return Recorders;
	}
#endif

	if (UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::ReturnNull))
	{
		if (URuntimeSyncRecorderSubsystem* Sub = World->GetSubsystem<URuntimeSyncRecorderSubsystem>())
		{
			Recorders.Add(Sub);
		}
	}

	return Recorders;
}


void URuntimeSyncConsoleCommands::CmdStartCapture(const TArray<FString>& Args)
{
	FString ScenarioName = TEXT("DefaultScenario");
	if (Args.Num() > 0)
	{
		ScenarioName = Args[0];
	}

	TArray<URuntimeSyncRecorderSubsystem*> Recorders = GetAllRecorderSubsystems();
	for (URuntimeSyncRecorderSubsystem* Recorder : Recorders)
	{
		Recorder->StartCapture(ScenarioName);
	}
	
	UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("Started capture on %d world(s)"), Recorders.Num());
}

void URuntimeSyncConsoleCommands::CmdStopCapture(const TArray<FString>& Args)
{
	TArray<URuntimeSyncRecorderSubsystem*> Recorders = GetAllRecorderSubsystems();
	for (URuntimeSyncRecorderSubsystem* Recorder : Recorders)
	{
		Recorder->StopCapture();
	}
	UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("Stopped capture on %d world(s)"), Recorders.Num());
}

void URuntimeSyncConsoleCommands::CmdClearCapture(const TArray<FString>& Args)
{
	TArray<URuntimeSyncRecorderSubsystem*> Recorders = GetAllRecorderSubsystems();
	for (URuntimeSyncRecorderSubsystem* Recorder : Recorders)
	{
		Recorder->ClearCapture();
	}
}

void URuntimeSyncConsoleCommands::CmdExportCsv(const TArray<FString>& Args)
{
	const URuntimeSyncDiagnosticsSettings* Settings = GetDefault<URuntimeSyncDiagnosticsSettings>();
	FString BaseExportDir = FPaths::ProjectDir() / Settings->DefaultCsvDirectory;
	
	if (Args.Num() > 0)
	{
		BaseExportDir = Args[0];
	}

	TArray<URuntimeSyncRecorderSubsystem*> Recorders = GetAllRecorderSubsystems();
	for (URuntimeSyncRecorderSubsystem* Recorder : Recorders)
	{
		// Suffix directory with world type to avoid overwriting files from different worlds in the same session
		FString WorldSuffix = TEXT("_Unknown");
		if (UWorld* World = Recorder->GetWorld())
		{
			if (World->GetNetMode() == NM_DedicatedServer || World->GetNetMode() == NM_ListenServer)
				WorldSuffix = TEXT("_Server");
			else
				WorldSuffix = FString::Printf(TEXT("_Client%s"), *World->GetOutermost()->GetName()); // Best effort ID
		}

		Recorder->ExportCsv(BaseExportDir);
	}
	
	UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("Exported CSVs from %d world(s) to %s"), Recorders.Num(), *BaseExportDir);
}

void URuntimeSyncConsoleCommands::CmdSummary(const TArray<FString>& Args)
{
	TArray<URuntimeSyncRecorderSubsystem*> Recorders = GetAllRecorderSubsystems();
	for (URuntimeSyncRecorderSubsystem* Recorder : Recorders)
	{
		FString WorldName = Recorder->GetWorld() ? Recorder->GetWorld()->GetName() : TEXT("UnknownWorld");
		UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("--- Diagnostics Summary [%s] ---"), *WorldName);
		UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("Samples: %d"), Recorder->GetSamples().Num());
		UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("Events: %d"), Recorder->GetEvents().Num());
		UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("Capturing: %s"), Recorder->IsCapturing() ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogRuntimeSyncDiagnostics, Log, TEXT("---------------------------"));
	}
}
