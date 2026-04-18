#include "Slate/SubmarineEditorToolkit.h"

#include "Editor/SubmarineEditorActor.h"
#include "Editor/SubmarinePreviewActor.h"
#include "Authoring/SubmarineAuthoringAsset.h"
#include "Bake/SubmarineBakeSubsystem.h"
#include "Data/CompiledSubmarineBaseAsset.h"
#include "Data/CompiledSubmarineRuntimeAsset.h"
#include "Slate/SSubmarineAppendagesPanel.h"
#include "Slate/SSubmarineHullStatsBar.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "IDetailsView.h"
#include "Internationalization/Text.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Preview/SubmarineBayDebugDraw.h"
#include "Tools/SubmarineNavigationTestService.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FSubmarineEditorToolkit"

const FName FSubmarineEditorToolkit::DrydockTabId(TEXT("SubmarineEditorToolkit.Drydock"));
const FName FSubmarineEditorToolkit::RingTabId(TEXT("SubmarineEditorToolkit.RingArchitecture"));
const FName FSubmarineEditorToolkit::EnvelopeTabId(TEXT("SubmarineEditorToolkit.OuterEnvelope"));
const FName FSubmarineEditorToolkit::AppendagesTabId(TEXT("SubmarineEditorToolkit.Appendages"));
const FName FSubmarineEditorToolkit::BaysTabId(TEXT("SubmarineEditorToolkit.Bays"));
const FName FSubmarineEditorToolkit::FloorsTabId(TEXT("SubmarineEditorToolkit.Floors"));
const FName FSubmarineEditorToolkit::OpeningsTabId(TEXT("SubmarineEditorToolkit.Openings"));
const FName FSubmarineEditorToolkit::PartitionsTabId(TEXT("SubmarineEditorToolkit.Partitions"));
const FName FSubmarineEditorToolkit::ValidateBakeTabId(TEXT("SubmarineEditorToolkit.ValidateBake"));

void FSubmarineEditorToolkit::Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UObject*>& InObjectsToEdit)
{
    ActiveAuthoringAsset = nullptr;
    ActivePreviewActor   = nullptr;
    AuthoringDetailsView.Reset();
    RingDetailsView.Reset();
    EnvelopeDetailsView.Reset();
    AppendagesDetailsView.Reset();
    BaysDetailsView.Reset();
    FloorsDetailsView.Reset();
    OpeningsDetailsView.Reset();
    PartitionsDetailsView.Reset();
    LastOperationStatus = TEXT("No operation yet.");
    LastOperationMessages.Reset();

    for (UObject* ObjectToEdit : InObjectsToEdit)
    {
        if (USub3DSubmarineAuthoringAsset* Asset = Cast<USub3DSubmarineAuthoringAsset>(ObjectToEdit))
        {
            ActiveAuthoringAsset = Asset;
            break;
        }
    }

    if (ActiveAuthoringAsset.IsValid())
    {
        FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
        FDetailsViewArgs DetailsArgs;
        DetailsArgs.bAllowSearch = true;
        DetailsArgs.bHideSelectionTip = true;
        DetailsArgs.bLockable = false;
        DetailsArgs.bUpdatesFromSelection = false;
        DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

        AuthoringDetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);
        AuthoringDetailsView->SetObject(ActiveAuthoringAsset.Get());

        RingDetailsView       = CreateDetailsViewForCategories({TEXT("[A] Pressure Hull")});
        EnvelopeDetailsView   = CreateDetailsViewForCategories({TEXT("[A] Outer Envelope"), TEXT("Outer Envelope")});
        AppendagesDetailsView = CreateDetailsViewForCategories({TEXT("[A] Appendages")});
        BaysDetailsView       = CreateDetailsViewForCategories({TEXT("[B] Structural Bays")});
        FloorsDetailsView     = CreateDetailsViewForCategories({TEXT("[B] Deck Levels"), TEXT("[B] Floor Regions")});
        OpeningsDetailsView   = CreateDetailsViewForCategories({TEXT("[B] Openings"), TEXT("[C] Connectors"), TEXT("[C] Closures")});
        PartitionsDetailsView = CreateDetailsViewForCategories({TEXT("[C] Partitions")});

        if (RingDetailsView.IsValid())
        {
            RingDetailsView->SetObject(ActiveAuthoringAsset.Get());
            RingDetailsView->OnFinishedChangingProperties().AddRaw(
                this, &FSubmarineEditorToolkit::OnPreviewPropertiesChanged);
        }
        if (EnvelopeDetailsView.IsValid())
        {
            EnvelopeDetailsView->SetObject(ActiveAuthoringAsset.Get());
            EnvelopeDetailsView->OnFinishedChangingProperties().AddRaw(
                this, &FSubmarineEditorToolkit::OnPreviewPropertiesChanged);
        }
        if (AppendagesDetailsView.IsValid())
        {
            AppendagesDetailsView->SetObject(ActiveAuthoringAsset.Get());
        }
        if (BaysDetailsView.IsValid())
        {
            BaysDetailsView->SetObject(ActiveAuthoringAsset.Get());
        }
        if (FloorsDetailsView.IsValid())
        {
            FloorsDetailsView->SetObject(ActiveAuthoringAsset.Get());
        }
        if (OpeningsDetailsView.IsValid())
        {
            OpeningsDetailsView->SetObject(ActiveAuthoringAsset.Get());
        }
        if (PartitionsDetailsView.IsValid())
        {
            PartitionsDetailsView->SetObject(ActiveAuthoringAsset.Get());
        }
    }

    const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("SubmarineEditorToolkit_Layout_v13")
        ->AddArea(
            FTabManager::NewPrimaryArea()
                ->SetOrientation(Orient_Horizontal)
                ->Split(
                    FTabManager::NewStack()
                        ->AddTab(DrydockTabId,      ETabState::OpenedTab)
                        ->AddTab(RingTabId,         ETabState::OpenedTab)
                        ->AddTab(EnvelopeTabId,     ETabState::OpenedTab)
                        ->AddTab(AppendagesTabId,   ETabState::OpenedTab)
                        ->AddTab(BaysTabId,         ETabState::OpenedTab)
                        ->AddTab(FloorsTabId,       ETabState::OpenedTab)
                        ->AddTab(OpeningsTabId,     ETabState::OpenedTab)
                        ->AddTab(PartitionsTabId,   ETabState::OpenedTab)
                        ->AddTab(ValidateBakeTabId, ETabState::OpenedTab)
                )
        );

    TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();
    ToolbarExtender->AddToolBarExtension(
        "Asset",
        EExtensionHook::After,
        GetToolkitCommands(),
        FToolBarExtensionDelegate::CreateRaw(this, &FSubmarineEditorToolkit::ExtendToolbar));
    AddToolbarExtender(ToolbarExtender);

    InitAssetEditor(Mode, InitToolkitHost, FName("SubmarineEditorToolkitApp"), Layout, true, true, InObjectsToEdit);
}

FName FSubmarineEditorToolkit::GetToolkitFName() const
{
    return FName("SubmarineEditorToolkit");
}

FText FSubmarineEditorToolkit::GetBaseToolkitName() const
{
    return LOCTEXT("ToolkitName", "Submarine Editor");
}

FString FSubmarineEditorToolkit::GetWorldCentricTabPrefix() const
{
    return TEXT("Submarine");
}

FLinearColor FSubmarineEditorToolkit::GetWorldCentricTabColorScale() const
{
    return FLinearColor(0.12f, 0.17f, 0.2f, 1.0f);
}

void FSubmarineEditorToolkit::ExtendToolbar(FToolBarBuilder& Builder)
{
    Builder.BeginSection("SubmarinePreview");
    Builder.AddToolBarButton(
        FUIAction(FExecuteAction::CreateRaw(this, &FSubmarineEditorToolkit::HandlePreviewLayoutVoid)),
        NAME_None,
        LOCTEXT("ToolbarPreview", "Preview"),
        LOCTEXT("ToolbarPreviewTooltip", "Spawn or refresh the hull preview actor in the viewport."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visible"));
    Builder.EndSection();
}

void FSubmarineEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

    InTabManager->RegisterTabSpawner(DrydockTabId, FOnSpawnTab::CreateRaw(this, &FSubmarineEditorToolkit::SpawnDrydockTab))
        .SetDisplayName(LOCTEXT("DrydockTab", "Drydock"));
    InTabManager->RegisterTabSpawner(RingTabId, FOnSpawnTab::CreateRaw(this, &FSubmarineEditorToolkit::SpawnRingTab))
        .SetDisplayName(LOCTEXT("RingTab", "Ring Architecture"));
    InTabManager->RegisterTabSpawner(EnvelopeTabId, FOnSpawnTab::CreateRaw(this, &FSubmarineEditorToolkit::SpawnEnvelopeTab))
        .SetDisplayName(LOCTEXT("EnvelopeTab", "Outer Envelope"));
    InTabManager->RegisterTabSpawner(AppendagesTabId, FOnSpawnTab::CreateRaw(this, &FSubmarineEditorToolkit::SpawnAppendagesTab))
        .SetDisplayName(LOCTEXT("AppendagesTab", "Appendages"));
    InTabManager->RegisterTabSpawner(BaysTabId, FOnSpawnTab::CreateRaw(this, &FSubmarineEditorToolkit::SpawnBaysTab))
        .SetDisplayName(LOCTEXT("BaysTab", "Bays"));
    InTabManager->RegisterTabSpawner(FloorsTabId, FOnSpawnTab::CreateRaw(this, &FSubmarineEditorToolkit::SpawnFloorsTab))
        .SetDisplayName(LOCTEXT("FloorsTab", "Decks / Floors"));
    InTabManager->RegisterTabSpawner(OpeningsTabId, FOnSpawnTab::CreateRaw(this, &FSubmarineEditorToolkit::SpawnOpeningsTab))
        .SetDisplayName(LOCTEXT("OpeningsTab", "Openings"));
    InTabManager->RegisterTabSpawner(PartitionsTabId, FOnSpawnTab::CreateRaw(this, &FSubmarineEditorToolkit::SpawnPartitionsTab))
        .SetDisplayName(LOCTEXT("PartitionsTab", "Partitions"));
    InTabManager->RegisterTabSpawner(ValidateBakeTabId, FOnSpawnTab::CreateRaw(this, &FSubmarineEditorToolkit::SpawnValidateBakeTab))
        .SetDisplayName(LOCTEXT("ValidateBakeTab", "Validate / Bake"));
}

void FSubmarineEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
    InTabManager->UnregisterTabSpawner(DrydockTabId);
    InTabManager->UnregisterTabSpawner(RingTabId);
    InTabManager->UnregisterTabSpawner(EnvelopeTabId);
    InTabManager->UnregisterTabSpawner(AppendagesTabId);
    InTabManager->UnregisterTabSpawner(BaysTabId);
    InTabManager->UnregisterTabSpawner(FloorsTabId);
    InTabManager->UnregisterTabSpawner(OpeningsTabId);
    InTabManager->UnregisterTabSpawner(PartitionsTabId);
    InTabManager->UnregisterTabSpawner(ValidateBakeTabId);
}

// ── Utility builders ──────────────────────────────────────────────────────────

TSharedRef<SWidget> FSubmarineEditorToolkit::BuildPresetRow()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SNew(STextBlock).Text(LOCTEXT("ApplyPresetLabel", "Apply Hull Preset (Warning: Overwrites current properties and Rings):"))
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("PresetKilo", "Fat Soviet (Kilo)"))
                .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleApplyPreset, FName("Kilo"))
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("PresetBarracuda", "Modern Attack (Barracuda)"))
                .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleApplyPreset, FName("Barracuda"))
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("PresetTyphoon", "Dreadnought (Typhoon)"))
                .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleApplyPreset, FName("Typhoon"))
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("PresetA26", "Compact AIP (A-26)"))
                .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleApplyPreset, FName("CompactAIP"))
            ]
        ];
}

FString FSubmarineEditorToolkit::GetAuthoringAssetName() const
{
    return ActiveAuthoringAsset.IsValid() ? ActiveAuthoringAsset->GetName() : TEXT("<No Authoring Asset>");
}

TSharedRef<SWidget> FSubmarineEditorToolkit::BuildPhaseSummary(const FText& PhaseTitle, const FText& Description, const FText& ScopeHint) const
{
    return SNew(SBorder)
        .Padding(10.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(STextBlock)
                .Text(PhaseTitle)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .Text(Description)
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
                .Text(ScopeHint)
            ]
        ];
}

TSharedPtr<IDetailsView> FSubmarineEditorToolkit::CreateDetailsViewForCategories(const TArray<FString>& Categories) const
{
    if (!ActiveAuthoringAsset.IsValid())
    {
        return nullptr;
    }

    FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs DetailsArgs;
    DetailsArgs.bAllowSearch = true;
    DetailsArgs.bHideSelectionTip = true;
    DetailsArgs.bLockable = false;
    DetailsArgs.bUpdatesFromSelection = false;
    DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

    TSharedPtr<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);
    if (!DetailsView.IsValid())
    {
        return nullptr;
    }

    const TSet<FString> CategorySet(Categories);

    DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda(
        [CategorySet](const FPropertyAndParent& PropertyAndParent)
        {
            const auto MatchesCategory = [&CategorySet](const FProperty* Property) -> bool
            {
                if (!Property)
                {
                    return false;
                }

                const FString CategoryMeta = Property->GetMetaData(TEXT("Category"));
                if (CategoryMeta.IsEmpty())
                {
                    return false;
                }

                TArray<FString> Tokens;
                CategoryMeta.ParseIntoArray(Tokens, TEXT("|"), true);
                TArray<FString> AltTokens;
                for (const FString& Token : Tokens)
                {
                    TArray<FString> CommaTokens;
                    Token.ParseIntoArray(CommaTokens, TEXT(","), true);
                    AltTokens.Append(CommaTokens);
                }

                for (const FString& Token : AltTokens)
                {
                    const FString Trimmed = Token.TrimStartAndEnd();
                    for (const FString& Wanted : CategorySet)
                    {
                        if (Trimmed.StartsWith(Wanted))
                        {
                            return true;
                        }
                    }
                }

                return false;
            };

            if (MatchesCategory(&PropertyAndParent.Property))
            {
                return true;
            }

            for (const FProperty* ParentProperty : PropertyAndParent.ParentProperties)
            {
                if (MatchesCategory(ParentProperty))
                {
                    return true;
                }
            }

            return false;
        }));

    return DetailsView;
}

TSharedRef<SWidget> FSubmarineEditorToolkit::BuildPhasePanel(
    const FText& PhaseTitle,
    const FText& Description,
    const FText& ScopeHint,
    const TSharedPtr<IDetailsView>& DetailsView) const
{
    return SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                BuildPhaseSummary(PhaseTitle, Description, ScopeHint)
            ]
            + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 6.0f, 0.0f, 0.0f)
            [
                DetailsView.IsValid()
                    ? StaticCastSharedRef<SWidget>(DetailsView.ToSharedRef())
                    : SNew(STextBlock).Text(LOCTEXT("NoPhaseDetails", "No authoring asset selected."))
            ]
        ];
}

// ── Tab spawners ──────────────────────────────────────────────────────────────

TSharedRef<SDockTab> FSubmarineEditorToolkit::SpawnDrydockTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SBorder)
                .Padding(10.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::Format(LOCTEXT("DrydockTitle", "Authoring Asset: {0}"), FText::FromString(GetAuthoringAssetName())))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                    [
                        SNew(STextBlock)
                        .AutoWrapText(true)
                        .Text(LOCTEXT(
                            "DrydockWorkflow",
                            "This toolkit is the main authoring path. Edit the asset here, then run Bake + Spawn Runtime Actor in Validate / Bake. The toolkit creates or reuses one SubmarineEditorActor for this asset and keeps the compiled assets in sync."))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                    [
                        BuildPresetRow()
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
                    [
                        SNew(SUniformGridPanel)
                        .SlotPadding(FMargin(4.0f, 0.0f))
                        + SUniformGridPanel::Slot(0, 0)
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("DrydockValidateButton", "Validate Authoring"))
                            .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleValidate)
                        ]
                        + SUniformGridPanel::Slot(1, 0)
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("DrydockBakeButton", "Bake Assets"))
                            .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleBake)
                        ]
                        + SUniformGridPanel::Slot(2, 0)
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("DrydockBakeSpawnButton", "Bake + Spawn Runtime Actor"))
                            .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleBakeAndSpawn)
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("DrydockPreviewButton", "Preview Layout"))
                            .ToolTipText(LOCTEXT("DrydockPreviewTooltip", "Spawn or refresh the hull preview actor in the viewport. Go to Ring Architecture tab to set ring count and generate rings."))
                            .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandlePreviewLayout)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
                        [
                            SNew(SButton)
                            .Text_Lambda([this]() -> FText { 
                                if (ActiveAuthoringAsset.IsValid() && ActiveAuthoringAsset->bHullGeometryConfirmed)
                                {
                                    return LOCTEXT("UnlockHullButton", "Unlock Hull (Re-edit A)");
                                }
                                return LOCTEXT("LockHullButton", "Confirm Hull (Unlock B)"); 
                            })
                            .ToolTipText(LOCTEXT("DrydockConfirmHullTooltip", "Lock the hull geometry (Layer A) to unlock Structural Bays and Decks (Layer B)."))
                            .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleConfirmGeometry)
                        ]
                    ]
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [
                        AuthoringDetailsView.IsValid()
                            ? StaticCastSharedRef<SWidget>(AuthoringDetailsView.ToSharedRef())
                            : SNew(STextBlock).Text(LOCTEXT("NoAssetDetails", "No authoring asset selected."))
                    ]
                ]
            ]
        ];
}

TSharedRef<SDockTab> FSubmarineEditorToolkit::SpawnRingTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)

                // Phase description
                + SVerticalBox::Slot().AutoHeight()
                [
                    BuildPhaseSummary(
                        LOCTEXT("RingTitle", "Pressure Hull / Ring Architecture"),
                        LOCTEXT("RingText", "Define the hull profile using a procedural preset (Myring, Series 58, Superellipse, Uniform) or manual control rings. Click Preview Layout to see the result in the level viewport. The preview refreshes automatically after each ring property change."),
                        LOCTEXT("RingScope", "Level A — hull geometry is locked after Confirm Layout. Control Rings must be strictly ordered by PositionX."))
                ]

                // Live stats bar
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    SNew(SSubmarineHullStatsBar)
                    .Asset(ActiveAuthoringAsset)
                ]

                // Generate rings row — same SUniformGridPanel pattern as Drydock
                + SVerticalBox::Slot().AutoHeight().Padding(10.0f, 8.0f, 10.0f, 2.0f)
                [
                    SNew(SUniformGridPanel)
                    .SlotPadding(FMargin(4.0f, 0.0f))
                    + SUniformGridPanel::Slot(0, 0)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                        [
                            SNew(STextBlock).Text(LOCTEXT("RingCountLabel", "Rings:"))
                        ]
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            SNew(SSpinBox<int32>)
                            .MinValue(2)
                            .MaxValue(64)
                            .Value_Lambda([this]() { return AutoRingCount; })
                            .OnValueChanged_Lambda([this](int32 NewVal) { AutoRingCount = NewVal; })
                        ]
                    ]
                    + SUniformGridPanel::Slot(1, 0)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("GenerateRingsButton", "Generate Rings"))
                        .ToolTipText(LOCTEXT("GenerateRingsTooltip", "Replace ControlRings with N evenly-spaced rings using hull defaults. Auto-previews."))
                        .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleGenerateRings)
                    ]
                ]

                // Ring details view
                + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    RingDetailsView.IsValid()
                        ? StaticCastSharedRef<SWidget>(RingDetailsView.ToSharedRef())
                        : SNew(STextBlock).Text(LOCTEXT("NoRingDetails", "No authoring asset selected."))
                ]
            ]
        ];
}

TSharedRef<SDockTab> FSubmarineEditorToolkit::SpawnEnvelopeTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        [
            BuildPhasePanel(
                LOCTEXT("EnvelopeTitle", "Outer Envelope / Casing"),
                LOCTEXT("EnvelopeText", "Configure the outer envelope (kiosque fairing and hull superstructure) and the deck casing. OuterEnvelopeRings define the secondary ring chain that drives the envelope silhouette."),
                LOCTEXT("EnvelopeScope", "Level A — envelope geometry is part of the locked hull. Casing thickness and anechoic coating are visual and do not affect bake geometry at this stage."),
                EnvelopeDetailsView)
        ];
}

TSharedRef<SDockTab> FSubmarineEditorToolkit::SpawnAppendagesTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        [
            SNew(SSubmarineAppendagesPanel)
            .Asset(ActiveAuthoringAsset)
            .DetailsView(AppendagesDetailsView)
        ];
}

TSharedRef<SDockTab> FSubmarineEditorToolkit::SpawnBaysTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    BuildPhaseSummary(
                        LOCTEXT("BaysTitle", "Structural Bays"),
                        LOCTEXT("BaysText", "Structural Bays are derived from explicit entries or from Frame-Rings marked as bay boundaries. Use Draw Structural Bays after a successful bake."),
                        LOCTEXT("BaysScope", "Level B — stable only after hull geometry (Level A) is confirmed."))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(10.0f, 6.0f, 10.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("DrawBaysButton", "Draw Structural Bays"))
                    .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleDebugDrawBays)
                ]
                + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SBox)
                    .IsEnabled_Lambda([this]() { return ActiveAuthoringAsset.IsValid() && ActiveAuthoringAsset->bHullGeometryConfirmed; })
                    [
                        BaysDetailsView.IsValid()
                            ? StaticCastSharedRef<SWidget>(BaysDetailsView.ToSharedRef())
                            : SNew(STextBlock).Text(LOCTEXT("NoBaysDetails", "No authoring asset selected."))
                    ]
                ]
            ]
        ];
}

TSharedRef<SDockTab> FSubmarineEditorToolkit::SpawnFloorsTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        [
            SNew(SBox)
            .IsEnabled_Lambda([this]() { return ActiveAuthoringAsset.IsValid() && ActiveAuthoringAsset->bHullGeometryConfirmed; })
            [
                BuildPhasePanel(
                    LOCTEXT("FloorsTitle", "Deck Levels / Floor Regions"),
                    LOCTEXT("FloorsText", "Edit Deck Levels and Floor Regions once Structural Bays are stable."),
                    LOCTEXT("FloorsScope", "Level B — do not tune walkable layout before hull, Frame-Rings, and Structural Bays are locked."),
                    FloorsDetailsView)
            ]
        ];
}

TSharedRef<SDockTab> FSubmarineEditorToolkit::SpawnOpeningsTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        [
            SNew(SBox)
            .IsEnabled_Lambda([this]() { return ActiveAuthoringAsset.IsValid() && ActiveAuthoringAsset->bHullGeometryConfirmed; })
            [
                BuildPhasePanel(
                    LOCTEXT("OpeningsTitle", "Openings / Connectors / Closures"),
                    LOCTEXT("OpeningsText", "Openings, Connectors, and Closures should be authored after Deck Levels and Floor Regions have stabilized."),
                    LOCTEXT("OpeningsScope", "Levels B/C — this phase defines traversal and sealing structure, not decorative meshes."),
                    OpeningsDetailsView)
            ]
        ];
}

TSharedRef<SDockTab> FSubmarineEditorToolkit::SpawnPartitionsTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        [
            SNew(SBox)
            .IsEnabled_Lambda([this]() { return ActiveAuthoringAsset.IsValid() && ActiveAuthoringAsset->bHullGeometryConfirmed; })
            [
                BuildPhasePanel(
                    LOCTEXT("PartitionsTitle", "Pressure Bulkheads / Internal Walls"),
                    LOCTEXT("PartitionsText", "Pressure Bulkheads, Internal Walls, and Derived Flood Volumes depend on the earlier phases being valid and baked."),
                    LOCTEXT("PartitionsScope", "Level C — use this phase to validate compartment topology and runtime partition data, not final art."),
                    PartitionsDetailsView)
            ]
        ];
}

TSharedRef<SDockTab> FSubmarineEditorToolkit::SpawnValidateBakeTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        [
            SNew(SBorder)
            .Padding(10.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                [
                    SNew(STextBlock).Text(LOCTEXT("ValidateBakeText", "Validation and bake actions"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                [
                    SNew(STextBlock)
                    .AutoWrapText(true)
                    .Text(this, &FSubmarineEditorToolkit::GetContractStatusText)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                [
                    SNew(STextBlock)
                    .AutoWrapText(true)
                    .ColorAndOpacity(FLinearColor(0.6f, 0.85f, 0.6f, 1.0f))
                    .Text(this, &FSubmarineEditorToolkit::GetBakeStatsText)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ValidateButton", "Validate"))
                    .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleValidate)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("BakeButton", "Bake Assets"))
                    .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleBake)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("BakeSpawnButton", "Bake + Spawn Runtime Actor"))
                    .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleBakeAndSpawn)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("PIEButton", "Run PIE Navigation Smoke"))
                    .OnClicked_Raw(this, &FSubmarineEditorToolkit::HandleTestPIE)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .AutoWrapText(true)
                    .Text(this, &FSubmarineEditorToolkit::GetOperationStatusText)
                ]
            ]
        ];
}

// ── Button handlers ───────────────────────────────────────────────────────────

FReply FSubmarineEditorToolkit::HandleValidate()
{
    if (!GEditor || !ActiveAuthoringAsset.IsValid())
    {
        return FReply::Handled();
    }

    USubmarineBakeSubsystem* BakeSubsystem = GEditor->GetEditorSubsystem<USubmarineBakeSubsystem>();
    if (!BakeSubsystem)
    {
        SetOperationStatus(TEXT("Validate failed: Bake subsystem unavailable."), {});
        return FReply::Handled();
    }

    TArray<FString> Errors;
    const bool bValid = BakeSubsystem->ValidateAuthoringAsset(ActiveAuthoringAsset.Get(), Errors);
    UE_LOG(LogTemp, Display, TEXT("SubmarineEditorToolkit Validate: %s"), bValid ? TEXT("Valid") : TEXT("Invalid"));
    for (const FString& Error : Errors)
    {
        UE_LOG(LogTemp, Error, TEXT("Validation Error: %s"), *Error);
    }

    SetOperationStatus(bValid ? TEXT("Validate: success.") : TEXT("Validate: failed."), Errors);
    return FReply::Handled();
}

FReply FSubmarineEditorToolkit::HandleBake()
{
    ASubmarineEditorActor* EditorActor = FindOrCreateEditorActor();
    if (!EditorActor)
    {
        SetOperationStatus(TEXT("Bake failed: unable to resolve SubmarineEditorActor."), {});
        return FReply::Handled();
    }

    const bool bSucceeded = EditorActor->FullBakeAssets();
    SetOperationStatus(
        bSucceeded ? TEXT("Bake Assets: success.") : TEXT("Bake Assets: failed."),
        EditorActor->LastMessages);
    return FReply::Handled();
}

FReply FSubmarineEditorToolkit::HandleBakeAndSpawn()
{
    ASubmarineEditorActor* EditorActor = FindOrCreateEditorActor();
    if (!EditorActor)
    {
        SetOperationStatus(TEXT("Bake + Spawn failed: unable to resolve SubmarineEditorActor."), {});
        return FReply::Handled();
    }

    const bool bSucceeded = EditorActor->FullBakeAndSpawnRuntimeActor();
    SetOperationStatus(
        bSucceeded ? TEXT("Bake + Spawn Runtime Actor: success.") : TEXT("Bake + Spawn Runtime Actor: failed."),
        EditorActor->LastMessages);
    return FReply::Handled();
}

FReply FSubmarineEditorToolkit::HandlePreviewLayout()
{
    if (!GEditor)
    {
        UE_LOG(LogTemp, Error, TEXT("SubmarineEditorToolkit::HandlePreviewLayout — GEditor is null"));
        SetOperationStatus(TEXT("Preview Layout: GEditor unavailable."), {});
        return FReply::Handled();
    }
    if (!ActiveAuthoringAsset.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("SubmarineEditorToolkit::HandlePreviewLayout — no authoring asset"));
        SetOperationStatus(TEXT("Preview Layout: no authoring asset."), {});
        return FReply::Handled();
    }

    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    UE_LOG(LogTemp, Display, TEXT("SubmarineEditorToolkit::HandlePreviewLayout — world: %s"),
        EditorWorld ? *EditorWorld->GetName() : TEXT("null"));

    ASubmarinePreviewActor* PreviewActor = FindOrCreatePreviewActor();
    if (!PreviewActor)
    {
        UE_LOG(LogTemp, Error, TEXT("SubmarineEditorToolkit::HandlePreviewLayout — SpawnActor returned null"));
        SetOperationStatus(TEXT("Preview Layout: SpawnActor<ASubmarinePreviewActor> returned null. Check Output Log."), {});
        return FReply::Handled();
    }

    UE_LOG(LogTemp, Display, TEXT("SubmarineEditorToolkit::HandlePreviewLayout — preview actor OK: %s"), *PreviewActor->GetName());
    ActivePreviewActor = PreviewActor;
    PreviewActor->RefreshPreview();
    SetOperationStatus(FString::Printf(TEXT("Preview Layout: OK — %s"), *PreviewActor->GetName()), {});
    return FReply::Handled();
}

FReply FSubmarineEditorToolkit::HandleGenerateRings()
{
    if (!ActiveAuthoringAsset.IsValid())
    {
        return FReply::Handled();
    }

    USub3DSubmarineAuthoringAsset* Asset = ActiveAuthoringAsset.Get();
    const FSubmarineHullDef& Hull = Asset->Hull;
    const int32 N = FMath::Clamp(AutoRingCount, 2, 64);

    GEditor->BeginTransaction(LOCTEXT("GenerateRings_Transaction", "Generate Control Rings"));
    Asset->Modify();

    Asset->ControlRings.Reset();
    Asset->ControlRings.Reserve(N);

    for (int32 i = 0; i < N; ++i)
    {
        FControlRingDef Ring;
        Ring.ControlRingId    = FName(FString::Printf(TEXT("Ring_%02d"), i));
        Ring.PositionX        = (N > 1) ? (i / static_cast<float>(N - 1)) * Hull.LengthCm : 0.0f;
        Ring.HalfWidthCm      = Hull.DefaultHalfWidthCm;
        Ring.HalfHeightCm     = Hull.DefaultHalfHeightCm;
        Ring.WallThicknessCm  = Hull.DefaultWallThicknessCm;
        Ring.SectionProfile   = Hull.DefaultSectionProfile;
        Ring.SectionRoundness = Hull.DefaultSectionRoundness;
        Asset->ControlRings.Add(Ring);
    }

    GEditor->EndTransaction();

    if (RingDetailsView.IsValid())
    {
        RingDetailsView->ForceRefresh();
    }

    if (ActivePreviewActor.IsValid())
    {
        ActivePreviewActor->RefreshPreview();
    }
    else
    {
        HandlePreviewLayout();
    }

    SetOperationStatus(FString::Printf(TEXT("Generated %d rings."), N), {});
    return FReply::Handled();
}

FReply FSubmarineEditorToolkit::HandleDebugDrawBays()
{
    if (!GEditor || !ActiveAuthoringAsset.IsValid())
    {
        return FReply::Handled();
    }

    USubmarineBakeSubsystem* BakeSubsystem = GEditor->GetEditorSubsystem<USubmarineBakeSubsystem>();
    if (!BakeSubsystem)
    {
        SetOperationStatus(TEXT("Draw Structural Bays failed: Bake subsystem unavailable."), {});
        return FReply::Handled();
    }

    if (!BakeSubsystem->LastBakedBaseAsset)
    {
        BakeSubsystem->BakeBase(ActiveAuthoringAsset.Get());
    }

    if (!BakeSubsystem->LastBakedBaseAsset)
    {
        SetOperationStatus(TEXT("Draw Structural Bays failed: no baked base asset."), {});
        return FReply::Handled();
    }

    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    Sub3DWave3::FSubmarineBayDebugDraw::DrawStructuralBays(
        EditorWorld,
        ActiveAuthoringAsset->Hull,
        BakeSubsystem->LastBakedBaseAsset->StructuralBays,
        false,
        5.0f);

    SetOperationStatus(TEXT("Draw Structural Bays: success."), {});
    return FReply::Handled();
}

FReply FSubmarineEditorToolkit::HandleTestPIE()
{
    if (!GEditor)
    {
        SetOperationStatus(TEXT("Run PIE Navigation Smoke failed: editor unavailable."), {});
        return FReply::Handled();
    }

    Sub3DWave6::FSubmarineNavigationTestService::RunPIENavigationSmoke(GEditor->GetEditorWorldContext().World());
    SetOperationStatus(TEXT("Run PIE Navigation Smoke: requested."), {});
    return FReply::Handled();
}

FReply FSubmarineEditorToolkit::HandleConfirmGeometry()
{
    if (ActiveAuthoringAsset.IsValid())
    {
        ActiveAuthoringAsset->Modify();
        ActiveAuthoringAsset->bHullGeometryConfirmed = !ActiveAuthoringAsset->bHullGeometryConfirmed;
        SetOperationStatus(ActiveAuthoringAsset->bHullGeometryConfirmed ? TEXT("Hull Geometry Confirmed (B/C Unlocked)") : TEXT("Hull Geometry Unlocked (Stage A Re-opened)"), {});
    }
    return FReply::Handled();
}

FReply FSubmarineEditorToolkit::HandleApplyPreset(FName PresetName)
{
    if (!ActiveAuthoringAsset.IsValid()) return FReply::Handled();

    USub3DSubmarineAuthoringAsset* Asset = ActiveAuthoringAsset.Get();
    Asset->Modify();

    // Layer A - Hull & Profile properties
    if (PresetName == "Kilo") {
        Asset->Hull.LengthCm = 7200.0f;
        Asset->Hull.ProfileParams.Profile = ESub3DHullLongitudinalProfile::Series58;
        Asset->Hull.ProfileParams.Series58Fineness = 6.0f;
    } else if (PresetName == "Barracuda") {
        Asset->Hull.LengthCm = 9900.0f;
        Asset->Hull.ProfileParams.Profile = ESub3DHullLongitudinalProfile::Myring;
    } else if (PresetName == "Typhoon") {
        Asset->Hull.LengthCm = 17500.0f;
        Asset->Hull.ProfileParams.Profile = ESub3DHullLongitudinalProfile::SuperellipseLongitudinal;
        Asset->Hull.ProfileParams.ParallelMidbodyFraction = 0.4f;
    } else if (PresetName == "CompactAIP") {
        Asset->Hull.LengthCm = 6200.0f;
        Asset->Hull.ProfileParams.Profile = ESub3DHullLongitudinalProfile::Myring;
    }

    HandleGenerateRings();
    return FReply::Handled();
}

// ── Actor finders ─────────────────────────────────────────────────────────────

ASubmarineEditorActor* FSubmarineEditorToolkit::FindOrCreateEditorActor() const
{
    if (!GEditor || !ActiveAuthoringAsset.IsValid())
    {
        return nullptr;
    }

    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!EditorWorld)
    {
        return nullptr;
    }

    for (TActorIterator<ASubmarineEditorActor> It(EditorWorld); It; ++It)
    {
        ASubmarineEditorActor* Existing = *It;
        if (Existing && Existing->AuthoringAsset == ActiveAuthoringAsset.Get())
        {
            return Existing;
        }
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ASubmarineEditorActor* NewActor = EditorWorld->SpawnActor<ASubmarineEditorActor>(
        ASubmarineEditorActor::StaticClass(),
        FTransform::Identity,
        SpawnParameters);

    if (!NewActor)
    {
        return nullptr;
    }

    NewActor->AuthoringAsset = ActiveAuthoringAsset.Get();

#if WITH_EDITOR
    NewActor->SetActorLabel(FString::Printf(TEXT("%s_EditorActor"), *ActiveAuthoringAsset->GetName()));
    GEditor->SelectNone(false, true, false);
    GEditor->SelectActor(NewActor, true, true, true);
#endif

    return NewActor;
}

ASubmarinePreviewActor* FSubmarineEditorToolkit::FindOrCreatePreviewActor() const
{
    if (!GEditor || !ActiveAuthoringAsset.IsValid())
    {
        return nullptr;
    }

    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!EditorWorld)
    {
        return nullptr;
    }

    // Reuse any existing preview actor in this world (there should be at most one).
    for (TActorIterator<ASubmarinePreviewActor> It(EditorWorld); It; ++It)
    {
        ASubmarinePreviewActor* Existing = *It;
        if (Existing)
        {
            // Re-bind in case the asset changed.
            Existing->InitializeForAsset(ActiveAuthoringAsset.Get());
            return Existing;
        }
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ASubmarinePreviewActor* NewActor = EditorWorld->SpawnActor<ASubmarinePreviewActor>(
        ASubmarinePreviewActor::StaticClass(),
        FTransform::Identity,
        SpawnParameters);

    if (!NewActor)
    {
        return nullptr;
    }

    NewActor->InitializeForAsset(ActiveAuthoringAsset.Get());

#if WITH_EDITOR
    NewActor->SetActorLabel(FString::Printf(TEXT("%s_Preview"), *ActiveAuthoringAsset->GetName()));
#endif

    return NewActor;
}

void FSubmarineEditorToolkit::OnPreviewPropertiesChanged(const FPropertyChangedEvent& /*Event*/)
{
    if (ActivePreviewActor.IsValid())
    {
        ActivePreviewActor->RefreshPreview();
    }
}

// ── Status / stats ────────────────────────────────────────────────────────────

void FSubmarineEditorToolkit::SetOperationStatus(const FString& InStatus, const TArray<FString>& InMessages)
{
    LastOperationStatus   = InStatus;
    LastOperationMessages = InMessages;
}

FText FSubmarineEditorToolkit::GetOperationStatusText() const
{
    FTextBuilder Builder;
    Builder.AppendLine(FText::FromString(LastOperationStatus));

    if (!LastOperationMessages.IsEmpty())
    {
        const int32 MaxLines = FMath::Min(LastOperationMessages.Num(), 8);
        for (int32 Index = 0; Index < MaxLines; ++Index)
        {
            Builder.AppendLine(FText::FromString(FString::Printf(TEXT("- %s"), *LastOperationMessages[Index])));
        }
    }

    return Builder.ToText();
}

FText FSubmarineEditorToolkit::GetBakeStatsText() const
{
    if (!GEditor)
    {
        return LOCTEXT("BakeStatsNoEditor", "Last Bake: editor unavailable.");
    }

    USubmarineBakeSubsystem* BakeSubsystem = GEditor->GetEditorSubsystem<USubmarineBakeSubsystem>();
    if (!BakeSubsystem || !BakeSubsystem->LastBakedBaseAsset)
    {
        return LOCTEXT("BakeStatsNone", "Last Bake: no baked data yet. Run Bake Assets first.");
    }

    const UCompiledSubmarineBaseAsset* Asset = BakeSubsystem->LastBakedBaseAsset;
    const int32 ExtTris  = Asset->ExteriorHull.Indices.Num() / 3;
    const int32 IntTris  = Asset->InteriorHull.Indices.Num() / 3;
    const int32 CollTris = Asset->CollisionProxy.Indices.Num() / 3;
    const int32 EnvTris  = Asset->OuterEnvelope.Indices.Num() / 3;

    const FString Stats = FString::Printf(
        TEXT("Last Bake Output:\n"
             "  Hull length: %.0f cm\n"
             "  Exterior: %d tris   Interior: %d tris   Collision: %d tris\n"
             "  Outer Envelope: %d tris\n"
             "  Frame-Rings: %d   Structural Bays: %d\n"
             "  Deck Levels: %d   Floor Regions: %d\n"
             "  Openings: %d   Connectors: %d   Closures: %d\n"
             "  Partitions: %d   Derived Flood Volumes: %d"),
        Asset->HullLengthCm,
        ExtTris, IntTris, CollTris,
        EnvTris,
        Asset->FrameRings.Num(), Asset->StructuralBays.Num(),
        Asset->CompiledDecks.Num(), Asset->CompiledFloorRegions.Num(),
        Asset->CompiledOpenings.Num(), Asset->CompiledConnectors.Num(), Asset->CompiledClosures.Num(),
        Asset->CompiledPartitions.Num(), Asset->FloodGraph.Volumes.Num());

    return FText::FromString(Stats);
}

void FSubmarineEditorToolkit::BuildContractWarnings(TArray<FString>& OutWarnings) const
{
    OutWarnings.Reset();

    if (!ActiveAuthoringAsset.IsValid())
    {
        OutWarnings.Add(TEXT("No authoring asset selected."));
        return;
    }

    const USub3DSubmarineAuthoringAsset* Asset = ActiveAuthoringAsset.Get();
    const float HullLength = Asset->Hull.LengthCm;

    if (Asset->ControlRings.Num() < 2 &&
        Asset->Hull.ProfileParams.Profile == ESub3DHullLongitudinalProfile::Manual)
    {
        OutWarnings.Add(TEXT("Manual profile requires at least two Control Rings."));
    }

    float PreviousX = -FLT_MAX;
    for (int32 Index = 0; Index < Asset->ControlRings.Num(); ++Index)
    {
        const FControlRingDef& Ring = Asset->ControlRings[Index];
        if (Ring.PositionX < 0.0f || Ring.PositionX > HullLength)
        {
            OutWarnings.Add(FString::Printf(TEXT("Control Ring %d PositionX must be inside [0..Hull.LengthCm]."), Index));
        }

        if (Index > 0 && Ring.PositionX <= PreviousX)
        {
            OutWarnings.Add(TEXT("Control Rings must be strictly increasing by PositionX."));
            break;
        }

        PreviousX = Ring.PositionX;
    }

    float PreviousAlpha = -1.0f;
    for (int32 Index = 0; Index < Asset->FrameRings.Num(); ++Index)
    {
        const FFrameRingDef& FrameRing = Asset->FrameRings[Index];
        if (FrameRing.SpineAlpha < 0.0f || FrameRing.SpineAlpha > 1.0f)
        {
            OutWarnings.Add(FString::Printf(TEXT("Frame-Ring %d SpineAlpha must be inside [0..1]."), Index));
        }

        if (Index > 0 && FrameRing.SpineAlpha <= PreviousAlpha)
        {
            OutWarnings.Add(TEXT("Frame-Rings must be strictly increasing by SpineAlpha."));
            break;
        }

        PreviousAlpha = FrameRing.SpineAlpha;
    }

    if (Asset->OuterEnvelope.bEnabled)
    {
        if (Asset->OuterEnvelope.LengthCm < 100.0f ||
            Asset->OuterEnvelope.WidthCm < 50.0f   ||
            Asset->OuterEnvelope.HeightCm < 50.0f)
        {
            OutWarnings.Add(TEXT("Outer Envelope dimensions are below minimum bake constraints."));
        }
    }

    // Layer A confirmation / stale checks
    if (!Asset->bHullGeometryConfirmed && Asset->StructuralBays.Num() > 0)
    {
        OutWarnings.Add(TEXT("Layer B data (Structural Bays) is present but bHullGeometryConfirmed is false. Confirm hull geometry before editing layout."));
    }

    if (Asset->bHullGeometryConfirmed && Asset->HullGeometryHash == 0)
    {
        OutWarnings.Add(TEXT("Hull geometry is flagged as confirmed but HullGeometryHash is 0. Run Bake Assets to stamp the hash."));
    }

    if (Asset->Sail.bEnabled && Asset->Sail.HeightCm <= 0.0f)
    {
        OutWarnings.Add(TEXT("Sail is enabled but HeightCm is zero."));
    }

    if (Asset->BowSection.bEnabled && Asset->BowSection.SonarDomeDiamCm > Asset->Hull.DefaultHalfWidthCm * 2.0f)
    {
        OutWarnings.Add(TEXT("Bow sonar dome diameter exceeds hull beam — verify intent."));
    }

    if (Asset->SternSection.bEnabled && Asset->SternSection.PropulsorDiamCm > Asset->Hull.DefaultHalfWidthCm * 2.0f)
    {
        OutWarnings.Add(TEXT("Stern propulsor diameter exceeds hull beam — verify intent."));
    }
}

FText FSubmarineEditorToolkit::GetContractStatusText() const
{
    TArray<FString> Warnings;
    BuildContractWarnings(Warnings);

    FTextBuilder Builder;
    if (Warnings.IsEmpty())
    {
        Builder.AppendLine(LOCTEXT("ContractStatusOk", "Contract Status: quick checks passed for Pressure Hull, Control Rings, Frame-Rings, Outer Envelope, and Appendages."));
        return Builder.ToText();
    }

    Builder.AppendLine(LOCTEXT("ContractStatusWarn", "Contract Status: quick checks found issues to fix before stable bake."));
    const int32 MaxLines = FMath::Min(Warnings.Num(), 10);
    for (int32 Index = 0; Index < MaxLines; ++Index)
    {
        Builder.AppendLine(FText::FromString(FString::Printf(TEXT("- %s"), *Warnings[Index])));
    }

    return Builder.ToText();
}

#undef LOCTEXT_NAMESPACE
