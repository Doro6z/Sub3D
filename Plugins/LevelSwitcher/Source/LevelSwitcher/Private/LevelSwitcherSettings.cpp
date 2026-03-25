// Copyright (c) 2026 DXV Systems.
#include "LevelSwitcherSettings.h"

ULevelSwitcherSettings::ULevelSwitcherSettings() {
  CategoryName = TEXT("Plugins");
  SectionName = TEXT("Level Switcher");
}

void ULevelSwitcherSettings::AddFavorite(const FString &LevelPath) {
  if (!FavoriteLevels.Contains(LevelPath)) {
    FavoriteLevels.Add(LevelPath);
    SaveConfig();
  }
}

void ULevelSwitcherSettings::RemoveFavorite(const FString &LevelPath) {
  if (FavoriteLevels.Remove(LevelPath) > 0) {
    // If removed level was startup, clear startup
    if (StartupLevel == LevelPath) {
      StartupLevel.Empty();
    }
    SaveConfig();
  }
}

void ULevelSwitcherSettings::ReorderFavorite(int32 FromIndex, int32 ToIndex) {
  if (FavoriteLevels.IsValidIndex(FromIndex) &&
      FavoriteLevels.IsValidIndex(ToIndex)) {
    FString Temp = FavoriteLevels[FromIndex];
    FavoriteLevels.RemoveAt(FromIndex);
    FavoriteLevels.Insert(Temp, ToIndex);
    SaveConfig();
  }
}

void ULevelSwitcherSettings::SetStartupLevel(const FString &LevelPath) {
  StartupLevel = LevelPath;
  SaveConfig();
}

bool ULevelSwitcherSettings::IsFavorite(const FString &LevelPath) const {
  return FavoriteLevels.Contains(LevelPath);
}

int32 ULevelSwitcherSettings::GetFavoriteIndex(const FString &LevelPath) const {
  return FavoriteLevels.IndexOfByKey(LevelPath);
}

void ULevelSwitcherSettings::SaveConfig() {
  UObject::SaveConfig(CPF_Config, *GetDefaultConfigFilename());
}
