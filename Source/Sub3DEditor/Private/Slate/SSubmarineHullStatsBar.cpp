#include "Slate/SSubmarineHullStatsBar.h"

#include "Authoring/SubmarineAuthoringAsset.h"
#include "Types/Sub3DHullTypes.h"
#include "Types/Sub3DCanonicalEnums.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
static const FText NotAvailable = FText::FromString(TEXT("—"));

static FString ProfileDisplayName(ESub3DHullLongitudinalProfile Profile)
{
    switch (Profile)
    {
    case ESub3DHullLongitudinalProfile::Myring:                  return TEXT("Myring");
    case ESub3DHullLongitudinalProfile::Series58:                return TEXT("Series 58");
    case ESub3DHullLongitudinalProfile::SuperellipseLongitudinal: return TEXT("Superellipse");
    case ESub3DHullLongitudinalProfile::Uniform:                 return TEXT("Uniform");
    default:                                                     return TEXT("Manual");
    }
}
} // namespace

void SSubmarineHullStatsBar::Construct(const FArguments& InArgs)
{
    Asset = InArgs._Asset;

    ChildSlot
    [
        SNew(SBorder)
        .Padding(FMargin(8.0f, 5.0f))
        .BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.07f, 1.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 20.0f, 0.0f)
            [
                MakeStat(FText::FromString(TEXT("LOA")),
                    TAttribute<FText>::CreateSP(this, &SSubmarineHullStatsBar::GetLOAText))
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 20.0f, 0.0f)
            [
                MakeStat(FText::FromString(TEXT("Beam")),
                    TAttribute<FText>::CreateSP(this, &SSubmarineHullStatsBar::GetBeamText))
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 20.0f, 0.0f)
            [
                MakeStat(FText::FromString(TEXT("L/D")),
                    TAttribute<FText>::CreateSP(this, &SSubmarineHullStatsBar::GetLDRatioText),
                    FLinearColor(0.5f, 0.85f, 1.0f, 1.0f))
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 20.0f, 0.0f)
            [
                MakeStat(FText::FromString(TEXT("Profile")),
                    TAttribute<FText>::CreateSP(this, &SSubmarineHullStatsBar::GetProfileText))
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                MakeStat(FText::FromString(TEXT("Rings")),
                    TAttribute<FText>::CreateSP(this, &SSubmarineHullStatsBar::GetRingCountText))
            ]
        ]
    ];
}

FText SSubmarineHullStatsBar::GetLOAText() const
{
    if (!Asset.IsValid()) { return NotAvailable; }
    const float LCm = Asset->Hull.LengthCm;
    return FText::FromString(FString::Printf(TEXT("%.0f cm (%.1f m)"), LCm, LCm / 100.0f));
}

FText SSubmarineHullStatsBar::GetBeamText() const
{
    if (!Asset.IsValid()) { return NotAvailable; }
    const float BCm = Asset->Hull.DefaultHalfWidthCm * 2.0f;
    return FText::FromString(FString::Printf(TEXT("%.0f cm (%.1f m)"), BCm, BCm / 100.0f));
}

FText SSubmarineHullStatsBar::GetLDRatioText() const
{
    if (!Asset.IsValid()) { return NotAvailable; }
    const float L = Asset->Hull.LengthCm;
    const float D = Asset->Hull.DefaultHalfWidthCm * 2.0f;
    if (D <= 0.0f) { return NotAvailable; }
    return FText::FromString(FString::Printf(TEXT("%.1f"), L / D));
}

FText SSubmarineHullStatsBar::GetProfileText() const
{
    if (!Asset.IsValid()) { return NotAvailable; }
    return FText::FromString(ProfileDisplayName(Asset->Hull.ProfileParams.Profile));
}

FText SSubmarineHullStatsBar::GetRingCountText() const
{
    if (!Asset.IsValid()) { return NotAvailable; }
    const int32 ManualCount = Asset->ControlRings.Num();
    const bool bAuto = Asset->Hull.ProfileParams.Profile != ESub3DHullLongitudinalProfile::Manual;
    if (bAuto)
    {
        return FText::FromString(FString::Printf(TEXT("%d manual + 6 auto"), ManualCount));
    }
    return FText::FromString(FString::Printf(TEXT("%d"), ManualCount));
}

// static
TSharedRef<SWidget> SSubmarineHullStatsBar::MakeStat(
    const FText& Label,
    TAttribute<FText> ValueAttr,
    FLinearColor ValueColor)
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Text(Label)
            .ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f, 1.0f))
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock)
            .Text(ValueAttr)
            .ColorAndOpacity(ValueColor)
        ];
}
