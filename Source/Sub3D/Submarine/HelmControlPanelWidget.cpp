#include "HelmControlPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "SubHelmWidget.h"

namespace
{
constexpr float SliderNeutralValue = 0.5f;
constexpr float SliderNeutralSnapRadius = 0.03f;

FSlateFontInfo MakeFont(const int32 Size, const bool bBold)
{
	return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
}

UTextBlock* MakeText(UWidgetTree* WidgetTree, const FName Name, const FString& Value, const int32 Size, const FLinearColor& Color, const bool bBold)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Text->SetText(FText::FromString(Value));
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetFont(MakeFont(Size, bBold));
	return Text;
}

UTextBlock* AddSectionLabel(UWidgetTree* WidgetTree, UVerticalBox* Parent, const FName Name, const FString& Value)
{
	UTextBlock* Text = MakeText(WidgetTree, Name, Value, 10, FLinearColor(0.63f, 0.88f, 0.88f, 0.92f), true);
	if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Text))
	{
		Slot->SetPadding(FMargin(0.f, 8.f, 0.f, 2.f));
	}
	return Text;
}

UHorizontalBox* AddAxisSliderRow(
	UWidgetTree* WidgetTree,
	UVerticalBox* Parent,
	const FName Name,
	const FString& Label,
	const FString& NegativeHint,
	const FString& PositiveHint,
	TObjectPtr<UTextBlock>& OutValueText,
	TObjectPtr<USlider>& OutSlider)
{
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("%s_HeaderRow"), *Name.ToString())));
	if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(HeaderRow))
	{
		Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
	}

	UTextBlock* LabelText = MakeText(WidgetTree, FName(*FString::Printf(TEXT("%s_Label"), *Name.ToString())), Label, 9, FLinearColor(0.84f, 0.92f, 0.95f, 0.88f), false);
	if (UHorizontalBoxSlot* LabelSlot = HeaderRow->AddChildToHorizontalBox(LabelText))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Left);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName(*FString::Printf(TEXT("%s_Spacer"), *Name.ToString())));
	if (UHorizontalBoxSlot* SpacerSlot = HeaderRow->AddChildToHorizontalBox(Spacer))
	{
		SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	OutValueText = MakeText(WidgetTree, FName(*FString::Printf(TEXT("%s_Value"), *Name.ToString())), TEXT("--"), 9, FLinearColor(0.89f, 0.96f, 0.98f, 1.f), true);
	if (UHorizontalBoxSlot* ValueSlot = HeaderRow->AddChildToHorizontalBox(OutValueText))
	{
		ValueSlot->SetHorizontalAlignment(HAlign_Right);
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}

	UHorizontalBox* ControlRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("%s_ControlRow"), *Name.ToString())));
	if (UVerticalBoxSlot* ControlSlot = Parent->AddChildToVerticalBox(ControlRow))
	{
		ControlSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	UTextBlock* NegativeText = MakeText(WidgetTree, FName(*FString::Printf(TEXT("%s_Negative"), *Name.ToString())), NegativeHint, 8, FLinearColor(0.74f, 0.84f, 0.88f, 0.86f), true);
	if (UHorizontalBoxSlot* NegativeSlot = ControlRow->AddChildToHorizontalBox(NegativeText))
	{
		NegativeSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		NegativeSlot->SetHorizontalAlignment(HAlign_Left);
		NegativeSlot->SetVerticalAlignment(VAlign_Center);
	}

	UOverlay* SliderOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName(*FString::Printf(TEXT("%s_SliderOverlay"), *Name.ToString())));
	if (UHorizontalBoxSlot* OverlaySlot = ControlRow->AddChildToHorizontalBox(SliderOverlay))
	{
		OverlaySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		OverlaySlot->SetVerticalAlignment(VAlign_Center);
	}

	OutSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), FName(*FString::Printf(TEXT("%s_Slider"), *Name.ToString())));
	OutSlider->SetStepSize(0.01f);
	OutSlider->MouseUsesStep = false;
	if (UOverlaySlot* SliderSlot = SliderOverlay->AddChildToOverlay(OutSlider))
	{
		SliderSlot->SetHorizontalAlignment(HAlign_Fill);
		SliderSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox* NeutralZone = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("%s_NeutralZone"), *Name.ToString())));
	NeutralZone->SetWidthOverride(14.f);
	NeutralZone->SetHeightOverride(14.f);
	NeutralZone->SetVisibility(ESlateVisibility::HitTestInvisible);
	UBorder* NeutralBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("%s_NeutralBorder"), *Name.ToString())));
	NeutralBorder->SetBrushColor(FLinearColor(1.f, 0.56f, 0.24f, 0.38f));
	NeutralBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	NeutralZone->SetContent(NeutralBorder);
	if (UOverlaySlot* NeutralSlot = SliderOverlay->AddChildToOverlay(NeutralZone))
	{
		NeutralSlot->SetHorizontalAlignment(HAlign_Center);
		NeutralSlot->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* PositiveText = MakeText(WidgetTree, FName(*FString::Printf(TEXT("%s_Positive"), *Name.ToString())), PositiveHint, 8, FLinearColor(0.74f, 0.84f, 0.88f, 0.86f), true);
	if (UHorizontalBoxSlot* PositiveSlot = ControlRow->AddChildToHorizontalBox(PositiveText))
	{
		PositiveSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
		PositiveSlot->SetHorizontalAlignment(HAlign_Right);
		PositiveSlot->SetVerticalAlignment(VAlign_Center);
	}

	return HeaderRow;
}

UHorizontalBox* AddToggleRow(
	UWidgetTree* WidgetTree,
	UVerticalBox* Parent,
	const FName Name,
	const FString& Label,
	TObjectPtr<UCheckBox>& OutCheckBox)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), Name);
	if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Row))
	{
		Slot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	}

	OutCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), FName(*FString::Printf(TEXT("%s_Check"), *Name.ToString())));
	if (UHorizontalBoxSlot* CheckSlot = Row->AddChildToHorizontalBox(OutCheckBox))
	{
		CheckSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		CheckSlot->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* LabelText = MakeText(WidgetTree, FName(*FString::Printf(TEXT("%s_Label"), *Name.ToString())), Label, 9, FLinearColor(0.80f, 0.90f, 0.92f, 0.90f), false);
	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	return Row;
}
}

TSharedRef<SWidget> UHelmControlPanelWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UHelmControlPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TryResolveHelmShellFromOuter();
	RefreshFromHelmData();
}

void UHelmControlPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TryResolveHelmShellFromOuter();
	RefreshFromHelmData();
}

void UHelmControlPanelWidget::InitForHelmShell(USubHelmWidget* InHelmShell)
{
	CachedHelmShell = InHelmShell;
	RefreshFromHelmData();
}

bool UHelmControlPanelWidget::IsBoundToHelmShell() const
{
	return CachedHelmShell.IsValid();
}

void UHelmControlPanelWidget::TryResolveHelmShellFromOuter()
{
	if (CachedHelmShell.IsValid())
	{
		return;
	}

	for (UObject* Outer = GetOuter(); Outer; Outer = Outer->GetOuter())
	{
		if (USubHelmWidget* HelmShell = Cast<USubHelmWidget>(Outer))
		{
			CachedHelmShell = HelmShell;
			return;
		}
	}
}

void UHelmControlPanelWidget::BuildWidgetTree()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	if (WidgetTree->RootWidget)
	{
		return;
	}

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RootBorder"));
	RootBorder->SetPadding(FMargin(12.f, 10.f));
	RootBorder->SetBrushColor(FLinearColor(0.015f, 0.030f, 0.045f, 0.96f));
	WidgetTree->RootWidget = RootBorder;

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	RootBorder->SetContent(RootBox);
	if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(RootBorder->GetContentSlot()))
	{
		BorderSlot->SetHorizontalAlignment(HAlign_Fill);
		BorderSlot->SetVerticalAlignment(VAlign_Fill);
	}

	HeaderText = MakeText(WidgetTree, TEXT("HeaderText"), TEXT("HELM CONTROLS"), 13, FLinearColor(0.39f, 0.94f, 0.88f, 1.f), true);
	if (UVerticalBoxSlot* HeaderSlot = RootBox->AddChildToVerticalBox(HeaderText))
	{
		HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
	}

	StateText = MakeText(WidgetTree, TEXT("StateText"), TEXT("UNBOUND"), 9, FLinearColor(1.f, 0.56f, 0.24f, 1.f), false);
	if (UVerticalBoxSlot* StateSlot = RootBox->AddChildToVerticalBox(StateText))
	{
		StateSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	AddSectionLabel(WidgetTree, RootBox, TEXT("DriveSectionLabel"), TEXT("MANUAL COMMANDS"));
	AddAxisSliderRow(WidgetTree, RootBox, TEXT("Throttle"), TEXT("PROPULSION"), TEXT("S"), TEXT("Z"), ThrottleValueText, ThrottleSlider);
	AddAxisSliderRow(WidgetTree, RootBox, TEXT("Rudder"), TEXT("RUDDER"), TEXT("L"), TEXT("R"), RudderValueText, RudderSlider);
	AddAxisSliderRow(WidgetTree, RootBox, TEXT("Trim"), TEXT("PLANES"), TEXT("DOWN"), TEXT("UP"), TrimValueText, TrimSlider);
	AddAxisSliderRow(WidgetTree, RootBox, TEXT("Ballast"), TEXT("BALLAST TARGET"), TEXT("0"), TEXT("100"), BallastValueText, BallastSlider);
	AddAxisSliderRow(WidgetTree, RootBox, TEXT("Pump"), TEXT("PUMP SPEED"), TEXT("0"), TEXT("100"), PumpValueText, PumpPowerSlider);

	AddToggleRow(WidgetTree, RootBox, TEXT("RudderHold"), TEXT("RUDDER HOLD"), RudderHoldCheck);
	AddToggleRow(WidgetTree, RootBox, TEXT("PlaneHold"), TEXT("PLANES HOLD"), PlaneHoldCheck);

	AddSectionLabel(WidgetTree, RootBox, TEXT("AutomationSectionLabel"), TEXT("STABILIZATION"));
	AddToggleRow(WidgetTree, RootBox, TEXT("StabilizationMaster"), TEXT("MASTER ENABLED"), StabilizationMasterCheck);
	AddToggleRow(WidgetTree, RootBox, TEXT("AutoSpeed"), TEXT("AUTO SPEED HOLD"), AutoSpeedCheck);
	AddToggleRow(WidgetTree, RootBox, TEXT("AutoDepth"), TEXT("AUTO DEPTH HOLD"), AutoDepthCheck);
	AddToggleRow(WidgetTree, RootBox, TEXT("AutoPitch"), TEXT("AUTO PITCH HOLD"), AutoPitchCheck);
	AddToggleRow(WidgetTree, RootBox, TEXT("BallastsActive"), TEXT("BALLAST CIRCUIT ACTIVE"), BallastsActiveCheck);
	AddToggleRow(WidgetTree, RootBox, TEXT("PumpActive"), TEXT("PUMPS ACTIVE"), PumpActiveCheck);

	AddSectionLabel(WidgetTree, RootBox, TEXT("BallastStateSectionLabel"), TEXT("BALLAST TANKS"));
	BallastGaugeBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BallastGaugeBox"));
	if (UVerticalBoxSlot* BallastGaugeSlot = RootBox->AddChildToVerticalBox(BallastGaugeBox))
	{
		BallastGaugeSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	MotionSummaryText = MakeText(WidgetTree, TEXT("MotionSummaryText"), TEXT("--"), 9, FLinearColor(0.82f, 0.90f, 0.94f, 0.94f), false);
	if (UVerticalBoxSlot* MotionSlot = RootBox->AddChildToVerticalBox(MotionSummaryText))
	{
		MotionSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 2.f));
	}

	AutoSummaryText = MakeText(WidgetTree, TEXT("AutoSummaryText"), TEXT("--"), 9, FLinearColor(0.69f, 0.84f, 0.88f, 0.86f), false);
	if (UVerticalBoxSlot* AutoSlot = RootBox->AddChildToVerticalBox(AutoSummaryText))
	{
		AutoSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));
	}

	ThrottleSlider->OnValueChanged.AddDynamic(this, &UHelmControlPanelWidget::HandleThrottleChanged);
	RudderSlider->OnValueChanged.AddDynamic(this, &UHelmControlPanelWidget::HandleRudderChanged);
	TrimSlider->OnValueChanged.AddDynamic(this, &UHelmControlPanelWidget::HandleTrimChanged);
	BallastSlider->OnValueChanged.AddDynamic(this, &UHelmControlPanelWidget::HandleBallastChanged);
	PumpPowerSlider->OnValueChanged.AddDynamic(this, &UHelmControlPanelWidget::HandlePumpPowerChanged);
	ThrottleSlider->OnMouseCaptureEnd.AddDynamic(this, &UHelmControlPanelWidget::HandleThrottleCaptureEnded);
	ThrottleSlider->OnControllerCaptureEnd.AddDynamic(this, &UHelmControlPanelWidget::HandleThrottleCaptureEnded);
	RudderSlider->OnMouseCaptureEnd.AddDynamic(this, &UHelmControlPanelWidget::HandleRudderCaptureEnded);
	RudderSlider->OnControllerCaptureEnd.AddDynamic(this, &UHelmControlPanelWidget::HandleRudderCaptureEnded);
	TrimSlider->OnMouseCaptureEnd.AddDynamic(this, &UHelmControlPanelWidget::HandleTrimCaptureEnded);
	TrimSlider->OnControllerCaptureEnd.AddDynamic(this, &UHelmControlPanelWidget::HandleTrimCaptureEnded);
	RudderHoldCheck->OnCheckStateChanged.AddDynamic(this, &UHelmControlPanelWidget::HandleRudderHoldChanged);
	PlaneHoldCheck->OnCheckStateChanged.AddDynamic(this, &UHelmControlPanelWidget::HandlePlaneHoldChanged);
	StabilizationMasterCheck->OnCheckStateChanged.AddDynamic(this, &UHelmControlPanelWidget::HandleStabilizationMasterChanged);
	AutoSpeedCheck->OnCheckStateChanged.AddDynamic(this, &UHelmControlPanelWidget::HandleAutoSpeedChanged);
	AutoDepthCheck->OnCheckStateChanged.AddDynamic(this, &UHelmControlPanelWidget::HandleAutoDepthChanged);
	AutoPitchCheck->OnCheckStateChanged.AddDynamic(this, &UHelmControlPanelWidget::HandleAutoPitchChanged);
	BallastsActiveCheck->OnCheckStateChanged.AddDynamic(this, &UHelmControlPanelWidget::HandleBallastsActiveChanged);
	PumpActiveCheck->OnCheckStateChanged.AddDynamic(this, &UHelmControlPanelWidget::HandlePumpActiveChanged);
}

void UHelmControlPanelWidget::RebuildBallastRows(int32 Count)
{
	if (!WidgetTree || !BallastGaugeBox)
	{
		return;
	}

	BallastGaugeBox->ClearChildren();
	BallastLabelTexts.Reset();
	BallastFillBars.Reset();
	BallastValueTexts.Reset();

	for (int32 BallastIndex = 0; BallastIndex < Count; ++BallastIndex)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("BallastTankRow_%d"), BallastIndex)));
		if (UVerticalBoxSlot* RowSlot = BallastGaugeBox->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));
		}

		UTextBlock* LabelText = MakeText(
			WidgetTree,
			FName(*FString::Printf(TEXT("BallastTankLabel_%d"), BallastIndex)),
			FString::Printf(TEXT("TANK %02d"), BallastIndex + 1),
			8,
			FLinearColor(0.74f, 0.84f, 0.88f, 0.86f),
			true);
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText))
		{
			LabelSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		BallastLabelTexts.Add(LabelText);

		UProgressBar* FillBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(*FString::Printf(TEXT("BallastTankFill_%d"), BallastIndex)));
		FillBar->SetPercent(0.f);
		FillBar->SetFillColorAndOpacity(FLinearColor(0.39f, 0.94f, 0.88f, 1.f));
		if (UHorizontalBoxSlot* FillSlot = Row->AddChildToHorizontalBox(FillBar))
		{
			FillSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			FillSlot->SetVerticalAlignment(VAlign_Center);
		}
		BallastFillBars.Add(FillBar);

		UTextBlock* ValueText = MakeText(
			WidgetTree,
			FName(*FString::Printf(TEXT("BallastTankValue_%d"), BallastIndex)),
			TEXT("--"),
			8,
			FLinearColor(0.89f, 0.96f, 0.98f, 1.f),
			true);
		if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(ValueText))
		{
			ValueSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
			ValueSlot->SetVerticalAlignment(VAlign_Center);
		}
		BallastValueTexts.Add(ValueText);
	}
}

void UHelmControlPanelWidget::RefreshFromHelmData()
{
	TryResolveHelmShellFromOuter();

	if (!HeaderText || !StateText || !CachedHelmShell.IsValid())
	{
		if (StateText)
		{
			StateText->SetText(FText::FromString(TEXT("UNBOUND")));
			StateText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.56f, 0.24f, 1.f)));
		}
		return;
	}

	const FHelmControlPanelData Data = CachedHelmShell->GetControlPanelData();
	const FLinearColor WarningColor(1.f, 0.56f, 0.24f, 1.f);
	const FLinearColor AccentColor(0.39f, 0.94f, 0.88f, 1.f);
	const FLinearColor NeutralColor(0.82f, 0.90f, 0.94f, 0.92f);

	FString StateLabel = TEXT("VALID");
	FLinearColor StateColor = AccentColor;
	switch (Data.State)
	{
	case EHelmPanelRuntimeState::Unbound:
		StateLabel = TEXT("UNBOUND");
		StateColor = WarningColor;
		break;
	case EHelmPanelRuntimeState::Warming:
		StateLabel = TEXT("WARMING");
		StateColor = WarningColor;
		break;
	case EHelmPanelRuntimeState::Degraded:
		StateLabel = TEXT("DEGRADED");
		StateColor = WarningColor;
		break;
	case EHelmPanelRuntimeState::Valid:
	default:
		break;
	}

	StateText->SetText(FText::FromString(StateLabel));
	StateText->SetColorAndOpacity(FSlateColor(StateColor));
	SetIsEnabled(Data.bBound);

	if (!Data.bBound)
	{
		if (MotionSummaryText)
		{
			MotionSummaryText->SetText(FText::FromString(TEXT("No runtime helm systems bound.")));
			MotionSummaryText->SetColorAndOpacity(FSlateColor(WarningColor));
		}
		if (AutoSummaryText)
		{
			AutoSummaryText->SetText(FText::FromString(TEXT("Attach helm widget to a resolved submarine controller path.")));
			AutoSummaryText->SetColorAndOpacity(FSlateColor(NeutralColor));
		}
		return;
	}

	bRefreshingFromRuntime = true;

	if (ThrottleSlider)
	{
		ThrottleSlider->SetValue(AxisToSlider(Data.CommandState.HelmThrottleCmd));
	}
	if (RudderSlider)
	{
		RudderSlider->SetValue(AxisToSlider(Data.CommandState.HelmYawCmd));
	}
	if (TrimSlider)
	{
		TrimSlider->SetValue(AxisToSlider(Data.CommandState.HelmTrimCmd));
	}
	if (BallastSlider)
	{
		BallastSlider->SetValue(FMath::Clamp(Data.CommandState.GlobalBallastTarget01, 0.f, 1.f));
	}
	if (PumpPowerSlider)
	{
		PumpPowerSlider->SetValue(FMath::Clamp(Data.CommandState.PumpPower01, 0.f, 1.f));
	}
	if (RudderHoldCheck)
	{
		RudderHoldCheck->SetIsChecked(Data.CommandState.bRudderHoldEnabled);
	}
	if (PlaneHoldCheck)
	{
		PlaneHoldCheck->SetIsChecked(Data.CommandState.bPlaneHoldEnabled);
	}

	if (StabilizationMasterCheck)
	{
		StabilizationMasterCheck->SetIsChecked(Data.CommandState.bStabilizationMasterEnabled);
	}
	if (AutoSpeedCheck)
	{
		AutoSpeedCheck->SetIsChecked(Data.CommandState.bAutoSpeedEnabled);
	}
	if (AutoDepthCheck)
	{
		AutoDepthCheck->SetIsChecked(Data.CommandState.bAutoDepthEnabled);
	}
	if (AutoPitchCheck)
	{
		AutoPitchCheck->SetIsChecked(Data.CommandState.bAutoPitchEnabled);
	}
	if (BallastsActiveCheck)
	{
		BallastsActiveCheck->SetIsChecked(Data.CommandState.bBallastsActive);
	}
	if (PumpActiveCheck)
	{
		PumpActiveCheck->SetIsChecked(Data.CommandState.bPumpActive);
	}

	bRefreshingFromRuntime = false;

	if (ThrottleValueText)
	{
		ThrottleValueText->SetText(FText::FromString(FString::Printf(TEXT("%+.0f%% / EFF %.0f%%"), Data.CommandState.HelmThrottleCmd * 100.f, Data.EffectivePowerInput * 100.f)));
	}
	if (RudderValueText)
	{
		RudderValueText->SetText(FText::FromString(FString::Printf(TEXT("%+.0f%%"), Data.CommandState.HelmYawCmd * 100.f)));
	}
	if (TrimValueText)
	{
		TrimValueText->SetText(FText::FromString(FString::Printf(TEXT("%+.0f%% / %+.1f DEG"), Data.CommandState.HelmTrimCmd * 100.f, Data.CurrentPitchDeg)));
	}
	if (BallastValueText)
	{
		BallastValueText->SetText(FText::FromString(FString::Printf(TEXT("TGT %.0f%% / CUR %.0f%%"), Data.CommandState.GlobalBallastTarget01 * 100.f, Data.GlobalBallastFill01 * 100.f)));
	}
	if (PumpValueText)
	{
		PumpValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), Data.CommandState.PumpPower01 * 100.f)));
	}

	if (BallastGaugeBox && BallastFillBars.Num() != Data.BallastTanks.Num())
	{
		RebuildBallastRows(Data.BallastTanks.Num());
	}

	for (int32 BallastIndex = 0; BallastIndex < Data.BallastTanks.Num(); ++BallastIndex)
	{
		const FHelmBallastTankPanelData& Tank = Data.BallastTanks[BallastIndex];
		if (BallastFillBars.IsValidIndex(BallastIndex) && BallastFillBars[BallastIndex])
		{
			FLinearColor FillColor = FLinearColor(0.39f, 0.94f, 0.88f, 1.f);
			if (Tank.PumpState == EPumpState::Degraded)
			{
				FillColor = FLinearColor(1.f, 0.56f, 0.24f, 1.f);
			}
			else if (Tank.PumpState == EPumpState::Dead)
			{
				FillColor = FLinearColor(0.82f, 0.24f, 0.24f, 1.f);
			}

			BallastFillBars[BallastIndex]->SetPercent(FMath::Clamp(Tank.FillLevel01, 0.f, 1.f));
			BallastFillBars[BallastIndex]->SetFillColorAndOpacity(FillColor);
		}

		if (BallastValueTexts.IsValidIndex(BallastIndex) && BallastValueTexts[BallastIndex])
		{
			const FString PumpLabel = Tank.PumpState == EPumpState::Dead
				? TEXT("DEAD")
				: (Tank.PumpState == EPumpState::Degraded ? TEXT("DEG") : TEXT("NOM"));
			BallastValueTexts[BallastIndex]->SetText(FText::FromString(FString::Printf(
				TEXT("%.0f%% / %.0f%% %s"),
				Tank.FillLevel01 * 100.f,
				Tank.TargetFill01 * 100.f,
				*PumpLabel)));
		}
	}

	if (MotionSummaryText)
	{
		MotionSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("SPD %+.1f KM/H   DEP %.1f M   PITCH %+.1f DEG"),
			Data.CurrentForwardSpeedCmS * 0.036f,
			Data.CurrentDepthMeters,
			Data.CurrentPitchDeg)));
		MotionSummaryText->SetColorAndOpacity(FSlateColor(NeutralColor));
	}

	if (AutoSummaryText)
	{
		const FString SpeedStatus = Data.bAutoSpeedActive ? TEXT("HOLD") : (Data.CommandState.bAutoSpeedEnabled ? TEXT("SUSP") : TEXT("OFF"));
		const FString DepthStatus = Data.bAutoDepthActive ? TEXT("HOLD") : (Data.CommandState.bAutoDepthEnabled ? TEXT("SUSP") : TEXT("OFF"));
		const FString PitchStatus = Data.bAutoPitchActive ? TEXT("HOLD") : (Data.CommandState.bAutoPitchEnabled ? TEXT("SUSP") : TEXT("OFF"));
		AutoSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("TARGET SPD %+.1f KM/H   DEP %.1f M   PITCH %+.1f DEG   AUTO [%s %s %s]   HOLD[R:%s P:%s]"),
			Data.CommandState.TargetSpeedCmS * 0.036f,
			Data.CommandState.TargetDepthMeters,
			Data.CommandState.TargetPitchDeg,
			*SpeedStatus,
			*DepthStatus,
			*PitchStatus,
			Data.CommandState.bRudderHoldEnabled ? TEXT("LOCK") : TEXT("RETURN"),
			Data.CommandState.bPlaneHoldEnabled ? TEXT("LOCK") : TEXT("RETURN"))));
		AutoSummaryText->SetColorAndOpacity(FSlateColor((Data.bAutoSpeedActive || Data.bAutoDepthActive || Data.bAutoPitchActive) ? AccentColor : NeutralColor));
	}
}

void UHelmControlPanelWidget::SnapSliderToNeutralIfNear(USlider* Slider)
{
	if (!Slider || !CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	const float Value = Slider->GetValue();
	if (FMath::Abs(Value - SliderNeutralValue) > SliderNeutralSnapRadius)
	{
		return;
	}

	bRefreshingFromRuntime = true;
	Slider->SetValue(SliderNeutralValue);
	bRefreshingFromRuntime = false;

	if (Slider == ThrottleSlider)
	{
		CachedHelmShell->RouteSetHelmThrottle(0.f);
	}
	else if (Slider == RudderSlider)
	{
		CachedHelmShell->RouteSetHelmSteer(0.f);
	}
	else if (Slider == TrimSlider)
	{
		CachedHelmShell->RouteSetHelmTrim(0.f);
	}
}

float UHelmControlPanelWidget::AxisToSlider(const float Value)
{
	return FMath::Clamp((Value + 1.f) * 0.5f, 0.f, 1.f);
}

float UHelmControlPanelWidget::SliderToAxis(const float Value)
{
	return FMath::Clamp((Value * 2.f) - 1.f, -1.f, 1.f);
}

void UHelmControlPanelWidget::HandleThrottleChanged(const float Value)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetHelmThrottle(SliderToAxis(Value));
}

void UHelmControlPanelWidget::HandleRudderChanged(const float Value)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetHelmSteer(SliderToAxis(Value));
}

void UHelmControlPanelWidget::HandleTrimChanged(const float Value)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetHelmTrim(SliderToAxis(Value));
}

void UHelmControlPanelWidget::HandleBallastChanged(const float Value)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetGlobalBallast(Value);
}

void UHelmControlPanelWidget::HandlePumpPowerChanged(const float Value)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetPumpPower(Value);
}

void UHelmControlPanelWidget::HandleThrottleCaptureEnded()
{
	SnapSliderToNeutralIfNear(ThrottleSlider);
}

void UHelmControlPanelWidget::HandleRudderCaptureEnded()
{
	SnapSliderToNeutralIfNear(RudderSlider);
}

void UHelmControlPanelWidget::HandleTrimCaptureEnded()
{
	SnapSliderToNeutralIfNear(TrimSlider);
}

void UHelmControlPanelWidget::HandleRudderHoldChanged(const bool bChecked)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetRudderHoldEnabled(bChecked);
}

void UHelmControlPanelWidget::HandlePlaneHoldChanged(const bool bChecked)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetPlaneHoldEnabled(bChecked);
}

void UHelmControlPanelWidget::HandleStabilizationMasterChanged(const bool bChecked)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetStabilizationMasterEnabled(bChecked);
}

void UHelmControlPanelWidget::HandleAutoSpeedChanged(const bool bChecked)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetAutoSpeedEnabled(bChecked);
}

void UHelmControlPanelWidget::HandleAutoDepthChanged(const bool bChecked)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetAutoDepthEnabled(bChecked);
}

void UHelmControlPanelWidget::HandleAutoPitchChanged(const bool bChecked)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetAutoPitchEnabled(bChecked);
}

void UHelmControlPanelWidget::HandleBallastsActiveChanged(const bool bChecked)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetBallastsActive(bChecked);
}

void UHelmControlPanelWidget::HandlePumpActiveChanged(const bool bChecked)
{
	if (!CachedHelmShell.IsValid() || bRefreshingFromRuntime)
	{
		return;
	}

	CachedHelmShell->RouteSetPumpActive(bChecked);
}
