#include "RuntimeSyncDiagnosticsModule.h"
#include "Commands/RuntimeSyncConsoleCommands.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "FRuntimeSyncDiagnosticsModule"

void FRuntimeSyncDiagnosticsModule::StartupModule()
{
	ConsoleCommands = NewObject<URuntimeSyncConsoleCommands>();
	ConsoleCommands->AddToRoot(); // Prevent GC
	ConsoleCommands->RegisterCommands();
	EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FRuntimeSyncDiagnosticsModule::CleanupConsoleCommands);
}

void FRuntimeSyncDiagnosticsModule::ShutdownModule()
{
	if (EnginePreExitHandle.IsValid())
	{
		FCoreDelegates::OnEnginePreExit.Remove(EnginePreExitHandle);
		EnginePreExitHandle.Reset();
	}

	CleanupConsoleCommands();
}

void FRuntimeSyncDiagnosticsModule::CleanupConsoleCommands()
{
	if (!ConsoleCommands)
	{
		return;
	}

	ConsoleCommands->UnregisterCommands();

	if (!GExitPurge)
	{
		ConsoleCommands->RemoveFromRoot();
	}

	ConsoleCommands = nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRuntimeSyncDiagnosticsModule, RuntimeSyncDiagnostics)
