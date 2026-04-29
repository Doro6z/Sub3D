#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;

/**
 * Panel content: a DetailsView wrapping USub3DDebugSettings CDO so every
 * Config UPROPERTY in that class (grouped by Category) is rendered and
 * auto-saved through UDeveloperSettings' own mechanism.
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
