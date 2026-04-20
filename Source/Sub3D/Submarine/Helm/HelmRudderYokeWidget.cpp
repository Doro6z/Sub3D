#include "HelmRudderYokeWidget.h"

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
USubHelmWidget* ResolveRudderYokeShell(const UUserWidget* Widget)
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

TSharedRef<SWidget> UHelmRudderYokeWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UHelmRudderYokeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (HoldRudderButton)
	{
		HoldRudderButton->OnClicked.AddDynamic(this, &UHelmRudderYokeWidget::HandleHoldRudderToggle);
	}
	if (RecenterButton)
	{
		RecenterButton->OnClicked.AddDynamic(this, &UHelmRudderYokeWidget::HandleRecenter);
	}
}

void UHelmRudderYokeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFromHelmData();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UHelmRudderYokeWidget::BuildWidgetTree()
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
	PaintArea->SetMinDesiredHeight(140.f);
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

	HoldRudderButton = MakeBtn(TEXT("HoldRudder"), TEXT("HOLD RUDDER"), FLinearColor(0.05f, 0.30f, 0.35f, 1.f));
	RecenterButton   = MakeBtn(TEXT("Recenter"),   TEXT("RECENTER"),    FLinearColor(0.25f, 0.25f, 0.25f, 1.f));

	if (UHorizontalBoxSlot* S = ModeRow->AddChildToHorizontalBox(HoldRudderButton))
	{
		FSlateChildSize Size; Size.SizeRule = ESlateSizeRule::Fill; Size.Value = 1.f;
		S->SetSize(Size);
		S->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
	}
	if (UHorizontalBoxSlot* S = ModeRow->AddChildToHorizontalBox(RecenterButton))
	{
		FSlateChildSize Size; Size.SizeRule = ESlateSizeRule::Fill; Size.Value = 1.f;
		S->SetSize(Size);
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.90f, 0.94f, 0.92f)));
	StatusText->SetText(FText::FromString(TEXT("--")));
	if (UVerticalBoxSlot* BoxSlot = Root->AddChildToVerticalBox(StatusText))
	{
		BoxSlot->SetPadding(FMargin(2.f, 4.f, 0.f, 0.f));
	}
}

void UHelmRudderYokeWidget::RefreshFromHelmData()
{
	USubHelmWidget* Shell = ResolveRudderYokeShell(this);
	if (!Shell)
	{
		return;
	}

	const FHelmControlPanelData Data = Shell->GetControlPanelData();
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	DisplayedRudderCmd   = FMath::FInterpTo(DisplayedRudderCmd, Data.CommandState.HelmYawCmd, Dt, 12.f);
	DisplayedYawRate     = FMath::FInterpTo(DisplayedYawRate, Data.CurrentYawRateDegPerSec, Dt, 12.f);
	CurrentHeadingDeg    = Data.CurrentHeadingDeg;
	bRudderHoldCached    = Data.CommandState.bRudderHoldEnabled;

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(
			TEXT("HDG %03.0f   YAW %+4.1f deg/s   %s"),
			CurrentHeadingDeg,
			DisplayedYawRate,
			bRudderHoldCached ? TEXT("HOLD") : TEXT("SPRING"))));
	}
}

int32 UHelmRudderYokeWidget::NativePaint(
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

	const float PaintH = FMath::Min(Size.Y, 140.f);
	const FVector2D Center(Size.X * 0.5f, PaintH * 0.5f);
	const float Radius = FMath::Min(Size.X * 0.35f, PaintH * 0.45f);

	auto MakePaintGeometry = [&AllottedGeometry](const FVector2D& Pos, const FVector2D& LocalSize)
	{
		return AllottedGeometry.ToPaintGeometry(
			FVector2f(static_cast<float>(LocalSize.X), static_cast<float>(LocalSize.Y)),
			FSlateLayoutTransform(FVector2f(static_cast<float>(Pos.X), static_cast<float>(Pos.Y))));
	};

	// Outer ring (wheel body).
	constexpr int32 RingSegments = 48;
	TArray<FVector2f> Ring;
	for (int32 i = 0; i <= RingSegments; ++i)
	{
		const float A = 2.f * PI * static_cast<float>(i) / RingSegments;
		const FVector2D P = Center + FVector2D(Radius * FMath::Cos(A), Radius * FMath::Sin(A));
		Ring.Add(FVector2f(P.X, P.Y));
	}
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		NextLayer,
		AllottedGeometry.ToPaintGeometry(),
		Ring,
		ESlateDrawEffect::None,
		FLinearColor(0.40f, 0.85f, 1.00f, 0.85f),
		true,
		2.f);

	// Tick marks every 10° rudder (visual range ±60° for readability).
	for (int32 t = -6; t <= 6; ++t)
	{
		const float TickRatio = static_cast<float>(t) / 6.f; // -1..+1
		const float TickAngleDeg = 270.f + TickRatio * 60.f;
		const float Rad = FMath::DegreesToRadians(TickAngleDeg);
		const FVector2D Inner = Center + FVector2D(Radius * 0.85f * FMath::Cos(Rad), Radius * 0.85f * FMath::Sin(Rad));
		const FVector2D Outer = Center + FVector2D(Radius * FMath::Cos(Rad), Radius * FMath::Sin(Rad));
		TArray<FVector2f> Tick;
		Tick.Add(FVector2f(Inner.X, Inner.Y));
		Tick.Add(FVector2f(Outer.X, Outer.Y));
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			NextLayer + 1,
			AllottedGeometry.ToPaintGeometry(),
			Tick,
			ESlateDrawEffect::None,
			FLinearColor(0.60f, 0.85f, 0.90f, 0.7f),
			true,
			(t == 0) ? 2.f : 1.f);
	}

	// Pointer — rotated by DisplayedRudderCmd (-1..+1) visually up to ±60°.
	const float PointerAngleDeg = 270.f + FMath::Clamp(DisplayedRudderCmd, -1.f, 1.f) * 60.f;
	const float PointerRad = FMath::DegreesToRadians(PointerAngleDeg);
	const FVector2D PointerTip = Center + FVector2D(Radius * FMath::Cos(PointerRad), Radius * FMath::Sin(PointerRad));
	TArray<FVector2f> Pointer;
	Pointer.Add(FVector2f(Center.X, Center.Y));
	Pointer.Add(FVector2f(PointerTip.X, PointerTip.Y));
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		NextLayer + 2,
		AllottedGeometry.ToPaintGeometry(),
		Pointer,
		ESlateDrawEffect::None,
		FLinearColor(1.f, 0.85f, 0.20f, 1.f),
		true,
		3.f);

	// Secondary arc — visualises actual yaw rate (DisplayedYawRate in deg/s).
	// Map ±30°/s onto an arc from 12 o'clock sweeping by equivalent angle.
	const float YawArcClamp = 30.f;
	const float YawRatio = FMath::Clamp(DisplayedYawRate / YawArcClamp, -1.f, 1.f);
	if (FMath::Abs(YawRatio) > 0.02f)
	{
		constexpr int32 ArcSteps = 16;
		TArray<FVector2f> YawArc;
		for (int32 i = 0; i <= ArcSteps; ++i)
		{
			const float T = static_cast<float>(i) / ArcSteps;
			const float A = 270.f + YawRatio * 55.f * T;
			const float ArcRad = FMath::DegreesToRadians(A);
			const FVector2D P = Center + FVector2D((Radius * 1.08f) * FMath::Cos(ArcRad), (Radius * 1.08f) * FMath::Sin(ArcRad));
			YawArc.Add(FVector2f(P.X, P.Y));
		}
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			NextLayer + 3,
			AllottedGeometry.ToPaintGeometry(),
			YawArc,
			ESlateDrawEffect::None,
			(YawRatio >= 0.f) ? FLinearColor(0.40f, 1.f, 0.60f, 0.9f) : FLinearColor(1.f, 0.40f, 0.40f, 0.9f),
			true,
			2.f);
	}

	// Center cap.
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		NextLayer + 4,
		MakePaintGeometry(Center - FVector2D(6.f, 6.f), FVector2D(12.f, 12.f)),
		&WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.39f, 0.94f, 0.88f, 1.f));

	return NextLayer + 5;
}

FReply UHelmRudderYokeWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D Size = InGeometry.GetLocalSize();
	const float PaintH = FMath::Min(Size.Y, 140.f);
	// Only capture drag on the paint area.
	if (LocalPos.Y > PaintH)
	{
		return FReply::Unhandled();
	}
	bDragging = true;
	LastDragLocalX = LocalPos.X;
	PushRudderFromDrag(LocalPos.X, Size.X * 0.5f);
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UHelmRudderYokeWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging)
	{
		return FReply::Unhandled();
	}
	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D Size = InGeometry.GetLocalSize();
	PushRudderFromDrag(LocalPos.X, Size.X * 0.5f);
	return FReply::Handled();
}

FReply UHelmRudderYokeWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging)
	{
		return FReply::Unhandled();
	}
	bDragging = false;
	// Release ramp intent to 0 so the spring-back (RudderReturnRate) kicks
	// in if hold is disabled. If hold is on, the last commanded rudder is
	// retained by the backend.
	if (USubHelmWidget* Shell = ResolveRudderYokeShell(this))
	{
		Shell->RouteSetHelmSteer(DisplayedRudderCmd);  // absolute, persists
	}
	return FReply::Handled().ReleaseMouseCapture();
}

void UHelmRudderYokeWidget::PushRudderFromDrag(float LocalX, float WidgetHalfWidth)
{
	// Map [0 .. 2 * HalfWidth] to [-1 .. +1]. Clamp.
	const float Ratio = FMath::Clamp((LocalX - WidgetHalfWidth) / FMath::Max(1.f, WidgetHalfWidth * 0.8f), -1.f, 1.f);
	if (USubHelmWidget* Shell = ResolveRudderYokeShell(this))
	{
		Shell->RouteSetHelmSteer(Ratio);
	}
}

void UHelmRudderYokeWidget::HandleHoldRudderToggle()
{
	if (USubHelmWidget* Shell = ResolveRudderYokeShell(this))
	{
		Shell->RouteSetRudderHoldEnabled(!bRudderHoldCached);
	}
}

void UHelmRudderYokeWidget::HandleRecenter()
{
	if (USubHelmWidget* Shell = ResolveRudderYokeShell(this))
	{
		Shell->RouteSetHelmSteer(0.f);
		// Ensure hold is disabled so the rudder truly returns to zero.
		Shell->RouteSetRudderHoldEnabled(false);
	}
}
