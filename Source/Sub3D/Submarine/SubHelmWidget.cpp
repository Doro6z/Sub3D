#include "SubHelmWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Helm/HelmCockpitWidget.h"
#include "HelmNavigationDisplayComponent.h"
#include "HelmNavigationDisplayWidget.h"
#include "HelmStatusStripWidget.h"
#include "ReconstructionViewWidget.h"
#include "SubFloodComponent.h"
#include "SubPlayerController.h"
#include "SubMovementComponent.h"
#include "SubSonarComponent.h"
#include "SubSonarDisplayWidget.h"
#include "SubSonarSystemComponent.h"
#include "SubmarineBase.h"
#include "SubmarineSystemsComponent.h"
#include "TacticalGraphViewWidget.h"

namespace
{
template <typename T>
T* FindNamedWidget(UWidgetTree* WidgetTree, const TCHAR* WidgetName)
{
	return WidgetTree ? Cast<T>(WidgetTree->FindWidget(FName(WidgetName))) : nullptr;
}

int32 CountPriorityTracks(const TArray<FSonarTrack>& Tracks)
{
	int32 PriorityCount = 0;
	for (const FSonarTrack& Track : Tracks)
	{
		if (Track.bPriority)
		{
			++PriorityCount;
		}
	}

	return PriorityCount;
}

float ComputeAverageBallastFill(const TArray<FBallastTank>& Ballasts)
{
	if (Ballasts.IsEmpty())
	{
		return 0.5f;
	}

	float FillSum = 0.f;
	for (const FBallastTank& Tank : Ballasts)
	{
		FillSum += Tank.FillLevel;
	}

	return FillSum / static_cast<float>(Ballasts.Num());
}
}

void USubHelmWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// FInputModeGameAndUI lets keyboard inputs (IA_SonarPing on Space, IA_Move
	// etc.) keep flowing to the player input stack while UI elements still
	// receive mouse clicks. Without this the widget steals focus on first
	// click and Space, Z, W etc. stop firing actions until the helm closes.
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}

	DiscoverWidgetReferencesFromTree();
	TryBindSonarDisplay();
	TryBindHelmNavigationDisplay();
	TryBindReconstructionView();
	TryBindForwardReconstructionView();
	TryBindTacticalGraphView();
	TryBindControlStackPanel();
	TryBindStatusStripPanel();
	ApplyDockedInstrumentStyle();
	UpdateDockedPanelLayout();
}

void USubHelmWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bAllowWidgetTreeFallbackDiscovery &&
		(!SonarDisplay || !ReconstructionView || !ForwardReconstructionView || !TacticalGraphView || !HelmNavigationDisplay))
	{
		DiscoverWidgetReferencesFromTree();
	}
	TryBindSonarDisplay();
	TryBindHelmNavigationDisplay();
	TryBindReconstructionView();
	TryBindForwardReconstructionView();
	TryBindTacticalGraphView();
	TryBindControlStackPanel();
	TryBindStatusStripPanel();
	ApplyDockedInstrumentStyle();
	UpdateDockedPanelLayout();
}

void USubHelmWidget::NativeDestruct()
{
	if (bOwnsAutoCreatedSonarDisplay && SonarDisplay)
	{
		SonarDisplay->RemoveFromParent();
		SonarDisplay = nullptr;
		bOwnsAutoCreatedSonarDisplay = false;
	}

	if (bOwnsAutoCreatedHelmNavigationDisplay && HelmNavigationDisplay)
	{
		HelmNavigationDisplay->RemoveFromParent();
		HelmNavigationDisplay = nullptr;
		bOwnsAutoCreatedHelmNavigationDisplay = false;
	}

	if (bOwnsAutoCreatedControlStackPanel && ControlStackPanel)
	{
		ControlStackPanel->RemoveFromParent();
		ControlStackPanel = nullptr;
		bOwnsAutoCreatedControlStackPanel = false;
	}

	if (bOwnsAutoCreatedStatusStripPanel && StatusStripPanel)
	{
		StatusStripPanel->RemoveFromParent();
		StatusStripPanel = nullptr;
		bOwnsAutoCreatedStatusStripPanel = false;
	}

	BoundSonar.Reset();
	BoundSonarSystem.Reset();
	BoundHelmNavigationDisplayComponent.Reset();
	BoundSystems.Reset();

	// Restore game-only input mode when the helm panel is closed so that
	// FInputModeGameAndUI doesn't leak into other gameplay contexts (on-foot,
	// other stations) where the cursor visibility / focus rules differ.
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}

	Super::NativeDestruct();
}

void USubHelmWidget::DiscoverWidgetReferencesFromTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (!SonarDisplay)
	{
		SonarDisplay = FindNamedWidget<USubSonarDisplayWidget>(WidgetTree, TEXT("SonarDisplay"));
	}
	if (!HelmNavigationDisplay)
	{
		HelmNavigationDisplay = FindNamedWidget<UHelmNavigationDisplayWidget>(WidgetTree, TEXT("HelmNavigationDisplay"));
	}
	if (!ReconstructionView)
	{
		ReconstructionView = FindNamedWidget<UReconstructionViewWidget>(WidgetTree, TEXT("ReconstructionView"));
	}
	if (!ForwardReconstructionView)
	{
		ForwardReconstructionView = FindNamedWidget<UReconstructionViewWidget>(WidgetTree, TEXT("ForwardReconstructionView"));
	}
	if (!TacticalGraphView)
	{
		TacticalGraphView = FindNamedWidget<UTacticalGraphViewWidget>(WidgetTree, TEXT("TacticalGraphView"));
	}
	if (!ControlStackPanel)
	{
		ControlStackPanel = FindNamedWidget<UHelmCockpitWidget>(WidgetTree, TEXT("ControlStackPanel"));
	}
	if (!StatusStripPanel)
	{
		StatusStripPanel = FindNamedWidget<UHelmStatusStripWidget>(WidgetTree, TEXT("StatusStripPanel"));
	}

	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (!SonarDisplay)
		{
			SonarDisplay = Cast<USubSonarDisplayWidget>(Widget);
		}
		if (!HelmNavigationDisplay)
		{
			HelmNavigationDisplay = Cast<UHelmNavigationDisplayWidget>(Widget);
		}
		if (!ReconstructionView)
		{
			ReconstructionView = Cast<UReconstructionViewWidget>(Widget);
		}
		if (!ForwardReconstructionView && Widget->GetFName() == FName(TEXT("ForwardReconstructionView")))
		{
			ForwardReconstructionView = Cast<UReconstructionViewWidget>(Widget);
		}
		if (!TacticalGraphView)
		{
			TacticalGraphView = Cast<UTacticalGraphViewWidget>(Widget);
		}
		if (!ControlStackPanel)
		{
			ControlStackPanel = Cast<UHelmCockpitWidget>(Widget);
		}
		if (!StatusStripPanel)
		{
			StatusStripPanel = Cast<UHelmStatusStripWidget>(Widget);
		}
	});
}

void USubHelmWidget::RouteSonarPing()
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		// Local cosmetic prediction: kick off the scan-circle visual + sound on
		// the client BEFORE the server RPC, so input feels instantaneous despite
		// the RPC roundtrip + replication delay in Play-as-Client. Authoritative
		// raycast still runs server-side; points arrive ~50ms later via OnRep.
		if (BoundSonar.IsValid())
		{
			BoundSonar->TriggerLocalCosmeticPing();
		}
		OwnerController->ServerRouteSonarPing();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSonarPing failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSonarPingHeldStart()
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		// First ping of a held burst is fired immediately on the server side
		// (StartContinuousPing → TryFirePing). Mirror the cosmetic kick-off here.
		if (BoundSonar.IsValid())
		{
			BoundSonar->TriggerLocalCosmeticPing();
		}
		OwnerController->ServerSetSonarPingHeld(true);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSonarPingHeldStart failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSonarPingHeldStop()
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerSetSonarPingHeld(false);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSonarPingHeldStop failed: OwnerController unresolved."), *GetName());
}

bool USubHelmWidget::IsSonarDisplayBound() const
{
	return SonarDisplay && BoundSonar.IsValid() && BoundSonarSystem.IsValid();
}

bool USubHelmWidget::IsHelmNavigationDisplayBound() const
{
	return HelmNavigationDisplay && BoundHelmNavigationDisplayComponent.IsValid();
}

bool USubHelmWidget::IsReconstructionViewBound() const
{
	return ReconstructionView && BoundHelmNavigationDisplayComponent.IsValid();
}

bool USubHelmWidget::IsForwardReconstructionViewBound() const
{
	return ForwardReconstructionView && BoundHelmNavigationDisplayComponent.IsValid();
}

bool USubHelmWidget::IsTacticalGraphViewBound() const
{
	return TacticalGraphView && BoundHelmNavigationDisplayComponent.IsValid() && BoundSonarSystem.IsValid();
}

bool USubHelmWidget::IsControlStackPanelBound() const
{
	return ControlStackPanel && ControlStackPanel->IsBoundToHelmShell();
}

bool USubHelmWidget::IsStatusStripPanelBound() const
{
	return StatusStripPanel && StatusStripPanel->IsBoundToHelmShell();
}

ASubmarineBase* USubHelmWidget::ResolveCurrentSubmarineForHelm() const
{
	USubHelmWidget* MutableThis = const_cast<USubHelmWidget*>(this);
	MutableThis->ResolveRuntimeRefs();
	return MutableThis->OwnerController.IsValid() ? MutableThis->OwnerController->GetResolvedCurrentSubmarine() : nullptr;
}

FHelmPerceptionPanelData USubHelmWidget::GetPerceptionPanelData() const
{
	FHelmPerceptionPanelData Data;

	const ASubmarineBase* Submarine = ResolveCurrentSubmarineForHelm();
	const USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	const USubSonarComponent* Sonar = Submarine ? Submarine->Sonar : nullptr;
	if (!SonarSystem || !Sonar)
	{
		return Data;
	}

	Data.bBound = true;
	Data.State = ResolvePerceptionPanelState();
	Data.SonarMode = SonarSystem->CurrentMode;
	Data.DisplayRangeCm = SonarSystem->GetDisplayRangeCm();
	Data.FocusBearingDeg = SonarSystem->FocusBearingDeg;
	Data.bSignalUnstable = SonarSystem->IsSignalUnstable();
	Data.AcousticClutterLevel = SonarSystem->GetAcousticClutterLevel();
	Data.TrackCount = SonarSystem->GetTracks().Num();
	Data.PriorityTrackCount = CountPriorityTracks(SonarSystem->GetTracks());
	Data.bPingReady = SonarSystem->IsPingReady();

	const float LastPingTime = Sonar->GetLastPingTime();
	const UWorld* World = Sonar->GetWorld();
	Data.SecondsSinceLastPing = (LastPingTime > 0.f && World) ? FMath::Max(0.f, World->GetTimeSeconds() - LastPingTime) : -1.f;
	return Data;
}

FHelmGuidancePanelData USubHelmWidget::GetGuidancePanelData() const
{
	FHelmGuidancePanelData Data;

	const ASubmarineBase* Submarine = ResolveCurrentSubmarineForHelm();
	const UHelmNavigationDisplayComponent* Display = Submarine ? Submarine->HelmNavigationDisplay : nullptr;
	if (!Display)
	{
		return Data;
	}

	Data.bBound = true;
	Data.State = ResolveNavigationPanelState();
	Data.InstrumentStatus = Display->GetInstrumentStatus();
	Data.TacticalGraph = Display->GetTacticalGraphViewData();
	Data.StoppingDistanceWarning = Display->GetStoppingDistanceWarning();
	Data.CommitmentWarning = Display->GetCommitmentWarning();
	return Data;
}

FHelmReconstructionPanelData USubHelmWidget::GetReconstructionPanelData() const
{
	FHelmReconstructionPanelData Data;

	const ASubmarineBase* Submarine = ResolveCurrentSubmarineForHelm();
	const UHelmNavigationDisplayComponent* Display = Submarine ? Submarine->HelmNavigationDisplay : nullptr;
	if (!Display)
	{
		return Data;
	}

	Data.bBound = true;
	Data.State = ResolveNavigationPanelState();
	Data.ReconstructionView = Display->GetReconstructionViewData();
	return Data;
}

FHelmControlPanelData USubHelmWidget::GetControlPanelData() const
{
	FHelmControlPanelData Data;

	const ASubmarineBase* Submarine = ResolveCurrentSubmarineForHelm();
	const USubmarineSystemsComponent* Systems = Submarine ? Submarine->Systems : nullptr;
	const USubMovementComponent* Movement = Submarine ? Submarine->SubMovement : nullptr;
	if (!Systems || !Movement)
	{
		return Data;
	}

	Data.bBound = true;
	Data.State = ResolveControlPanelState();
	Data.CommandState = Systems->GetCommandState();
	Data.CurrentForwardSpeedCmS = Movement->ForwardSpeedCmS;
	Data.MaxForwardSpeedCmS = FMath::Max(1.f, Movement->MaxForwardSpeed);
	Data.MaxReverseSpeedCmS = FMath::Max(1.f, Movement->MaxReverseSpeed);
	Data.CurrentSpooledPower = Movement->GetSpooledPower();
	Data.CurrentYawRateDegPerSec = Movement->GetYawRateDegPerSec();
	{
		const float YawWorld = FRotator::NormalizeAxis(Submarine->GetActorRotation().Yaw);
		Data.CurrentHeadingDeg = (YawWorld < 0.f) ? (YawWorld + 360.f) : YawWorld;
	}
	Data.CurrentDepthMeters = Movement->CurrentDepth;
	Data.CurrentPitchDeg = Submarine ? FRotator::NormalizeAxis(Submarine->GetActorRotation().Pitch) : 0.f;
	Data.EffectivePowerInput = Movement->EffectivePowerInput;
	Data.GlobalBallastFill01 = ComputeAverageBallastFill(Movement->Ballasts);
	for (int32 BallastIndex = 0; BallastIndex < Movement->Ballasts.Num(); ++BallastIndex)
	{
		const FBallastTank& Tank = Movement->Ballasts[BallastIndex];
		FHelmBallastTankPanelData TankData;
		TankData.Index = BallastIndex;
		TankData.FillLevel01 = Tank.FillLevel;
		TankData.TargetFill01 = Tank.TargetFill;
		TankData.PumpState = Tank.PumpState;
		Data.BallastTanks.Add(TankData);
	}
	Data.bAutoSpeedActive = Systems->IsAutoSpeedActive();
	Data.bAutoDepthActive = Systems->IsAutoDepthActive();
	Data.bAutoPitchActive = Systems->IsAutoPitchActive();
	return Data;
}

FHelmAlertPanelData USubHelmWidget::GetAlertPanelData() const
{
	FHelmAlertPanelData Data;

	const ASubmarineBase* Submarine = ResolveCurrentSubmarineForHelm();
	const UHelmNavigationDisplayComponent* Display = Submarine ? Submarine->HelmNavigationDisplay : nullptr;
	const USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	const USubMovementComponent* Movement = Submarine ? Submarine->SubMovement : nullptr;
	if (!Submarine || !Movement)
	{
		return Data;
	}

	Data.bBound = true;
	Data.State = Display ? ResolveNavigationPanelState() : ResolveControlPanelState();
	Data.HeadingDeg = FRotator::NormalizeAxis(Submarine->GetActorRotation().Yaw);
	Data.SpeedKmh = FMath::Abs(Movement->ForwardSpeedCmS) * 0.036f;
	Data.DepthMeters = Movement->CurrentDepth;
	Data.PitchDeg = FRotator::NormalizeAxis(Submarine->GetActorRotation().Pitch);
	Data.RollDeg = FRotator::NormalizeAxis(Submarine->GetActorRotation().Roll);
	Data.bSignalUnstable = SonarSystem ? SonarSystem->IsSignalUnstable() : false;
	Data.bNavigationDataValid = Display ? Display->HasValidNavigationData() : false;
	Data.StoppingDistanceWarning = Display ? Display->GetStoppingDistanceWarning() : FTunnelNavStoppingDistanceWarning();
	Data.CommitmentWarning = Display ? Display->GetCommitmentWarning() : FTunnelNavCommitmentWarning();
	Data.InstrumentStatus = Display ? Display->GetInstrumentStatus() : FHelmInstrumentStatus();

	// Critical alarm aggregation. SubFlood owns both: the per-compartment
	// flood level (for the "FLOODING xx%" tier) and the breach list (for
	// the "HULL BREACH" tier). Both fields stay 0 / 0.0 if SubFlood isn't
	// initialised yet, which is the right idle state.
	if (Submarine->SubFlood && Submarine->SubFlood->IsInitialized())
	{
		const TArray<FCompartmentBreachState>& Breaches = Submarine->SubFlood->GetBreaches();
		int32 ActiveBreaches = 0;
		for (const FCompartmentBreachState& Breach : Breaches)
		{
			if (Breach.bBreached)
			{
				++ActiveBreaches;
			}
		}
		Data.ActiveBreachCount = ActiveBreaches;

		TArray<FCompartmentState> States;
		Submarine->SubFlood->ExportCompartmentStates(States);
		float MaxFlood = 0.f;
		for (const FCompartmentState& State : States)
		{
			MaxFlood = FMath::Max(MaxFlood, State.FloodLevel01);
		}
		Data.MaxCompartmentFloodFraction = MaxFlood;
	}

	return Data;
}

void USubHelmWidget::RouteSetSonarMode(ESonarMode NewMode)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->SetSonarMode(NewMode);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetSonarMode failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetSonarFocusBearing(float BearingDeg)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->SetSonarFocusBearing(BearingDeg);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetSonarFocusBearing failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetSonarRangePreset(int32 PresetIndex)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->SetSonarRangePreset(PresetIndex);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetSonarRangePreset failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteIncreaseSonarRangePreset()
{
	RouteAdjustSonarRangePreset(+1);
}

void USubHelmWidget::RouteDecreaseSonarRangePreset()
{
	RouteAdjustSonarRangePreset(-1);
}

void USubHelmWidget::RouteAdjustSonarRangePreset(int32 Delta)
{
	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] RouteAdjustSonarRangePreset failed: OwnerController unresolved."), *GetName());
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	if (!SonarSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] RouteAdjustSonarRangePreset failed: SonarSystem unresolved."), *GetName());
		return;
	}

	const int32 CurrentIndex = SonarSystem->GetRangePresetIndex();
	const int32 MaxIndex = FMath::Max(0, SonarSystem->GetRangePresetCount() - 1);
	const int32 NewIndex = FMath::Clamp(CurrentIndex + Delta, 0, MaxIndex);
	OwnerController->SetSonarRangePreset(NewIndex);
}

void USubHelmWidget::RouteSetSonarRangeNormalized(float Normalized01)
{
	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetSonarRangeNormalized failed: OwnerController unresolved."), *GetName());
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	if (!SonarSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetSonarRangeNormalized failed: SonarSystem unresolved."), *GetName());
		return;
	}

	const int32 NewIndex = SonarSystem->ResolveRangePresetIndexFromNormalized(Normalized01);
	OwnerController->SetSonarRangePreset(NewIndex);
}

int32 USubHelmWidget::GetCurrentSonarRangePresetIndex() const
{
	const_cast<USubHelmWidget*>(this)->ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return 0;
	}

	const ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	const USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	return SonarSystem ? SonarSystem->GetRangePresetIndex() : 0;
}

int32 USubHelmWidget::GetCurrentSonarRangePresetCount() const
{
	const_cast<USubHelmWidget*>(this)->ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return 1;
	}

	const ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	const USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	return SonarSystem ? SonarSystem->GetRangePresetCount() : 1;
}

float USubHelmWidget::GetCurrentSonarDisplayRangeCm() const
{
	const_cast<USubHelmWidget*>(this)->ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return 0.f;
	}

	const ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	const USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	return SonarSystem ? SonarSystem->GetDisplayRangeCm() : 0.f;
}

void USubHelmWidget::RouteMarkPriorityTrack(int32 TrackId, bool bPriority)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->MarkSonarPriorityTrack(TrackId, bPriority);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteMarkPriorityTrack failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetHelmThrottle(float Value)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteHelmThrust(Value);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetHelmThrottle failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetHelmSteer(float Value)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteHelmSteer(Value);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetHelmSteer failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetHelmTrim(float Value)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteHelmDive(Value);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetHelmTrim failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetRudderHoldEnabled(bool bEnabled)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteRudderHoldEnabled(bEnabled);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetRudderHoldEnabled failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetPlaneHoldEnabled(bool bEnabled)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRoutePlaneHoldEnabled(bEnabled);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetPlaneHoldEnabled failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetGlobalBallast(float Target01)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteBallastGlobal(Target01);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetGlobalBallast failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetBallastsActive(bool bActive)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteBallastActive(bActive);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetBallastsActive failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetBallastByIndex(int32 TankIndex, float Target01)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteBallastByIndex(TankIndex, Target01);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetBallastByIndex failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetPumpActive(bool bActive)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRoutePumpActive(bActive);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetPumpActive failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetPumpPower(float Value01)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRoutePumpPower(Value01);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetPumpPower failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetStabilizationMasterEnabled(bool bEnabled)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteStabilizationMaster(bEnabled);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetStabilizationMasterEnabled failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetAutoSpeedEnabled(bool bEnabled)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteAutoSpeedEnabled(bEnabled);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetAutoSpeedEnabled failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetAutoDepthEnabled(bool bEnabled)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteAutoDepthEnabled(bEnabled);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetAutoDepthEnabled failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetAutoPitchEnabled(bool bEnabled)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteAutoPitchEnabled(bEnabled);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetAutoPitchEnabled failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetTargetSpeedCmS(float SpeedCmS)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteTargetSpeedCmS(SpeedCmS);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetTargetSpeedCmS failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetTargetDepthMeters(float DepthMeters)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteTargetDepthMeters(DepthMeters);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetTargetDepthMeters failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::RouteSetTargetPitchDeg(float PitchDeg)
{
	ResolveRuntimeRefs();
	if (OwnerController.IsValid())
	{
		OwnerController->ServerRouteTargetPitchDeg(PitchDeg);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] RouteSetTargetPitchDeg failed: OwnerController unresolved."), *GetName());
}

void USubHelmWidget::TryBindSonarDisplay()
{
	if (!SonarDisplay)
	{
		SonarDisplay = FindNamedWidget<USubSonarDisplayWidget>(WidgetTree, TEXT("SonarDisplay"));
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!SonarDisplay)
				{
					SonarDisplay = Cast<USubSonarDisplayWidget>(Widget);
				}
			});
		}

		if (bAutoCreateSonarDisplayIfMissing)
		{
			APlayerController* PC = GetOwningPlayer();
			if (PC)
			{
				UClass* DisplayClass = SonarDisplayClass ? SonarDisplayClass.Get() : USubSonarDisplayWidget::StaticClass();
				USubSonarDisplayWidget* CreatedDisplay = CreateWidget<USubSonarDisplayWidget>(PC, DisplayClass);
				if (CreatedDisplay)
				{
					CreatedDisplay->AddToViewport(AutoCreatedSonarDisplayZOrder);
					SonarDisplay = CreatedDisplay;
					bOwnsAutoCreatedSonarDisplay = true;
				}
			}
		}

		if (!SonarDisplay)
		{
			if (!bLoggedMissingSonarDisplay)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] TryBindSonarDisplay: no USubSonarDisplayWidget found in widget tree and auto-create disabled."), *GetName());
				bLoggedMissingSonarDisplay = true;
			}
			return;
		}
	}

	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	if (!Submarine || !Submarine->Sonar || !Submarine->SonarSystem)
	{
		return;
	}

	if (BoundSonar.Get() == Submarine->Sonar && BoundSonarSystem.Get() == Submarine->SonarSystem)
	{
		return;
	}

	SonarDisplay->InitForSonarSources(Submarine->Sonar, Submarine->SonarSystem);
	BoundSonar = Submarine->Sonar;
	BoundSonarSystem = Submarine->SonarSystem;
	BoundSystems = Submarine->Systems;
}

void USubHelmWidget::TryBindHelmNavigationDisplay()
{
	if (!HelmNavigationDisplay)
	{
		HelmNavigationDisplay = FindNamedWidget<UHelmNavigationDisplayWidget>(WidgetTree, TEXT("HelmNavigationDisplay"));
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!HelmNavigationDisplay)
				{
					HelmNavigationDisplay = Cast<UHelmNavigationDisplayWidget>(Widget);
				}
			});
		}

		if (bAutoCreateHelmNavigationDisplayIfMissing)
		{
			APlayerController* PC = GetOwningPlayer();
			if (PC)
			{
				UClass* DisplayClass = HelmNavigationDisplayClass ? HelmNavigationDisplayClass.Get() : UHelmNavigationDisplayWidget::StaticClass();
				UHelmNavigationDisplayWidget* CreatedDisplay = CreateWidget<UHelmNavigationDisplayWidget>(PC, DisplayClass);
				if (CreatedDisplay)
				{
					CreatedDisplay->AddToViewport(AutoCreatedHelmNavigationDisplayZOrder);
					HelmNavigationDisplay = CreatedDisplay;
					bOwnsAutoCreatedHelmNavigationDisplay = true;
				}
			}
		}

		if (!HelmNavigationDisplay)
		{
			if (!bLoggedMissingHelmNavigationDisplay)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] TryBindHelmNavigationDisplay: no UHelmNavigationDisplayWidget found in widget tree and auto-create disabled."), *GetName());
				bLoggedMissingHelmNavigationDisplay = true;
			}
			return;
		}
	}

	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	if (!Submarine || !Submarine->HelmNavigationDisplay)
	{
		return;
	}

	if (BoundHelmNavigationDisplayComponent.Get() == Submarine->HelmNavigationDisplay &&
		HelmNavigationDisplay->IsNavigationDisplayBound())
	{
		return;
	}

	HelmNavigationDisplay->InitForNavigationDisplay(Submarine->HelmNavigationDisplay);
	BoundHelmNavigationDisplayComponent = Submarine->HelmNavigationDisplay;
	BoundSystems = Submarine->Systems;
}

void USubHelmWidget::TryBindReconstructionView()
{
	if (!ReconstructionView)
	{
		ReconstructionView = FindNamedWidget<UReconstructionViewWidget>(WidgetTree, TEXT("ReconstructionView"));
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!ReconstructionView)
				{
					ReconstructionView = Cast<UReconstructionViewWidget>(Widget);
				}
			});
		}

		if (!ReconstructionView)
		{
			if (!bLoggedMissingReconstructionView)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] TryBindReconstructionView: no UReconstructionViewWidget found in widget tree."), *GetName());
				bLoggedMissingReconstructionView = true;
			}
			return;
		}
	}

	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	if (!Submarine || !Submarine->HelmNavigationDisplay)
	{
		return;
	}

	if (BoundHelmNavigationDisplayComponent.Get() == Submarine->HelmNavigationDisplay && ReconstructionView->IsReconstructionViewBound())
	{
		ReconstructionView->SetViewMode(EReconstructionViewMode::CrossSection);
		return;
	}

	ReconstructionView->InitForReconstructionView(Submarine->HelmNavigationDisplay);
	ReconstructionView->SetViewMode(EReconstructionViewMode::CrossSection);
	BoundHelmNavigationDisplayComponent = Submarine->HelmNavigationDisplay;
	BoundSystems = Submarine->Systems;
}

void USubHelmWidget::TryBindForwardReconstructionView()
{
	if (!ForwardReconstructionView)
	{
		ForwardReconstructionView = FindNamedWidget<UReconstructionViewWidget>(WidgetTree, TEXT("ForwardReconstructionView"));
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!ForwardReconstructionView && Widget->GetFName() == FName(TEXT("ForwardReconstructionView")))
				{
					ForwardReconstructionView = Cast<UReconstructionViewWidget>(Widget);
				}
			});
		}

		if (!ForwardReconstructionView)
		{
			if (!bLoggedMissingForwardReconstructionView)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] TryBindForwardReconstructionView: no UReconstructionViewWidget named ForwardReconstructionView found in widget tree."), *GetName());
				bLoggedMissingForwardReconstructionView = true;
			}
			return;
		}
	}

	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	if (!Submarine || !Submarine->HelmNavigationDisplay)
	{
		return;
	}

	if (BoundHelmNavigationDisplayComponent.Get() == Submarine->HelmNavigationDisplay && ForwardReconstructionView->IsReconstructionViewBound())
	{
		ForwardReconstructionView->SetViewMode(EReconstructionViewMode::ForwardProfile);
		return;
	}

	ForwardReconstructionView->InitForReconstructionView(Submarine->HelmNavigationDisplay);
	ForwardReconstructionView->SetViewMode(EReconstructionViewMode::ForwardProfile);
	BoundHelmNavigationDisplayComponent = Submarine->HelmNavigationDisplay;
	BoundSystems = Submarine->Systems;
}

void USubHelmWidget::TryBindTacticalGraphView()
{
	if (!TacticalGraphView)
	{
		TacticalGraphView = FindNamedWidget<UTacticalGraphViewWidget>(WidgetTree, TEXT("TacticalGraphView"));
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!TacticalGraphView)
				{
					TacticalGraphView = Cast<UTacticalGraphViewWidget>(Widget);
				}
			});
		}

		if (!TacticalGraphView)
		{
			if (!bLoggedMissingTacticalGraphView)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] TryBindTacticalGraphView: no UTacticalGraphViewWidget found in widget tree."), *GetName());
				bLoggedMissingTacticalGraphView = true;
			}
			return;
		}
	}

	ResolveRuntimeRefs();
	if (!OwnerController.IsValid())
	{
		return;
	}

	ASubmarineBase* Submarine = OwnerController->GetResolvedCurrentSubmarine();
	if (!Submarine || !Submarine->HelmNavigationDisplay || !Submarine->SonarSystem)
	{
		return;
	}

	if (BoundHelmNavigationDisplayComponent.Get() == Submarine->HelmNavigationDisplay &&
		BoundSonarSystem.Get() == Submarine->SonarSystem &&
		TacticalGraphView->IsTacticalGraphViewBound())
	{
		return;
	}

	TacticalGraphView->InitForTacticalGraphSources(Submarine->HelmNavigationDisplay, Submarine->SonarSystem);
	BoundHelmNavigationDisplayComponent = Submarine->HelmNavigationDisplay;
	BoundSonarSystem = Submarine->SonarSystem;
	BoundSystems = Submarine->Systems;
}

void USubHelmWidget::TryBindControlStackPanel()
{
	if (!ControlStackPanel)
	{
		ControlStackPanel = FindNamedWidget<UHelmCockpitWidget>(WidgetTree, TEXT("ControlStackPanel"));
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!ControlStackPanel)
				{
					ControlStackPanel = Cast<UHelmCockpitWidget>(Widget);
				}
			});
		}

		if (!ControlStackPanel && bAutoCreateControlStackPanelIfMissing)
		{
			if (UCanvasPanel* RootCanvas = ResolveRootCanvasPanel())
			{
				UClass* PanelClass = ControlStackPanelClass ? ControlStackPanelClass.Get() : UHelmCockpitWidget::StaticClass();
				ControlStackPanel = WidgetTree->ConstructWidget<UHelmCockpitWidget>(PanelClass, TEXT("ControlStackPanel"));
				if (ControlStackPanel)
				{
					RootCanvas->AddChild(ControlStackPanel);
					bOwnsAutoCreatedControlStackPanel = true;
				}
			}
		}

		if (!ControlStackPanel)
		{
			if (!bLoggedMissingControlStackPanel)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] TryBindControlStackPanel: expected widget named ControlStackPanel of class UHelmCockpitWidget in WBP_SubHelm."), *GetName());
				bLoggedMissingControlStackPanel = true;
			}
			return;
		}
	}

	ControlStackPanel->InitForHelmShell(this);
}

void USubHelmWidget::TryBindStatusStripPanel()
{
	if (!StatusStripPanel)
	{
		StatusStripPanel = FindNamedWidget<UHelmStatusStripWidget>(WidgetTree, TEXT("StatusStripPanel"));
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (!StatusStripPanel)
				{
					StatusStripPanel = Cast<UHelmStatusStripWidget>(Widget);
				}
			});
		}

		if (!StatusStripPanel && bAutoCreateStatusStripPanelIfMissing)
		{
			if (UCanvasPanel* RootCanvas = ResolveRootCanvasPanel())
			{
				UClass* PanelClass = StatusStripPanelClass ? StatusStripPanelClass.Get() : UHelmStatusStripWidget::StaticClass();
				StatusStripPanel = WidgetTree->ConstructWidget<UHelmStatusStripWidget>(PanelClass, TEXT("StatusStripPanel"));
				if (StatusStripPanel)
				{
					RootCanvas->AddChild(StatusStripPanel);
					bOwnsAutoCreatedStatusStripPanel = true;
				}
			}
		}

		if (!StatusStripPanel)
		{
			if (!bLoggedMissingStatusStripPanel)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] TryBindStatusStripPanel: expected widget named StatusStripPanel of class UHelmStatusStripWidget in WBP_SubHelm."), *GetName());
				bLoggedMissingStatusStripPanel = true;
			}
			return;
		}
	}

	StatusStripPanel->InitForHelmShell(this);
}

UCanvasPanel* USubHelmWidget::ResolveRootCanvasPanel() const
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
	{
		return RootCanvas;
	}

	UCanvasPanel* FoundCanvas = nullptr;
	WidgetTree->ForEachWidget([&FoundCanvas](UWidget* Widget)
	{
		if (!FoundCanvas)
		{
			FoundCanvas = Cast<UCanvasPanel>(Widget);
		}
	});

	return FoundCanvas;
}

void USubHelmWidget::UpdateDockedPanelLayout()
{
	if (!bDockInstrumentPanels)
	{
		return;
	}

	const FVector2D RootSize = GetCachedGeometry().GetLocalSize();
	if (RootSize.X < 900.f || RootSize.Y < 500.f)
	{
		return;
	}

	const auto SetCanvasRect = [](UWidget* Widget, const FVector2D& Position, const FVector2D& Size)
	{
		if (!Widget)
		{
			return;
		}

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetPosition(Position);
			CanvasSlot->SetSize(Size);
		}
	};

	const float Margin = 20.f;
	const float BottomStripHeight = 60.f;
	const float LeftWidth = FMath::Clamp(RootSize.X * 0.26f, 320.f, 460.f);
	const float RightWidth = FMath::Clamp(RootSize.X * 0.24f, 320.f, 420.f);
	const float CenterWidth = FMath::Max(360.f, RootSize.X - LeftWidth - RightWidth - (Margin * 4.f));
	const float AvailableHeight = RootSize.Y - BottomStripHeight - (Margin * 3.f);
	const bool bHasSplitReconstruction = ForwardReconstructionView != nullptr;
	const float TacticalHeight = bHasSplitReconstruction
		? FMath::Clamp(AvailableHeight * 0.30f, 180.f, 240.f)
		: FMath::Clamp(AvailableHeight * 0.44f, 210.f, 320.f);
	const float RemainingLeftHeight = FMath::Max(180.f, AvailableHeight - TacticalHeight - Margin);
	const float ReconstructionHeight = bHasSplitReconstruction
		? FMath::Max(140.f, (RemainingLeftHeight - Margin) * 0.5f)
		: RemainingLeftHeight;
	const float ForwardHeight = bHasSplitReconstruction
		? FMath::Max(140.f, RemainingLeftHeight - ReconstructionHeight - Margin)
		: 0.f;

	SetCanvasRect(TacticalGraphView, FVector2D(Margin, Margin), FVector2D(LeftWidth, TacticalHeight));
	SetCanvasRect(ReconstructionView, FVector2D(Margin, Margin + TacticalHeight + Margin), FVector2D(LeftWidth, ReconstructionHeight));
	SetCanvasRect(ForwardReconstructionView, FVector2D(Margin, Margin + TacticalHeight + Margin + ReconstructionHeight + Margin), FVector2D(LeftWidth, ForwardHeight));
	SetCanvasRect(SonarDisplay, FVector2D(Margin * 2.f + LeftWidth, Margin), FVector2D(CenterWidth, AvailableHeight));
	SetCanvasRect(ControlStackPanel, FVector2D(RootSize.X - RightWidth - Margin, Margin), FVector2D(RightWidth, AvailableHeight));
	SetCanvasRect(StatusStripPanel, FVector2D(Margin, RootSize.Y - BottomStripHeight - Margin), FVector2D(RootSize.X - (Margin * 2.f), BottomStripHeight));

	if (ReconstructionView)
	{
		ReconstructionView->SetRenderTranslation(FVector2D::ZeroVector);
	}
	if (ForwardReconstructionView)
	{
		ForwardReconstructionView->SetRenderTranslation(FVector2D::ZeroVector);
	}
	if (TacticalGraphView)
	{
		TacticalGraphView->SetRenderTranslation(FVector2D::ZeroVector);
	}
}

void USubHelmWidget::ApplyDockedInstrumentStyle()
{
	if (!bDockInstrumentPanels)
	{
		return;
	}

	if (ReconstructionView)
	{
		ReconstructionView->bDraggable = false;
		ReconstructionView->bCollapsible = false;
		ReconstructionView->TitleBarHeightPx = 22.f;
		ReconstructionView->ContentPaddingPx = 10.f;
		ReconstructionView->SetViewMode(EReconstructionViewMode::CrossSection);
		ReconstructionView->WindowBackgroundColor = FLinearColor(0.015f, 0.028f, 0.040f, 0.92f);
		ReconstructionView->TitleBarColor = FLinearColor(0.025f, 0.050f, 0.070f, 0.96f);
		ReconstructionView->FrameColor = FLinearColor(0.18f, 0.44f, 0.48f, 0.88f);
		ReconstructionView->AccentColor = FLinearColor(0.39f, 0.94f, 0.88f, 1.f);
		ReconstructionView->WarningColor = FLinearColor(1.f, 0.56f, 0.24f, 1.f);
		ReconstructionView->VelocityColor = FLinearColor(0.98f, 0.82f, 0.24f, 1.f);
		ReconstructionView->SafeColor = FLinearColor(0.48f, 0.86f, 0.95f, 1.f);
	}

	if (ForwardReconstructionView)
	{
		ForwardReconstructionView->bDraggable = false;
		ForwardReconstructionView->bCollapsible = false;
		ForwardReconstructionView->TitleBarHeightPx = 22.f;
		ForwardReconstructionView->ContentPaddingPx = 10.f;
		ForwardReconstructionView->SetViewMode(EReconstructionViewMode::ForwardProfile);
		ForwardReconstructionView->WindowBackgroundColor = FLinearColor(0.015f, 0.028f, 0.040f, 0.92f);
		ForwardReconstructionView->TitleBarColor = FLinearColor(0.025f, 0.050f, 0.070f, 0.96f);
		ForwardReconstructionView->FrameColor = FLinearColor(0.18f, 0.44f, 0.48f, 0.88f);
		ForwardReconstructionView->AccentColor = FLinearColor(0.39f, 0.94f, 0.88f, 1.f);
		ForwardReconstructionView->WarningColor = FLinearColor(1.f, 0.56f, 0.24f, 1.f);
		ForwardReconstructionView->VelocityColor = FLinearColor(0.98f, 0.82f, 0.24f, 1.f);
		ForwardReconstructionView->SafeColor = FLinearColor(0.48f, 0.86f, 0.95f, 1.f);
	}

	if (TacticalGraphView)
	{
		TacticalGraphView->bDraggable = false;
		TacticalGraphView->bCollapsible = false;
		TacticalGraphView->TitleBarHeightPx = 22.f;
		TacticalGraphView->ContentPaddingPx = 10.f;
		TacticalGraphView->SetWindowTitle(FText::FromString(TEXT("LOCAL GUIDANCE")));
		TacticalGraphView->WindowBackgroundColor = FLinearColor(0.015f, 0.028f, 0.040f, 0.92f);
		TacticalGraphView->TitleBarColor = FLinearColor(0.025f, 0.050f, 0.070f, 0.96f);
		TacticalGraphView->FrameColor = FLinearColor(0.18f, 0.44f, 0.48f, 0.88f);
		TacticalGraphView->AccentColor = FLinearColor(0.39f, 0.94f, 0.88f, 1.f);
		TacticalGraphView->WarningColor = FLinearColor(1.f, 0.56f, 0.24f, 1.f);
		TacticalGraphView->SafeColor = FLinearColor(0.48f, 0.86f, 0.95f, 1.f);
		TacticalGraphView->PriorityColor = FLinearColor(0.98f, 0.82f, 0.24f, 1.f);
	}

	if (HelmNavigationDisplay && bHideLegacyHelmNavigationDisplayWhenSplitPanelsAvailable &&
		(ReconstructionView || ForwardReconstructionView || TacticalGraphView))
	{
		HelmNavigationDisplay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

EHelmPanelRuntimeState USubHelmWidget::ResolvePerceptionPanelState() const
{
	const ASubmarineBase* Submarine = ResolveCurrentSubmarineForHelm();
	const USubSonarSystemComponent* SonarSystem = Submarine ? Submarine->SonarSystem : nullptr;
	const USubSonarComponent* Sonar = Submarine ? Submarine->Sonar : nullptr;
	if (!SonarSystem || !Sonar)
	{
		return EHelmPanelRuntimeState::Unbound;
	}

	if (SonarSystem->GetDisplayRangeCm() <= 0.f)
	{
		return EHelmPanelRuntimeState::Warming;
	}

	return SonarSystem->IsSignalUnstable() ? EHelmPanelRuntimeState::Degraded : EHelmPanelRuntimeState::Valid;
}

EHelmPanelRuntimeState USubHelmWidget::ResolveNavigationPanelState() const
{
	const ASubmarineBase* Submarine = ResolveCurrentSubmarineForHelm();
	const UHelmNavigationDisplayComponent* Display = Submarine ? Submarine->HelmNavigationDisplay : nullptr;
	if (!Display)
	{
		return EHelmPanelRuntimeState::Unbound;
	}

	const FHelmInstrumentStatus Status = Display->GetInstrumentStatus();
	if (!Status.bBound || !Status.bRuntimeReady)
	{
		return EHelmPanelRuntimeState::Warming;
	}

	if (!Display->HasValidNavigationData() || Status.bProjectionSuspect || Status.bStale)
	{
		return EHelmPanelRuntimeState::Degraded;
	}

	return EHelmPanelRuntimeState::Valid;
}

EHelmPanelRuntimeState USubHelmWidget::ResolveControlPanelState() const
{
	const ASubmarineBase* Submarine = ResolveCurrentSubmarineForHelm();
	if (!Submarine || !Submarine->Systems || !Submarine->SubMovement)
	{
		return EHelmPanelRuntimeState::Unbound;
	}

	return EHelmPanelRuntimeState::Valid;
}
