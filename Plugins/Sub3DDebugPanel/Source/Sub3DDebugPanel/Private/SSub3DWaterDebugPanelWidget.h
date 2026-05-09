#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ASubmarineBase;
class UFloodWaterPlaneComponent;
class UCompartmentVolumeComponent;
class STextBlock;
class SVerticalBox;
class SEditableTextBox;

/**
 * Editor-only Water Debug Panel. Distinct from the main Sub3D Debug Panel.
 * Drives the live water heightfield + breach systems via the public API on
 * UFloodWaterPlaneComponent / USubFloodComponent. Editor-only: no shipping cost.
 *
 * Sections (all wrapped in SScrollBox so nothing blocks panel content):
 *  1. Submarine picker + refresh.
 *  2. Per-compartment list — expandable rows with Inject / Breach / Clear / Reset
 *     and a per-compartment status readout.
 *  3. Global tunables (WaveSpeed / Damping / HeightfieldAmplitudeCm) applied to all
 *     UFloodWaterPlaneComponent instances on the picked sub.
 *  4. Crew-anchored shortcuts (Inject @ crew, Breach @ crew).
 *  5. Visualization toggles bound to USub3DDebugSettings.
 *
 * The panel is read-from-PIE: when no PIE world is running, compartments list is empty
 * and a hint is shown.
 */
class SSub3DWaterDebugPanelWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSub3DWaterDebugPanelWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SSub3DWaterDebugPanelWidget() override;

private:
	// ── Submarine resolution ─────────────────────────────────────────────────
	TWeakObjectPtr<ASubmarineBase> SelectedSubmarine;
	TArray<TWeakObjectPtr<ASubmarineBase>> AvailableSubs;

	void RebuildSubmarineList();
	FReply OnRefreshClicked();
	FText GetStatusText() const;

	TSharedPtr<STextBlock> StatusTextBlock;
	TSharedPtr<SVerticalBox> CompartmentsList;

	// ── Per-compartment rebuild ──────────────────────────────────────────────
	void RebuildCompartmentList();
	TSharedRef<SWidget> MakeCompartmentRow(UFloodWaterPlaneComponent* Plane);

	// ── Per-compartment actions ──────────────────────────────────────────────
	FReply OnInjectAtCompartmentCenter(UFloodWaterPlaneComponent* Plane);
	FReply OnBreachAtCompartmentCenter(UFloodWaterPlaneComponent* Plane);
	FReply OnClearBreach(UFloodWaterPlaneComponent* Plane);
	FReply OnResetHeightfield(UFloodWaterPlaneComponent* Plane);
	FReply OnFillCompartment(UFloodWaterPlaneComponent* Plane, float Level01);
	FReply OnDrainCompartment(UFloodWaterPlaneComponent* Plane);
	FReply OnDumpMIDParams(UFloodWaterPlaneComponent* Plane);
	FReply OnOpenHeightfieldTexture(UFloodWaterPlaneComponent* Plane);

	// ── Selection / viewport highlight ───────────────────────────────────────
	TSet<FName> ExpandedCompartments;
	void OnCompartmentExpansionChanged(bool bExpanded, FName CompId);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// ── Global tunables (apply to all planes on selected sub) ────────────────
	float GlobalWaveSpeed = 200.f;
	float GlobalDamping = 0.995f;
	float GlobalHeightfieldAmplitudeCm = 10.f;
	float GlobalHeightfieldUpdateHz = 60.f;

	void OnGlobalWaveSpeedChanged(float NewValue);
	void OnGlobalDampingChanged(float NewValue);
	void OnGlobalAmplitudeChanged(float NewValue);
	FReply OnApplyTunablesClicked();

	// ── Crew-anchored shortcuts ──────────────────────────────────────────────
	FReply OnInjectAtCrew();
	FReply OnBreachAtCrew();

	// ── Common inputs ────────────────────────────────────────────────────────
	float InjectForce = 30.f;
	float InjectRadiusCm = 80.f;
	float BreachInflowLps = 50.f;

	void OnInjectForceChanged(float NewValue) { InjectForce = NewValue; }
	void OnInjectRadiusChanged(float NewValue) { InjectRadiusCm = NewValue; }
	void OnBreachInflowChanged(float NewValue) { BreachInflowLps = NewValue; }

	// ── PIE delegates (auto-refresh on PIE start/end) ────────────────────────
	FDelegateHandle PIEStartHandle;
	FDelegateHandle PIEEndHandle;
	void HandlePIEStarted(const bool bIsSimulating);
	void HandlePIEEnded(const bool bIsSimulating);

	// ── Helpers ──────────────────────────────────────────────────────────────
	UWorld* ResolvePieWorld() const;
	void ForEachWaterPlane(const TFunctionRef<void(UFloodWaterPlaneComponent*)>& Fn) const;
	FString LastActionMessage;
	void SetLastAction(const FString& Msg);
	TSharedPtr<STextBlock> LastActionTextBlock;

	// ── Log section (rolling history of panel actions) ───────────────────────
	TArray<FString> ActionHistory;
	static constexpr int32 MaxHistoryEntries = 30;
	TSharedPtr<class SMultiLineEditableTextBox> LogTextBox;
	FText GetLogText() const;
};
