#include "ReconstructionViewWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Input/Reply.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	FSlateLayoutTransform MakeReconstructionLayoutTransform(const FVector2D& Position)
	{
		return FSlateLayoutTransform(FVector2f(static_cast<float>(Position.X), static_cast<float>(Position.Y)));
	}

	float SafeMetersFromCentimeters(const float ValueCm)
	{
		return ValueCm / 100.f;
	}
}

UReconstructionViewWidget::UReconstructionViewWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RefreshWindowTitle();
}

void UReconstructionViewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CachedDisplayComponent.IsValid())
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void UReconstructionViewWidget::InitForReconstructionView(UHelmNavigationDisplayComponent* InDisplayComponent)
{
	CachedDisplayComponent = InDisplayComponent;
	RefreshWindowTitle();
}

bool UReconstructionViewWidget::IsReconstructionViewBound() const
{
	return CachedDisplayComponent.IsValid();
}

UHelmNavigationDisplayComponent* UReconstructionViewWidget::GetBoundNavigationDisplay() const
{
	return CachedDisplayComponent.Get();
}

void UReconstructionViewWidget::SetViewMode(const EReconstructionViewMode InViewMode)
{
	if (ViewMode == InViewMode)
	{
		RefreshWindowTitle();
		return;
	}

	ViewMode = InViewMode;
	RefreshWindowTitle();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UReconstructionViewWidget::SetBlendAlpha01(const float InBlendAlpha01)
{
	BlendAlpha01 = FMath::Clamp(InBlendAlpha01, 0.f, 1.f);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UReconstructionViewWidget::RefreshWindowTitle()
{
	switch (ViewMode)
	{
	case EReconstructionViewMode::ForwardProfile:
		SetWindowTitle(FText::FromString(TEXT("FORWARD PROFILE / SIDE")));
		break;
	case EReconstructionViewMode::CrossSection:
	default:
		SetWindowTitle(FText::FromString(TEXT("CROSS SECTION / FACE")));
		break;
	}
}

int32 UReconstructionViewWidget::PaintInstrumentContent(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled,
	const FSlateRect& ContentRect) const
{
	int32 NextLayer = LayerId;
	static const FSlateColorBrush WhiteBrush(FLinearColor::White);
	const FSlateFontInfo TextFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9);

	const auto MakePaintGeometry = [&AllottedGeometry](const FVector2D& LocalPos, const FVector2D& LocalSize)
	{
		return AllottedGeometry.MakeChild(
			FVector2f(static_cast<float>(LocalSize.X), static_cast<float>(LocalSize.Y)),
			MakeReconstructionLayoutTransform(LocalPos)).ToPaintGeometry();
	};

	const auto DrawTextLine = [&](const FVector2D& Position, const FString& Text, const FLinearColor& Color, const int32 DrawLayer, const FSlateFontInfo& Font)
	{
		FSlateDrawElement::MakeText(
			OutDrawElements,
			DrawLayer,
			MakePaintGeometry(Position, FVector2D(420.f, 16.f)),
			Text,
			Font,
			ESlateDrawEffect::None,
			Color);
	};

	const FVector2D ContentPos(ContentRect.Left, ContentRect.Top);
	const FVector2D ContentSize(ContentRect.Right - ContentRect.Left, ContentRect.Bottom - ContentRect.Top);
	if (ContentSize.X <= 10.f || ContentSize.Y <= 10.f)
	{
		return NextLayer;
	}

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		NextLayer++,
		MakePaintGeometry(ContentPos, ContentSize),
		&WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.01f, 0.02f, 0.03f, 0.55f));

	if (!CachedDisplayComponent.IsValid())
	{
		DrawTextLine(ContentPos + FVector2D(8.f, 6.f), TEXT("UNBOUND"), WarningColor, NextLayer + 1, TextFont);
		DrawTextLine(ContentPos + FVector2D(8.f, 22.f), TEXT("Bind this widget explicitly from WBP_SubHelm."), FrameColor, NextLayer + 1, SmallFont);
		DrawTextLine(
			ContentPos + FVector2D(8.f, 38.f),
			ViewMode == EReconstructionViewMode::CrossSection
				? TEXT("This panel shows the face slice around the hull.")
				: TEXT("This panel shows the forward ceiling/floor profile."),
			AccentColor,
			NextLayer + 1,
			SmallFont);
		return NextLayer + 2;
	}

	const FHelmInstrumentStatus InstrumentStatus = CachedDisplayComponent->GetInstrumentStatus();
	const FHelmCrossSectionViewData CrossSection = CachedDisplayComponent->GetCrossSectionViewData();
	const FHelmForwardAnticipationViewData ForwardView = CachedDisplayComponent->GetForwardAnticipationViewData();
	const FTunnelNavStoppingDistanceWarning StopWarning = CachedDisplayComponent->GetStoppingDistanceWarning();
	const FTunnelNavCommitmentWarning CommitmentWarning = CachedDisplayComponent->GetCommitmentWarning();

	if (ViewMode == EReconstructionViewMode::CrossSection)
	{
		if (!CrossSection.bValid)
		{
			DrawTextLine(ContentPos + FVector2D(8.f, 6.f), TEXT("NO CROSS SECTION"), WarningColor, NextLayer + 1, TextFont);
			DrawTextLine(ContentPos + FVector2D(8.f, 22.f), TEXT("TunnelNavigationRuntime is not producing a face slice."), FrameColor, NextLayer + 1, SmallFont);
			return NextLayer + 2;
		}

		const float HeaderStatusHeight = 18.f;
		const float FooterHeight = 36.f;
		const FVector2D PlotPos = ContentPos + FVector2D(8.f, 8.f + HeaderStatusHeight);
		const FVector2D PlotSize(ContentSize.X - 16.f, FMath::Max(40.f, ContentSize.Y - FooterHeight - 16.f - HeaderStatusHeight));
		const FVector2D PlotCenter = PlotPos + PlotSize * 0.5f;
		const float PlotRadius = FMath::Min(PlotSize.X, PlotSize.Y) * 0.38f;

		DrawTextLine(
			ContentPos + FVector2D(8.f, 6.f),
			FString::Printf(
				TEXT("FACE SLICE   CONF %d%%   %s"),
				FMath::RoundToInt(InstrumentStatus.Confidence01 * 100.f),
				CrossSection.bHardClearanceViolation ? TEXT("HARD VIOLATION") : (CrossSection.bNearWallWarning ? TEXT("NEAR WALL") : TEXT("CLEAR"))),
			CrossSection.bHardClearanceViolation || CrossSection.bNearWallWarning ? WarningColor : AccentColor,
			NextLayer + 1,
			SmallFont);

		for (int32 GridIndex = 1; GridIndex <= 5; ++GridIndex)
		{
			const float T = static_cast<float>(GridIndex) / 6.f;
			const float X = PlotPos.X + T * PlotSize.X;
			TArray<FVector2f> VerticalLine;
			VerticalLine.Add(FVector2f(X, PlotPos.Y));
			VerticalLine.Add(FVector2f(X, PlotPos.Y + PlotSize.Y));
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), VerticalLine, ESlateDrawEffect::None, FLinearColor(FrameColor.R, FrameColor.G, FrameColor.B, 0.18f), true, 1.f);
		}

		for (int32 GridIndex = 1; GridIndex <= 3; ++GridIndex)
		{
			const float T = static_cast<float>(GridIndex) / 4.f;
			const float Y = PlotPos.Y + T * PlotSize.Y;
			TArray<FVector2f> HorizontalLine;
			HorizontalLine.Add(FVector2f(PlotPos.X, Y));
			HorizontalLine.Add(FVector2f(PlotPos.X + PlotSize.X, Y));
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), HorizontalLine, ESlateDrawEffect::None, FLinearColor(FrameColor.R, FrameColor.G, FrameColor.B, 0.18f), true, 1.f);
		}
		++NextLayer;

		const FLinearColor CrossColor = CrossSection.bHardClearanceViolation ? WarningColor : AccentColor;
		if (CrossSection.ContourPointsNormalized.Num() >= 2)
		{
			TArray<FVector2f> Contour;
			Contour.Reserve(CrossSection.ContourPointsNormalized.Num());
			for (const FVector2D& Point : CrossSection.ContourPointsNormalized)
			{
				const FVector2D P = PlotCenter + FVector2D(Point.X, Point.Y) * PlotRadius;
				Contour.Add(FVector2f(P.X, P.Y));
			}
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), Contour, ESlateDrawEffect::None, CrossColor, true, 1.6f);
		}

		TArray<FVector2f> AxisH;
		AxisH.Add(FVector2f(PlotCenter.X - PlotRadius, PlotCenter.Y));
		AxisH.Add(FVector2f(PlotCenter.X + PlotRadius, PlotCenter.Y));
		TArray<FVector2f> AxisV;
		AxisV.Add(FVector2f(PlotCenter.X, PlotCenter.Y - PlotRadius));
		AxisV.Add(FVector2f(PlotCenter.X, PlotCenter.Y + PlotRadius));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), AxisH, ESlateDrawEffect::None, FLinearColor(FrameColor.R, FrameColor.G, FrameColor.B, 0.55f), true, 1.f);
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), AxisV, ESlateDrawEffect::None, FLinearColor(FrameColor.R, FrameColor.G, FrameColor.B, 0.55f), true, 1.f);

		const FVector2D SubPos = PlotCenter + CrossSection.SubOffsetNormalized * PlotRadius;
		const FVector2D HeadingEnd = SubPos + CrossSection.HeadingVectorNormalized * PlotRadius;
		const FVector2D VelocityEnd = SubPos + CrossSection.VelocityVectorNormalized * PlotRadius;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer + 1,
			MakePaintGeometry(SubPos - FVector2D(4.f, 4.f), FVector2D(8.f, 8.f)),
			&WhiteBrush,
			ESlateDrawEffect::None,
			CrossColor);

		TArray<FVector2f> HeadingLine;
		HeadingLine.Add(FVector2f(SubPos.X, SubPos.Y));
		HeadingLine.Add(FVector2f(HeadingEnd.X, HeadingEnd.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), HeadingLine, ESlateDrawEffect::None, AccentColor, true, 1.3f);

		TArray<FVector2f> VelocityLine;
		VelocityLine.Add(FVector2f(SubPos.X, SubPos.Y));
		VelocityLine.Add(FVector2f(VelocityEnd.X, VelocityEnd.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), VelocityLine, ESlateDrawEffect::None, VelocityColor, true, 1.5f);

		const FVector2D TopGaugePos(PlotCenter.X - 40.f, PlotPos.Y + 6.f);
		const FVector2D BottomGaugePos(PlotCenter.X - 40.f, PlotPos.Y + PlotSize.Y - 18.f);
		const FVector2D LeftGaugePos(PlotPos.X + 6.f, PlotCenter.Y - 40.f);
		const FVector2D RightGaugePos(PlotPos.X + PlotSize.X - 18.f, PlotCenter.Y - 40.f);

		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer, MakePaintGeometry(TopGaugePos, FVector2D(80.f, 10.f)), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.22f));
		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer, MakePaintGeometry(BottomGaugePos, FVector2D(80.f, 10.f)), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.22f));
		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer, MakePaintGeometry(LeftGaugePos, FVector2D(10.f, 80.f)), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.22f));
		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer, MakePaintGeometry(RightGaugePos, FVector2D(10.f, 80.f)), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.22f));

		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer + 1, MakePaintGeometry(TopGaugePos, FVector2D(80.f * FMath::Clamp(CrossSection.SafeAbove01, 0.f, 1.f), 10.f)), &WhiteBrush, ESlateDrawEffect::None, SafeColor);
		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer + 1, MakePaintGeometry(BottomGaugePos, FVector2D(80.f * FMath::Clamp(CrossSection.SafeBelow01, 0.f, 1.f), 10.f)), &WhiteBrush, ESlateDrawEffect::None, SafeColor);
		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer + 1, MakePaintGeometry(LeftGaugePos + FVector2D(0.f, 80.f * (1.f - FMath::Clamp(CrossSection.SafeLeftCm / FMath::Max(CrossSection.ClearanceMarginCm * 2.f, 1.f), 0.f, 1.f))), FVector2D(10.f, 80.f * FMath::Clamp(CrossSection.SafeLeftCm / FMath::Max(CrossSection.ClearanceMarginCm * 2.f, 1.f), 0.f, 1.f))), &WhiteBrush, ESlateDrawEffect::None, SafeColor);
		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer + 1, MakePaintGeometry(RightGaugePos + FVector2D(0.f, 80.f * (1.f - FMath::Clamp(CrossSection.SafeRightCm / FMath::Max(CrossSection.ClearanceMarginCm * 2.f, 1.f), 0.f, 1.f))), FVector2D(10.f, 80.f * FMath::Clamp(CrossSection.SafeRightCm / FMath::Max(CrossSection.ClearanceMarginCm * 2.f, 1.f), 0.f, 1.f))), &WhiteBrush, ESlateDrawEffect::None, SafeColor);

		const FVector2D FooterPos(ContentPos.X + 8.f, ContentPos.Y + ContentSize.Y - FooterHeight + 6.f);
		DrawTextLine(
			FooterPos,
			FString::Printf(
				TEXT("UP %.1fm   DOWN %.1fm   LEFT %.1fm   RIGHT %.1fm"),
				SafeMetersFromCentimeters(CrossSection.SafeAboveCm),
				SafeMetersFromCentimeters(CrossSection.SafeBelowCm),
				SafeMetersFromCentimeters(CrossSection.SafeLeftCm),
				SafeMetersFromCentimeters(CrossSection.SafeRightCm)),
			CrossSection.bNearWallWarning ? WarningColor : AccentColor,
			NextLayer + 2,
			SmallFont);
		DrawTextLine(
			FooterPos + FVector2D(0.f, 14.f),
			FString::Printf(TEXT("CENTER R %.1fm   U %.1fm   MARGIN %.1fm"), SafeMetersFromCentimeters(CrossSection.OffsetRightCm), SafeMetersFromCentimeters(CrossSection.OffsetUpCm), SafeMetersFromCentimeters(CrossSection.ClearanceMarginCm)),
			CrossSection.bHardClearanceViolation ? WarningColor : FrameColor,
			NextLayer + 2,
			SmallFont);

		return NextLayer + 3;
	}

	if (!ForwardView.bValid)
	{
		DrawTextLine(ContentPos + FVector2D(8.f, 6.f), TEXT("NO FORWARD PROFILE"), WarningColor, NextLayer + 1, TextFont);
		DrawTextLine(ContentPos + FVector2D(8.f, 22.f), TEXT("TunnelNavigationRuntime is not producing a forward side profile."), FrameColor, NextLayer + 1, SmallFont);
		return NextLayer + 2;
	}

	const float HeaderStatusHeight = 18.f;
	const float FooterHeight = 36.f;
	const FVector2D PlotPos = ContentPos + FVector2D(8.f, 8.f + HeaderStatusHeight);
	const FVector2D PlotSize(ContentSize.X - 16.f, FMath::Max(40.f, ContentSize.Y - FooterHeight - 16.f - HeaderStatusHeight));
	const FVector2D Center(PlotPos.X + PlotSize.X * 0.5f, PlotPos.Y + PlotSize.Y * 0.5f);
	const float HalfHeight = PlotSize.Y * 0.35f;

	DrawTextLine(
		ContentPos + FVector2D(8.f, 6.f),
		FString::Printf(
			TEXT("FORWARD SIDE   LOOKAHEAD %.0fm   %s"),
			ForwardView.LookaheadMeters,
			ForwardView.bCrashStopAlreadyLate ? TEXT("BRAKE LATE") : (ForwardView.bCommitmentZone ? TEXT("COMMIT ZONE") : TEXT("CLEAR"))),
		ForwardView.bCrashStopAlreadyLate || ForwardView.bCommitmentZone ? WarningColor : AccentColor,
		NextLayer + 1,
		SmallFont);

	for (int32 GridIndex = 1; GridIndex <= 5; ++GridIndex)
	{
		const float T = static_cast<float>(GridIndex) / 6.f;
		const float X = PlotPos.X + T * PlotSize.X;
		TArray<FVector2f> VerticalLine;
		VerticalLine.Add(FVector2f(X, PlotPos.Y));
		VerticalLine.Add(FVector2f(X, PlotPos.Y + PlotSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), VerticalLine, ESlateDrawEffect::None, FLinearColor(FrameColor.R, FrameColor.G, FrameColor.B, 0.18f), true, 1.f);
	}

	TArray<FVector2f> CenterLine;
	CenterLine.Add(FVector2f(PlotPos.X, Center.Y));
	CenterLine.Add(FVector2f(PlotPos.X + PlotSize.X, Center.Y));
	FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), CenterLine, ESlateDrawEffect::None, FLinearColor(FrameColor.R, FrameColor.G, FrameColor.B, 0.35f), true, 1.f);
	++NextLayer;

	TArray<FVector2f> CeilingLine;
	TArray<FVector2f> FloorLine;
	for (const FHelmForwardAnticipationSampleViewData& Sample : ForwardView.Samples)
	{
		const float X = PlotPos.X + Sample.Distance01 * PlotSize.X;
		const float CeilingY = Center.Y - Sample.Ceiling01 * HalfHeight;
		const float FloorY = Center.Y + Sample.Floor01 * HalfHeight;
		CeilingLine.Add(FVector2f(X, CeilingY));
		FloorLine.Add(FVector2f(X, FloorY));

		const float BarBaseY = PlotPos.Y + PlotSize.Y;
		const float BarHeight = Sample.Width01 * (PlotSize.Y * 0.18f);
		FLinearColor BarColor = AccentColor;
		if (Sample.bCritical)
		{
			BarColor = WarningColor;
		}
		else if (Sample.bNoTurnZone)
		{
			BarColor = VelocityColor;
		}
		else if (Sample.bHubTransition)
		{
			BarColor = SafeColor;
		}

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer,
			MakePaintGeometry(FVector2D(X - 1.f, BarBaseY - BarHeight), FVector2D(2.f, BarHeight)),
			&WhiteBrush,
			ESlateDrawEffect::None,
			BarColor);
	}

	if (CeilingLine.Num() >= 2)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), CeilingLine, ESlateDrawEffect::None, AccentColor, true, 1.6f);
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), FloorLine, ESlateDrawEffect::None, AccentColor, true, 1.6f);
	}

	const float SubMarkerX = PlotPos.X + 10.f;
	const float SubMarkerCenterY = Center.Y + (CrossSection.bValid ? CrossSection.SubOffsetNormalized.Y * HalfHeight : 0.f);
	const FVector2D SubMarkerSize(12.f, 20.f);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		NextLayer + 1,
		MakePaintGeometry(FVector2D(SubMarkerX - SubMarkerSize.X * 0.5f, SubMarkerCenterY - SubMarkerSize.Y * 0.5f), SubMarkerSize),
		&WhiteBrush,
		ESlateDrawEffect::None,
		AccentColor);

	if (ForwardView.LookaheadMeters > KINDA_SMALL_NUMBER && ForwardView.StoppingDistanceMeters >= 0.f)
	{
		const float Stop01 = FMath::Clamp(ForwardView.StoppingDistanceMeters / FMath::Max(ForwardView.LookaheadMeters, 0.01f), 0.f, 1.f);
		const float StopX = PlotPos.X + Stop01 * PlotSize.X;
		TArray<FVector2f> StopLine;
		StopLine.Add(FVector2f(StopX, PlotPos.Y));
		StopLine.Add(FVector2f(StopX, PlotPos.Y + PlotSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), StopLine, ESlateDrawEffect::None, VelocityColor, true, 1.2f);
	}

	const FVector2D FooterPos(ContentPos.X + 8.f, ContentPos.Y + ContentSize.Y - FooterHeight + 6.f);
	DrawTextLine(
		FooterPos,
		FString::Printf(
			TEXT("LOOKAHEAD %.0fm   STOP %.1fm   CRIT %.1fm"),
			ForwardView.LookaheadMeters,
			ForwardView.StoppingDistanceMeters,
			ForwardView.FirstCriticalObstacleMeters),
		StopWarning.bWarning ? WarningColor : AccentColor,
		NextLayer + 2,
		SmallFont);
	DrawTextLine(
		FooterPos + FVector2D(0.f, 14.f),
		FString::Printf(
			TEXT("RECOMMENDED %.1f KM/H   %s"),
			ForwardView.RecommendedMaxSpeedKmh,
			CommitmentWarning.bWarning ? TEXT("COMMIT WARNING") : TEXT("FLOW OK")),
		CommitmentWarning.bWarning ? WarningColor : FrameColor,
		NextLayer + 2,
		SmallFont);

	return NextLayer + 3;
}

FReply UReconstructionViewWidget::HandleContentMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition)
{
	bAdjustingBlend = false;
	return FReply::Unhandled();
}

FReply UReconstructionViewWidget::HandleContentMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition)
{
	bAdjustingBlend = false;
	return FReply::Unhandled();
}

FReply UReconstructionViewWidget::HandleContentMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition)
{
	return FReply::Unhandled();
}

void UReconstructionViewWidget::HandleContentMouseLeave(const FPointerEvent& InMouseEvent)
{
	bAdjustingBlend = false;
}
