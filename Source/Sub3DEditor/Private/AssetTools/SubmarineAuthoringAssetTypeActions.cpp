#include "AssetTools/SubmarineAuthoringAssetTypeActions.h"

#include "Authoring/SubmarineAuthoringAsset.h"
#include "Slate/SubmarineEditorToolkit.h"

#define LOCTEXT_NAMESPACE "FSubmarineAuthoringAssetTypeActions"

FText FSubmarineAuthoringAssetTypeActions::GetName() const
{
    return LOCTEXT("AssetTypeName", "Submarine Authoring Asset");
}

FColor FSubmarineAuthoringAssetTypeActions::GetTypeColor() const
{
    return FColor(18, 43, 51);
}

UClass* FSubmarineAuthoringAssetTypeActions::GetSupportedClass() const
{
    return USub3DSubmarineAuthoringAsset::StaticClass();
}

uint32 FSubmarineAuthoringAssetTypeActions::GetCategories()
{
    return EAssetTypeCategories::Misc;
}

void FSubmarineAuthoringAssetTypeActions::OpenAssetEditor(
    const TArray<UObject*>& InObjects,
    TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
    for (UObject* Object : InObjects)
    {
        USub3DSubmarineAuthoringAsset* Asset = Cast<USub3DSubmarineAuthoringAsset>(Object);
        if (!Asset)
        {
            continue;
        }

        TSharedRef<FSubmarineEditorToolkit> NewToolkit = MakeShared<FSubmarineEditorToolkit>();
        TArray<UObject*> ObjectsToEdit;
        ObjectsToEdit.Add(Asset);
        NewToolkit->Init(EToolkitMode::Standalone, EditWithinLevelEditor, ObjectsToEdit);
    }
}

#undef LOCTEXT_NAMESPACE
