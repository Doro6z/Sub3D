#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RuntimeSyncConsoleCommands.generated.h"

/**
 * Object to register and handle runtime console commands for the diagnostics plugin.
 */
UCLASS()
class RUNTIMESYNCDIAGNOSTICS_API URuntimeSyncConsoleCommands : public UObject
{
	GENERATED_BODY()

public:

	URuntimeSyncConsoleCommands();

	void RegisterCommands();
	void UnregisterCommands();

protected:

	void CmdStartCapture(const TArray<FString>& Args);
	void CmdStopCapture(const TArray<FString>& Args);
	void CmdClearCapture(const TArray<FString>& Args);
	void CmdExportCsv(const TArray<FString>& Args);
	void CmdSummary(const TArray<FString>& Args);

private:

	IConsoleCommand* StartCommand;
	IConsoleCommand* StopCommand;
	IConsoleCommand* ClearCommand;
	IConsoleCommand* ExportCommand;
	IConsoleCommand* SummaryCommand;
};
