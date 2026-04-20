#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HelmCockpitState.h"
#include "HelmThrottleTelegraphWidget.generated.h"

class UButton;
class UTextBlock;

// Throttle telegraph: 7 discrete preset sectors (FULL REV..FULL AHEAD)
// drawn as a half-dial with a current-speed needle, plus HOLD CRUISE
// and STOP mode buttons. NativePaint draws the dial; UButtons handle
// preset / mode clicks.
//
// Reads (from FHelmControlPanelData):
//   CurrentForwardSpeedCmS, MaxForwardSpeedCmS, MaxReverseSpeedCmS,
//   CurrentSpooledPower, CommandState.TargetSpeedCmS,
//   CommandState.bAutoSpeedEnabled, bAutoSpeedActive
//
// Writes (via USubHelmWidget):
//   RouteSetTargetSpeedCmS, RouteSetAutoSpeedEnabled
UCLASS(Blueprintable)
class SUB3D_API UHelmThrottleTelegraphWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	// One UFUNCTION per mode button (UButton::OnClicked is parameter-less).
	UFUNCTION() void HandleHoldCruise();
	UFUNCTION() void HandleStop();

	void BuildWidgetTree();
	void RefreshFromHelmData();

	// Apply a preset by its index in GetTelegraphPresets() (defined in cpp).
	void ApplyPreset(int32 PresetIndex);

	// Hit-test a local widget position against the dial sectors.
	// Returns INDEX_NONE if outside the dial.
	int32 PresetIndexAtLocalPosition(const FVector2D& LocalPos, const FGeometry& Geom) const;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HoldCruiseButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StopButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	// Cached display state, smoothed in NativeTick for visual stability.
	float DisplayedSpeedCmS = 0.f;
	float DisplayedSpooledPower = 0.f;
	float DisplayedTargetSpeedCmS = 0.f;
	float CurrentMaxForwardSpeed = 650.f;
	float CurrentMaxReverseSpeed = 250.f;
	bool bAutoSpeedActiveCached = false;
	bool bAutoSpeedEnabledCached = false;
	int32 ActivePresetIndex = INDEX_NONE;
};
