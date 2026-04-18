#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class UMaterialInstanceConstant;

class SCraniataMaterialEditorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCraniataMaterialEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	struct FMaterialEntry
	{
		FString Label;
		FString AssetPath;
	};

	void BuildMaterialEntries();
	void RefreshDetailsView();
	UMaterialInstanceConstant* LoadMaterialAtIndex(int32 Index) const;
	FReply HandleSelectMaterial(int32 Index);
	FReply HandleRefresh();
	FReply HandleBrowseSelected();
	FText GetSelectedLabelText() const;

	TArray<FMaterialEntry> MaterialEntries;
	int32 SelectedIndex = INDEX_NONE;
	TSharedPtr<IDetailsView> DetailsView;
};
