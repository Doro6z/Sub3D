#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class USub3DSubmarineAuthoringAsset;

/**
 * Compact horizontal stats bar displayed at the top of the Ring Architecture tab.
 * Reads live from the bound AuthoringAsset — no manual refresh needed.
 *
 * Displays: LOA | Beam | L/D | Profile | Rings
 */
class SSubmarineHullStatsBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSubmarineHullStatsBar) {}
        SLATE_ARGUMENT(TWeakObjectPtr<USub3DSubmarineAuthoringAsset>, Asset)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TWeakObjectPtr<USub3DSubmarineAuthoringAsset> Asset;

    FText GetLOAText() const;
    FText GetBeamText() const;
    FText GetLDRatioText() const;
    FText GetProfileText() const;
    FText GetRingCountText() const;

    static TSharedRef<SWidget> MakeStat(
        const FText& Label,
        TAttribute<FText> ValueAttr,
        FLinearColor ValueColor = FLinearColor(0.9f, 0.85f, 0.5f, 1.0f));
};
