// Copyright (c) 2026 DXV Systems.
#include "LevelSwitcherModule.h"
#include "Containers/Ticker.h" // For Ticker/Delay
#include "FileHelpers.h"
#include "LevelSwitcherSettings.h"
#include "Misc/CoreDelegates.h"
#include "LevelSwitcherCommands.h"
#include "LevelSwitcherStyle.h"
#include "SLevelSwitcherWidget.h"
#include "SDocumentationToolTip.h"
#include "ToolMenus.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

static const FName LevelSwitcherTabName("LevelSwitcher");

#define LOCTEXT_NAMESPACE "FLevelSwitcherModule"

void FLevelSwitcherModule::StartupModule() {
  // This code will execute after your module is loaded into memory; the exact
  // timing is specified in the .uplugin file per-module

  FLevelSwitcherStyle::Initialize();
  FLevelSwitcherStyle::ReloadTextures();

  FLevelSwitcherCommands::Register();

  PluginCommands = MakeShareable(new FUICommandList);

  PluginCommands->MapAction(
      FLevelSwitcherCommands::Get().OpenPluginWindow,
      FExecuteAction::CreateRaw(this,
                                &FLevelSwitcherModule::PluginButtonClicked),
      FCanExecuteAction());

  FGlobalTabmanager::Get()
      ->RegisterNomadTabSpawner(
          LevelSwitcherTabName,
          FOnSpawnTab::CreateRaw(this,
                                 &FLevelSwitcherModule::OnSpawnPluginTab))
      .SetDisplayName(LOCTEXT("DisplayName", "Level Switcher"))
      .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
      .SetIcon(FSlateIcon(FLevelSwitcherStyle::GetStyleSetName(),
                          "LevelSwitcher.Icon"))
      .SetMenuType(ETabSpawnerMenuType::Enabled);

  UToolMenus::RegisterStartupCallback(
      FSimpleMulticastDelegate::FDelegate::CreateRaw(
          this, &FLevelSwitcherModule::RegisterMenus));

  FCoreDelegates::OnPostEngineInit.AddRaw(
      this, &FLevelSwitcherModule::OnPostEngineInit);
}

void FLevelSwitcherModule::ShutdownModule() {
  // This function may be called during shutdown to clean up your module.  For
  // modules that support dynamic reloading, we call this function before
  // unloading the module.

  UToolMenus::UnregisterOwner(this);

  FLevelSwitcherStyle::Shutdown();

  FLevelSwitcherCommands::Unregister();

  FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LevelSwitcherTabName);
}

TSharedRef<SDockTab>
FLevelSwitcherModule::OnSpawnPluginTab(const FSpawnTabArgs &SpawnTabArgs) {
  const FText ToolTipText = LOCTEXT(
      "LevelSwitcherTabTooltip",
      "Lists all .uasset level assets in the current project (including plugins).");
  const TSharedRef<SWidget> ExtendedToolTipContent =
      SNew(SVerticalBox)

      + SVerticalBox::Slot()
      .AutoHeight()
      .Padding(0, 2, 0, 4)
      [
        SNew(STextBlock)
        .Text(LOCTEXT("LevelSwitcherTooltipMoreHeader", "Navigation"))
        .Font(FAppStyle::GetFontStyle("SmallFontBold"))
      ]

      + SVerticalBox::Slot()
      .AutoHeight()
      [
        SNew(STextBlock)
        .Text(LOCTEXT("LevelSwitcherTooltipMoreOpen", "- Double-click: open level"))
        .Font(FAppStyle::GetFontStyle("SmallFont"))
      ]

      + SVerticalBox::Slot()
      .AutoHeight()
      [
        SNew(STextBlock)
        .Text(LOCTEXT("LevelSwitcherTooltipMoreOpenBtn", "- Open button: load level"))
        .Font(FAppStyle::GetFontStyle("SmallFont"))
      ]

      + SVerticalBox::Slot()
      .AutoHeight()
      [
        SNew(STextBlock)
        .Text(LOCTEXT("LevelSwitcherTooltipMoreFavorite", "- Star: add/remove favorites"))
        .Font(FAppStyle::GetFontStyle("SmallFont"))
      ]

      + SVerticalBox::Slot()
      .AutoHeight()
      [
        SNew(STextBlock)
        .Text(LOCTEXT("LevelSwitcherTooltipMoreBrowse", "- Folder: browse to asset"))
        .Font(FAppStyle::GetFontStyle("SmallFont"))
      ]

      + SVerticalBox::Slot()
      .AutoHeight()
      [
        SNew(STextBlock)
        .Text(LOCTEXT("LevelSwitcherTooltipMoreDrag", "- Drag favorites to reorder"))
        .Font(FAppStyle::GetFontStyle("SmallFont"))
      ]

      + SVerticalBox::Slot()
      .AutoHeight()
      [
        SNew(STextBlock)
        .Text(LOCTEXT("LevelSwitcherTooltipMoreStartup", "- Drag a level to Startup to set it"))
        .Font(FAppStyle::GetFontStyle("SmallFont"))
      ]

      + SVerticalBox::Slot()
      .AutoHeight()
      [
        SNew(STextBlock)
        .Text(LOCTEXT("LevelSwitcherTooltipMoreSearch", "- Search filters levels"))
        .Font(FAppStyle::GetFontStyle("SmallFont"))
      ]

      + SVerticalBox::Slot()
      .AutoHeight()
      [
        SNew(STextBlock)
        .Text(LOCTEXT("LevelSwitcherTooltipMoreContext", "- Right-click: context menu"))
        .Font(FAppStyle::GetFontStyle("SmallFont"))
      ];
  const TSharedRef<SDocumentationToolTip> DocToolTip =
      SNew(SDocumentationToolTip)
          .Text(ToolTipText)
          .OverrideExtendedToolTipContent(ExtendedToolTipContent);

  const TSharedRef<SToolTip> TooltipWidget =
      SNew(SToolTip)
          .IsInteractive(DocToolTip, &SDocumentationToolTip::IsInteractive)
          .BorderImage(FCoreStyle::Get().GetBrush("ToolTip.BrightBackground"))
          .TextMargin(FMargin(0.0f))
          [
            DocToolTip
          ];

  return SNew(SDockTab)
      .TabRole(ETabRole::NomadTab)
      .ToolTip(TooltipWidget)
      [SNew(SLevelSwitcherWidget)];
}

void FLevelSwitcherModule::RegisterMenus() {
  // Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
  FToolMenuOwnerScoped OwnerScoped(this);

  UToolMenu *ToolbarMenu =
      UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.AssetsToolBar");
  FToolMenuSection &ToolbarSection =
      ToolbarMenu->FindOrAddSection("Content");
  FToolMenuEntry ToolbarEntry = FToolMenuEntry::InitComboButton(
      "LevelSwitcherToolbar",
      FUIAction(),
      FOnGetContent::CreateRaw(this, &FLevelSwitcherModule::MakeToolbarMenu),
      FText::GetEmpty(),
      LOCTEXT("LevelSwitcherToolbar_Tooltip",
              "Quick access to favorite levels"),
      FSlateIcon(FLevelSwitcherStyle::GetStyleSetName(),
                 "LevelSwitcher.Icon"),
      false);
  ToolbarEntry.StyleNameOverride = "AssetEditorToolbar";
  ToolbarSection.AddEntry(ToolbarEntry);
}

void FLevelSwitcherModule::PluginButtonClicked() {
  FGlobalTabmanager::Get()->TryInvokeTab(LevelSwitcherTabName);
}

TSharedRef<SWidget> FLevelSwitcherModule::MakeToolbarMenu() {
  FMenuBuilder MenuBuilder(true, PluginCommands);
  MenuBuilder.AddMenuEntry(
      LOCTEXT("OpenLevelSwitcher", "Open Level Switcher"),
      LOCTEXT("OpenLevelSwitcher_Tooltip",
              "Open the Level Switcher window"),
      FSlateIcon(FLevelSwitcherStyle::GetStyleSetName(),
                 "LevelSwitcher.Icon"),
      FUIAction(FExecuteAction::CreateRaw(
          this, &FLevelSwitcherModule::PluginButtonClicked)));

  MenuBuilder.AddMenuSeparator();
  MenuBuilder.BeginSection("LevelSwitcherFavorites",
                           LOCTEXT("LevelSwitcherFavorites", "Favorites"));
  const ULevelSwitcherSettings *Settings =
      GetDefault<ULevelSwitcherSettings>();
  if (Settings && Settings->FavoriteLevels.Num() > 0) {
    for (const FString &LevelPath : Settings->FavoriteLevels) {
      if (LevelPath.IsEmpty()) {
        continue;
      }
      const FString LevelName = FPaths::GetBaseFilename(LevelPath);
      MenuBuilder.AddMenuEntry(
          FText::FromString(LevelName),
          FText::FromString(LevelPath),
          FSlateIcon(FLevelSwitcherStyle::GetStyleSetName(),
                     "LevelSwitcher.Icon"),
          FUIAction(FExecuteAction::CreateLambda([LevelPath]() {
            FEditorFileUtils::LoadMap(LevelPath, false, true);
          })));
    }
  } else {
    MenuBuilder.AddMenuEntry(
        LOCTEXT("NoFavorites", "No favorites"),
        LOCTEXT("NoFavorites_Tooltip",
                "Add favorites in the Level Switcher."),
        FSlateIcon(),
        FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() {
          return false;
        })));
  }
  MenuBuilder.EndSection();

  return MenuBuilder.MakeWidget();
}

void FLevelSwitcherModule::OnPostEngineInit() {
  // Delay load by one tick to allow Editor to finish its initial map load setup
  // This prevents "World Memory Leaks" by ensuring the initial world is stable
  // before we replace it.
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([this](float DeltaTime) {
        const ULevelSwitcherSettings *Settings =
            GetDefault<ULevelSwitcherSettings>();
        if (Settings && !Settings->StartupLevel.IsEmpty()) {
          if (GEditor && !IsRunningCommandlet()) {
            // Check if the file actually exists on disk to avoid errors
            if (FPaths::FileExists(Settings->StartupLevel) ||
                FPackageName::DoesPackageExist(Settings->StartupLevel)) {
              // Check if a map is already loaded that MIGHT be the user's
              // intent? Actually, just load it. `false` = don't show save
              // prompt (maybe we should? No, startup should be clean). But if
              // the editor just opened, there's nothing to save.
              FEditorFileUtils::LoadMap(Settings->StartupLevel, false, true);
            }
          }
        }
        return false; // Return false to stop ticking (run once)
      }));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLevelSwitcherModule, LevelSwitcher)
