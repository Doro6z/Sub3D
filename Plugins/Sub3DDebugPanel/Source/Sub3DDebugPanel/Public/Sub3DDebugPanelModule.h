#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSub3DDebugPanelModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Bound to the toolbar command — brings up the debug panel tab. */
	void PluginButtonClicked();

	/** Brings up the dedicated water debug tab. */
	void WaterDebugButtonClicked();

private:
	void RegisterMenus();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);
	TSharedRef<class SDockTab> OnSpawnWaterDebugTab(const class FSpawnTabArgs& SpawnTabArgs);

	TSharedPtr<class FUICommandList> PluginCommands;
};
