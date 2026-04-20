#include "HelmDiveBoardWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HelmCockpitWidget.h"
#include "Rendering/DrawElements.h"
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

constexpr float RibbonHeight = 40.f;
constexpr float TankPipeHeight = 90.f;
constexpr float PitchRangeDeg = 30.f;
}

TSharedRef<SWidget> UHelmDiveBoardWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UHelmDiveBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (DirectButton)   DirectButton->OnClicked.AddDynamic(this,   &UHelmDiveBoardWidget::HandleDirect);
	if (HoldVertButton) HoldVertButton->OnClicked.AddDynamic(this, &UHelmDiveBoardWidget::HandleHoldVert);
	if (SurfaceButton)  SurfaceButton->OnClicked.AddDynamic(this,  &UHelmDiveBoardWidget::HandleSurface);
	if (DiveButton)     DiveButton->OnClicked.AddDynamic(this,     &UHelmDiveBoardWidget::HandleDiveCommand);
}

void UHelmDiveBoardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFromHelmData();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UHelmDiveBoardWidget::BuildWidgetTree()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	if (WidgetTree->RootWidget)
	{
		return;
	}

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	USizeBox* PaintArea = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PaintArea"));
	PaintArea->SetMinDesiredHeight(RibbonHeight + TankPipeHeight + 16.f);
	if (UVerticalBoxSlot* BoxSlot = Root->AddChildToVerticalBox(PaintArea))
	{
		BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	UHorizontalBox* ModeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ModeRow"));
	Root->AddChildToVerticalBox(ModeRow);

	const auto MakeBtn = [&](const FName Name, const FString& Label, FLinearColor IdleColor) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor  = FSlateColor(IdleColor);
		Style.Hovered.TintColor = FSlateColor(IdleColor * 1.4f);
		Style.Pressed.TintColor = FSlateColor(IdleColor * 0.8f);
		Btn->SetStyle(Style);

		UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("%s_Lbl"), *Name.ToString())));
		Txt->SetText(FText::FromString(Label));
		Txt->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9));
		Txt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Txt->SetJustification(ETextJustify::Center);
		Btn->SetContent(Txt);
		return Btn;
	};

	DirectButton   = MakeBtn(TEXT("Direct"),   TEXT("DIRECT"),    FLinearColor(0.25f, 0.25f, 0.25f, 1.f));
	HoldVertButton = MakeBtn(TEXT("HoldVert"), TEXT("HOLD VERT"), FLinearColor(0.05f, 0.30f, 0.45f, 1.f));
	SurfaceButton  = MakeBtn(TEXT("Surface"),  TEXT("SURFACE"),   FLinearColor(0.05f, 0.35f, 0.25f, 1.f));
	DiveButton     = MakeBtn(TEXT("Dive"),     TEXT("DIVE"),      FLinearColor(0.35f, 0.10f, 0.10f, 1.f));

	const auto AddButton = [ModeRow](UButton* Btn, float PaddingRight)
	{
		if (UHorizontalBoxSlot* S = ModeRow->AddChildToHorizontalBox(Btn))
		{
			FSlateChildSize Size; Size.SizeRule = ESlateSizeRule::Fill; Size.Value = 1.f;
			S->SetSize(Size);
			S->SetPadding(FMargin(0.f, 0.f, PaddingRight, 0.f));
		}
	};
	AddButton(DirectButton, 4.f);
	AddButton(HoldVertButton, 4.f);
	AddButton(SurfaceButton, 4.f);
	AddButton(DiveButton, 0.f);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.90f, 0.94f, 0.92f)));
	StatusText->SetText(FText::FromString(TEXT("--")));
	if (UVerticalBoxSlot* BoxSlot = Root->AddChildToVerticalBox(StatusText))
	{
		BoxSlot->SetPadding(FMargin(2.f, 4.f, 0.f, 0.f));
	}
}

void UHelmDiveBoardWidget::RefreshFromHelmData()
{
	USubHelmWidget* Shell = ResolveShell(this);
	if (!Shell)
	{
		return;
	}
	const FHelmControlPanelData Data = Shell->GetControlPanelData();
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;

	DisplayedPitchDeg = FMath::FInterpTo(DisplayedPitchDeg, Data.CurrentPitchDeg, Dt, 10.f);
	DisplayedTrimCmd  = FMath::FInterpTo(DisplayedTrimCmd, Data.CommandState.HelmTrimCmd, Dt, 10.f);
	bAutoDepthActiveCached  = Data.bAutoDepthActive;
	bAutoDepthEnabledCached = Data.CommandState.bAutoDepthEnabled;

	CachedTankCount = Data.BallastTanks.Num();
	if (CachedTankCount > 0)
	{
		DisplayedTank0Fill   = FMath::FInterpTo(DisplayedTank0Fill, Data.BallastTanks[0].FillLevel01, Dt, 6.f);
		DisplayedTank0Target = FMath::FInterpTo(DisplayedTank0Target, Data.BallastTanks[0].TargetFill01, Dt, 8.f);
	}
	if (CachedTankCount > 1)
	{
		DisplayedTank1Fill   = FMath::FInterpTo(DisplayedTank1Fill, Data.BallastTanks[1].FillLevel01, Dt, 6.f);
		DisplayedTank1Target = FMath::FInterpTo(DisplayedTank1Target, Data.BallastTanks[1].TargetFill01, Dt, 8.f);
	}

	if (StatusText)
	{
		const FString Mode = bAutoDepthActiveCached
			? TEXT("HOLD VERT")
			: (bAutoDepthEnabledCached ? TEXT("HOLD VERT (suspended)") : TEXT("DIRECT"));
		StatusText->SetText(FText::FromString(FString::Printf(
			TEXT("%s   pitch %+.1f deg   ballast %.0f%%"),
			*Mode,
			Data.CurrentPitchDeg,
			Data.GlobalBallastFill01 * 100.f)));
	}
}

int32 UHelmDiveBoardWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 NextLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X < 8.f || Size.Y < 8.f)
	{
		return NextLayer;
	}

	static const FSlateColorBrush WhiteBrush(FLinearColor::White);

	auto MakeGeom = [&AllottedGeometry](const FVector2D& Pos, const FVector2D& LS)
	{
		return AllottedGeometry.ToPaintGeometry(
			FVector2f(static_cast<float>(LS.X), static_cast<float>(LS.Y)),
			FSlateLayoutTransform(FVector2f(static_cast<float>(Pos.X), static_cast<float>(Pos.Y))));
	};

	// ── Pitch ribbon (top) ─────────────────────────────────────────
	const float RibbonY = 4.f;
	const float RibbonW = Size.X - 16.f;
	const float RibbonX = 8.f;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		NextLayer,
		MakeGeom(FVector2D(RibbonX, RibbonY + RibbonHeight * 0.5f - 1.f), FVector2D(RibbonW, 2.f)),
		&WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.40f, 0.85f, 0.90f, 0.6f));

	// Ticks every 10°.
	const FSlateFontInfo TickFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 7);
	for (int32 p = -3; p <= 3; ++p)
	{
		const float Deg = static_cast<float>(p) * 10.f;
		const float Ratio = (Deg + PitchRangeDeg) / (2.f * PitchRangeDeg);
		const float TickX = RibbonX + Ratio * RibbonW;
		TArray<FVector2f> Tick;
		Tick.Add(FVector2f(TickX, RibbonY + 6.f));
		Tick.Add(FVector2f(TickX, RibbonY + RibbonHeight - 6.f));
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			NextLayer + 1,
			AllottedGeometry.ToPaintGeometry(),
			Tick,
			ESlateDrawEffect::None,
			FLinearColor(0.60f, 0.85f, 0.90f, 0.7f),
			true,
			(p == 0) ? 2.f : 1.f);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			NextLayer + 2,
			MakeGeom(FVector2D(TickX - 10.f, RibbonY + RibbonHeight - 10.f), FVector2D(20.f, 10.f)),
			FString::Printf(TEXT("%+d"), static_cast<int32>(Deg)),
			TickFont,
			ESlateDrawEffect::None,
			FLinearColor(0.80f, 0.90f, 0.95f, 0.7f));
	}

	// Current pitch marker (big dot).
	const float PitchRatio = FMath::Clamp((DisplayedPitchDeg + PitchRangeDeg) / (2.f * PitchRangeDeg), 0.f, 1.f);
	const float PitchX = RibbonX + PitchRatio * RibbonW;
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		NextLayer + 3,
		MakeGeom(FVector2D(PitchX - 4.f, RibbonY + RibbonHeight * 0.5f - 4.f), FVector2D(8.f, 8.f)),
		&WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(1.f, 0.85f, 0.20f, 1.f));

	// Commanded trim arrow (below the ribbon line).
	const float TrimRatio = FMath::Clamp((DisplayedTrimCmd + 1.f) * 0.5f, 0.f, 1.f);
	const float TrimX = RibbonX + TrimRatio * RibbonW;
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		NextLayer + 3,
		MakeGeom(FVector2D(TrimX - 2.f, RibbonY + RibbonHeight * 0.5f + 4.f), FVector2D(4.f, 10.f)),
		&WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.30f, 0.65f, 1.00f, 0.9f));

	// ── Ballast pipes (bottom) ────────────────────────────────────
	if (CachedTankCount > 0)
	{
		const float PipeY = RibbonY + RibbonHeight + 6.f;
		const float PipeW = (CachedTankCount == 1) ? 80.f : 60.f;
		const float TotalPipesW = PipeW * CachedTankCount + (CachedTankCount - 1) * 16.f;
		float PipeXBase = (Size.X - TotalPipesW) * 0.5f;

		const auto DrawTank = [&](float Fill01, float Target01, const FVector2D& Origin, const TCHAR* Label)
		{
			// Pipe outline. Cache the first corner to avoid Add-from-own-array.
			const FVector2f Corner0(Origin.X, Origin.Y);
			TArray<FVector2f> Outline;
			Outline.Reserve(5);
			Outline.Add(Corner0);
			Outline.Add(FVector2f(Origin.X + PipeW, Origin.Y));
			Outline.Add(FVector2f(Origin.X + PipeW, Origin.Y + TankPipeHeight));
			Outline.Add(FVector2f(Origin.X, Origin.Y + TankPipeHeight));
			Outline.Add(Corner0);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				NextLayer + 1,
				AllottedGeometry.ToPaintGeometry(),
				Outline,
				ESlateDrawEffect::None,
				FLinearColor(0.50f, 0.85f, 0.90f, 0.85f),
				true,
				1.6f);

			// Water fill (current).
			const float FillH = FMath::Clamp(Fill01, 0.f, 1.f) * TankPipeHeight;
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				NextLayer + 2,
				MakeGeom(FVector2D(Origin.X + 2.f, Origin.Y + TankPipeHeight - FillH + 2.f), FVector2D(PipeW - 4.f, FillH - 4.f)),
				&WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.15f, 0.55f, 0.85f, 0.85f));

			// Target level (ghost line across pipe).
			const float TargetH = FMath::Clamp(Target01, 0.f, 1.f) * TankPipeHeight;
			const float TargetLineY = Origin.Y + TankPipeHeight - TargetH;
			TArray<FVector2f> TargetLine;
			TargetLine.Add(FVector2f(Origin.X, TargetLineY));
			TargetLine.Add(FVector2f(Origin.X + PipeW, TargetLineY));
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				NextLayer + 3,
				AllottedGeometry.ToPaintGeometry(),
				TargetLine,
				ESlateDrawEffect::None,
				FLinearColor(1.f, 0.85f, 0.20f, 0.85f),
				true,
				1.5f);

			// Label.
			const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				NextLayer + 4,
				MakeGeom(FVector2D(Origin.X, Origin.Y + TankPipeHeight + 2.f), FVector2D(PipeW, 10.f)),
				FString::Printf(TEXT("%s  %.0f%%"), Label, Fill01 * 100.f),
				LabelFont,
				ESlateDrawEffect::None,
				FLinearColor(0.95f, 0.97f, 1.f, 0.92f));
		};

		DrawTank(DisplayedTank0Fill, DisplayedTank0Target, FVector2D(PipeXBase, PipeY), TEXT("TANK 1"));
		if (CachedTankCount > 1)
		{
			DrawTank(DisplayedTank1Fill, DisplayedTank1Target, FVector2D(PipeXBase + PipeW + 16.f, PipeY), TEXT("TANK 2"));
		}
	}

	return NextLayer + 5;
}

FReply UHelmDiveBoardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Pitch ribbon hit-test: clicking on the ribbon sets the trim command.
	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D Size = InGeometry.GetLocalSize();
	const float RibbonY = 4.f;
	const float RibbonX = 8.f;
	const float RibbonW = Size.X - 16.f;
	if (LocalPos.Y >= RibbonY && LocalPos.Y <= RibbonY + RibbonHeight &&
		LocalPos.X >= RibbonX && LocalPos.X <= RibbonX + RibbonW)
	{
		const float Ratio = FMath::Clamp((LocalPos.X - RibbonX) / RibbonW, 0.f, 1.f);
		const float Trim = FMath::Clamp(Ratio * 2.f - 1.f, -1.f, 1.f);
		if (USubHelmWidget* Shell = ResolveShell(this))
		{
			Shell->RouteSetHelmTrim(Trim);
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void UHelmDiveBoardWidget::HandleDirect()
{
	if (USubHelmWidget* Shell = ResolveShell(this))
	{
		Shell->RouteSetAutoDepthEnabled(false);
	}
}

void UHelmDiveBoardWidget::HandleHoldVert()
{
	if (USubHelmWidget* Shell = ResolveShell(this))
	{
		Shell->RouteSetAutoDepthEnabled(true);
		Shell->RouteSetBallastsActive(true);
	}
}

void UHelmDiveBoardWidget::HandleSurface()
{
	USubHelmWidget* Shell = ResolveShell(this);
	if (!Shell)
	{
		return;
	}
	Shell->RouteSetAutoDepthEnabled(false);
	Shell->RouteSetBallastsActive(true);
	const int32 NumTanks = CachedTankCount;
	for (int32 i = 0; i < NumTanks; ++i)
	{
		Shell->RouteSetBallastByIndex(i, 0.f);
	}
	Shell->RouteSetGlobalBallast(0.f);
}

void UHelmDiveBoardWidget::HandleDiveCommand()
{
	USubHelmWidget* Shell = ResolveShell(this);
	if (!Shell)
	{
		return;
	}
	Shell->RouteSetAutoDepthEnabled(false);
	Shell->RouteSetBallastsActive(true);
	const int32 NumTanks = CachedTankCount;
	for (int32 i = 0; i < NumTanks; ++i)
	{
		Shell->RouteSetBallastByIndex(i, 1.f);
	}
	Shell->RouteSetGlobalBallast(1.f);
}
