#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class SDockTab;
class FSubmarineAuthoringAssetTypeActions;
class FSubmarineEditorToolkit;
class FSubmarineRingHandleVisualizer;
class FSpawnTabArgs;

class FSub3DEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void OpenSubmarineEditor();
    void OpenCraniataMaterialEditor();
    TSharedRef<SDockTab> SpawnCraniataMaterialEditorTab(const FSpawnTabArgs& Args);

    static const FName CraniataMaterialEditorTabId;

    TSharedPtr<FSubmarineAuthoringAssetTypeActions> AssetTypeActions;
    TSharedPtr<FSubmarineEditorToolkit>             ActiveToolkit;
    TSharedPtr<FSubmarineRingHandleVisualizer>      RingHandleVisualizer;
};
