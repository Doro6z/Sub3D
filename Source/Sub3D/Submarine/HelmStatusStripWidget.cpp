#include "HelmStatusStripWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "SubHelmWidget.h"

namespace
{
FSlateFontInfo MakeStatusFont(const int32 Size, const bool bBold)
{
	return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
}

UTextBlock* MakeStatusText(UWidgetTree* WidgetTree, const FName Name, const int32 Size, const FLinearColor& Color, const bool bBold)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Text->SetFont(MakeStatusFont(Size, bBold));
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetAutoWrapText(true);
	return Text;
}

void AddStatusBlock(
	UWidgetTree* WidgetTree,
	UHorizontalBox* Parent,
	const FName Name,
	const float FillWeight,
	TObjectPtr<UTextBlock>& OutHeader,
	TObjectPtr<UTextBlock>& OutValue,
	TObjectPtr<UTextBlock>* OutDetail = nullptr)
{
	UBorder* BlockBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("%s_BlockBorder"), *Name.ToString())));
	BlockBorder->SetPadding(FMargin(10.f, 8.f));
	BlockBorder->SetBrushColor(FLinearColor(0.030f, 0.050f, 0.070f, 0.96f));
	if (UHorizontalBoxSlot* RowSlot = Parent->AddChildToHorizontalBox(BlockBorder))
	{
		FSlateChildSize SlotSize(ESlateSizeRule::Fill);
		SlotSize.Value = FillWeight;
		RowSlot->SetSize(SlotSize);
		RowSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		RowSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* BlockBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*FString::Printf(TEXT("%s_BlockBox"), *Name.ToString())));
	BlockBorder->SetContent(BlockBox);

	OutHeader = MakeStatusText(WidgetTree, FName(*FString::Printf(TEXT("%s_Header"), *Name.ToString())), 8, FLinearColor(0.39f, 0.94f, 0.88f, 1.f), true);
	if (UVerticalBoxSlot* HeaderSlot = BlockBox->AddChildToVerticalBox(OutHeader))
	{
		HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}

	OutValue = MakeStatusText(WidgetTree, FName(*FString::Printf(TEXT("%s_Value"), *Name.ToString())), 10, FLinearColor(0.88f, 0.94f, 0.97f, 1.f), true);
	if (UVerticalBoxSlot* ValueSlot = BlockBox->AddChildToVerticalBox(OutValue))
	{
		ValueSlot->SetPadding(FMargin(0.f, 0.f, 0.f, OutDetail ? 4.f : 0.f));
	}

	if (OutDetail)
	{
		*OutDetail = MakeStatusText(WidgetTree, FName(*FString::Printf(TEXT("%s_Detail"), *Name.ToString())), 8, FLinearColor(0.70f, 0.82f, 0.88f, 0.88f), false);
		if (UVerticalBoxSlot* DetailSlot = BlockBox->AddChildToVerticalBox(*OutDetail))
		{
			DetailSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));
		}
	}
}
}

TSharedRef<SWidget> UHelmStatusStripWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UHelmStatusStripWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TryResolveHelmShellFromOuter();
	RefreshFromHelmData();
}

void UHelmStatusStripWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TryResolveHelmShellFromOuter();
	RefreshFromHelmData();
}

void UHelmStatusStripWidget::InitForHelmShell(USubHelmWidget* InHelmShell)
{
	CachedHelmShell = InHelmShell;
	RefreshFromHelmData();
}

bool UHelmStatusStripWidget::IsBoundToHelmShell() const
{
	return CachedHelmShell.IsValid();
}

void UHelmStatusStripWidget::TryResolveHelmShellFromOuter()
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

void UHelmStatusStripWidget::BuildWidgetTree()
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
	RootBorder->SetBrushColor(FLinearColor(0.015f, 0.028f, 0.040f, 0.94f));
	WidgetTree->RootWidget = RootBorder;

	RootRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RootRow"));
	RootBorder->SetContent(RootRow);
	if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(RootBorder->GetContentSlot()))
	{
		BorderSlot->SetHorizontalAlignment(HAlign_Fill);
		BorderSlot->SetVerticalAlignment(VAlign_Fill);
	}

	AddStatusBlock(WidgetTree, RootRow, TEXT("Motion"), 1.2f, MotionHeaderText, MotionValueText);
	AddStatusBlock(WidgetTree, RootRow, TEXT("Sonar"), 1.1f, SonarHeaderText, SonarValueText);
	AddStatusBlock(WidgetTree, RootRow, TEXT("Control"), 1.1f, ControlHeaderText, ControlValueText);
	AddStatusBlock(WidgetTree, RootRow, TEXT("Warning"), 1.0f, WarningHeaderText, WarningValueText, &WarningDetailText);
}

void UHelmStatusStripWidget::RefreshFromHelmData()
{
	TryResolveHelmShellFromOuter();

	if (!MotionHeaderText || !MotionValueText || !SonarHeaderText || !SonarValueText || !ControlHeaderText || !ControlValueText || !WarningHeaderText || !WarningValueText || !WarningDetailText)
	{
		return;
	}

	MotionHeaderText->SetText(FText::FromString(TEXT("SHIP")));
	SonarHeaderText->SetText(FText::FromString(TEXT("SONAR")));
	ControlHeaderText->SetText(FText::FromString(TEXT("CONTROL")));
	WarningHeaderText->SetText(FText::FromString(TEXT("ALERT")));

	if (!CachedHelmShell.IsValid())
	{
		MotionValueText->SetText(FText::FromString(TEXT("HDG ---\nSPD --.- KM/H\nDEP ---.- M\nPIT --.-   ROLL --.-")));
		SonarValueText->SetText(FText::FromString(TEXT("UNBOUND\nRNG --- M   FOCUS ---\nTRACKS --/--")));
		ControlValueText->SetText(FText::FromString(TEXT("MASTER --   PUMP --\nAUTO S:-- D:-- P:--\nHOLD R:-- P:--")));
		WarningValueText->SetText(FText::FromString(TEXT("UNBOUND")));
		WarningDetailText->SetText(FText::FromString(TEXT("Helm shell not resolved.")));
		return;
	}

	const FHelmAlertPanelData Alert = CachedHelmShell->GetAlertPanelData();
	const FHelmPerceptionPanelData Perception = CachedHelmShell->GetPerceptionPanelData();
	const FHelmControlPanelData Control = CachedHelmShell->GetControlPanelData();

	MotionValueText->SetText(FText::FromString(FString::Printf(
		TEXT("HDG %03.0f\nSPD %.1f KM/H\nDEP %.1f M\nPIT %+.1f   ROLL %+.1f"),
		Alert.HeadingDeg,
		Alert.SpeedKmh,
		Alert.DepthMeters,
		Alert.PitchDeg,
		Alert.RollDeg)));

	const FString SonarState = Perception.bBound
		? FString::Printf(TEXT("%s\nRNG %.0f M   FOCUS %03.0f\nTRACKS %d/%d"),
			*StaticEnum<ESonarMode>()->GetDisplayNameTextByValue(static_cast<int64>(Perception.SonarMode)).ToString().ToUpper(),
			Perception.DisplayRangeCm / 100.f,
			Perception.FocusBearingDeg,
			Perception.PriorityTrackCount,
			Perception.TrackCount)
		: TEXT("UNBOUND\nRNG --- M   FOCUS ---\nTRACKS --/--");
	SonarValueText->SetText(FText::FromString(SonarState));
	SonarValueText->SetColorAndOpacity(FSlateColor(Perception.bSignalUnstable ? FLinearColor(1.f, 0.56f, 0.24f, 1.f) : FLinearColor(0.39f, 0.94f, 0.88f, 1.f)));

	const FString SystemsState = FString::Printf(
		TEXT("MASTER %s   PUMP %s %.0f%%\nAUTO S:%s  D:%s  P:%s\nHOLD R:%s  P:%s"),
		Control.CommandState.bStabilizationMasterEnabled ? TEXT("ON") : TEXT("OFF"),
		Control.CommandState.bPumpActive ? TEXT("ON") : TEXT("OFF"),
		Control.CommandState.PumpPower01 * 100.f,
		Control.bAutoSpeedActive ? TEXT("HOLD") : (Control.CommandState.bAutoSpeedEnabled ? TEXT("SUSP") : TEXT("OFF")),
		Control.bAutoDepthActive ? TEXT("HOLD") : (Control.CommandState.bAutoDepthEnabled ? TEXT("SUSP") : TEXT("OFF")),
		Control.bAutoPitchActive ? TEXT("HOLD") : (Control.CommandState.bAutoPitchEnabled ? TEXT("SUSP") : TEXT("OFF")),
		Control.CommandState.bRudderHoldEnabled ? TEXT("LOCK") : TEXT("RETURN"),
		Control.CommandState.bPlaneHoldEnabled ? TEXT("LOCK") : TEXT("RETURN"));
	ControlValueText->SetText(FText::FromString(SystemsState));

	FString WarningLabel = TEXT("FLOW OK");
	FLinearColor WarningColor(0.39f, 0.94f, 0.88f, 1.f);
	FString WarningDetail = FString::Printf(
		TEXT("NAV %s   BALLAST %.0f%%"),
		Alert.bNavigationDataValid ? TEXT("READY") : TEXT("DEGRADED"),
		Control.GlobalBallastFill01 * 100.f);

	// Critical-tier alarms (smart strip Option γ). Promoted above the
	// existing stop-margin / commit / nav warnings because hull integrity
	// trumps trajectory feedback. Bright red text + pulse-friendly color.
	const FLinearColor CriticalColor(1.0f, 0.20f, 0.10f, 1.f);
	if (!Alert.bBound)
	{
		WarningLabel = TEXT("STATUS UNBOUND");
		WarningColor = FLinearColor(1.f, 0.56f, 0.24f, 1.f);
		WarningDetail = TEXT("No alert panel data.");
	}
	else if (Alert.ActiveBreachCount > 0)
	{
		WarningLabel = (Alert.ActiveBreachCount > 1)
			? FString::Printf(TEXT("HULL BREACH x%d"), Alert.ActiveBreachCount)
			: TEXT("HULL BREACH");
		WarningColor = CriticalColor;
		WarningDetail = TEXT("Compartments flooding. Seal doors, run pumps.");
	}
	else if (Alert.MaxCompartmentFloodFraction >= 0.5f)
	{
		WarningLabel = FString::Printf(TEXT("FLOODING %.0f%%"), Alert.MaxCompartmentFloodFraction * 100.f);
		WarningColor = CriticalColor;
		WarningDetail = TEXT("Compartment past half flood. Pump immediately.");
	}
	else if (Alert.MaxCompartmentFloodFraction >= 0.15f)
	{
		WarningLabel = FString::Printf(TEXT("WATER INGRESS %.0f%%"), Alert.MaxCompartmentFloodFraction * 100.f);
		WarningColor = FLinearColor(1.f, 0.56f, 0.24f, 1.f);
		WarningDetail = TEXT("Minor flooding detected.");
	}
	else if (Alert.StoppingDistanceWarning.bWarning)
	{
		WarningLabel = FString::Printf(TEXT("STOP MARGIN %.1f M"), Alert.StoppingDistanceWarning.MarginCm / 100.f);
		WarningColor = FLinearColor(1.f, 0.56f, 0.24f, 1.f);
		WarningDetail = TEXT("Reduce speed and prepare to brake.");
	}
	else if (Alert.CommitmentWarning.bWarning)
	{
		WarningLabel = FString::Printf(TEXT("COMMIT %.1f M"), Alert.CommitmentWarning.DistanceToCommitmentCm / 100.f);
		WarningColor = FLinearColor(1.f, 0.56f, 0.24f, 1.f);
		WarningDetail = TEXT("Route merge is near. Avoid late turn-in.");
	}
	else if (Alert.bSignalUnstable)
	{
		WarningLabel = TEXT("SIGNAL DEGRADED");
		WarningColor = FLinearColor(1.f, 0.56f, 0.24f, 1.f);
		WarningDetail = TEXT("Sonar return is unstable.");
	}
	else if (!Alert.bNavigationDataValid)
	{
		WarningLabel = TEXT("NAV DEGRADED");
		WarningColor = FLinearColor(1.f, 0.56f, 0.24f, 1.f);
		WarningDetail = TEXT("Navigation runtime data is incomplete.");
	}

	WarningValueText->SetText(FText::FromString(WarningLabel));

	// Pulse the value label when in critical-tier red so it draws the eye
	// without needing animation in BP. Pulse only when red to avoid flicker
	// on routine warnings.
	const bool bCritical = (WarningColor.R > 0.9f && WarningColor.G < 0.35f);
	FLinearColor RenderColor = WarningColor;
	if (bCritical)
	{
		const double Now = FPlatformTime::Seconds();
		const float Pulse = 0.5f + 0.5f * FMath::Sin(static_cast<float>(Now) * 6.28f * 1.5f);
		RenderColor.A = FMath::Lerp(0.6f, 1.0f, Pulse);
	}
	WarningValueText->SetColorAndOpacity(FSlateColor(RenderColor));
	WarningDetailText->SetText(FText::FromString(WarningDetail));
	WarningDetailText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.90f, 0.94f, 0.92f)));
}
