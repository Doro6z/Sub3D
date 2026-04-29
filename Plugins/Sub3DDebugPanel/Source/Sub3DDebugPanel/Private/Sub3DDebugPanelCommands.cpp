#include "Sub3DDebugPanelCommands.h"

#define LOCTEXT_NAMESPACE "FSub3DDebugPanelModule"

void FSub3DDebugPanelCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow,
		"Sub3D Debug",
		"Bring up the Sub3D Debug Panel",
		EUserInterfaceActionType::Button,
		FInputChord());
}

#undef LOCTEXT_NAMESPACE
