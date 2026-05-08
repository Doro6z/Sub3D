#include "SSub3DWaterDebugPanelWidget.h"

#include "CompartmentVolumeComponent.h"
#include "Debug/Sub3DDebugSettings.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FloodWaterPlaneComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Layout/Margin.h"
#include "SubFloodComponent.h"
#include "SubCrewCharacter.h"
#include "SubmarineBase.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Sub3DWaterDebugPanel"

namespace Sub3DWaterDebugPanel
{
	static const FMargin RowPadding(8.f, 4.f);
	static const FMargin SectionPadding(8.f, 8.f);
}

void SSub3DWaterDebugPanelWidget::Construct(const FArguments& InArgs)
{
	using namespace Sub3DWaterDebugPanel;

	// Hook PIE start/end so the compartment list auto-refreshes when the user enters
	// or exits Play mode. Without this the panel sits stale showing the previous
	// session's data (or empty when no PIE has been opened yet).
	PIEStartHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &SSub3DWaterDebugPanelWidget::HandlePIEStarted);
	PIEEndHandle = FEditorDelegates::EndPIE.AddRaw(this, &SSub3DWaterDebugPanelWidget::HandlePIEEnded);

	CompartmentsList = SNew(SVerticalBox);

	ChildSlot
	[
		SNew(SScrollBox)

		// ── Header ───────────────────────────────────────────────────────────
		+ SScrollBox::Slot()
		.Padding(SectionPadding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "Sub3D — Water Debug"))
				.Font(FAppStyle::Get().GetFontStyle("HeadingLarge"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(StatusTextBlock, STextBlock)
				.Text_Lambda([this]() { return GetStatusText(); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("Refresh", "Refresh"))
					.ToolTipText(LOCTEXT("RefreshTip", "Re-scan PIE world for ASubmarineBase actors and rebuild the compartment list."))
					.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnRefreshClicked)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SAssignNew(LastActionTextBlock, STextBlock)
					.Text(LOCTEXT("NoAction", "—"))
					.ColorAndOpacity(FLinearColor(0.6f, 0.85f, 1.f))
				]
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator).Orientation(Orient_Horizontal)
		]

		// ── Common inputs (Force, Radius, Inflow) ────────────────────────────
		+ SScrollBox::Slot()
		.Padding(SectionPadding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("InputsHeader", "Inputs"))
				.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(RowPadding)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Force", "Inject Force"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).MaxWidth(140.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return InjectForce; })
					.OnValueChanged(this, &SSub3DWaterDebugPanelWidget::OnInjectForceChanged)
					.MinValue(0.f).MaxValue(500.f)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 8, 0).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Radius", "Radius (cm)"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).MaxWidth(140.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return InjectRadiusCm; })
					.OnValueChanged(this, &SSub3DWaterDebugPanelWidget::OnInjectRadiusChanged)
					.MinValue(1.f).MaxValue(2000.f)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 8, 0).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Inflow", "Breach Inflow (L/s)"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).MaxWidth(140.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return BreachInflowLps; })
					.OnValueChanged(this, &SSub3DWaterDebugPanelWidget::OnBreachInflowChanged)
					.MinValue(0.f).MaxValue(2000.f)
				]
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator).Orientation(Orient_Horizontal)
		]

		// ── Crew shortcuts ───────────────────────────────────────────────────
		+ SScrollBox::Slot()
		.Padding(SectionPadding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CrewShortcuts", "Crew-anchored shortcuts"))
				.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(RowPadding)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("InjectAtCrew", "Inject Splash @ Crew"))
					.ToolTipText(LOCTEXT("InjectAtCrewTip", "Inject a wave at the controlled crew's current world position. Auto-resolves the compartment."))
					.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnInjectAtCrew)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("BreachAtCrew", "Trigger Breach @ Crew"))
					.ToolTipText(LOCTEXT("BreachAtCrewTip", "Create a breach at the controlled crew's current world position (uses Breach Inflow input)."))
					.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnBreachAtCrew)
				]
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator).Orientation(Orient_Horizontal)
		]

		// ── Compartments ─────────────────────────────────────────────────────
		+ SScrollBox::Slot()
		.Padding(SectionPadding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CompartmentsHeader", "Compartments"))
				.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				CompartmentsList.ToSharedRef()
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator).Orientation(Orient_Horizontal)
		]

		// ── Global tunables ──────────────────────────────────────────────────
		+ SScrollBox::Slot()
		.Padding(SectionPadding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GlobalTunables", "Global heightfield tunables (apply-to-all)"))
				.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(RowPadding)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0).VAlign(VAlign_Center).MinWidth(160.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::Format(LOCTEXT("WaveSpeedFmt", "Wave Speed: {0}  (0..1500)"),
						FText::AsNumber(GlobalWaveSpeed)); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SSlider)
					.Value_Lambda([this]() { return GlobalWaveSpeed / 1500.f; })
					.OnValueChanged_Lambda([this](float V) { OnGlobalWaveSpeedChanged(V * 1500.f); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).MaxWidth(120.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return GlobalWaveSpeed; })
					.OnValueChanged(this, &SSub3DWaterDebugPanelWidget::OnGlobalWaveSpeedChanged)
					.MinValue(0.f).MaxValue(5000.f)
					.ToolTipText(LOCTEXT("WaveSpeedNumTip", "Direct numeric entry — beyond slider range if you need (0..5000)"))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(RowPadding)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0).VAlign(VAlign_Center).MinWidth(160.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::Format(LOCTEXT("DampingFmt", "Damping: {0}  (0.9..1.0)"),
						FText::AsNumber(GlobalDamping)); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SSlider)
					// 0.9..1.0 mapped to slider 0..1
					.Value_Lambda([this]() { return (GlobalDamping - 0.9f) / 0.1f; })
					.OnValueChanged_Lambda([this](float V) { OnGlobalDampingChanged(0.9f + V * 0.1f); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).MaxWidth(120.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return GlobalDamping; })
					.OnValueChanged(this, &SSub3DWaterDebugPanelWidget::OnGlobalDampingChanged)
					.MinValue(0.9f).MaxValue(1.f)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(RowPadding)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0).VAlign(VAlign_Center).MinWidth(160.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::Format(LOCTEXT("AmplitudeFmt", "Amplitude (cm): {0}  (0..50)"),
						FText::AsNumber(GlobalHeightfieldAmplitudeCm)); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SSlider)
					.Value_Lambda([this]() { return GlobalHeightfieldAmplitudeCm / 50.f; })
					.OnValueChanged_Lambda([this](float V) { OnGlobalAmplitudeChanged(V * 50.f); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).MaxWidth(120.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return GlobalHeightfieldAmplitudeCm; })
					.OnValueChanged(this, &SSub3DWaterDebugPanelWidget::OnGlobalAmplitudeChanged)
					.MinValue(0.f).MaxValue(500.f)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(RowPadding)
			[
				SNew(SButton)
				.Text(LOCTEXT("ApplyTunables", "Apply tunables to all compartments"))
				.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnApplyTunablesClicked)
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator).Orientation(Orient_Horizontal)
		]

		// ── Visualization toggles ────────────────────────────────────────────
		+ SScrollBox::Slot()
		.Padding(SectionPadding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("VizHeader", "Visualization"))
				.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(RowPadding)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([]()
				{
					const USub3DDebugSettings* S = GetDefault<USub3DDebugSettings>();
					return (S && S->bDrawBreachMarkers) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					if (USub3DDebugSettings* S = GetMutableDefault<USub3DDebugSettings>())
					{
						S->bDrawBreachMarkers = (NewState == ECheckBoxState::Checked);
						S->SaveConfig();
					}
				})
				.Content()
				[
					SNew(STextBlock).Text(LOCTEXT("DrawBreachMarkers", "Draw breach markers (red box at world breach point)"))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(RowPadding)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([]()
				{
					const USub3DDebugSettings* S = GetDefault<USub3DDebugSettings>();
					return (S && S->bDrawCompartmentWater) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					if (USub3DDebugSettings* S = GetMutableDefault<USub3DDebugSettings>())
					{
						S->bDrawCompartmentWater = (NewState == ECheckBoxState::Checked);
						S->SaveConfig();
					}
				})
				.Content()
				[
					SNew(STextBlock).Text(LOCTEXT("DrawCompartmentWater", "Draw compartment water surface wireframe"))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(RowPadding)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([]()
				{
					const USub3DDebugSettings* S = GetDefault<USub3DDebugSettings>();
					return (S && S->bDrawCompartmentVolumes) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					if (USub3DDebugSettings* S = GetMutableDefault<USub3DDebugSettings>())
					{
						S->bDrawCompartmentVolumes = (NewState == ECheckBoxState::Checked);
						S->SaveConfig();
					}
				})
				.Content()
				[
					SNew(STextBlock).Text(LOCTEXT("DrawVolumes", "Draw compartment volumes (CompartmentId labels + bounds)"))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(RowPadding)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([]()
				{
					const USub3DDebugSettings* S = GetDefault<USub3DDebugSettings>();
					return (S && S->bDrawWaterInjectMarkers) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					if (USub3DDebugSettings* S = GetMutableDefault<USub3DDebugSettings>())
					{
						S->bDrawWaterInjectMarkers = (NewState == ECheckBoxState::Checked);
						S->SaveConfig();
					}
				})
				.Content()
				[
					SNew(STextBlock).Text(LOCTEXT("DrawInjectMarkers", "Draw water inject markers (cyan sphere + radius circle)"))
				]
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator).Orientation(Orient_Horizontal)
		]

		// ── Log section ──────────────────────────────────────────────────────
		+ SScrollBox::Slot()
		.Padding(SectionPadding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LogHeader", "Log (last actions)"))
					.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("ClearLog", "Clear"))
					.OnClicked_Lambda([this]()
					{
						ActionHistory.Reset();
						if (LogTextBox.IsValid()) LogTextBox->SetText(FText::GetEmpty());
						return FReply::Handled();
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(180.f)
				[
					SAssignNew(LogTextBox, SMultiLineEditableTextBox)
					.IsReadOnly(true)
					.AlwaysShowScrollbars(true)
					.AllowMultiLine(true)
					.Text(this, &SSub3DWaterDebugPanelWidget::GetLogText)
				]
			]
		]
	];

	// First population if a PIE world is already running.
	RebuildSubmarineList();
	RebuildCompartmentList();
}

SSub3DWaterDebugPanelWidget::~SSub3DWaterDebugPanelWidget()
{
	if (PIEStartHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(PIEStartHandle);
	}
	if (PIEEndHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(PIEEndHandle);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Resolution / refresh
// ─────────────────────────────────────────────────────────────────────────────

UWorld* SSub3DWaterDebugPanelWidget::ResolvePieWorld() const
{
	if (!GEditor)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEditor->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			return Context.World();
		}
	}
	return nullptr;
}

void SSub3DWaterDebugPanelWidget::RebuildSubmarineList()
{
	AvailableSubs.Reset();
	UWorld* World = ResolvePieWorld();
	if (!World)
	{
		SelectedSubmarine.Reset();
		return;
	}

	for (TActorIterator<ASubmarineBase> It(World); It; ++It)
	{
		AvailableSubs.Add(*It);
	}

	// Auto-select the first sub if nothing was selected or selection went stale.
	if (!SelectedSubmarine.IsValid())
	{
		SelectedSubmarine = AvailableSubs.Num() > 0 ? AvailableSubs[0] : nullptr;
	}
}

void SSub3DWaterDebugPanelWidget::RebuildCompartmentList()
{
	if (!CompartmentsList.IsValid())
	{
		return;
	}
	CompartmentsList->ClearChildren();

	const ASubmarineBase* Sub = SelectedSubmarine.Get();
	if (!Sub)
	{
		CompartmentsList->AddSlot().AutoHeight().Padding(8, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoSub", "Start PIE then click Refresh — no ASubmarineBase visible yet."))
			.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.4f))
		];
		return;
	}

	TArray<UFloodWaterPlaneComponent*> Planes;
	Sub->GetComponents<UFloodWaterPlaneComponent>(Planes);

	if (Planes.Num() == 0)
	{
		CompartmentsList->AddSlot().AutoHeight().Padding(8, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoPlanes", "No UFloodWaterPlaneComponent on this submarine. Compartment volumes resolved?"))
			.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.4f))
		];
		return;
	}

	for (UFloodWaterPlaneComponent* Plane : Planes)
	{
		if (!Plane) continue;
		CompartmentsList->AddSlot().AutoHeight().Padding(0, 2)
		[
			MakeCompartmentRow(Plane)
		];
	}
}

TSharedRef<SWidget> SSub3DWaterDebugPanelWidget::MakeCompartmentRow(UFloodWaterPlaneComponent* Plane)
{
	const UCompartmentVolumeComponent* Vol = Plane ? Plane->SourceVolume.Get() : nullptr;
	const FName CompId = Vol ? Vol->CompartmentId : NAME_None;
	const FString Title = CompId.IsNone() ? FString(TEXT("(no CompartmentId)")) : CompId.ToString();

	TWeakObjectPtr<UFloodWaterPlaneComponent> WeakPlane(Plane);

	auto LevelLabel = [WeakPlane]()
	{
		const UFloodWaterPlaneComponent* P = WeakPlane.Get();
		if (!P) return FText::FromString(TEXT("(stale)"));
		const UCompartmentVolumeComponent* V = P->SourceVolume.Get();
		const float L = V ? V->GetWaterLevel01() : 0.f;
		const float HCm = V ? V->GetWaterHeightCm() : 0.f;
		return FText::Format(LOCTEXT("LevelFmt", "level={0}  H={1} cm"),
			FText::AsNumber(L), FText::AsNumber(HCm));
	};

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(6.f))
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.OnAreaExpansionChanged_Lambda([this, CompId](bool bExpanded)
			{
				OnCompartmentExpansionChanged(bExpanded, CompId);
			})
			.AreaTitle(FText::FromString(Title))
			.AreaTitleFont(FAppStyle::Get().GetFontStyle("BoldFont"))
			.HeaderContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[
					SNew(STextBlock).Text(FText::FromString(Title))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor(0.6f, 0.85f, 1.f))
					.Text_Lambda(LevelLabel)
				]
			]
			.BodyContent()
			[
				SNew(SVerticalBox)
				// Row 1: heightfield actions
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("InjectCenter", "Inject @ center"))
						.ToolTipText(LOCTEXT("InjectCenterTip", "Inject a wave at the compartment volume center. Cap mesh must be visible (compartment must have water) for the ripple to be seen — but the cyan debug sphere will fire either way."))
						.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnInjectAtCompartmentCenter, Plane)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("BreachCenter", "Breach @ center"))
						.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnBreachAtCompartmentCenter, Plane)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("ClearBreach", "Clear breach"))
						.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnClearBreach, Plane)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("ResetHF", "Reset heightfield"))
						.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnResetHeightfield, Plane)
					]
				]
				// Row 2: water level shortcuts (so injects become visually meaningful)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("FillLabel", "Water level:"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("Fill25", "25%"))
						.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnFillCompartment, Plane, 0.25f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("Fill50", "50%"))
						.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnFillCompartment, Plane, 0.50f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("Fill75", "75%"))
						.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnFillCompartment, Plane, 0.75f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("Fill100", "100%"))
						.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnFillCompartment, Plane, 1.f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("Drain", "Drain"))
						.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnDrainCompartment, Plane)
					]
				]
			]
		];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Per-compartment actions
// ─────────────────────────────────────────────────────────────────────────────

FReply SSub3DWaterDebugPanelWidget::OnInjectAtCompartmentCenter(UFloodWaterPlaneComponent* Plane)
{
	if (!Plane) return FReply::Handled();
	const UCompartmentVolumeComponent* Vol = Plane->SourceVolume.Get();
	if (!Vol)
	{
		SetLastAction(TEXT("Inject failed: no SourceVolume."));
		return FReply::Handled();
	}
	// Compartment center in sub-local space.
	const FVector LocalCenter = Vol->GetRelativeLocation();
	Plane->InjectAt(FVector2D(LocalCenter.X, LocalCenter.Y), InjectForce, InjectRadiusCm);
	SetLastAction(FString::Printf(TEXT("Inject @ center | Comp=%s | Force=%.1f Radius=%.1fcm"),
		*Vol->CompartmentId.ToString(), InjectForce, InjectRadiusCm));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnBreachAtCompartmentCenter(UFloodWaterPlaneComponent* Plane)
{
	if (!Plane) return FReply::Handled();
	const UCompartmentVolumeComponent* Vol = Plane->SourceVolume.Get();
	ASubmarineBase* Sub = Cast<ASubmarineBase>(Plane->GetOwner());
	if (!Vol || !Sub || !Sub->SubFlood)
	{
		SetLastAction(TEXT("Breach failed: no Vol/Sub/SubFlood."));
		return FReply::Handled();
	}
	const FVector LocalCenter = Vol->GetRelativeLocation();
	Sub->SubFlood->CreateBreach(Vol->CompartmentId, BreachInflowLps, LocalCenter);
	SetLastAction(FString::Printf(TEXT("Breach @ center | Comp=%s | %.1f L/s"),
		*Vol->CompartmentId.ToString(), BreachInflowLps));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnClearBreach(UFloodWaterPlaneComponent* Plane)
{
	if (!Plane) return FReply::Handled();
	const UCompartmentVolumeComponent* Vol = Plane->SourceVolume.Get();
	ASubmarineBase* Sub = Cast<ASubmarineBase>(Plane->GetOwner());
	if (!Vol || !Sub || !Sub->SubFlood)
	{
		return FReply::Handled();
	}
	// CreateBreach with 0 inflow clears it (per CreateBreach impl: sets bBreached = inflow > 0).
	Sub->SubFlood->CreateBreach(Vol->CompartmentId, 0.f, FVector::ZeroVector);
	SetLastAction(FString::Printf(TEXT("Cleared breach | Comp=%s"), *Vol->CompartmentId.ToString()));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnResetHeightfield(UFloodWaterPlaneComponent* Plane)
{
	if (!Plane) return FReply::Handled();
	Plane->ResetHeightfield();
	const UCompartmentVolumeComponent* Vol = Plane->SourceVolume.Get();
	SetLastAction(FString::Printf(TEXT("Reset heightfield | Comp=%s"),
		Vol ? *Vol->CompartmentId.ToString() : TEXT("?")));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnFillCompartment(UFloodWaterPlaneComponent* Plane, float Level01)
{
	if (!Plane) return FReply::Handled();
	const UCompartmentVolumeComponent* Vol = Plane->SourceVolume.Get();
	ASubmarineBase* Sub = Cast<ASubmarineBase>(Plane->GetOwner());
	if (!Vol || !Sub || !Sub->SubFlood)
	{
		SetLastAction(TEXT("Fill failed: no Vol/Sub/SubFlood."));
		return FReply::Handled();
	}
	Sub->SubFlood->SetCompartmentFloodDirect(Vol->CompartmentId, Level01);
	SetLastAction(FString::Printf(TEXT("Fill | Comp=%s | Level=%.2f"),
		*Vol->CompartmentId.ToString(), Level01));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnDrainCompartment(UFloodWaterPlaneComponent* Plane)
{
	return OnFillCompartment(Plane, 0.f);
}

void SSub3DWaterDebugPanelWidget::OnCompartmentExpansionChanged(bool bExpanded, FName CompId)
{
	if (CompId.IsNone()) return;
	if (bExpanded) ExpandedCompartments.Add(CompId);
	else ExpandedCompartments.Remove(CompId);
}

void SSub3DWaterDebugPanelWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (ExpandedCompartments.Num() == 0) return;
	const ASubmarineBase* Sub = SelectedSubmarine.Get();
	if (!Sub) return;
	UWorld* World = Sub->GetWorld();
	if (!World) return;

	TArray<UCompartmentVolumeComponent*> Vols;
	Sub->GetComponents<UCompartmentVolumeComponent>(Vols);
	for (const UCompartmentVolumeComponent* Vol : Vols)
	{
		if (!Vol) continue;
		if (!ExpandedCompartments.Contains(Vol->CompartmentId)) continue;
		// World-space oriented box around the volume — green to mark "selected from panel".
		const FTransform Xf = Vol->GetComponentTransform();
		const FVector Center = Xf.GetLocation();
		const FVector Extent = Vol->GetScaledBoxExtent();
		const FQuat Rot = Xf.GetRotation();
		// One-shot draw per Tick — DrawDebug requires a non-zero lifetime to render. Use a tiny
		// lifetime equal to the slate tick delta so the box repaints every frame without lingering
		// after collapse. Persistent flag false so engine cleans up.
		DrawDebugBox(World, Center, Extent, Rot, FColor::Green, false, InDeltaTime + 0.05f, SDPG_World, 4.f);
		const FString Label = FString::Printf(TEXT("[selected] %s"), *Vol->CompartmentId.ToString());
		DrawDebugString(World, Center + FVector(0, 0, Extent.Z + 30.f), Label, nullptr, FColor::Green, 0.f, true, 1.2f);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Crew-anchored shortcuts
// ─────────────────────────────────────────────────────────────────────────────

FReply SSub3DWaterDebugPanelWidget::OnInjectAtCrew()
{
	UWorld* World = ResolvePieWorld();
	if (!World)
	{
		SetLastAction(TEXT("Inject @ crew failed: no PIE world."));
		return FReply::Handled();
	}
	APlayerController* PC = World->GetFirstPlayerController();
	const ASubCrewCharacter* Crew = PC ? Cast<ASubCrewCharacter>(PC->GetPawn()) : nullptr;
	if (!Crew)
	{
		SetLastAction(TEXT("Inject @ crew failed: no possessed SubCrew."));
		return FReply::Handled();
	}
	const FName CompId = Crew->CurrentCompartmentId;
	if (CompId.IsNone())
	{
		SetLastAction(TEXT("Inject @ crew failed: crew is outside any compartment."));
		return FReply::Handled();
	}
	ASubmarineBase* Sub = SelectedSubmarine.Get();
	if (!Sub)
	{
		SetLastAction(TEXT("Inject @ crew failed: no submarine selected."));
		return FReply::Handled();
	}

	// Find the matching plane.
	TArray<UFloodWaterPlaneComponent*> Planes;
	Sub->GetComponents<UFloodWaterPlaneComponent>(Planes);
	UFloodWaterPlaneComponent* Match = nullptr;
	for (UFloodWaterPlaneComponent* P : Planes)
	{
		if (P && P->SourceVolume.IsValid() && P->SourceVolume->CompartmentId == CompId)
		{
			Match = P;
			break;
		}
	}
	if (!Match)
	{
		SetLastAction(FString::Printf(TEXT("Inject @ crew failed: no FloodWaterPlane for Comp=%s"), *CompId.ToString()));
		return FReply::Handled();
	}

	const FVector CrewWorld = Crew->GetActorLocation();
	if (Match->InjectAtWorldPoint(CrewWorld, InjectForce, InjectRadiusCm))
	{
		SetLastAction(FString::Printf(TEXT("Inject @ crew | Comp=%s | World=%s | Force=%.1f"),
			*CompId.ToString(), *CrewWorld.ToString(), InjectForce));
	}
	else
	{
		SetLastAction(FString::Printf(TEXT("Inject @ crew rejected: out of bounds for Comp=%s"), *CompId.ToString()));
	}
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnBreachAtCrew()
{
	UWorld* World = ResolvePieWorld();
	if (!World)
	{
		SetLastAction(TEXT("Breach @ crew failed: no PIE world."));
		return FReply::Handled();
	}
	APlayerController* PC = World->GetFirstPlayerController();
	const ASubCrewCharacter* Crew = PC ? Cast<ASubCrewCharacter>(PC->GetPawn()) : nullptr;
	if (!Crew)
	{
		SetLastAction(TEXT("Breach @ crew failed: no possessed SubCrew."));
		return FReply::Handled();
	}
	const FName CompId = Crew->CurrentCompartmentId;
	if (CompId.IsNone())
	{
		SetLastAction(TEXT("Breach @ crew failed: crew is outside any compartment."));
		return FReply::Handled();
	}
	ASubmarineBase* Sub = SelectedSubmarine.Get();
	if (!Sub || !Sub->SubFlood)
	{
		SetLastAction(TEXT("Breach @ crew failed: no Sub/SubFlood."));
		return FReply::Handled();
	}

	const USceneComponent* SubRoot = Sub->GetRootComponent();
	if (!SubRoot)
	{
		return FReply::Handled();
	}
	const FVector LocalCenter = SubRoot->GetComponentTransform().InverseTransformPosition(Crew->GetActorLocation());
	Sub->SubFlood->CreateBreach(CompId, BreachInflowLps, LocalCenter);
	SetLastAction(FString::Printf(TEXT("Breach @ crew | Comp=%s | Local=%s | %.1f L/s"),
		*CompId.ToString(), *LocalCenter.ToString(), BreachInflowLps));
	return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Global tunables
// ─────────────────────────────────────────────────────────────────────────────

void SSub3DWaterDebugPanelWidget::OnGlobalWaveSpeedChanged(float NewValue)
{
	GlobalWaveSpeed = NewValue;
}

void SSub3DWaterDebugPanelWidget::OnGlobalDampingChanged(float NewValue)
{
	GlobalDamping = NewValue;
}

void SSub3DWaterDebugPanelWidget::OnGlobalAmplitudeChanged(float NewValue)
{
	GlobalHeightfieldAmplitudeCm = NewValue;
}

FReply SSub3DWaterDebugPanelWidget::OnApplyTunablesClicked()
{
	int32 Count = 0;
	ForEachWaterPlane([&](UFloodWaterPlaneComponent* P)
	{
		P->WaveSpeed = GlobalWaveSpeed;
		P->Damping = GlobalDamping;
		P->HeightfieldAmplitudeCm = GlobalHeightfieldAmplitudeCm;
		++Count;
	});
	SetLastAction(FString::Printf(TEXT("Applied tunables to %d plane(s)  (Wave=%.2f Damp=%.4f Amp=%.1fcm)"),
		Count, GlobalWaveSpeed, GlobalDamping, GlobalHeightfieldAmplitudeCm));
	return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

void SSub3DWaterDebugPanelWidget::ForEachWaterPlane(const TFunctionRef<void(UFloodWaterPlaneComponent*)>& Fn) const
{
	const ASubmarineBase* Sub = SelectedSubmarine.Get();
	if (!Sub) return;
	TArray<UFloodWaterPlaneComponent*> Planes;
	Sub->GetComponents<UFloodWaterPlaneComponent>(Planes);
	for (UFloodWaterPlaneComponent* P : Planes)
	{
		if (P) Fn(P);
	}
}

void SSub3DWaterDebugPanelWidget::SetLastAction(const FString& Msg)
{
	LastActionMessage = Msg;
	if (LastActionTextBlock.IsValid())
	{
		LastActionTextBlock->SetText(FText::FromString(Msg));
	}

	// Push to rolling history. Newest at the bottom (chrono order, easier to read).
	const FString TimeStamp = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
	ActionHistory.Add(FString::Printf(TEXT("[%s] %s"), *TimeStamp, *Msg));
	while (ActionHistory.Num() > MaxHistoryEntries)
	{
		ActionHistory.RemoveAt(0);
	}
	if (LogTextBox.IsValid())
	{
		LogTextBox->SetText(GetLogText());
	}
}

FText SSub3DWaterDebugPanelWidget::GetLogText() const
{
	return FText::FromString(FString::Join(ActionHistory, TEXT("\n")));
}

FText SSub3DWaterDebugPanelWidget::GetStatusText() const
{
	const UWorld* World = ResolvePieWorld();
	if (!World)
	{
		return LOCTEXT("StatusNoPIE", "PIE not running. Start Play to populate.");
	}
	const ASubmarineBase* Sub = SelectedSubmarine.Get();
	if (!Sub)
	{
		return FText::Format(LOCTEXT("StatusNoSub", "PIE running. Submarines available: {0}. Click Refresh."),
			FText::AsNumber(AvailableSubs.Num()));
	}
	TArray<UFloodWaterPlaneComponent*> Planes;
	Sub->GetComponents<UFloodWaterPlaneComponent>(Planes);
	return FText::Format(LOCTEXT("StatusOK", "PIE running. Sub: {0}  |  Compartments: {1}"),
		FText::FromString(Sub->GetName()), FText::AsNumber(Planes.Num()));
}

FReply SSub3DWaterDebugPanelWidget::OnRefreshClicked()
{
	RebuildSubmarineList();
	RebuildCompartmentList();
	SetLastAction(TEXT("Refreshed."));
	return FReply::Handled();
}

void SSub3DWaterDebugPanelWidget::HandlePIEStarted(const bool bIsSimulating)
{
	RebuildSubmarineList();
	RebuildCompartmentList();
	SetLastAction(TEXT("PIE started — list rebuilt."));
}

void SSub3DWaterDebugPanelWidget::HandlePIEEnded(const bool bIsSimulating)
{
	SelectedSubmarine.Reset();
	AvailableSubs.Reset();
	RebuildCompartmentList();
	SetLastAction(TEXT("PIE ended."));
}

#undef LOCTEXT_NAMESPACE
