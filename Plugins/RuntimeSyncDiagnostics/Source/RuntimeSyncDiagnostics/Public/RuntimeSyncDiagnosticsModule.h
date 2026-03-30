#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Modules/ModuleManager.h"

class URuntimeSyncConsoleCommands;

class FRuntimeSyncDiagnosticsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void CleanupConsoleCommands();

	URuntimeSyncConsoleCommands* ConsoleCommands = nullptr;
	FDelegateHandle EnginePreExitHandle;
};
