#include "HelmCockpitWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HelmThrottleTelegraphWidget.h"
#include "HelmRudderYokeWidget.h"
#include "HelmDiveBoardWidget.h"
#include "HelmKillSwitchWidget.h"
#include "../SubHelmWidget.h"

TSharedRef<SWidget> UHelmCockpitWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UHelmCockpitWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TryResolveHelmShellFromOuter();
}

void UHelmCockpitWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TryResolveHelmShellFromOuter();
}

void UHelmCockpitWidget::InitForHelmShell(USubHelmWidget* InHelmShell)
{
	CachedHelmShell = InHelmShell;
}

bool UHelmCockpitWidget::IsBoundToHelmShell() const
{
	return CachedHelmShell.IsValid();
}

void UHelmCockpitWidget::TryResolveHelmShellFromOuter()
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

void UHelmCockpitWidget::BuildWidgetTree()
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
	RootBorder->SetPadding(FMargin(8.f, 8.f));
	RootBorder->SetBrushColor(FLinearColor(0.010f, 0.025f, 0.035f, 0.97f));
	WidgetTree->RootWidget = RootBorder;

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	RootBorder->SetContent(RootBox);
	if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(RootBorder->GetContentSlot()))
	{
		BorderSlot->SetHorizontalAlignment(HAlign_Fill);
		BorderSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// Instrument children live inside this widget's own WidgetTree. Using
	// WidgetTree->ConstructWidget instead of the global CreateWidget<T>() is
	// critical: CreateWidget needs a World / PlayerController context, which
	// is not available when the BP is loaded for designer preview — that
	// asymmetry is what crashed UE when WBP_HelmCockpit was dragged into
	// WBP_SubHelm in the editor.
	ThrottleInstrument = WidgetTree->ConstructWidget<UHelmThrottleTelegraphWidget>(UHelmThrottleTelegraphWidget::StaticClass(), TEXT("ThrottleInstrument"));
	RudderInstrument   = WidgetTree->ConstructWidget<UHelmRudderYokeWidget>(UHelmRudderYokeWidget::StaticClass(), TEXT("RudderInstrument"));
	DiveInstrument     = WidgetTree->ConstructWidget<UHelmDiveBoardWidget>(UHelmDiveBoardWidget::StaticClass(), TEXT("DiveInstrument"));
	KillSwitchInstrument = WidgetTree->ConstructWidget<UHelmKillSwitchWidget>(UHelmKillSwitchWidget::StaticClass(), TEXT("KillSwitchInstrument"));

	const auto AddInstrument = [this](UUserWidget* Instrument, float MarginBottom)
	{
		if (!Instrument || !RootBox)
		{
			return;
		}
		if (UVerticalBoxSlot* BoxSlot = RootBox->AddChildToVerticalBox(Instrument))
		{
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, MarginBottom));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	};

	AddInstrument(ThrottleInstrument, 8.f);
	AddInstrument(RudderInstrument, 8.f);
	AddInstrument(DiveInstrument, 8.f);
	AddInstrument(KillSwitchInstrument, 0.f);
}
