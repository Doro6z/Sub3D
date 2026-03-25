// Copyright (c) 2026 DXV Systems.
#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"


class FLevelDragDropOp : public FDragDropOperation {
public:
  DRAG_DROP_OPERATOR_TYPE(FLevelDragDropOp, FDragDropOperation)

  FString LevelPath;
  FString LevelName;
  bool bIsFromFavorites;
  int32 OriginalIndex; // For reordering

  static TSharedRef<FLevelDragDropOp>
  New(const FString &InLevelPath, bool bFromFavorites, int32 Index = -1);

  virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
};
