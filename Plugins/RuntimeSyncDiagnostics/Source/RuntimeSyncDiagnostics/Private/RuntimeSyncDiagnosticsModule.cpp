#include "RuntimeSyncDiagnosticsModule.h"
#include "Commands/RuntimeSyncConsoleCommands.h"

#define LOCTEXT_NAMESPACE "FRuntimeSyncDiagnosticsModule"

void FRuntimeSyncDiagnosticsModule::StartupModule()
{
	ConsoleCommands = NewObject<URuntimeSyncConsoleCommands>();
	ConsoleCommands->AddToRoot(); // Prevent GC
	ConsoleCommands->RegisterCommands();
}

void FRuntimeSyncDiagnosticsModule::ShutdownModule()
{
	if (ConsoleCommands)
	{
		ConsoleCommands->UnregisterCommands();
		ConsoleCommands->RemoveFromRoot();
		ConsoleCommands = nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRuntimeSyncDiagnosticsModule, RuntimeSyncDiagnostics)
