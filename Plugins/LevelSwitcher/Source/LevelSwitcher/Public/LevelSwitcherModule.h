// Copyright (c) 2026 DXV Systems.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FLevelSwitcherModule : public IModuleInterface {
public:
  /** IModuleInterface implementation */
  virtual void StartupModule() override;
  virtual void ShutdownModule() override;

  /** This function will be bound to Command (by default it will bring up plugin
   * window) */
  void PluginButtonClicked();

private:
  void RegisterMenus();
  TSharedRef<class SWidget> MakeToolbarMenu();

  TSharedRef<class SDockTab>
  OnSpawnPluginTab(const class FSpawnTabArgs &SpawnTabArgs);

  void OnPostEngineInit();

private:
  TSharedPtr<class FUICommandList> PluginCommands;
};
