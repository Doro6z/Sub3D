#pragma once

#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Toolkits/AssetEditorToolkit.h"

class ASubmarineEditorActor;
class ASubmarinePreviewActor;
class IDetailsView;
class SWidget;
class USub3DSubmarineAuthoringAsset;

class FSubmarineEditorToolkit : public FAssetEditorToolkit
{
public:
    void Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UObject*>& InObjectsToEdit);

    virtual FName GetToolkitFName() const override;
    virtual FText GetBaseToolkitName() const override;
    virtual FString GetWorldCentricTabPrefix() const override;
    virtual FLinearColor GetWorldCentricTabColorScale() const override;

    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

private:
    TSharedRef<SDockTab> SpawnDrydockTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnRingTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnEnvelopeTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnAppendagesTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnBaysTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnFloorsTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnOpeningsTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnPartitionsTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnValidateBakeTab(const FSpawnTabArgs& Args);

    void ExtendToolbar(FToolBarBuilder& Builder);
    void HandlePreviewLayoutVoid() { HandlePreviewLayout(); }

    FReply HandleValidate();
    FReply HandleBake();
    FReply HandleBakeAndSpawn();
    FReply HandlePreviewLayout();
    FReply HandleGenerateRings();
    FReply HandleDebugDrawBays();
    FReply HandleTestPIE();
    FReply HandleConfirmGeometry();
    FReply HandleApplyPreset(FName PresetName);

    TSharedRef<SWidget> BuildPresetRow();
    FString GetAuthoringAssetName() const;
    ASubmarineEditorActor* FindOrCreateEditorActor() const;
    ASubmarinePreviewActor* FindOrCreatePreviewActor() const;
    void OnPreviewPropertiesChanged(const FPropertyChangedEvent& Event);
    TSharedRef<SWidget> BuildPhaseSummary(const FText& PhaseTitle, const FText& Description, const FText& ScopeHint) const;
    TSharedPtr<IDetailsView> CreateDetailsViewForCategories(const TArray<FString>& Categories) const;
    TSharedRef<SWidget> BuildPhasePanel(const FText& PhaseTitle, const FText& Description, const FText& ScopeHint, const TSharedPtr<IDetailsView>& DetailsView) const;
    void SetOperationStatus(const FString& InStatus, const TArray<FString>& InMessages);
    FText GetOperationStatusText() const;
    FText GetContractStatusText() const;
    FText GetBakeStatsText() const;
    void BuildContractWarnings(TArray<FString>& OutWarnings) const;

    static const FName DrydockTabId;
    static const FName RingTabId;
    static const FName EnvelopeTabId;
    static const FName AppendagesTabId;
    static const FName BaysTabId;
    static const FName FloorsTabId;
    static const FName OpeningsTabId;
    static const FName PartitionsTabId;
    static const FName ValidateBakeTabId;

    TWeakObjectPtr<USub3DSubmarineAuthoringAsset> ActiveAuthoringAsset;
    TWeakObjectPtr<ASubmarinePreviewActor>        ActivePreviewActor;
    int32 AutoRingCount = 8;
    TSharedPtr<IDetailsView> AuthoringDetailsView;
    TSharedPtr<IDetailsView> RingDetailsView;
    TSharedPtr<IDetailsView> EnvelopeDetailsView;
    TSharedPtr<IDetailsView> AppendagesDetailsView;
    TSharedPtr<IDetailsView> BaysDetailsView;
    TSharedPtr<IDetailsView> FloorsDetailsView;
    TSharedPtr<IDetailsView> OpeningsDetailsView;
    TSharedPtr<IDetailsView> PartitionsDetailsView;
    FString LastOperationStatus;
    TArray<FString> LastOperationMessages;
};
