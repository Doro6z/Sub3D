#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Sub3DDebugPanelStyle.h"

class FSub3DDebugPanelCommands : public TCommands<FSub3DDebugPanelCommands>
{
public:
	FSub3DDebugPanelCommands()
		: TCommands<FSub3DDebugPanelCommands>(
			TEXT("Sub3DDebugPanel"),
			NSLOCTEXT("Contexts", "Sub3DDebugPanel", "Sub3D Debug Panel"),
			NAME_None,
			FSub3DDebugPanelStyle::GetStyleSetName())
	{}

	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> OpenPluginWindow;
};
