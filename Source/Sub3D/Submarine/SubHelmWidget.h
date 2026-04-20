#pragma once

#include "CoreMinimal.h"
#include "HelmNavigationDisplayComponent.h"
#include "SubmarineRuntimeTypes.h"
#include "SubmarineTypes.h"
#include "Templates/SubclassOf.h"
#include "SubNavWidget.h"
#include "SubSonarV2Types.h"
#include "SubHelmWidget.generated.h"

class USubSonarDisplayWidget;
class USubSonarComponent;
class USubSonarSystemComponent;
class UHelmNavigationDisplayWidget;
class UHelmNavigationDisplayComponent;
class UHelmCockpitWidget;
class UHelmStatusStripWidget;
class UReconstructionViewWidget;
class UTacticalGraphViewWidget;
class USubmarineSystemsComponent;
class UCanvasPanel;
class ASubmarineBase;

UENUM(BlueprintType)
enum class EHelmPanelRuntimeState : uint8
{
	Unbound UMETA(DisplayName = "Unbound"),
	Warming UMETA(DisplayName = "Warming"),
	Degraded UMETA(DisplayName = "Degraded"),
	Valid UMETA(DisplayName = "Valid")
};

USTRUCT(BlueprintType)
struct FHelmPerceptionPanelData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bBound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	EHelmPanelRuntimeState State = EHelmPanelRuntimeState::Unbound;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	ESonarMode SonarMode = ESonarMode::PassiveStandard;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float DisplayRangeCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float FocusBearingDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bSignalUnstable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float AcousticClutterLevel = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	int32 TrackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	int32 PriorityTrackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bPingReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float SecondsSinceLastPing = -1.f;
};

USTRUCT(BlueprintType)
struct FHelmGuidancePanelData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bBound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	EHelmPanelRuntimeState State = EHelmPanelRuntimeState::Unbound;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	FHelmInstrumentStatus InstrumentStatus;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	FHelmTacticalGraphViewData TacticalGraph;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	FTunnelNavStoppingDistanceWarning StoppingDistanceWarning;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	FTunnelNavCommitmentWarning CommitmentWarning;
};

USTRUCT(BlueprintType)
struct FHelmReconstructionPanelData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bBound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	EHelmPanelRuntimeState State = EHelmPanelRuntimeState::Unbound;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	FHelmReconstructionViewData ReconstructionView;
};

USTRUCT(BlueprintType)
struct FHelmBallastTankPanelData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float FillLevel01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float TargetFill01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	EPumpState PumpState = EPumpState::Nominal;
};

USTRUCT(BlueprintType)
struct FHelmControlPanelData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bBound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	EHelmPanelRuntimeState State = EHelmPanelRuntimeState::Unbound;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	FSubmarineCommandState CommandState;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float CurrentForwardSpeedCmS = 0.f;

	// Max forward speed authored on the submarine, used by the telegraph
	// panel to convert preset ratios (-1..+1) into command-state cm/s.
	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float MaxForwardSpeedCmS = 650.f;

	// Max reverse speed (separately authored). Telegraph reverse presets
	// must scale against this — backend clamps reverse to -MaxReverseSpeed
	// independently from MaxForwardSpeed (asymmetric subs).
	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float MaxReverseSpeedCmS = 250.f;

	// Engine spool state (-1..+1). Lags HelmThrottleCmd; cockpit shows
	// the gap between commanded and actual throttle.
	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float CurrentSpooledPower = 0.f;

	// Yaw rate (deg/s, +ccw). Drives the rudder yoke's secondary arc
	// and the heading-change feedback.
	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float CurrentYawRateDegPerSec = 0.f;

	// Compass heading (0..360°). Cockpit display.
	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float CurrentHeadingDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float CurrentDepthMeters = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float CurrentPitchDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float EffectivePowerInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float GlobalBallastFill01 = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	TArray<FHelmBallastTankPanelData> BallastTanks;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bAutoSpeedActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bAutoDepthActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bAutoPitchActive = false;
};

USTRUCT(BlueprintType)
struct FHelmAlertPanelData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bBound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	EHelmPanelRuntimeState State = EHelmPanelRuntimeState::Unbound;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float HeadingDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float SpeedKmh = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float DepthMeters = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float PitchDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	float RollDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bSignalUnstable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	bool bNavigationDataValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	FTunnelNavStoppingDistanceWarning StoppingDistanceWarning;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	FTunnelNavCommitmentWarning CommitmentWarning;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	FHelmInstrumentStatus InstrumentStatus;

	// Critical-tier alarms (smart strip Option γ). Populated by
	// GetAlertPanelData from SubFlood / Compartments queries. The strip
	// promotes these above stop-margin / commit warnings and renders red.
	UPROPERTY(BlueprintReadOnly, Category = "Helm|Alarm")
	int32 ActiveBreachCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Helm|Alarm")
	float MaxCompartmentFloodFraction = 0.f;
};

/**
 * Specialized widget for the Helm Station.
 * Extends SubNavStationWidget with sonar display binding.
 * The SonarDisplay reference is set in Blueprint after widget construction.
 */
UCLASS()
class SUB3D_API USubHelmWidget : public USubNavStationWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSonarPing();

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSonarPingHeldStart();

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSonarPingHeldStop();

	UFUNCTION(BlueprintPure, Category = "Sonar")
	bool IsSonarDisplayBound() const;

	UFUNCTION(BlueprintPure, Category = "HelmNav")
	bool IsHelmNavigationDisplayBound() const;

	UFUNCTION(BlueprintPure, Category = "HelmNav")
	bool IsReconstructionViewBound() const;

	UFUNCTION(BlueprintPure, Category = "HelmNav")
	bool IsForwardReconstructionViewBound() const;

	UFUNCTION(BlueprintPure, Category = "HelmNav")
	bool IsTacticalGraphViewBound() const;

	UFUNCTION(BlueprintPure, Category = "Helm")
	bool IsControlStackPanelBound() const;

	UFUNCTION(BlueprintPure, Category = "Helm")
	bool IsStatusStripPanelBound() const;

	UFUNCTION(BlueprintPure, Category = "Helm")
	FHelmPerceptionPanelData GetPerceptionPanelData() const;

	UFUNCTION(BlueprintPure, Category = "Helm")
	FHelmGuidancePanelData GetGuidancePanelData() const;

	UFUNCTION(BlueprintPure, Category = "Helm")
	FHelmReconstructionPanelData GetReconstructionPanelData() const;

	UFUNCTION(BlueprintPure, Category = "Helm")
	FHelmControlPanelData GetControlPanelData() const;

	UFUNCTION(BlueprintPure, Category = "Helm")
	FHelmAlertPanelData GetAlertPanelData() const;

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSetSonarMode(ESonarMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSetSonarFocusBearing(float BearingDeg);

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSetSonarRangePreset(int32 PresetIndex);

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteIncreaseSonarRangePreset();

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteDecreaseSonarRangePreset();

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteAdjustSonarRangePreset(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSetSonarRangeNormalized(float Normalized01);

	UFUNCTION(BlueprintPure, Category = "Sonar")
	int32 GetCurrentSonarRangePresetIndex() const;

	UFUNCTION(BlueprintPure, Category = "Sonar")
	int32 GetCurrentSonarRangePresetCount() const;

	UFUNCTION(BlueprintPure, Category = "Sonar")
	float GetCurrentSonarDisplayRangeCm() const;

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteMarkPriorityTrack(int32 TrackId, bool bPriority);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetHelmThrottle(float Value);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetHelmSteer(float Value);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetHelmTrim(float Value);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetRudderHoldEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetPlaneHoldEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetGlobalBallast(float Target01);

	// Per-tank ballast target. Used by the dive board for SURFACE / DIVE
	// commands (force individual tanks to 0% / 100%) and per-pipe drag.
	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetBallastByIndex(int32 TankIndex, float Target01);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetBallastsActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetPumpActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetPumpPower(float Value01);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetStabilizationMasterEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetAutoSpeedEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetAutoDepthEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetAutoPitchEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetTargetSpeedCmS(float SpeedCmS);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetTargetDepthMeters(float DepthMeters);

	UFUNCTION(BlueprintCallable, Category = "Helm|Control")
	void RouteSetTargetPitchDeg(float PitchDeg);

	// Reference to the sonar CRT display sub-widget.
	// Can be assigned from BP or discovered from the widget tree at runtime.
	UPROPERTY(BlueprintReadWrite, Category = "Sonar", meta = (BindWidgetOptional))
	TObjectPtr<USubSonarDisplayWidget> SonarDisplay;

	// Legacy transitional widget. Prefer ReconstructionView + TacticalGraphView.
	UPROPERTY(BlueprintReadWrite, Category = "HelmNav")
	TObjectPtr<UHelmNavigationDisplayWidget> HelmNavigationDisplay;

	UPROPERTY(BlueprintReadWrite, Category = "HelmNav", meta = (BindWidgetOptional))
	TObjectPtr<UReconstructionViewWidget> ReconstructionView;

	UPROPERTY(BlueprintReadWrite, Category = "HelmNav", meta = (BindWidgetOptional))
	TObjectPtr<UReconstructionViewWidget> ForwardReconstructionView;

	UPROPERTY(BlueprintReadWrite, Category = "HelmNav", meta = (BindWidgetOptional))
	TObjectPtr<UTacticalGraphViewWidget> TacticalGraphView;

	UPROPERTY(BlueprintReadWrite, Category = "Helm", meta = (BindWidgetOptional))
	TObjectPtr<UHelmCockpitWidget> ControlStackPanel;

	UPROPERTY(BlueprintReadWrite, Category = "Helm", meta = (BindWidgetOptional))
	TObjectPtr<UHelmStatusStripWidget> StatusStripPanel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WidgetBinding")
	bool bAllowWidgetTreeFallbackDiscovery = false;

	// If SonarDisplay is not provided by BP, create one automatically.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	bool bAutoCreateSonarDisplayIfMissing = false;

	// Optional class for auto-created sonar display (defaults to USubSonarDisplayWidget).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	TSubclassOf<USubSonarDisplayWidget> SonarDisplayClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	int32 AutoCreatedSonarDisplayZOrder = 60;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmNav")
	bool bAutoCreateHelmNavigationDisplayIfMissing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmNav")
	TSubclassOf<UHelmNavigationDisplayWidget> HelmNavigationDisplayClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmNav")
	int32 AutoCreatedHelmNavigationDisplayZOrder = 61;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helm")
	bool bAutoCreateControlStackPanelIfMissing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helm")
	TSubclassOf<UHelmCockpitWidget> ControlStackPanelClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helm")
	bool bAutoCreateStatusStripPanelIfMissing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helm")
	TSubclassOf<UHelmStatusStripWidget> StatusStripPanelClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmLayout")
	bool bDockInstrumentPanels = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmLayout")
	bool bHideLegacyHelmNavigationDisplayWhenSplitPanelsAvailable = true;

private:
	void DiscoverWidgetReferencesFromTree();
	void TryBindSonarDisplay();
	void TryBindHelmNavigationDisplay();
	void TryBindReconstructionView();
	void TryBindForwardReconstructionView();
	void TryBindTacticalGraphView();
	void TryBindControlStackPanel();
	void TryBindStatusStripPanel();
	void UpdateDockedPanelLayout();
	void ApplyDockedInstrumentStyle();
	UCanvasPanel* ResolveRootCanvasPanel() const;
	ASubmarineBase* ResolveCurrentSubmarineForHelm() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<USubSonarComponent> BoundSonar;

	UPROPERTY(Transient)
	TWeakObjectPtr<USubSonarSystemComponent> BoundSonarSystem;

	UPROPERTY(Transient)
	TWeakObjectPtr<UHelmNavigationDisplayComponent> BoundHelmNavigationDisplayComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<USubmarineSystemsComponent> BoundSystems;

	bool bOwnsAutoCreatedSonarDisplay = false;
	bool bLoggedMissingSonarDisplay = false;
	bool bOwnsAutoCreatedHelmNavigationDisplay = false;
	bool bLoggedMissingHelmNavigationDisplay = false;
	bool bLoggedMissingReconstructionView = false;
	bool bLoggedMissingForwardReconstructionView = false;
	bool bLoggedMissingTacticalGraphView = false;
	bool bOwnsAutoCreatedControlStackPanel = false;
	bool bLoggedMissingControlStackPanel = false;
	bool bOwnsAutoCreatedStatusStripPanel = false;
	bool bLoggedMissingStatusStripPanel = false;

	EHelmPanelRuntimeState ResolvePerceptionPanelState() const;
	EHelmPanelRuntimeState ResolveNavigationPanelState() const;
	EHelmPanelRuntimeState ResolveControlPanelState() const;
};
