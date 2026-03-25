// Copyright (c) 2026 DXV Systems.
#include "LevelSwitcherStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Slate/SlateGameResources.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FLevelSwitcherStyle::StyleInstance = NULL;

void FLevelSwitcherStyle::Initialize() {
  if (!StyleInstance.IsValid()) {
    StyleInstance = Create();
    FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
  }
}

void FLevelSwitcherStyle::Shutdown() {
  FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
  ensure(StyleInstance.IsUnique());
  StyleInstance.Reset();
}

FName FLevelSwitcherStyle::GetStyleSetName() {
  static FName StyleSetName(TEXT("LevelSwitcherStyle"));
  return StyleSetName;
}

const ISlateStyle &FLevelSwitcherStyle::Get() { return *StyleInstance; }

void FLevelSwitcherStyle::ReloadTextures() {
  if (FSlateApplication::IsInitialized()) {
    FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
  }
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef<FSlateStyleSet> FLevelSwitcherStyle::Create() {
  TSharedRef<FSlateStyleSet> Style =
      MakeShareable(new FSlateStyleSet("LevelSwitcherStyle"));
  Style->SetContentRoot(
      IPluginManager::Get().FindPlugin("LevelSwitcher")->GetBaseDir() /
      TEXT("Resources"));

  Style->Set(
      "LevelSwitcher.Icon",
      new FSlateVectorImageBrush(
          Style->RootToContentDir(TEXT("LevelSwitcher"), TEXT(".svg")),
          Icon20x20));

  // Unreal Engine color scheme
  const FLinearColor PrimaryColor(0.728f, 1.0f, 0.0f,
                                  1.0f); // Lime Green (Unreal signature)
  const FLinearColor AccentColor(1.0f, 0.5f, 0.0f, 1.0f); // Orange
  const FLinearColor BackgroundColor(0.02f, 0.02f, 0.02f,
                                     1.0f);                  // Unreal Dark Gray
  const FLinearColor SecondaryColor(0.1f, 0.1f, 0.1f, 1.0f); // Lighter Gray

  // Style de bouton principal
  FButtonStyle PrimaryButtonStyle =
      FButtonStyle()
          .SetNormal(FSlateRoundedBoxBrush(SecondaryColor, 4.0f))
          .SetHovered(FSlateRoundedBoxBrush(PrimaryColor, 4.0f))
          .SetPressed(FSlateRoundedBoxBrush(PrimaryColor * 0.8f, 4.0f))
          .SetNormalPadding(FMargin(8, 4))
          .SetPressedPadding(FMargin(8, 5, 8, 3));

  Style->Set("LevelManager.PrimaryButton", PrimaryButtonStyle);

  // Style de bouton accent
  FButtonStyle AccentButtonStyle =
      FButtonStyle()
          .SetNormal(FSlateRoundedBoxBrush(AccentColor * 0.7f, 4.0f))
          .SetHovered(FSlateRoundedBoxBrush(AccentColor, 4.0f))
          .SetPressed(FSlateRoundedBoxBrush(AccentColor * 0.9f, 4.0f))
          .SetNormalPadding(FMargin(8, 4))
          .SetPressedPadding(FMargin(8, 5, 8, 3));

  Style->Set("LevelManager.AccentButton", AccentButtonStyle);

  // Style de texte
  FTextBlockStyle HeaderTextStyle =
      FTextBlockStyle()
          .SetColorAndOpacity(FSlateColor(FLinearColor::White))
          .SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));

  Style->Set("LevelManager.HeaderText", HeaderTextStyle);

  FTextBlockStyle SubHeaderTextStyle =
      FTextBlockStyle()
          .SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)))
          .SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));

  Style->Set("LevelManager.SubHeaderText", SubHeaderTextStyle);

  // Bordures et backgrounds
  Style->Set(
      "LevelManager.Section",
      new FSlateRoundedBoxBrush(FLinearColor(0.02f, 0.02f, 0.03f, 1.0f), 6.0f));

  Style->Set(
      "LevelManager.Border",
      new FSlateRoundedBoxBrush(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f), 2.0f,
                                FLinearColor(0.3f, 0.3f, 0.35f, 1.0f), 1.0f));

  return Style;
}
