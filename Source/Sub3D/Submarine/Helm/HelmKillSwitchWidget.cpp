#include "HelmKillSwitchWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "HelmCockpitWidget.h"
#include "Styling/CoreStyle.h"
#include "../SubHelmWidget.h"

namespace
{
USubHelmWidget* ResolveShell(const UUserWidget* Widget)
{
	for (UObject* Outer = Widget ? Widget->GetOuter() : nullptr; Outer; Outer = Outer->GetOuter())
	{
		if (UHelmCockpitWidget* Cockpit = Cast<UHelmCockpitWidget>(Outer))
		{
			return Cockpit->GetHelmShell();
		}
		if (USubHelmWidget* Shell = Cast<USubHelmWidget>(Outer))
		{
			return Shell;
		}
	}
	return nullptr;
}
}

TSharedRef<SWidget> UHelmKillSwitchWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UHelmKillSwitchWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button)
	{
		Button->OnClicked.AddDynamic(this, &UHelmKillSwitchWidget::HandleToggle);
	}
}

void UHelmKillSwitchWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFromHelmData();
}

void UHelmKillSwitchWidget::BuildWidgetTree()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	if (WidgetTree->RootWidget)
	{
		return;
	}

	Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("KillBtn"));
	WidgetTree->RootWidget = Button;

	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KillBtnLabel"));
	Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Label->SetJustification(ETextJustify::Center);
	Label->SetText(FText::FromString(TEXT("KILL ALL AUTOPILOTS")));
	Button->SetContent(Label);
}

void UHelmKillSwitchWidget::RefreshFromHelmData()
{
	USubHelmWidget* Shell = ResolveShell(this);
	if (!Shell)
	{
		return;
	}
	const FHelmControlPanelData Data = Shell->GetControlPanelData();
	bMasterEnabledCached = Data.CommandState.bStabilizationMasterEnabled;

	if (Button)
	{
		FButtonStyle Style = Button->GetStyle();
		const FLinearColor Active = FLinearColor(0.55f, 0.08f, 0.08f, 1.f); // red = armed (master ON, kill available)
		const FLinearColor Killed = FLinearColor(0.10f, 0.10f, 0.10f, 1.f); // dark = killed
		const FLinearColor Base = bMasterEnabledCached ? Active : Killed;
		Style.Normal.TintColor  = FSlateColor(Base);
		Style.Hovered.TintColor = FSlateColor(Base * 1.35f);
		Style.Pressed.TintColor = FSlateColor(Base * 0.8f);
		Button->SetStyle(Style);
	}
	if (Label)
	{
		Label->SetText(FText::FromString(
			bMasterEnabledCached
				? TEXT("KILL ALL AUTOPILOTS")
				: TEXT("AUTOPILOTS DEAD — CLICK TO ARM")));
	}
}

void UHelmKillSwitchWidget::HandleToggle()
{
	if (USubHelmWidget* Shell = ResolveShell(this))
	{
		Shell->RouteSetStabilizationMasterEnabled(!bMasterEnabledCached);
	}
}
