#include "Slate/SSubmarineAppendagesPanel.h"

#include "Authoring/SubmarineAuthoringAsset.h"
#include "IDetailsView.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SSubmarineAppendagesPanel::Construct(const FArguments& InArgs)
{
    Asset = InArgs._Asset;
    TSharedPtr<IDetailsView> DetailsView = InArgs._DetailsView;

    ChildSlot
    [
        SNew(SVerticalBox)

        // ── Phase description ──────────────────────────────────────────────
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SBorder)
            .Padding(FMargin(10.0f, 8.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Appendages — Level A")))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .AutoWrapText(true)
                    .ColorAndOpacity(FLinearColor(0.65f, 0.65f, 0.65f, 1.0f))
                    .Text(FText::FromString(
                        TEXT("Define the Sail (fin/kiosque), Bow Section (sonar dome, planes, "
                             "torpedo tubes), and Stern Section (control surfaces, propulsor, "
                             "towed array). These are Level A — part of the locked hull geometry.")))
                ]
            ]
        ]

        // ── Active appendage status row ────────────────────────────────────
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SBorder)
            .Padding(FMargin(10.0f, 6.0f))
            .BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.06f, 1.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 16.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(this, &SSubmarineAppendagesPanel::GetSailBadge)
                    .ColorAndOpacity(this, &SSubmarineAppendagesPanel::GetSailColor)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 16.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(this, &SSubmarineAppendagesPanel::GetBowBadge)
                    .ColorAndOpacity(this, &SSubmarineAppendagesPanel::GetBowColor)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 16.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(this, &SSubmarineAppendagesPanel::GetSternBadge)
                    .ColorAndOpacity(this, &SSubmarineAppendagesPanel::GetSternColor)
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(STextBlock)
                    .Text(this, &SSubmarineAppendagesPanel::GetCasingBadge)
                    .ColorAndOpacity(this, &SSubmarineAppendagesPanel::GetCasingColor)
                ]
            ]
        ]

        // ── Details view ───────────────────────────────────────────────────
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            DetailsView.IsValid()
                ? StaticCastSharedRef<SWidget>(DetailsView.ToSharedRef())
                : SNew(STextBlock).Text(FText::FromString(TEXT("No authoring asset selected.")))
        ]
    ];
}

// ── Badge helpers ─────────────────────────────────────────────────────────────

FText SSubmarineAppendagesPanel::GetSailBadge() const
{
    if (!Asset.IsValid()) { return FText::FromString(TEXT("○ Sail")); }
    return FText::FromString(Asset->Sail.bEnabled ? TEXT("✓ Sail") : TEXT("○ Sail"));
}

FText SSubmarineAppendagesPanel::GetBowBadge() const
{
    if (!Asset.IsValid()) { return FText::FromString(TEXT("○ Bow")); }
    return FText::FromString(Asset->BowSection.bEnabled ? TEXT("✓ Bow") : TEXT("○ Bow"));
}

FText SSubmarineAppendagesPanel::GetSternBadge() const
{
    if (!Asset.IsValid()) { return FText::FromString(TEXT("○ Stern")); }
    return FText::FromString(Asset->SternSection.bEnabled ? TEXT("✓ Stern") : TEXT("○ Stern"));
}

FText SSubmarineAppendagesPanel::GetCasingBadge() const
{
    if (!Asset.IsValid()) { return FText::FromString(TEXT("○ Casing")); }
    return FText::FromString(Asset->OuterCasing.bEnabled ? TEXT("✓ Casing") : TEXT("○ Casing"));
}

FSlateColor SSubmarineAppendagesPanel::GetSailColor() const
{
    return (Asset.IsValid() && Asset->Sail.bEnabled) ? EnabledColor() : DisabledColor();
}

FSlateColor SSubmarineAppendagesPanel::GetBowColor() const
{
    return (Asset.IsValid() && Asset->BowSection.bEnabled) ? EnabledColor() : DisabledColor();
}

FSlateColor SSubmarineAppendagesPanel::GetSternColor() const
{
    return (Asset.IsValid() && Asset->SternSection.bEnabled) ? EnabledColor() : DisabledColor();
}

FSlateColor SSubmarineAppendagesPanel::GetCasingColor() const
{
    return (Asset.IsValid() && Asset->OuterCasing.bEnabled) ? EnabledColor() : DisabledColor();
}

// static
FSlateColor SSubmarineAppendagesPanel::EnabledColor()
{
    return FSlateColor(FLinearColor(0.4f, 0.9f, 0.5f, 1.0f));
}

// static
FSlateColor SSubmarineAppendagesPanel::DisabledColor()
{
    return FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f));
}
