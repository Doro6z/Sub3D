// Copyright (c) 2026 DXV Systems.
#include "SLevelSwitcherWidget.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IContentBrowserSingleton.h"
#include "LevelDragDropOp.h"
#include "LevelSwitcherSettings.h"
#include "SDropTarget.h"
#include "Editor/EditorEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateTypes.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Containers/Ticker.h"

namespace {
const FLinearColor StarMuted(0.7f, 0.62f, 0.28f, 0.85f);
const FLinearColor HeaderStar(0.85f, 0.85f, 0.85f, 0.9f);
const FLinearColor OpenYellow(0.9f, 0.78f, 0.28f, 1.0f);
const FLinearColor ActiveLevelBorderColor(1.0f, 0.6118f, 0.0f, 1.0f);
const FSlateRoundedBoxBrush ActiveLevelBorderBrush(
    FLinearColor::Transparent, 2.0f, ActiveLevelBorderColor, 1.0f);
const FLinearColor StartupIconColor(0.9f, 0.78f, 0.28f, 0.9f);

FString NormalizeLevelPath(const FString &Path) {
  // Normalize paths from filename/object/package formats to a long package name.
  if (Path.IsEmpty()) {
    return Path;
  }
  FString PackagePath;
  if (FPackageName::TryConvertFilenameToLongPackageName(Path, PackagePath)) {
    return PackagePath;
  }
  const FString FullPath = FPaths::ConvertRelativePathToFull(Path);
  if (FPackageName::TryConvertFilenameToLongPackageName(FullPath, PackagePath)) {
    return PackagePath;
  }
  FString Normalized = Path;
  if (Normalized.StartsWith(TEXT("Game/"))) {
    Normalized = TEXT("/") + Normalized;
  }
  if (Normalized.EndsWith(TEXT(".umap"))) {
    Normalized = FPaths::GetPath(Normalized) / FPaths::GetBaseFilename(Normalized);
  }
  if (FPackageName::IsValidObjectPath(Normalized)) {
    return FPackageName::ObjectPathToPackageName(Normalized);
  }
  int32 DotIndex;
  if (Normalized.FindChar('.', DotIndex)) {
    Normalized = Normalized.Left(DotIndex);
  }
  return Normalized;
}
} // namespace

void SLevelSwitcherWidget::Construct(const FArguments &InArgs) {
  Settings = GetMutableDefault<ULevelSwitcherSettings>();

  LoadFavorites();
  ScanAllLevels();

  if (!MapOpenedHandle.IsValid()) {
    MapOpenedHandle =
        FEditorDelegates::OnMapOpened.AddSP(
            SharedThis(this), &SLevelSwitcherWidget::OnEditorMapOpened);
  }
  if (!PostLoadMapHandle.IsValid()) {
    PostLoadMapHandle =
        FCoreUObjectDelegates::PostLoadMapWithWorld.AddSP(
            SharedThis(this), &SLevelSwitcherWidget::OnPostLoadMap);
  }

  ChildSlot
  [
    SNew(SBorder)
    .BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
    .Padding(0)
    [
      SNew(SVerticalBox)

      // Startup Level (très compact)
      + SVerticalBox::Slot()
      .AutoHeight()
      .Padding(6, 4)
      [
        CreateSettingsSection()
      ]

      + SVerticalBox::Slot()
      .FillHeight(1.0f)
      .Padding(0, 2, 0, 0)
      [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [
          SNew(SVerticalBox)

          // Favorites
          + SVerticalBox::Slot()
          .AutoHeight()
          .MaxHeight(180.0f)
          .Padding(0, 0, 0, 2)
          [
            CreateFavoritesSection()
          ]

          // All Levels
          + SVerticalBox::Slot()
          .AutoHeight()
          .Padding(0, 0, 0, 0)
          [
            CreateAllLevelsSection()
          ]
        ]
      ]
    ]
  ];

  // Init startup
  if (Settings && !Settings->StartupLevel.IsEmpty()) {
    for (const auto &Item : AllLevels) {
      if (*Item == Settings->StartupLevel) {
        StartupLevelComboBox->SetSelectedItem(Item);
        break;
      }
    }
  }

  RefreshUI();

  // One-shot refresh after the first tick so list views update their rows.
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakThis = TWeakPtr<SLevelSwitcherWidget>(SharedThis(this))](float) {
        if (TSharedPtr<SLevelSwitcherWidget> Pinned = WeakThis.Pin()) {
          Pinned->RefreshUI();
        }
        return false;
      }));
}

SLevelSwitcherWidget::~SLevelSwitcherWidget() {
  if (MapOpenedHandle.IsValid()) {
    FEditorDelegates::OnMapOpened.Remove(MapOpenedHandle);
  }
  if (PostLoadMapHandle.IsValid()) {
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
  }
}

void SLevelSwitcherWidget::OnEditorMapOpened(const FString &Filename, bool bAsTemplate) {
  ActiveLevelPath = NormalizeLevelPath(Filename);
  RefreshUI();
}

void SLevelSwitcherWidget::OnPostLoadMap(UWorld *LoadedWorld) {
  if (LoadedWorld) {
    ActiveLevelPath = NormalizeLevelPath(LoadedWorld->GetPathName());
  }
  RefreshUI();
}

TSharedRef<SWidget> SLevelSwitcherWidget::CreateSettingsSection() {
  return SNew(SDropTarget)
    .OnAllowDrop(this, &SLevelSwitcherWidget::OnStartupDropAllowDrop)
    .OnDropped(this, &SLevelSwitcherWidget::OnStartupDropAccept)
    [
      SNew(SBorder)
      .BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
      .Padding(0)
      .OnMouseButtonDown_Lambda([this](const FGeometry &Geometry, const FPointerEvent &Event) {
        if (Event.GetEffectingButton() == EKeys::RightMouseButton) {
          FSlateApplication::Get().PushMenu(
              AsShared(),
              FWidgetPath(),
              MakeStartupContextMenu(),
              Event.GetScreenSpacePosition(),
              FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
          return FReply::Handled();
        }
        return FReply::Unhandled();
      })
      [
        SNew(SHorizontalBox)
      
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(0, 0, 4, 0)
      [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("Startup:")))
        .Font(FAppStyle::GetFontStyle("SmallFont"))
        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
      ]
      
      + SHorizontalBox::Slot()
      .FillWidth(1.0f)
      .VAlign(VAlign_Center)
      [
        SAssignNew(StartupLevelComboBox, SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&AllLevels)
        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
          return SNew(STextBlock)
            .Text(FText::FromString(FPaths::GetBaseFilename(*Item)))
            .Font(FAppStyle::GetFontStyle("SmallFont"));
        })
        .OnSelectionChanged(this, &SLevelSwitcherWidget::OnSetStartupLevel)
        [
          SNew(STextBlock)
          .Text_Lambda([this]() {
            if (Settings && !Settings->StartupLevel.IsEmpty()) {
              return FText::FromString(FPaths::GetBaseFilename(Settings->StartupLevel));
            }
            return FText::FromString(TEXT("None"));
          })
          .Font(FAppStyle::GetFontStyle("SmallFont"))
        ]
      ]
      
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(3, 0, 0, 0)
      [
        SNew(SButton)
        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
        .ContentPadding(1)
        .ToolTipText(FText::FromString(TEXT("Set currently open level")))
        .OnClicked_Lambda([this]() {
          FString Current = GetCurrentLevelPath();
          if (!Current.IsEmpty()) {
            Settings->SetStartupLevel(Current);
            RefreshUI();
          }
          return FReply::Handled();
        })
        [
          SNew(SImage)
          .Image(FAppStyle::Get().GetBrush("DeviceDetails.PowerOn"))
          .ColorAndOpacity(FSlateColor::UseForeground())
        ]
      ]
      ]
    ];
}

TSharedRef<SWidget> SLevelSwitcherWidget::CreateFavoritesSection() {
  TSharedRef<SListView<TSharedPtr<FString>>> ListView =
    SNew(SListView<TSharedPtr<FString>>)
    .ListItemsSource(&FavoriteLevels)
    .OnGenerateRow(this, &SLevelSwitcherWidget::OnGenerateFavoriteRow)
    .OnMouseButtonDoubleClick(this, &SLevelSwitcherWidget::OnLevelDoubleClick)
    .OnSelectionChanged(this, &SLevelSwitcherWidget::OnFavoriteSelectionChanged)
    .SelectionMode(ESelectionMode::Single)
    .OnContextMenuOpening(FOnContextMenuOpening::CreateLambda([this]() {
      if (!FavoritesListView.IsValid()) {
        return TSharedPtr<SWidget>();
      }
      TArray<TSharedPtr<FString>> SelectedItems;
      FavoritesListView->GetSelectedItems(SelectedItems);
      if (SelectedItems.Num() > 0 && SelectedItems[0].IsValid()) {
        return TSharedPtr<SWidget>(MakeLevelContextMenu(SelectedItems[0], true));
      }
      return TSharedPtr<SWidget>();
    }))
    .HeaderRow(SNew(SHeaderRow).Visibility(EVisibility::Collapsed));

  FavoritesListView = ListView;

  return SNew(SExpandableArea)
    .BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
    .BodyBorderImage(FAppStyle::Get().GetBrush("NoBorder"))
    .HeaderPadding(FMargin(6, 2))
    .Padding(0.0f)
    .AllowAnimatedTransition(false)
    .InitiallyCollapsed(false)
    .HeaderContent()
    [
      SNew(SHorizontalBox)

      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      [
        SNew(SBox)
        .WidthOverride(12.0f)
        .HeightOverride(12.0f)
        [
          SNew(SImage)
          .Image(FAppStyle::Get().GetBrush("Icons.Star.Outline"))
          .ColorAndOpacity(HeaderStar)
        ]
      ]

      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(4, 0)
      [
        SNew(SSeparator)
        .Orientation(Orient_Vertical)
        .Thickness(1.0f)
      ]

      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      [
        SNew(STextBlock)
        .Text_Lambda([this]() {
          return FText::Format(
            FText::FromString(TEXT("Favorites ({0})")),
            FText::AsNumber(FavoriteLevels.Num())
          );
        })
        .Font(FAppStyle::GetFontStyle("SmallFontBold"))
      ]
    ]
    .BodyContent()
    [
      SNew(SScrollBox)
      + SScrollBox::Slot()
      [
        ListView
      ]
    ];
}

TSharedRef<SWidget> SLevelSwitcherWidget::CreateAllLevelsSection() {
  return SNew(SExpandableArea)
    .BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
    .BodyBorderImage(FAppStyle::Get().GetBrush("NoBorder"))
    .HeaderPadding(FMargin(6, 2))
    .Padding(0.0f)
    .AllowAnimatedTransition(false)
    .InitiallyCollapsed(false)
    .HeaderContent()
    [
      SNew(STextBlock)
      .Text_Lambda([this]() {
        return FText::Format(
          FText::FromString(TEXT("All Levels ({0})")),
          FText::AsNumber(AllLevels.Num())
        );
      })
      .Font(FAppStyle::GetFontStyle("SmallFontBold"))
    ]
    .BodyContent()
    [
      SNew(SVerticalBox)
      
      + SVerticalBox::Slot()
      .AutoHeight()
      .Padding(6, 0, 6, 2)
      [
        SNew(SHorizontalBox)
        
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
          SAssignNew(LevelFilterBox, SSearchBox)
          .HintText(FText::FromString(TEXT("Search levels...")))
          .OnTextChanged(this, &SLevelSwitcherWidget::OnSearchChanged)
        ]
        
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(3, 0, 0, 0)
        [
          SNew(STextBlock)
          .Text_Lambda([this]() {
            return FText::AsNumber(FilteredLevels.Num());
          })
          .Font(FAppStyle::GetFontStyle("SmallFont"))
          .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ]
      ]
      
      + SVerticalBox::Slot()
      .FillHeight(1.0f)
      [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [
          SAssignNew(AllLevelsListView, SListView<TSharedPtr<FString>>)
          .ListItemsSource(&FilteredLevels)
          .OnGenerateRow(this, &SLevelSwitcherWidget::OnGenerateLevelFilteredRow)
          .OnMouseButtonDoubleClick(this, &SLevelSwitcherWidget::OnLevelDoubleClick)
          .OnSelectionChanged(this, &SLevelSwitcherWidget::OnAllLevelSelectionChanged)
          .SelectionMode(ESelectionMode::Single)
          .OnContextMenuOpening(FOnContextMenuOpening::CreateLambda([this]() {
            if (!AllLevelsListView.IsValid()) {
              return TSharedPtr<SWidget>();
            }
            TArray<TSharedPtr<FString>> SelectedItems;
            AllLevelsListView->GetSelectedItems(SelectedItems);
            if (SelectedItems.Num() > 0 && SelectedItems[0].IsValid()) {
              return TSharedPtr<SWidget>(MakeLevelContextMenu(SelectedItems[0], false));
            }
            return TSharedPtr<SWidget>();
          }))
          .HeaderRow(SNew(SHeaderRow).Visibility(EVisibility::Collapsed))
        ]
      ]
    ];
}

void SLevelSwitcherWidget::OnFavoriteSelectionChanged(
    TSharedPtr<FString> LevelPath, ESelectInfo::Type SelectInfo) {
  if (LevelPath.IsValid() && AllLevelsListView.IsValid()) {
    AllLevelsListView->ClearSelection();
  }
  SelectedLevelPath = LevelPath.IsValid() ? *LevelPath : FString();
}

void SLevelSwitcherWidget::OnAllLevelSelectionChanged(
    TSharedPtr<FString> LevelPath, ESelectInfo::Type SelectInfo) {
  if (LevelPath.IsValid() && FavoritesListView.IsValid()) {
    FavoritesListView->ClearSelection();
  }
  SelectedLevelPath = LevelPath.IsValid() ? *LevelPath : FString();
}

TSharedRef<SWidget> SLevelSwitcherWidget::MakeStartupContextMenu() {
  FMenuBuilder MenuBuilder(true, nullptr);
  if (SelectedLevelPath.IsEmpty()) {
    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Select a level to set as startup")),
        FText::GetEmpty(),
        FSlateIcon(),
        FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() {
          return false;
        })));
  } else {
    const FString LevelName = FPaths::GetBaseFilename(SelectedLevelPath);
    MenuBuilder.AddMenuEntry(
        FText::FromString(FString::Printf(TEXT("Set startup: %s"), *LevelName)),
        FText::FromString(SelectedLevelPath),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "DeviceDetails.PowerOn"),
        FUIAction(FExecuteAction::CreateLambda([this]() {
          if (Settings && !SelectedLevelPath.IsEmpty()) {
            Settings->SetStartupLevel(SelectedLevelPath);
            RefreshUI();
          }
        })));
  }
  return MenuBuilder.MakeWidget();
}

TSharedRef<ITableRow> SLevelSwitcherWidget::OnGenerateFavoriteRow(
    TSharedPtr<FString> LevelPath,
    const TSharedRef<STableViewBase> &OwnerTable) {
  
  FString LevelName = FPaths::GetBaseFilename(*LevelPath);
  int32 Index = FavoriteLevels.IndexOfByKey(LevelPath);

  return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
    .Padding(FMargin(3, 1))
    .OnDragDetected(this, &SLevelSwitcherWidget::OnFavoriteDragDetected, LevelPath, Index)
    .OnCanAcceptDrop(this, &SLevelSwitcherWidget::OnFavoriteCanAcceptDrop)
    .OnAcceptDrop(this, &SLevelSwitcherWidget::OnFavoriteAcceptDrop)
    [
      SNew(SOverlay)

      + SOverlay::Slot()
      [
        SNew(SBorder)
        .BorderImage(FAppStyle::Get().GetBrush("PlainBorder"))
        .BorderBackgroundColor(FStyleColors::Recessed)
        .Padding(FMargin(4, 2, 1, 2))
        [
        SNew(SHorizontalBox)

      // Open
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(0, 0, 3, 0)
      [
        SNew(SButton)
        .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
        .ForegroundColor(OpenYellow)
        .ContentPadding(FMargin(4, 1))
        .OnClicked_Lambda([this, LevelPath]() {
          return OnLoadLevel(*LevelPath);
        })
        [
          SNew(STextBlock)
          .Text(FText::FromString(TEXT("Open")))
          .Font(FAppStyle::GetFontStyle("SmallFont"))
          .ColorAndOpacity(OpenYellow)
        ]
      ]

      // Name
      + SHorizontalBox::Slot()
      .FillWidth(1.0f)
      .VAlign(VAlign_Center)
      [
        SNew(STextBlock)
        .Text(FText::FromString(LevelName))
        .Font(FAppStyle::GetFontStyle("SmallFont"))
      ]

      // Star (remove)
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(2, 0)
      [
        SNew(SButton)
        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
        .ContentPadding(0)
        .ToolTipText(FText::FromString(TEXT("Remove")))
        .OnClicked_Lambda([this, LevelPath]() {
          OnToggleFavorite(*LevelPath);
          return FReply::Handled();
        })
        [
          SNew(SImage)
          .Image(FAppStyle::Get().GetBrush("Icons.Star"))
          .ColorAndOpacity(StarMuted)
        ]
      ]

      // Startup icon
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(2, 0)
      [
        SNew(SBox)
        .WidthOverride(12.0f)
        .HeightOverride(12.0f)
        .Visibility_Lambda([this, LevelPath]() {
          if (!Settings || !LevelPath.IsValid()) {
            return EVisibility::Collapsed;
          }
          return Settings->StartupLevel == *LevelPath
              ? EVisibility::Visible
              : EVisibility::Collapsed;
        })
        [
          SNew(SImage)
          .Image(FAppStyle::Get().GetBrush("DeviceDetails.PowerOn"))
          .ColorAndOpacity(StartupIconColor)
        ]
      ]

      // Browse icon
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(2, 0, 0, 0)
      [
        SNew(SButton)
        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
        .ContentPadding(0)
        .ToolTipText(FText::FromString(TEXT("Browse")))
        .OnClicked_Lambda([this, LevelPath]() {
          return OnBrowseToInternal(*LevelPath);
        })
        [
          SNew(SImage)
          .Image(FAppStyle::Get().GetBrush("Icons.FolderOpen"))
          .ColorAndOpacity(FSlateColor::UseForeground())
        ]
      ]
        ]
      ]

      + SOverlay::Slot()
      .HAlign(HAlign_Fill)
      .VAlign(VAlign_Fill)
      [
        SNew(SBorder)
        .Visibility_Lambda([this, LevelPath]() {
          if (!LevelPath.IsValid()) {
            return EVisibility::Collapsed;
          }
          return IsLevelActive(*LevelPath)
              ? EVisibility::HitTestInvisible
              : EVisibility::Collapsed;
        })
        .BorderImage(&ActiveLevelBorderBrush)
        .BorderBackgroundColor(FLinearColor::Transparent)
        .Padding(0)
      ]
    ];
}

TSharedRef<ITableRow> SLevelSwitcherWidget::OnGenerateLevelFilteredRow(
    TSharedPtr<FString> LevelPath,
    const TSharedRef<STableViewBase> &OwnerTable) {
  
  FString LevelName = FPaths::GetBaseFilename(*LevelPath);
  bool bIsFavorite = Settings && Settings->IsFavorite(*LevelPath);

  return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
    .Padding(FMargin(3, 1))
    .OnDragDetected(this, &SLevelSwitcherWidget::OnAllLevelDragDetected, LevelPath)
    [
      SNew(SOverlay)

      + SOverlay::Slot()
      [
        SNew(SBorder)
        .BorderImage(FAppStyle::Get().GetBrush("PlainBorder"))
        .BorderBackgroundColor(FStyleColors::Recessed)
        .Padding(FMargin(4, 2, 1, 2))
        [
        SNew(SHorizontalBox)

      // Open
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(0, 0, 3, 0)
      [
        SNew(SButton)
        .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
        .ForegroundColor(OpenYellow)
        .ContentPadding(FMargin(4, 1))
        .OnClicked_Lambda([this, LevelPath]() {
          return OnLoadLevel(*LevelPath);
        })
        [
          SNew(STextBlock)
          .Text(FText::FromString(TEXT("Open")))
          .Font(FAppStyle::GetFontStyle("SmallFont"))
          .ColorAndOpacity(OpenYellow)
        ]
      ]

      // Name + Path
      + SHorizontalBox::Slot()
      .FillWidth(1.0f)
      .VAlign(VAlign_Center)
      [
        SNew(SVerticalBox)
        
        + SVerticalBox::Slot()
        .AutoHeight()
        [
          SNew(STextBlock)
          .Text(FText::FromString(LevelName))
          .Font(FAppStyle::GetFontStyle("SmallFont"))
        ]
        
        + SVerticalBox::Slot()
        .AutoHeight()
        [
          SNew(STextBlock)
          .Text(FText::FromString(*LevelPath))
          .Font(FAppStyle::GetFontStyle("TinyText"))
          .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ]
      ]

      // Star (add/remove)
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(2, 0)
      [
        SNew(SButton)
        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
        .ContentPadding(0)
        .ToolTipText(bIsFavorite ? 
          FText::FromString(TEXT("Remove from favorites")) : 
          FText::FromString(TEXT("Add to favorites")))
        .OnClicked_Lambda([this, LevelPath]() {
          OnToggleFavorite(*LevelPath);
          return FReply::Handled();
        })
      [
        SNew(SImage)
        .Image_Lambda([this, LevelPath]() -> const FSlateBrush* {
          if (Settings && LevelPath.IsValid() && Settings->IsFavorite(*LevelPath)) {
            return FAppStyle::Get().GetBrush("Icons.Star");
          }
          return FAppStyle::Get().GetBrush("Icons.Star.Outline");
        })
        .ColorAndOpacity_Lambda([this, LevelPath]() {
          if (Settings && LevelPath.IsValid() && Settings->IsFavorite(*LevelPath)) {
            return FSlateColor(StarMuted);
          }
          return FSlateColor::UseSubduedForeground();
        })
      ]
      ]

      // Startup icon
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(2, 0)
      [
        SNew(SBox)
        .WidthOverride(12.0f)
        .HeightOverride(12.0f)
        .Visibility_Lambda([this, LevelPath]() {
          if (!Settings || !LevelPath.IsValid()) {
            return EVisibility::Collapsed;
          }
          return Settings->StartupLevel == *LevelPath
              ? EVisibility::Visible
              : EVisibility::Collapsed;
        })
        [
          SNew(SImage)
          .Image(FAppStyle::Get().GetBrush("DeviceDetails.PowerOn"))
          .ColorAndOpacity(StartupIconColor)
        ]
      ]

      // Browse icon
      + SHorizontalBox::Slot()
      .AutoWidth()
      .VAlign(VAlign_Center)
      .Padding(2, 0, 0, 0)
      [
        SNew(SButton)
        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
        .ContentPadding(0)
        .ToolTipText(FText::FromString(TEXT("Browse")))
        .OnClicked_Lambda([this, LevelPath]() {
          return OnBrowseToInternal(*LevelPath);
        })
        [
          SNew(SImage)
          .Image(FAppStyle::Get().GetBrush("Icons.FolderOpen"))
          .ColorAndOpacity(FSlateColor::UseForeground())
        ]
      ]
        ]
      ]

      + SOverlay::Slot()
      .HAlign(HAlign_Fill)
      .VAlign(VAlign_Fill)
      [
        SNew(SBorder)
        .Visibility_Lambda([this, LevelPath]() {
          if (!LevelPath.IsValid()) {
            return EVisibility::Collapsed;
          }
          return IsLevelActive(*LevelPath)
              ? EVisibility::HitTestInvisible
              : EVisibility::Collapsed;
        })
        .BorderImage(&ActiveLevelBorderBrush)
        .BorderBackgroundColor(FLinearColor::Transparent)
        .Padding(0)
      ]
    ];
}

// Suite dans le prochain fichier...
// === PARTIE 2 : Fonctions utilitaires et callbacks ===

void SLevelSwitcherWidget::LoadFavorites() {
  FavoriteLevels.Empty();
  if (Settings) {
    for (const FString &FavPath : Settings->FavoriteLevels) {
      FavoriteLevels.Add(MakeShared<FString>(FavPath));
    }
  }
}

void SLevelSwitcherWidget::ScanAllLevels() {
  AllLevels.Empty();
  FilteredLevels.Empty();

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

  TArray<FAssetData> MapAssets;
  AssetRegistryModule.Get().GetAssetsByClass(UWorld::StaticClass()->GetClassPathName(), MapAssets);

  for (const FAssetData &Asset : MapAssets) {
    FString PackagePath = Asset.PackageName.ToString();
    AllLevels.Add(MakeShared<FString>(PackagePath));
    FilteredLevels.Add(AllLevels.Last());
  }
}

void SLevelSwitcherWidget::RefreshUI() {
  // Cache the active level path to keep highlight stable across list refreshes.
  const FString CurrentLevel = GetCurrentLevelPath();
  if (!CurrentLevel.IsEmpty()) {
    ActiveLevelPath = CurrentLevel;
  }
  LoadFavorites();
  if (FavoritesListView.IsValid()) {
    FavoritesListView->RequestListRefresh();
  }
  if (AllLevelsListView.IsValid()) {
    AllLevelsListView->RequestListRefresh();
  }
}

void SLevelSwitcherWidget::OnSearchChanged(const FText &Text) {
  CurrentSearchText = Text.ToString();
  FilteredLevels.Empty();

  if (CurrentSearchText.IsEmpty()) {
    FilteredLevels = AllLevels;
  } else {
    for (const auto &Level : AllLevels) {
      if (Level->Contains(CurrentSearchText)) {
        FilteredLevels.Add(Level);
      }
    }
  }

  if (AllLevelsListView.IsValid()) {
    AllLevelsListView->RequestListRefresh();
  }
}

TSharedRef<SWidget> SLevelSwitcherWidget::MakeLevelContextMenu(
    TSharedPtr<FString> LevelPath, bool bIsFavorite) {
  FMenuBuilder MenuBuilder(true, nullptr);

  MenuBuilder.AddMenuEntry(
      FText::FromString(TEXT("Set as startup level")),
      FText::FromString(TEXT("Auto-load on editor start")),
      FSlateIcon(FAppStyle::GetAppStyleSetName(), "DeviceDetails.PowerOn"),
      FUIAction(FExecuteAction::CreateLambda([this, LevelPath]() {
        OnSetStartupLevel(LevelPath, ESelectInfo::Direct);
      })));

  MenuBuilder.AddMenuSeparator();

  if (bIsFavorite) {
    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Remove from favorites")),
        FText::GetEmpty(),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Star"),
        FUIAction(FExecuteAction::CreateLambda([this, LevelPath]() {
          OnToggleFavorite(*LevelPath);
        })));
  } else {
    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Add to favorites")),
        FText::GetEmpty(),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Star"),
        FUIAction(FExecuteAction::CreateLambda([this, LevelPath]() {
          OnToggleFavorite(*LevelPath);
        })));
  }

  MenuBuilder.AddMenuSeparator();

  MenuBuilder.AddMenuEntry(
      FText::FromString(TEXT("Open in Content Browser")),
      FText::GetEmpty(),
      FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"),
      FUIAction(FExecuteAction::CreateLambda([this, LevelPath]() {
        OnBrowseToInternal(*LevelPath);
      })));

  MenuBuilder.AddMenuEntry(
      FText::FromString(TEXT("Copy reference")),
      FText::GetEmpty(),
      FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Clipboard"),
      FUIAction(FExecuteAction::CreateLambda([LevelPath]() {
        FPlatformApplicationMisc::ClipboardCopy(**LevelPath);
      })));

  MenuBuilder.AddMenuEntry(
      FText::FromString(TEXT("Reveal in Explorer")),
      FText::GetEmpty(),
      FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderExplore"),
      FUIAction(FExecuteAction::CreateLambda([LevelPath]() {
        FString FullPath;
        if (FPackageName::TryConvertLongPackageNameToFilename(*LevelPath, FullPath, TEXT(".umap"))) {
          FPlatformProcess::ExploreFolder(*FPaths::GetPath(FullPath));
        }
      })));

  return MenuBuilder.MakeWidget();
}

void SLevelSwitcherWidget::OnSetStartupLevel(TSharedPtr<FString> NewLevel, ESelectInfo::Type SelectInfo) {
  if (Settings && NewLevel.IsValid()) {
    Settings->SetStartupLevel(*NewLevel);
    RefreshUI();
  }
}

FReply SLevelSwitcherWidget::OnBrowseToInternal(const FString &LevelPath) {
  FContentBrowserModule &ContentBrowserModule =
      FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
  TArray<FAssetData> Assets;
  Assets.Add(FAssetData(FName(*LevelPath), FName(*FPaths::GetPath(LevelPath)),
                        FName(*FPaths::GetBaseFilename(LevelPath)),
                        UWorld::StaticClass()->GetClassPathName()));
  ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
  return FReply::Handled();
}

FReply SLevelSwitcherWidget::OnLoadLevel(const FString &LevelPath) {
  if (GEditor) {
    ActiveLevelPath = NormalizeLevelPath(LevelPath);
    FEditorFileUtils::LoadMap(LevelPath, false, true);
    RefreshUI();
  }
  return FReply::Handled();
}

void SLevelSwitcherWidget::OnToggleFavorite(const FString &LevelPath) {
  if (Settings) {
    if (Settings->IsFavorite(LevelPath)) {
      Settings->RemoveFavorite(LevelPath);
    } else {
      Settings->AddFavorite(LevelPath);
    }
    RefreshUI();
  }
}

void SLevelSwitcherWidget::OnLevelDoubleClick(TSharedPtr<FString> LevelPath) {
  if (LevelPath.IsValid()) {
    OnLoadLevel(*LevelPath);
  }
}

FReply SLevelSwitcherWidget::OnFavoriteDragDetected(
    const FGeometry &Geometry, const FPointerEvent &PointerEvent,
    TSharedPtr<FString> LevelPath, int32 Index) {
  if (PointerEvent.IsMouseButtonDown(EKeys::LeftMouseButton)) {
    return FReply::Handled().BeginDragDrop(
      FLevelDragDropOp::New(*LevelPath, true, Index));
  }
  return FReply::Unhandled();
}

TOptional<EItemDropZone> SLevelSwitcherWidget::OnFavoriteCanAcceptDrop(
    const FDragDropEvent &DragDropEvent, EItemDropZone DropZone,
    TSharedPtr<FString> TargetItem) {
  if (DragDropEvent.GetOperationAs<FLevelDragDropOp>().IsValid()) {
    return EItemDropZone::AboveItem;
  }
  return TOptional<EItemDropZone>();
}

FReply SLevelSwitcherWidget::OnFavoriteAcceptDrop(
    const FDragDropEvent &DragDropEvent, EItemDropZone DropZone,
    TSharedPtr<FString> TargetItem) {
  TSharedPtr<FLevelDragDropOp> DragDropOp =
      DragDropEvent.GetOperationAs<FLevelDragDropOp>();
  if (!DragDropOp.IsValid())
    return FReply::Unhandled();

  if (DragDropOp->bIsFromFavorites) {
    int32 TargetIndex = FavoriteLevels.IndexOfByKey(TargetItem);
    Settings->ReorderFavorite(DragDropOp->OriginalIndex, TargetIndex);
  } else {
    Settings->AddFavorite(DragDropOp->LevelPath);
  }

  RefreshUI();
  return FReply::Handled();
}

FReply SLevelSwitcherWidget::OnAllLevelDragDetected(
    const FGeometry &Geometry, const FPointerEvent &PointerEvent,
    TSharedPtr<FString> LevelPath) {
  if (PointerEvent.IsMouseButtonDown(EKeys::LeftMouseButton)) {
    return FReply::Handled().BeginDragDrop(
      FLevelDragDropOp::New(*LevelPath, false));
  }
  return FReply::Unhandled();
}

bool SLevelSwitcherWidget::OnStartupDropAllowDrop(TSharedPtr<FDragDropOperation> DragDropOp) {
  return DragDropOp->IsOfType<FLevelDragDropOp>();
}

FReply SLevelSwitcherWidget::OnStartupDropAccept(const FGeometry &Geometry, const FDragDropEvent &DragDropEvent) {
  TSharedPtr<FLevelDragDropOp> DragDropOp =
      DragDropEvent.GetOperationAs<FLevelDragDropOp>();
  if (DragDropOp.IsValid()) {
    for (const auto &Level : AllLevels) {
      if (*Level == DragDropOp->LevelPath) {
        OnSetStartupLevel(Level, ESelectInfo::Direct);
        break;
      }
    }
    return FReply::Handled();
  }
  return FReply::Unhandled();
}

FString SLevelSwitcherWidget::GetCurrentLevelPath() const {
  if (GEditor) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (World) {
      return NormalizeLevelPath(World->GetPathName());
    }
  }
  return "";
}

bool SLevelSwitcherWidget::IsLevelActive(const FString &LevelPath) const {
  FString ActivePath = ActiveLevelPath;
  if (ActivePath.IsEmpty()) {
    ActivePath = GetCurrentLevelPath();
  }
  if (ActivePath.IsEmpty()) {
    return false;
  }
  return NormalizeLevelPath(ActivePath)
      .Equals(NormalizeLevelPath(LevelPath), ESearchCase::IgnoreCase);
}
