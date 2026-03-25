// Copyright (c) 2026 DXV Systems.
#include "LevelSwitcherCommands.h"

#define LOCTEXT_NAMESPACE "FLevelSwitcherModule"

void FLevelSwitcherCommands::RegisterCommands() {
  UI_COMMAND(OpenPluginWindow, "Level Switcher",
             "Bring up Level Switcher window",
             EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
