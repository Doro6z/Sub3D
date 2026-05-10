#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;

/**
 * Sub3D Debug Panel — DetailsView wrapping USub3DDebugSettings CDO so every Config
 * UPROPERTY in that class (grouped by Category) is rendered and auto-saved through
 * UDeveloperSettings' own mechanism.
 *
 * The Authoring section (bake water + spawn viewers) was moved to SSub3DWaterDebugPanelWidget
 * (placed at its bottom) so it doesn't compete with the rest of the panel for space.
 */
class SSub3DDebugPanelWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSub3DDebugPanelWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<IDetailsView> DetailsView;
};
