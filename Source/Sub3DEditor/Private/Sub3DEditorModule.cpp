#include "Sub3DEditorModule.h"
#include "AssetTools/SubmarineAuthoringAssetTypeActions.h"
#include "Authoring/SubmarineAuthoringAsset.h"
#include "Editor/SubmarineRingHandlesComponent.h"
#include "Preview/SubmarineRingHandleVisualizer.h"
#include "Slate/SCraniataMaterialEditorPanel.h"
#include "Slate/SubmarineEditorToolkit.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FSub3DEditorModule"

const FName FSub3DEditorModule::CraniataMaterialEditorTabId(TEXT("Sub3D.CraniataMaterialEditor"));

void FSub3DEditorModule::StartupModule()
{
    // ── Asset type actions ────────────────────────────────────────────────────
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    AssetTypeActions = MakeShared<FSubmarineAuthoringAssetTypeActions>();
    AssetTools.RegisterAssetTypeActions(AssetTypeActions.ToSharedRef());

    // ── Component visualizer ──────────────────────────────────────────────────
    if (GUnrealEd)
    {
        RingHandleVisualizer = MakeShared<FSubmarineRingHandleVisualizer>();
        GUnrealEd->RegisterComponentVisualizer(
            USubmarineRingHandlesComponent::StaticClass()->GetFName(),
            RingHandleVisualizer);
    }

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSub3DEditorModule::RegisterMenus));

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        CraniataMaterialEditorTabId,
        FOnSpawnTab::CreateRaw(this, &FSub3DEditorModule::SpawnCraniataMaterialEditorTab))
        .SetDisplayName(LOCTEXT("CraniataMaterialEditorTab", "Craniata Material Editor"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FSub3DEditorModule::ShutdownModule()
{
    // ── Unregister visualizer ─────────────────────────────────────────────────
    if (GUnrealEd && RingHandleVisualizer.IsValid())
    {
        GUnrealEd->UnregisterComponentVisualizer(
            USubmarineRingHandlesComponent::StaticClass()->GetFName());
    }
    RingHandleVisualizer.Reset();

    // ── Unregister asset tools ────────────────────────────────────────────────
    if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
    {
        IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
        if (AssetTypeActions.IsValid())
        {
            AssetTools.UnregisterAssetTypeActions(AssetTypeActions.ToSharedRef());
        }
    }

    AssetTypeActions.Reset();
    ActiveToolkit.Reset();

    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CraniataMaterialEditorTabId);
}

void FSub3DEditorModule::RegisterMenus()
{
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
    if (!Menu) { return; }

    FToolMenuSection& Section = Menu->FindOrAddSection("Sub3D");
    Section.AddMenuEntry(
        "OpenSubmarineEditorToolkit",
        LOCTEXT("OpenSubmarineEditorToolkit", "Submarine Editor"),
        LOCTEXT("OpenSubmarineEditorToolkit_Tooltip", "Open the Submarine Editor toolkit."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FSub3DEditorModule::OpenSubmarineEditor)));

    Section.AddMenuEntry(
        "OpenCraniataMaterialEditor",
        LOCTEXT("OpenCraniataMaterialEditor", "Craniata Material Editor"),
        LOCTEXT("OpenCraniataMaterialEditor_Tooltip", "Open the Craniata material editor panel."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FSub3DEditorModule::OpenCraniataMaterialEditor)));
}

void FSub3DEditorModule::OpenSubmarineEditor()
{
    USub3DSubmarineAuthoringAsset* SelectedAsset = nullptr;
    if (GEditor)
    {
        if (USelection* SelectedObjects = GEditor->GetSelectedObjects())
        {
            SelectedAsset = Cast<USub3DSubmarineAuthoringAsset>(
                SelectedObjects->GetTop(USub3DSubmarineAuthoringAsset::StaticClass()));
        }
    }

    if (!SelectedAsset)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Select a USub3DSubmarineAuthoringAsset in the Content Browser before opening the Submarine Editor."));

        if (ActiveToolkit.IsValid())
        {
            ActiveToolkit->BringToolkitToFront();
        }
        return;
    }

    ActiveToolkit = MakeShared<FSubmarineEditorToolkit>();
    TArray<UObject*> ObjectsToEdit;
    ObjectsToEdit.Add(SelectedAsset);
    ActiveToolkit->Init(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), ObjectsToEdit);
}

void FSub3DEditorModule::OpenCraniataMaterialEditor()
{
    FGlobalTabmanager::Get()->TryInvokeTab(CraniataMaterialEditorTabId);
}

TSharedRef<SDockTab> FSub3DEditorModule::SpawnCraniataMaterialEditorTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SCraniataMaterialEditorPanel)
        ];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSub3DEditorModule, Sub3DEditor)
