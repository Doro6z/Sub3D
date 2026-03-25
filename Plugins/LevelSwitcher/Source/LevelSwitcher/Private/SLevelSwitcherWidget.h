// Copyright (c) 2026 DXV Systems.
#pragma once

#include "CoreMinimal.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ULevelSwitcherSettings;
class UWorld;

/**
 * Level Switcher - Compact & Native
 * Simple favorites + quick startup level selector
 */
class SLevelSwitcherWidget : public SCompoundWidget {
public:
  SLATE_BEGIN_ARGS(SLevelSwitcherWidget) {}
  SLATE_END_ARGS()

  void Construct(const FArguments &InArgs);
  virtual ~SLevelSwitcherWidget();

private:
  // === Sections ===
  TSharedRef<SWidget> CreateSettingsSection();
  TSharedRef<SWidget> CreateFavoritesSection();
  TSharedRef<SWidget> CreateAllLevelsSection();

  // === Data Loading ===
  void LoadFavorites();
  void ScanAllLevels();
  void RefreshUI();
  void OnSearchChanged(const FText &Text);
  void OnEditorMapOpened(const FString &Filename, bool bAsTemplate);
  void OnPostLoadMap(UWorld *LoadedWorld);
  void OnFavoriteSelectionChanged(
      TSharedPtr<FString> LevelPath, ESelectInfo::Type SelectInfo);
  void OnAllLevelSelectionChanged(
      TSharedPtr<FString> LevelPath, ESelectInfo::Type SelectInfo);
  TSharedRef<SWidget> MakeStartupContextMenu();

  // === Context Menu ===
  TSharedRef<SWidget> MakeLevelContextMenu(TSharedPtr<FString> LevelPath, bool bIsFavorite);

  // === Actions ===
  void OnSetStartupLevel(TSharedPtr<FString> NewLevel, ESelectInfo::Type SelectInfo);
  FReply OnBrowseToInternal(const FString &LevelPath);
  FReply OnLoadLevel(const FString &LevelPath);
  void OnToggleFavorite(const FString &LevelPath);
  void OnLevelDoubleClick(TSharedPtr<FString> LevelPath);

  // === Row Generation ===
  TSharedRef<ITableRow> OnGenerateFavoriteRow(
      TSharedPtr<FString> LevelPath,
      const TSharedRef<STableViewBase> &OwnerTable);
  TSharedRef<ITableRow> OnGenerateLevelFilteredRow(
      TSharedPtr<FString> LevelPath,
      const TSharedRef<STableViewBase> &OwnerTable);

  // === Drag & Drop (Favorites) ===
  FReply OnFavoriteDragDetected(
      const FGeometry &Geometry,
      const FPointerEvent &PointerEvent,
      TSharedPtr<FString> LevelPath,
      int32 Index);
  FReply OnFavoriteAcceptDrop(
      const FDragDropEvent &DragDropEvent,
      EItemDropZone DropZone,
      TSharedPtr<FString> TargetItem);
  TOptional<EItemDropZone> OnFavoriteCanAcceptDrop(
      const FDragDropEvent &DragDropEvent,
      EItemDropZone DropZone,
      TSharedPtr<FString> TargetItem);

  // === Drag & Drop (All Levels) ===
  FReply OnAllLevelDragDetected(
      const FGeometry &Geometry,
      const FPointerEvent &PointerEvent,
      TSharedPtr<FString> LevelPath);

  // === Drag & Drop (Startup) ===
  bool OnStartupDropAllowDrop(TSharedPtr<FDragDropOperation> DragDropOp);
  FReply OnStartupDropAccept(
      const FGeometry &Geometry,
      const FDragDropEvent &DragDropEvent);

  // === Utility ===
  FString GetCurrentLevelPath() const;
  bool IsLevelActive(const FString &LevelPath) const;
  FText GetStatusText() const;

  // === Data ===
  ULevelSwitcherSettings *Settings = nullptr;
  TArray<TSharedPtr<FString>> AllLevels;
  TArray<TSharedPtr<FString>> FavoriteLevels;
  TArray<TSharedPtr<FString>> FilteredLevels;

  // === UI Elements ===
  TSharedPtr<SComboBox<TSharedPtr<FString>>> StartupLevelComboBox;
  TSharedPtr<SListView<TSharedPtr<FString>>> FavoritesListView;
  TSharedPtr<SListView<TSharedPtr<FString>>> AllLevelsListView;
  TSharedPtr<SSearchBox> LevelFilterBox;
  FString CurrentSearchText;
  FString ActiveLevelPath;
  FString SelectedLevelPath;
  FDelegateHandle MapOpenedHandle;
  FDelegateHandle PostLoadMapHandle;
};
