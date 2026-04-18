#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HelmControlPanelWidget.generated.h"

class UBorder;
class UCheckBox;
class UHorizontalBox;
class UOverlay;
class UProgressBar;
class USlider;
class UTextBlock;
class UVerticalBox;
class USubHelmWidget;

UCLASS(Blueprintable)
class SUB3D_API UHelmControlPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Helm")
	void InitForHelmShell(USubHelmWidget* InHelmShell);

	UFUNCTION(BlueprintPure, Category = "Helm")
	bool IsBoundToHelmShell() const;

private:
	void BuildWidgetTree();
	void RefreshFromHelmData();
	void TryResolveHelmShellFromOuter();
	void RebuildBallastRows(int32 Count);
	void SnapSliderToNeutralIfNear(USlider* Slider);
	static float AxisToSlider(float Value);
	static float SliderToAxis(float Value);

	UFUNCTION()
	void HandleThrottleChanged(float Value);

	UFUNCTION()
	void HandleRudderChanged(float Value);

	UFUNCTION()
	void HandleTrimChanged(float Value);

	UFUNCTION()
	void HandleBallastChanged(float Value);

	UFUNCTION()
	void HandlePumpPowerChanged(float Value);

	UFUNCTION()
	void HandleThrottleCaptureEnded();

	UFUNCTION()
	void HandleRudderCaptureEnded();

	UFUNCTION()
	void HandleTrimCaptureEnded();

	UFUNCTION()
	void HandleRudderHoldChanged(bool bChecked);

	UFUNCTION()
	void HandlePlaneHoldChanged(bool bChecked);

	UFUNCTION()
	void HandleStabilizationMasterChanged(bool bChecked);

	UFUNCTION()
	void HandleAutoSpeedChanged(bool bChecked);

	UFUNCTION()
	void HandleAutoDepthChanged(bool bChecked);

	UFUNCTION()
	void HandleAutoPitchChanged(bool bChecked);

	UFUNCTION()
	void HandleBallastsActiveChanged(bool bChecked);

	UFUNCTION()
	void HandlePumpActiveChanged(bool bChecked);

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<USubHelmWidget> CachedHelmShell;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StateText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MotionSummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AutoSummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ThrottleValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RudderValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrimValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BallastValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PumpValueText;

	UPROPERTY(Transient)
	TObjectPtr<USlider> ThrottleSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> RudderSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> TrimSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> BallastSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> PumpPowerSlider;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BallastGaugeBox;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> StabilizationMasterCheck;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> AutoSpeedCheck;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> AutoDepthCheck;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> AutoPitchCheck;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> BallastsActiveCheck;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> PumpActiveCheck;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> RudderHoldCheck;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> PlaneHoldCheck;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BallastLabelTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> BallastFillBars;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BallastValueTexts;

	bool bRefreshingFromRuntime = false;
};
