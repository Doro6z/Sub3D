#include "SSub3DWaterDebugPanelWidget.h"

#include "AssetRegistry/AssetData.h"
#include "CompartmentVolumeComponent.h"
#include "Debug/Sub3DDebugSettings.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "FloodWaterPlaneComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Generator/SubmarineDefinition.h"
#include "Generator/SubmarineDefinitionTypes.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Layout/Margin.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PropertyCustomizationHelpers.h"
#include "SubFloodComponent.h"
#include "SubCrewCharacter.h"
#include "SubmarineBase.h"
#include "SubmarineWaterBakerLibrary.h"
#include "Styling/AppStyle.h"
#include "Types/CompartmentWaterBake.h"
#include "WaterBakeViewer.h"
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
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("DumpFloodGraph", "Dump Flood Graph"))
					.ToolTipText(LOCTEXT("DumpFloodGraphTip", "Logs to Output Log: Definition.Connections + FloodGraph.Edges + runtime EdgeStates with Closed/Area/Type. Tells you whether water can flow between compartments at all."))
					.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnDumpFloodGraphClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("DumpSubDef", "Dump SubDef"))
					.ToolTipText(LOCTEXT("DumpSubDefTip", "Full GeneratedDefinition dump to Output Log: Hull metrics, Performance, Compartments (capacity, hydroBounds, walkable Z, semantic), Connections (type, area, doors), FloodGraph (volumes + edges), Stations, Spawns, Mesh data sizes, WaterBakes link map, Flood defaults."))
					.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnDumpSubDefClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("OpenAll", "Open All Internal"))
					.ToolTipText(LOCTEXT("OpenAllTip", "Open every internal connection (skips ExteriorHatch). Lets water flow freely across all compartments — fastest way to validate the flood graph end-to-end."))
					.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnOpenAllInternalClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("CloseAll", "Close All Internal"))
					.ToolTipText(LOCTEXT("CloseAllTip", "Close every internal connection (skips ExteriorHatch). Compartments become isolated."))
					.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnCloseAllInternalClicked)
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
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0).VAlign(VAlign_Center).MinWidth(160.f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::Format(LOCTEXT("UpdateHzFmt", "Update Hz: {0}  (60..240)"),
						FText::AsNumber(GlobalHeightfieldUpdateHz)); })
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SSlider)
					.Value_Lambda([this]() { return (GlobalHeightfieldUpdateHz - 30.f) / 210.f; })
					.OnValueChanged_Lambda([this](float V) { GlobalHeightfieldUpdateHz = 30.f + V * 210.f; })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).MaxWidth(120.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return GlobalHeightfieldUpdateHz; })
					.OnValueChanged_Lambda([this](float V) { GlobalHeightfieldUpdateHz = V; })
					.MinValue(10.f).MaxValue(480.f)
					.ToolTipText(LOCTEXT("UpdateHzNumTip", "Substep rate of the wave equation. Higher = waves propagate faster in real time + more stable past CFL but heavier CPU."))
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

		+ SScrollBox::Slot()
		[
			SNew(SSeparator).Orientation(Orient_Horizontal)
		]

		// ── Authoring section (bake water + spawn viewers) ──────────────────
		// Sits at the bottom of the panel by design — keeps the per-compartment + tunables
		// sections at the top where they're used most often during water debug.
		+ SScrollBox::Slot()
		.Padding(SectionPadding)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.AreaTitle(LOCTEXT("AuthoringHeader", "Authoring (bake)"))
			.AreaTitleFont(FAppStyle::Get().GetFontStyle("BoldFont"))
			.BodyContent()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[
						SNew(STextBlock).Text(LOCTEXT("DefinitionLabel", "Submarine Definition"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SObjectPropertyEntryBox)
						.AllowedClass(USubmarineDefinition::StaticClass())
						.ObjectPath_Raw(this, &SSub3DWaterDebugPanelWidget::GetSelectedDefinitionPath)
						.OnObjectChanged_Raw(this, &SSub3DWaterDebugPanelWidget::OnDefinitionPicked)
						.AllowClear(true)
						.DisplayUseSelected(true)
						.DisplayBrowse(true)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("BakeWaterButton", "Bake Water"))
						.ToolTipText(LOCTEXT("BakeWaterTip", "Iterate every compartment in the picked Submarine Definition, find a BP_Submarine_* actor in the active level, voxelise + Marching Squares, save CWB_<id>.uasset under Submarines/<sub>/Water/."))
						.OnClicked_Raw(this, &SSub3DWaterDebugPanelWidget::OnBakeWaterClicked)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("CopyBakeReportButton", "Copy Report"))
						.ToolTipText(LOCTEXT("CopyBakeReportTip", "Copy the bake report to the clipboard."))
						.OnClicked_Raw(this, &SSub3DWaterDebugPanelWidget::OnCopyBakeReportClicked)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
				[
					SNew(SBox)
					.HeightOverride(220.f)
					[
						SAssignNew(BakeReportTextBox, SMultiLineEditableTextBox)
						.IsReadOnly(true)
						.AlwaysShowScrollbars(true)
						.AutoWrapText(false)
						.Text(FText::FromString(LastBakeReport))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BakeViewersHeader", "Bake Viewers"))
					.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("SpawnAllViewersButton", "Spawn All Viewers"))
						.ToolTipText(LOCTEXT("SpawnAllViewersTip", "For every CWB asset produced by the last bake, spawn an AWaterBakeViewer at the submarine's location + (0, 5000, 0) cm. Move individual viewers via gizmo."))
						.OnClicked_Raw(this, &SSub3DWaterDebugPanelWidget::OnSpawnAllViewersClicked)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("DespawnAllViewersButton", "Despawn All Viewers"))
						.ToolTipText(LOCTEXT("DespawnAllViewersTip", "Destroy every AWaterBakeViewer spawned by this panel session."))
						.OnClicked_Raw(this, &SSub3DWaterDebugPanelWidget::OnDespawnAllViewersClicked)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return ViewerStatus; })
					.AutoWrapText(true)
				]
			]
		]
	];

	LastBakeReport = TEXT("Idle. Pick a Submarine Definition + open the gameplay map (with BP_Submarine_* placed) + click Bake Water.");
	ViewerStatus = LOCTEXT("ViewerStatusIdle", "No viewers spawned. Run Bake Water then click Spawn All Viewers.");

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
				// Row 1.5: material debug
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("DumpMID", "Dump MID params (log)"))
						.ToolTipText(LOCTEXT("DumpMIDTip", "Logs every Scalar/Vector/Texture parameter currently bound on the cap mesh's MID. Verify what the runtime is pushing vs what the material expects."))
						.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnDumpMIDParams, Plane)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("OpenHFTex", "Open heightfield texture"))
						.ToolTipText(LOCTEXT("OpenHFTexTip", "Opens the runtime R32F heightfield texture in the editor texture viewer (transient asset)."))
						.OnClicked(this, &SSub3DWaterDebugPanelWidget::OnOpenHeightfieldTexture, Plane)
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

FReply SSub3DWaterDebugPanelWidget::OnDumpMIDParams(UFloodWaterPlaneComponent* Plane)
{
	if (!Plane) return FReply::Handled();
	const UCompartmentVolumeComponent* Vol = Plane->SourceVolume.Get();
	const FName CompId = Vol ? Vol->CompartmentId : NAME_None;
	UMaterialInstanceDynamic* MID = Plane->GetBakeCapMID();
	if (!MID)
	{
		SetLastAction(FString::Printf(TEXT("Dump MID failed | Comp=%s | No BakeCapMID (cap mesh not yet rendered?)"),
			*CompId.ToString()));
		return FReply::Handled();
	}

	UE_LOG(LogTemp, Display, TEXT("──── MID Params Dump | Comp=%s | MID=%s | Parent=%s ────"),
		*CompId.ToString(), *MID->GetName(), *GetNameSafe(MID->Parent));

	// Iterate scalar params
	TArray<FMaterialParameterInfo> ScalarInfos;
	TArray<FGuid> ScalarGuids;
	MID->GetAllScalarParameterInfo(ScalarInfos, ScalarGuids);
	for (const FMaterialParameterInfo& Info : ScalarInfos)
	{
		float Value = 0.f;
		MID->GetScalarParameterValue(Info, Value);
		UE_LOG(LogTemp, Display, TEXT("  Scalar  %s = %.4f"), *Info.Name.ToString(), Value);
	}

	// Iterate vector params
	TArray<FMaterialParameterInfo> VectorInfos;
	TArray<FGuid> VectorGuids;
	MID->GetAllVectorParameterInfo(VectorInfos, VectorGuids);
	for (const FMaterialParameterInfo& Info : VectorInfos)
	{
		FLinearColor Value;
		MID->GetVectorParameterValue(Info, Value);
		UE_LOG(LogTemp, Display, TEXT("  Vector  %s = (%.3f, %.3f, %.3f, %.3f)"),
			*Info.Name.ToString(), Value.R, Value.G, Value.B, Value.A);
	}

	// Iterate texture params
	TArray<FMaterialParameterInfo> TexInfos;
	TArray<FGuid> TexGuids;
	MID->GetAllTextureParameterInfo(TexInfos, TexGuids);
	for (const FMaterialParameterInfo& Info : TexInfos)
	{
		UTexture* Tex = nullptr;
		MID->GetTextureParameterValue(Info, Tex);
		UE_LOG(LogTemp, Display, TEXT("  Texture %s = %s"), *Info.Name.ToString(), *GetNameSafe(Tex));
	}

	UE_LOG(LogTemp, Display, TEXT("──── End dump  Comp=%s ────"), *CompId.ToString());
	SetLastAction(FString::Printf(TEXT("Dumped MID params for %s — see Output Log"), *CompId.ToString()));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnOpenHeightfieldTexture(UFloodWaterPlaneComponent* Plane)
{
	if (!Plane) return FReply::Handled();
	UTexture2D* Tex = Plane->GetHeightfieldTexture();
	if (!Tex)
	{
		SetLastAction(TEXT("Heightfield texture not yet created."));
		return FReply::Handled();
	}
	if (GEditor)
	{
		if (UAssetEditorSubsystem* AESS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AESS->OpenEditorForAsset(Tex);
		}
	}
	SetLastAction(FString::Printf(TEXT("Opened heightfield texture %s in editor"), *Tex->GetName()));
	return FReply::Handled();
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
		P->HeightfieldUpdateHz = GlobalHeightfieldUpdateHz;
		++Count;
	});
	SetLastAction(FString::Printf(TEXT("Applied tunables to %d plane(s)  (Wave=%.2f Damp=%.4f Amp=%.1fcm UpdateHz=%.0f)"),
		Count, GlobalWaveSpeed, GlobalDamping, GlobalHeightfieldAmplitudeCm, GlobalHeightfieldUpdateHz));
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

FReply SSub3DWaterDebugPanelWidget::OnOpenAllInternalClicked()
{
	const ASubmarineBase* Sub = SelectedSubmarine.Get();
	if (!Sub || !Sub->SubFlood)
	{
		SetLastAction(TEXT("Open All: no Sub/SubFlood."));
		return FReply::Handled();
	}
	int32 Count = 0;
	for (const FFloodEdgeState& E : Sub->SubFlood->GetEdgeStates())
	{
		if (E.bExteriorEdge) continue; // skip exterior — would drain the sub
		if (E.ClosureId.IsNone()) continue;
		Sub->SubFlood->SetDoorState(E.ClosureId, false);
		++Count;
	}
	SetLastAction(FString::Printf(TEXT("Opened %d internal connections (exterior skipped)."), Count));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnCloseAllInternalClicked()
{
	const ASubmarineBase* Sub = SelectedSubmarine.Get();
	if (!Sub || !Sub->SubFlood)
	{
		SetLastAction(TEXT("Close All: no Sub/SubFlood."));
		return FReply::Handled();
	}
	int32 Count = 0;
	for (const FFloodEdgeState& E : Sub->SubFlood->GetEdgeStates())
	{
		if (E.bExteriorEdge) continue;
		if (E.ClosureId.IsNone()) continue;
		Sub->SubFlood->SetDoorState(E.ClosureId, true);
		++Count;
	}
	SetLastAction(FString::Printf(TEXT("Closed %d internal connections."), Count));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnDumpSubDefClicked()
{
	const ASubmarineBase* Sub = SelectedSubmarine.Get();
	if (!Sub)
	{
		SetLastAction(TEXT("Dump SubDef: no sub selected."));
		return FReply::Handled();
	}
	const USubmarineDefinition* Def = Sub->GeneratedDefinition;
	if (!Def)
	{
		SetLastAction(TEXT("Dump SubDef: no GeneratedDefinition."));
		return FReply::Handled();
	}

	UE_LOG(LogTemp, Display, TEXT("════════ SubDef dump | %s ════════"), *GetNameSafe(Def));

	UE_LOG(LogTemp, Display, TEXT("── Hull ──"));
	UE_LOG(LogTemp, Display, TEXT("  Length=%.0fcm Beam=%.0fcm Height=%.0fcm WallThickness=%.1fcm"),
		Def->HullLengthCm, Def->HullBeamCm, Def->HullHeightCm, Def->WallThicknessCm);
	UE_LOG(LogTemp, Display, TEXT("  BaseMass=%.0fkg SubmergedVolume=%.0fL"),
		Def->BaseMassKg, Def->SubmergedVolumeLiters);

	UE_LOG(LogTemp, Display, TEXT("── Performance ──"));
	UE_LOG(LogTemp, Display, TEXT("  MaxFwd=%.0f MaxRev=%.0f MaxVert=%.0f MaxThrust=%.0f"),
		Def->MaxForwardSpeedCmS, Def->MaxReverseSpeedCmS, Def->MaxVerticalSpeedCmS, Def->MaxThrustN);

	UE_LOG(LogTemp, Display, TEXT("── Compartments (%d) ──"), Def->Compartments.Num());
	for (const FGeneratedCompartmentDef& C : Def->Compartments)
	{
		UE_LOG(LogTemp, Display,
			TEXT("  [%s] Capacity=%.0fL  MaxH=%.0fcm  Floor=%.0fcm  Sem=%d"),
			*C.CompartmentId.ToString(), C.CapacityLiters, C.MaxWaterHeightCm,
			C.WalkableFloorZCm, static_cast<int32>(C.SemanticType));
		UE_LOG(LogTemp, Display,
			TEXT("    HydroBounds: Min=%s Max=%s"),
			*C.HydroBoundsMin.ToString(), *C.HydroBoundsMax.ToString());
	}

	UE_LOG(LogTemp, Display, TEXT("── Connections (%d) ──"), Def->Connections.Num());
	for (const FGeneratedConnectionDef& C : Def->Connections)
	{
		FString TypeName;
		switch (C.ConnectionType)
		{
			case EConnectionType::Door:          TypeName = TEXT("Door"); break;
			case EConnectionType::Hatch:         TypeName = TEXT("Hatch"); break;
			case EConnectionType::ExteriorHatch: TypeName = TEXT("ExteriorHatch"); break;
			case EConnectionType::Open:          TypeName = TEXT("Open"); break;
			default: TypeName = TEXT("?"); break;
		}
		const FString CompBStr = C.CompartmentB.IsNone() ? TEXT("(EXT)") : C.CompartmentB.ToString();
		UE_LOG(LogTemp, Display,
			TEXT("  [%s] Type=%s  %s ↔ %s  Area=%.0fcm²  StartsClosed=%s  Door=%.0fx%.0fcm"),
			*C.ConnectionId.ToString(), *TypeName,
			*C.CompartmentA.ToString(), *CompBStr,
			C.FlowAreaCm2, C.bStartsClosed ? TEXT("true") : TEXT("false"),
			C.DoorWidthCm, C.DoorHeightCm);
	}

	UE_LOG(LogTemp, Display, TEXT("── FloodGraph.Volumes (%d) ──"), Def->FloodGraph.Volumes.Num());
	for (const FDerivedFloodVolume& V : Def->FloodGraph.Volumes)
	{
		UE_LOG(LogTemp, Display, TEXT("  [%s] Capacity=%.0fL"),
			*V.VolumeId.ToString(), V.CapacityLiters);
	}

	UE_LOG(LogTemp, Display, TEXT("── FloodGraph.Edges (%d) ──"), Def->FloodGraph.Edges.Num());
	for (const FFloodGraphEdge& E : Def->FloodGraph.Edges)
	{
		const FString VBStr = E.VolumeB.IsNone() ? TEXT("(EXT)") : E.VolumeB.ToString();
		UE_LOG(LogTemp, Display,
			TEXT("  [%s] %s ↔ %s  Area=%.0fcm²  Exterior=%s"),
			*E.ClosureId.ToString(),
			*E.VolumeA.ToString(), *VBStr,
			E.PassageAreaCm2, E.bExteriorEdge ? TEXT("true") : TEXT("false"));
	}

	UE_LOG(LogTemp, Display, TEXT("── StationSlots (%d) ──"), Def->StationSlots.Num());
	for (const FGeneratedStationSlotDef& S : Def->StationSlots)
	{
		UE_LOG(LogTemp, Display, TEXT("  [%s] Type=%d  Compartment=%s"),
			*S.StationId.ToString(), static_cast<int32>(S.StationType), *S.CompartmentId.ToString());
	}

	UE_LOG(LogTemp, Display, TEXT("── SpawnPoints (%d) ──"), Def->SpawnPoints.Num());
	for (const FGeneratedSpawnPointDef& Sp : Def->SpawnPoints)
	{
		UE_LOG(LogTemp, Display, TEXT("  [%s] Role=%d"),
			*Sp.SpawnId.ToString(), static_cast<int32>(Sp.Role));
	}

	UE_LOG(LogTemp, Display, TEXT("── Mesh data summary ──"));
	UE_LOG(LogTemp, Display, TEXT("  ExteriorHull: vertices=%d, indices=%d"),
		Def->ExteriorHullMesh.Vertices.Num(), Def->ExteriorHullMesh.Triangles.Num());
	UE_LOG(LogTemp, Display, TEXT("  InteriorMeshes: %d entries"), Def->InteriorMeshes.Num());
	UE_LOG(LogTemp, Display, TEXT("  BulkheadMeshes: %d entries"), Def->BulkheadMeshes.Num());

	UE_LOG(LogTemp, Display, TEXT("── WaterBakes (%d entries) ──"), Def->WaterBakes.Num());
	for (const TPair<FName, TObjectPtr<UCompartmentWaterBake>>& Pair : Def->WaterBakes)
	{
		UE_LOG(LogTemp, Display, TEXT("  [%s] → %s"),
			*Pair.Key.ToString(), *GetNameSafe(Pair.Value));
	}

	UE_LOG(LogTemp, Display, TEXT("── Flood defaults ──"));
	UE_LOG(LogTemp, Display, TEXT("  MaxExteriorInflow=%.0fL/s  DefaultPumpRate=%.0fL/s"),
		Def->MaxExteriorInflowLitersPerSec, Def->DefaultPumpRateLitersPerSec);

	UE_LOG(LogTemp, Display, TEXT("════════ End SubDef dump ════════"));

	SetLastAction(FString::Printf(TEXT("Dumped SubDef '%s' (%d compartments, %d connections, %d edges) — see Output Log"),
		*GetNameSafe(Def),
		Def->Compartments.Num(), Def->Connections.Num(), Def->FloodGraph.Edges.Num()));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnDumpFloodGraphClicked()
{
	const ASubmarineBase* Sub = SelectedSubmarine.Get();
	if (!Sub)
	{
		SetLastAction(TEXT("Dump flood graph: no sub selected."));
		return FReply::Handled();
	}
	const USubFloodComponent* Flood = Sub->FindComponentByClass<USubFloodComponent>();
	if (!Flood)
	{
		SetLastAction(TEXT("Dump flood graph: no SubFloodComponent."));
		return FReply::Handled();
	}

	UE_LOG(LogTemp, Display, TEXT("──── Flood graph dump | Sub=%s ────"), *Sub->GetName());
	UE_LOG(LogTemp, Display, TEXT("  GeneratedDefinition = %s"), *GetNameSafe(Sub->GeneratedDefinition));

	if (Sub->GeneratedDefinition)
	{
		UE_LOG(LogTemp, Display, TEXT("  Definition.Compartments = %d"), Sub->GeneratedDefinition->Compartments.Num());
		UE_LOG(LogTemp, Display, TEXT("  Definition.Connections  = %d"), Sub->GeneratedDefinition->Connections.Num());
		UE_LOG(LogTemp, Display, TEXT("  Definition.FloodGraph.Volumes = %d"), Sub->GeneratedDefinition->FloodGraph.Volumes.Num());
		UE_LOG(LogTemp, Display, TEXT("  Definition.FloodGraph.Edges   = %d"), Sub->GeneratedDefinition->FloodGraph.Edges.Num());
		for (const FGeneratedConnectionDef& C : Sub->GeneratedDefinition->Connections)
		{
			UE_LOG(LogTemp, Display, TEXT("    Connection %s | %s ↔ %s | Type=%d | Area=%.0f cm² | StartsClosed=%s"),
				*C.ConnectionId.ToString(), *C.CompartmentA.ToString(), *C.CompartmentB.ToString(),
				static_cast<int32>(C.ConnectionType), C.FlowAreaCm2, C.bStartsClosed ? TEXT("true") : TEXT("false"));
		}
	}

	const TArray<FFloodEdgeState>& Edges = Flood->GetEdgeStates();
	UE_LOG(LogTemp, Display, TEXT("  Runtime EdgeStates = %d  (these drive AdvanceFlooding)"), Edges.Num());
	for (const FFloodEdgeState& E : Edges)
	{
		UE_LOG(LogTemp, Display, TEXT("    Edge %s | %s ↔ %s | Area=%.0f cm² | Closed=%s | Exterior=%s"),
			*E.ClosureId.ToString(), *E.VolumeA.ToString(), *E.VolumeB.ToString(),
			E.PassageAreaCm2, E.bClosed ? TEXT("true") : TEXT("false"), E.bExteriorEdge ? TEXT("true") : TEXT("false"));
	}

	UE_LOG(LogTemp, Display, TEXT("──── End flood graph dump ────"));
	SetLastAction(FString::Printf(TEXT("Dumped flood graph: %d connections, %d edges. See Output Log."),
		Sub->GeneratedDefinition ? Sub->GeneratedDefinition->Connections.Num() : 0, Edges.Num()));
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

// ─────────────────────────────────────────────────────────────────────────────
//  Authoring section (moved from SSub3DDebugPanelWidget — bake + viewers)
// ─────────────────────────────────────────────────────────────────────────────

FString SSub3DWaterDebugPanelWidget::GetSelectedDefinitionPath() const
{
	return SelectedDefinition.IsValid() ? SelectedDefinition->GetPathName() : FString();
}

void SSub3DWaterDebugPanelWidget::OnDefinitionPicked(const FAssetData& Asset)
{
	SelectedDefinition = Cast<USubmarineDefinition>(Asset.GetAsset());
}

FReply SSub3DWaterDebugPanelWidget::OnBakeWaterClicked()
{
	auto SetReport = [this](const FString& Text)
	{
		LastBakeReport = Text;
		if (BakeReportTextBox.IsValid()) BakeReportTextBox->SetText(FText::FromString(LastBakeReport));
	};

	USubmarineDefinition* Definition = SelectedDefinition.Get();
	if (!Definition)
	{
		SetReport(TEXT("[FAIL] Pick a Submarine Definition first."));
		return FReply::Handled();
	}

	UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
	if (!ActorSubsystem)
	{
		SetReport(TEXT("[FAIL] EditorActorSubsystem unavailable."));
		return FReply::Handled();
	}

	TArray<AActor*> AllActors = ActorSubsystem->GetAllLevelActors();
	AActor* HullActor = nullptr;
	for (AActor* Actor : AllActors)
	{
		if (!Actor) continue;
		if (Actor->GetClass()->GetName().StartsWith(TEXT("BP_Submarine_")))
		{
			HullActor = Actor;
			break;
		}
	}
	if (!HullActor)
	{
		SetReport(TEXT("[FAIL] No BP_Submarine_* actor in the active level. Open the gameplay map first."));
		return FReply::Handled();
	}

	FSubmarineWaterBakeParams Params;
	FString Report;
	Report += FString::Printf(
		TEXT("Bake on %s — %d compartments — params: NumSlices=%d CellSize=%.1fcm RingsCount=%d ResampleN=%d Inset=%.1fcm\n"),
		*HullActor->GetName(), Definition->Compartments.Num(),
		Params.NumSlices, Params.CellSizeCm, Params.RingsCount, Params.PolygonResampleN, Params.CapInsetCm);

	LastBakedAssets.Reset();
	int32 SuccessCount = 0;
	for (const FGeneratedCompartmentDef& Comp : Definition->Compartments)
	{
		if (Comp.CompartmentId.IsNone()) continue;
		UCompartmentWaterBake* Bake = USubmarineWaterBakerLibrary::BakeCompartment(
			Definition, Comp.CompartmentId, HullActor, Params, Report);
		if (Bake)
		{
			++SuccessCount;
			LastBakedAssets.Add(Bake);
		}
	}

	Report += FString::Printf(TEXT("─ Done: %d/%d compartments baked ─\n"),
		SuccessCount, Definition->Compartments.Num());

	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString ReportPath = FPaths::ProjectSavedDir() / TEXT("Sub3D/BakeReports")
		/ FString::Printf(TEXT("BakeReport_%s.txt"), *Timestamp);
	if (FFileHelper::SaveStringToFile(Report, *ReportPath))
	{
		Report += FString::Printf(TEXT("Report saved to: %s\n"), *FPaths::ConvertRelativePathToFull(ReportPath));
	}

	SetReport(Report);
	ViewerStatus = FText::Format(LOCTEXT("ViewerStatusReady", "{0} bake(s) ready. Click Spawn All Viewers."),
		FText::AsNumber(LastBakedAssets.Num()));
	SetLastAction(FString::Printf(TEXT("Bake done: %d/%d"),
		SuccessCount, Definition->Compartments.Num()));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnSpawnAllViewersClicked()
{
	if (LastBakedAssets.Num() == 0)
	{
		ViewerStatus = LOCTEXT("ViewerStatusNoBakes", "No bakes captured. Run Bake Water first.");
		return FReply::Handled();
	}

	UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
	if (!ActorSubsystem)
	{
		ViewerStatus = LOCTEXT("ViewerStatusNoSubsystem", "EditorActorSubsystem unavailable.");
		return FReply::Handled();
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		ViewerStatus = LOCTEXT("ViewerStatusNoWorld", "No editor world.");
		return FReply::Handled();
	}

	TArray<AActor*> AllActors = ActorSubsystem->GetAllLevelActors();
	AActor* HullActor = nullptr;
	for (AActor* Actor : AllActors)
	{
		if (Actor && Actor->GetClass()->GetName().StartsWith(TEXT("BP_Submarine_")))
		{
			HullActor = Actor;
			break;
		}
	}
	const FVector AnchorLocation = HullActor ? HullActor->GetActorLocation() : FVector::ZeroVector;
	const FRotator AnchorRotation = HullActor ? HullActor->GetActorRotation() : FRotator::ZeroRotator;
	const FVector SpawnLocation = AnchorLocation + FVector(0.f, 5000.f, 0.f);

	int32 Spawned = 0;
	for (const TWeakObjectPtr<UCompartmentWaterBake>& WeakBake : LastBakedAssets)
	{
		UCompartmentWaterBake* Bake = WeakBake.Get();
		if (!Bake) continue;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AWaterBakeViewer* Viewer = World->SpawnActor<AWaterBakeViewer>(
			AWaterBakeViewer::StaticClass(), SpawnLocation, AnchorRotation, SpawnParams);
		if (!Viewer) continue;

		Viewer->BakeToView = Bake;
#if WITH_EDITOR
		Viewer->SetActorLabel(FString::Printf(TEXT("BakeViewer_%s"), *Bake->CompartmentId.ToString()));
#endif
		SpawnedViewers.Add(Viewer);
		++Spawned;
	}

	ViewerStatus = FText::Format(LOCTEXT("ViewerStatusSpawned",
		"Spawned {0} viewer(s) at sub + (0, 5000, 0). Move them via gizmo to inspect."),
		FText::AsNumber(Spawned));
	SetLastAction(FString::Printf(TEXT("Spawned %d viewers."), Spawned));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnDespawnAllViewersClicked()
{
	int32 Destroyed = 0;
	for (const TWeakObjectPtr<AWaterBakeViewer>& WeakViewer : SpawnedViewers)
	{
		if (AWaterBakeViewer* Viewer = WeakViewer.Get())
		{
			Viewer->Destroy();
			++Destroyed;
		}
	}
	SpawnedViewers.Reset();
	ViewerStatus = FText::Format(LOCTEXT("ViewerStatusDespawned", "Despawned {0} viewer(s)."),
		FText::AsNumber(Destroyed));
	SetLastAction(FString::Printf(TEXT("Despawned %d viewers."), Destroyed));
	return FReply::Handled();
}

FReply SSub3DWaterDebugPanelWidget::OnCopyBakeReportClicked()
{
	FPlatformApplicationMisc::ClipboardCopy(*LastBakeReport);
	SetLastAction(TEXT("Bake report copied to clipboard."));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
