#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FRuntimeSyncDiagnosticsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	UPROPERTY()
	class URuntimeSyncConsoleCommands* ConsoleCommands;
};
