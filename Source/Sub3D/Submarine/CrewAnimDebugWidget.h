#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CrewAnimDebugWidget.generated.h"

class USlider;
class UCheckBox;
class UTextBlock;
class UVerticalBox;
class USubCrewAnimInstance;

/**
 * Runtime debug panel to tweak all procedural animation parameters.
 * Inherits draggable/collapsible window from DraggableInstrumentWindowWidget.
 * Toggle: console command ToggleAnimDebug
 */
UCLASS()
class SUB3D_API UCrewAnimDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCrewAnimDebugWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	struct FSliderRow
	{
		UTextBlock* Label = nullptr;
		USlider* Slider = nullptr;
		UTextBlock* ValueText = nullptr;
		float MinVal = 0.f;
		float MaxVal = 1.f;
	};

private:
	USubCrewAnimInstance* GetAnimInstance() const;
	void BuildWidgetTree();
	FSliderRow AddSliderRow(UVerticalBox* Parent, const FText& LabelText, float Min, float Max, float Current);
	void AddCheckboxRow(UVerticalBox* Parent, const FText& LabelText, bool bCurrent, int32 ParamIndex);
	void AddSectionHeader(UVerticalBox* Parent, const FText& HeaderText);

	// Slider callbacks bound by index
	UFUNCTION() void OnSliderChanged_LegSwingDeg(float Value);
	UFUNCTION() void OnSliderChanged_ArmSwingDeg(float Value);
	UFUNCTION() void OnSliderChanged_PelvisBob(float Value);
	UFUNCTION() void OnSliderChanged_CycleRate(float Value);
	UFUNCTION() void OnSliderChanged_CalfBend(float Value);
	UFUNCTION() void OnSliderChanged_ArmRest(float Value);
	UFUNCTION() void OnSliderChanged_ForearmRest(float Value);
	UFUNCTION() void OnSliderChanged_BreathAmp(float Value);
	UFUNCTION() void OnSliderChanged_BreathRate(float Value);
	UFUNCTION() void OnSliderChanged_PostureBend(float Value);
	UFUNCTION() void OnSliderChanged_SubLean(float Value);
	UFUNCTION() void OnSliderChanged_SubStumble(float Value);
	UFUNCTION() void OnSliderChanged_LegAxis(float Value);
	UFUNCTION() void OnSliderChanged_ArmAxis(float Value);
	UFUNCTION() void OnSliderChanged_ArmRestAxis(float Value);
	UFUNCTION() void OnSliderChanged_SpineBendAxis(float Value);
	UFUNCTION() void OnSliderChanged_SpineTwistAxis(float Value);

	UFUNCTION() void OnCheckChanged_NegateLeg(bool bChecked);
	UFUNCTION() void OnCheckChanged_NegateArm(bool bChecked);
	UFUNCTION() void OnCheckChanged_NegateSpine(bool bChecked);

	// Cached widget refs
	UPROPERTY(Transient) UVerticalBox* RootBox = nullptr;

	// Slider rows for live value display
	FSliderRow SR_LegSwing, SR_ArmSwing, SR_PelvisBob, SR_CycleRate, SR_CalfBend;
	FSliderRow SR_ArmRest, SR_ForearmRest;
	FSliderRow SR_BreathAmp, SR_BreathRate;
	FSliderRow SR_PostureBend;
	FSliderRow SR_SubLean, SR_SubStumble;
	FSliderRow SR_LegAxis, SR_ArmAxis, SR_ArmRestAxis, SR_SpineBendAxis, SR_SpineTwistAxis;

	// Debug readout
	UPROPERTY(Transient) UTextBlock* DebugText = nullptr;
};
