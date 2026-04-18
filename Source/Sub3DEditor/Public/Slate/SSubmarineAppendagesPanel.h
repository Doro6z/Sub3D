#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class USub3DSubmarineAuthoringAsset;
class IDetailsView;

/**
 * Appendages tab panel.
 *
 * Renders a one-line status summary (Sail / Bow / Stern / Casing enabled state)
 * above the filtered details view so the author can see at a glance which
 * appendages are active without scrolling into properties.
 */
class SSubmarineAppendagesPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSubmarineAppendagesPanel) {}
        SLATE_ARGUMENT(TWeakObjectPtr<USub3DSubmarineAuthoringAsset>, Asset)
        SLATE_ARGUMENT(TSharedPtr<IDetailsView>, DetailsView)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TWeakObjectPtr<USub3DSubmarineAuthoringAsset> Asset;

    // Status badge helpers — each returns "✓ Name" or "○ Name"
    FText GetSailBadge() const;
    FText GetBowBadge() const;
    FText GetSternBadge() const;
    FText GetCasingBadge() const;

    FSlateColor GetSailColor() const;
    FSlateColor GetBowColor() const;
    FSlateColor GetSternColor() const;
    FSlateColor GetCasingColor() const;

    static FSlateColor EnabledColor();
    static FSlateColor DisabledColor();
};
