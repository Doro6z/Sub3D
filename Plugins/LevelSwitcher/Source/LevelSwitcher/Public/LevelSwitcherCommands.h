// Copyright (c) 2026 DXV Systems.
#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "LevelSwitcherStyle.h"

class FLevelSwitcherCommands : public TCommands<FLevelSwitcherCommands> {
public:
  FLevelSwitcherCommands()
      : TCommands<FLevelSwitcherCommands>(
            TEXT("LevelSwitcher"),
            NSLOCTEXT("Contexts", "LevelSwitcher", "LevelSwitcher Plugin"),
            NAME_None, FLevelSwitcherStyle::GetStyleSetName()) {}

  // TCommands<> interface
  virtual void RegisterCommands() override;

public:
  TSharedPtr<FUICommandInfo> OpenPluginWindow;
};
