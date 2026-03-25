// Copyright (c) 2026 DXV Systems.
#include "LevelDragDropOp.h"
#include "Styling/AppStyle.h"

TSharedRef<FLevelDragDropOp> FLevelDragDropOp::New(const FString &InLevelPath,
                                                   bool bFromFavorites,
                                                   int32 Index) {
  TSharedRef<FLevelDragDropOp> Operation = MakeShared<FLevelDragDropOp>();
  Operation->LevelPath = InLevelPath;
  Operation->LevelName = FPaths::GetBaseFilename(InLevelPath);
  Operation->bIsFromFavorites = bFromFavorites;
  Operation->OriginalIndex = Index;
  Operation->Construct();
  return Operation;
}

TSharedPtr<SWidget> FLevelDragDropOp::GetDefaultDecorator() const {
  return SNew(SBorder)
      .BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
      .Content()[SNew(SHorizontalBox) +
                 SHorizontalBox::Slot().AutoWidth().Padding(
                     5)[SNew(STextBlock).Text(FText::FromString(TEXT("Map")))] +
                 SHorizontalBox::Slot().AutoWidth()
                     [SNew(STextBlock).Text(FText::FromString(LevelName))]];
}
