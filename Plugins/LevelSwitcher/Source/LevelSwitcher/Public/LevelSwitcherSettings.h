// Copyright (c) 2026 DXV Systems.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LevelSwitcherSettings.generated.h"

/**
 * Level Switcher Settings
 * Stores favorites and startup level in Config INI
 */
UCLASS(Config = EditorPerProjectUserSettings, defaultconfig,
       meta = (DisplayName = "Level Switcher"))
class LEVELSWITCHER_API ULevelSwitcherSettings : public UDeveloperSettings {
  GENERATED_BODY()

public:
  ULevelSwitcherSettings();

  /** Array of favorite level paths (soft object paths as strings) */
  UPROPERTY(Config)
  TArray<FString> FavoriteLevels;

  /** Startup level path (auto-loads on editor start) */
  UPROPERTY(Config)
  FString StartupLevel;

  // === API ===

  /** Add level to favorites */
  void AddFavorite(const FString &LevelPath);

  /** Remove level from favorites */
  void RemoveFavorite(const FString &LevelPath);

  /** Reorder favorite from one index to another */
  void ReorderFavorite(int32 FromIndex, int32 ToIndex);

  /** Set startup level (auto-clears previous) */
  void SetStartupLevel(const FString &LevelPath);

  /** Check if level is in favorites */
  bool IsFavorite(const FString &LevelPath) const;

  /** Get index of level in favorites (-1 if not found) */
  int32 GetFavoriteIndex(const FString &LevelPath) const;

  /** Save config to disk */
  void SaveConfig();
};
