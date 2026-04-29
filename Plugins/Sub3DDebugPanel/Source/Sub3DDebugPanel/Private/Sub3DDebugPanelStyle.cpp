#include "Sub3DDebugPanelStyle.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FSub3DDebugPanelStyle::StyleInstance = nullptr;

void FSub3DDebugPanelStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FSub3DDebugPanelStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FSub3DDebugPanelStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("Sub3DDebugPanelStyle"));
	return StyleSetName;
}

const ISlateStyle& FSub3DDebugPanelStyle::Get()
{
	return *StyleInstance;
}

void FSub3DDebugPanelStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

TSharedRef<FSlateStyleSet> FSub3DDebugPanelStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(TEXT("Sub3DDebugPanelStyle")));

	// Palette reprise de LevelSwitcher (signature Unreal : lime green + orange).
	const FLinearColor PrimaryColor(0.728f, 1.0f, 0.0f, 1.0f);
	const FLinearColor AccentColor(1.0f, 0.5f, 0.0f, 1.0f);
	const FLinearColor SecondaryColor(0.1f, 0.1f, 0.1f, 1.0f);

	FButtonStyle PrimaryButtonStyle = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(SecondaryColor, 4.0f))
		.SetHovered(FSlateRoundedBoxBrush(PrimaryColor, 4.0f))
		.SetPressed(FSlateRoundedBoxBrush(PrimaryColor * 0.8f, 4.0f))
		.SetNormalPadding(FMargin(8, 4))
		.SetPressedPadding(FMargin(8, 5, 8, 3));
	Style->Set("Sub3DDebug.PrimaryButton", PrimaryButtonStyle);

	FButtonStyle AccentButtonStyle = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(AccentColor * 0.7f, 4.0f))
		.SetHovered(FSlateRoundedBoxBrush(AccentColor, 4.0f))
		.SetPressed(FSlateRoundedBoxBrush(AccentColor * 0.9f, 4.0f))
		.SetNormalPadding(FMargin(8, 4))
		.SetPressedPadding(FMargin(8, 5, 8, 3));
	Style->Set("Sub3DDebug.AccentButton", AccentButtonStyle);

	FTextBlockStyle HeaderTextStyle = FTextBlockStyle()
		.SetColorAndOpacity(FSlateColor(FLinearColor::White))
		.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
	Style->Set("Sub3DDebug.HeaderText", HeaderTextStyle);

	FTextBlockStyle SubHeaderTextStyle = FTextBlockStyle()
		.SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)))
		.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
	Style->Set("Sub3DDebug.SubHeaderText", SubHeaderTextStyle);

	Style->Set("Sub3DDebug.Section",
		new FSlateRoundedBoxBrush(FLinearColor(0.02f, 0.02f, 0.03f, 1.0f), 6.0f));

	Style->Set("Sub3DDebug.Border",
		new FSlateRoundedBoxBrush(
			FLinearColor(0.1f, 0.1f, 0.12f, 1.0f), 2.0f,
			FLinearColor(0.3f, 0.3f, 0.35f, 1.0f), 1.0f));

	return Style;
}
