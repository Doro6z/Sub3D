#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class USubmarineDefinition;
class UCompartmentWaterBake;
class AWaterBakeViewer;
class STextBlock;
class SMultiLineEditableTextBox;

/**
 * Panel content. Two sections, top-to-bottom:
 *   1. Authoring — command buttons (currently: Bake Water).
 *   2. Debug — DetailsView wrapping USub3DDebugSettings CDO so every Config UPROPERTY in
 *      that class (grouped by Category) is rendered and auto-saved through
 *      UDeveloperSettings' own mechanism.
 */
class SSub3DDebugPanelWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSub3DDebugPanelWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// ── Debug Toggles section ────────────────────────────────────────────
	TSharedPtr<IDetailsView> DetailsView;

	// ── Authoring section state ──────────────────────────────────────────
	/** Currently picked Submarine Definition (input to bake). */
	TWeakObjectPtr<USubmarineDefinition> SelectedDefinition;

	/** Multi-line bake report (header + per-compartment lines + footer). Shown read-only in the
	 *  multi-line text box; user can select-all + Ctrl+C, or hit the "Copy" button. */
	FString LastBakeReport;

	TSharedPtr<SMultiLineEditableTextBox> ReportTextBox;

	/** CWB assets produced by the most recent successful bake. Drives Spawn All Viewers. */
	TArray<TWeakObjectPtr<UCompartmentWaterBake>> LastBakedAssets;

	/** Currently spawned bake viewer actors. Tracked so we can cleanly Despawn All. */
	TArray<TWeakObjectPtr<AWaterBakeViewer>> SpawnedViewers;

	/** Inline status for the Bake Viewers section. */
	FText ViewerStatus;

	// ── Authoring handlers ───────────────────────────────────────────────
	FReply OnBakeWaterClicked();
	FReply OnCopyReportClicked();
	FReply OnSpawnAllViewersClicked();
	FReply OnDespawnAllViewersClicked();
	void OnDefinitionPicked(const struct FAssetData& Asset);
	FString GetSelectedDefinitionPath() const;
};
