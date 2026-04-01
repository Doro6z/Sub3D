#include "ReconstructionViewWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Input/Reply.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	float EaseInOut01(float Value)
	{
		const float Clamped = FMath::Clamp(Value, 0.f, 1.f);
		return Clamped * Clamped * (3.f - 2.f * Clamped);
	}

	FSlateLayoutTransform MakeLayoutTransform(const FVector2D& Position)
	{
		return FSlateLayoutTransform(FVector2f(static_cast<float>(Position.X), static_cast<float>(Position.Y)));
	}
}

UReconstructionViewWidget::UReconstructionViewWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WindowTitle = FText::FromString(TEXT("Reconstruction View"));
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
}

bool UReconstructionViewWidget::IsReconstructionViewBound() const
{
	return CachedDisplayComponent.IsValid();
}

UHelmNavigationDisplayComponent* UReconstructionViewWidget::GetBoundNavigationDisplay() const
{
	return CachedDisplayComponent.Get();
}

void UReconstructionViewWidget::SetBlendAlpha01(float InBlendAlpha01)
{
	BlendAlpha01 = FMath::Clamp(InBlendAlpha01, 0.f, 1.f);
	Invalidate(EInvalidateWidgetReason::Paint);
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
		return AllottedGeometry.ToPaintGeometry(
			FVector2f(static_cast<float>(LocalSize.X), static_cast<float>(LocalSize.Y)),
			MakeLayoutTransform(LocalPos));
	};

	const auto DrawTextLine = [&](const FVector2D& Position, const FString& Text, const FLinearColor& Color, int32 DrawLayer, const FSlateFontInfo& Font)
	{
		FSlateDrawElement::MakeText(
			OutDrawElements,
			DrawLayer,
			MakePaintGeometry(Position, FVector2D(320.f, 14.f)),
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
		DrawTextLine(ContentPos + FVector2D(8.f, 38.f), TEXT("Hold + drag inside the plot to rotate the reconstruction."), AccentColor, NextLayer + 1, SmallFont);
		return NextLayer + 2;
	}

	const FHelmReconstructionViewData Reconstruction = CachedDisplayComponent->GetReconstructionViewData();
	const FHelmInstrumentStatus InstrumentStatus = CachedDisplayComponent->GetInstrumentStatus();
	const FHelmCrossSectionViewData CrossSection = Reconstruction.CrossSection;
	const FHelmForwardAnticipationViewData ForwardView = Reconstruction.Forward;
	const FTunnelNavStoppingDistanceWarning StopWarning = CachedDisplayComponent->GetStoppingDistanceWarning();
	const FTunnelNavCommitmentWarning CommitmentWarning = CachedDisplayComponent->GetCommitmentWarning();

	if (!Reconstruction.bValid)
	{
		DrawTextLine(ContentPos + FVector2D(8.f, 6.f), TEXT("NO RUNTIME DATA"), WarningColor, NextLayer + 1, TextFont);
		DrawTextLine(ContentPos + FVector2D(8.f, 22.f), TEXT("TunnelNavigationRuntime is not producing a valid reconstruction."), FrameColor, NextLayer + 1, SmallFont);
		return NextLayer + 2;
	}

	const float Blend = EaseInOut01(BlendAlpha01);
	const float CrossAlpha = 1.f - Blend;
	const float ForwardAlpha = Blend;

	const float HeaderStatusHeight = 18.f;
	const float FooterHeight = 44.f;
	const FVector2D PlotPos = ContentPos + FVector2D(8.f, 8.f + HeaderStatusHeight);
	const FVector2D PlotSize(ContentSize.X - 16.f, FMath::Max(40.f, ContentSize.Y - FooterHeight - 16.f - HeaderStatusHeight));
	const FVector2D PlotCenter = PlotPos + PlotSize * 0.5f;
	const float PlotRadius = FMath::Min(PlotSize.X, PlotSize.Y) * 0.38f;
	DrawTextLine(
		ContentPos + FVector2D(8.f, 6.f),
		FString::Printf(
			TEXT("RECON %.1fms   AGE %.0fms   CONF %d%%   %s"),
			InstrumentStatus.ReconstructionTimeMs,
			InstrumentStatus.UpdateAgeMs,
			FMath::RoundToInt(InstrumentStatus.Confidence01 * 100.f),
			InstrumentStatus.bProjectionSuspect ? TEXT("SUSPECT") : (InstrumentStatus.bStale ? TEXT("STALE") : TEXT("STABLE"))),
		InstrumentStatus.bProjectionSuspect ? WarningColor : AccentColor,
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

	if (CrossSection.bValid && CrossAlpha > KINDA_SMALL_NUMBER)
	{
		const FVector2D CrossCenter(PlotCenter.X - Blend * PlotSize.X * 0.12f, PlotCenter.Y);
		const float CrossRadius = PlotRadius * FMath::Lerp(1.f, 0.82f, Blend);
		const FLinearColor CrossColor = FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, CrossAlpha);
		const FLinearColor HeadingColor = FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, CrossAlpha * 0.95f);
		const FLinearColor VelocityLineColor = FLinearColor(VelocityColor.R, VelocityColor.G, VelocityColor.B, CrossAlpha * 0.95f);

		if (CrossSection.ContourPointsNormalized.Num() >= 2)
		{
			TArray<FVector2f> Contour;
			Contour.Reserve(CrossSection.ContourPointsNormalized.Num());
			for (const FVector2D& Point : CrossSection.ContourPointsNormalized)
			{
				const FVector2D P = CrossCenter + FVector2D(Point.X, Point.Y) * CrossRadius;
				Contour.Add(FVector2f(P.X, P.Y));
			}
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), Contour, ESlateDrawEffect::None, CrossColor, true, 1.6f);
		}

		TArray<FVector2f> AxisH;
		AxisH.Add(FVector2f(CrossCenter.X - CrossRadius, CrossCenter.Y));
		AxisH.Add(FVector2f(CrossCenter.X + CrossRadius, CrossCenter.Y));
		TArray<FVector2f> AxisV;
		AxisV.Add(FVector2f(CrossCenter.X, CrossCenter.Y - CrossRadius));
		AxisV.Add(FVector2f(CrossCenter.X, CrossCenter.Y + CrossRadius));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), AxisH, ESlateDrawEffect::None, FLinearColor(FrameColor.R, FrameColor.G, FrameColor.B, CrossAlpha * 0.55f), true, 1.f);
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), AxisV, ESlateDrawEffect::None, FLinearColor(FrameColor.R, FrameColor.G, FrameColor.B, CrossAlpha * 0.55f), true, 1.f);

		const FVector2D SubPos = CrossCenter + CrossSection.SubOffsetNormalized * CrossRadius;
		const FVector2D HeadingEnd = SubPos + CrossSection.HeadingVectorNormalized * CrossRadius;
		const FVector2D VelocityEnd = SubPos + CrossSection.VelocityVectorNormalized * CrossRadius;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer + 1,
			MakePaintGeometry(SubPos - FVector2D(4.f, 4.f), FVector2D(8.f, 8.f)),
			&WhiteBrush,
			ESlateDrawEffect::None,
			CrossSection.bHardClearanceViolation ? FLinearColor(WarningColor.R, WarningColor.G, WarningColor.B, CrossAlpha) : CrossColor);

		TArray<FVector2f> HeadingLine;
		HeadingLine.Add(FVector2f(SubPos.X, SubPos.Y));
		HeadingLine.Add(FVector2f(HeadingEnd.X, HeadingEnd.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), HeadingLine, ESlateDrawEffect::None, HeadingColor, true, 1.3f);

		TArray<FVector2f> VelocityLine;
		VelocityLine.Add(FVector2f(SubPos.X, SubPos.Y));
		VelocityLine.Add(FVector2f(VelocityEnd.X, VelocityEnd.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), VelocityLine, ESlateDrawEffect::None, VelocityLineColor, true, 1.5f);

		const float GaugeHeight = PlotRadius * 0.95f;
		const FVector2D GaugeUpPos(PlotPos.X + 8.f, PlotCenter.Y - GaugeHeight);
		const FVector2D GaugeDownPos(PlotPos.X + 24.f, PlotCenter.Y);
		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer, MakePaintGeometry(GaugeUpPos, FVector2D(10.f, GaugeHeight)), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.22f * CrossAlpha));
		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer, MakePaintGeometry(GaugeDownPos, FVector2D(10.f, GaugeHeight)), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.22f * CrossAlpha));
		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer + 1, MakePaintGeometry(GaugeUpPos + FVector2D(0.f, GaugeHeight * (1.f - CrossSection.SafeAbove01)), FVector2D(10.f, GaugeHeight * CrossSection.SafeAbove01)), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(SafeColor.R, SafeColor.G, SafeColor.B, CrossAlpha));
		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer + 1, MakePaintGeometry(GaugeDownPos, FVector2D(10.f, GaugeHeight * CrossSection.SafeBelow01)), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(SafeColor.R, SafeColor.G, SafeColor.B, CrossAlpha));
	}

	if (ForwardView.bValid && ForwardAlpha > KINDA_SMALL_NUMBER)
	{
		const FVector2D GraphPos = PlotPos + FVector2D(Blend * 12.f, PlotSize.Y * 0.1f);
		const FVector2D GraphSize(PlotSize.X - Blend * 12.f, PlotSize.Y * 0.8f);
		const FVector2D Center(GraphPos.X + GraphSize.X * 0.5f, GraphPos.Y + GraphSize.Y * 0.5f);
		const float HalfHeight = GraphSize.Y * 0.35f;
		TArray<FVector2f> CeilingLine;
		TArray<FVector2f> FloorLine;
		TArray<FVector2f> TurnGuideLine;

		for (const FHelmForwardAnticipationSampleViewData& Sample : ForwardView.Samples)
		{
			const float X = GraphPos.X + Sample.Distance01 * GraphSize.X;
			const float CeilingY = Center.Y - Sample.Ceiling01 * HalfHeight;
			const float FloorY = Center.Y + Sample.Floor01 * HalfHeight;
			CeilingLine.Add(FVector2f(X, CeilingY));
			FloorLine.Add(FVector2f(X, FloorY));
			const float TurnGuideY = GraphPos.Y + GraphSize.Y * 0.12f + ((1.f - ((Sample.TurnSigned01 + 1.f) * 0.5f)) * GraphSize.Y * 0.18f);
			TurnGuideLine.Add(FVector2f(X, TurnGuideY));

			const float BarBaseY = GraphPos.Y + GraphSize.Y;
			const float BarHeight = Sample.Width01 * (GraphSize.Y * 0.18f);
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
			BarColor.A = ForwardAlpha * 0.95f;

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
			const FLinearColor ForwardColor(AccentColor.R, AccentColor.G, AccentColor.B, ForwardAlpha);
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), CeilingLine, ESlateDrawEffect::None, ForwardColor, true, 1.4f);
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), FloorLine, ESlateDrawEffect::None, ForwardColor, true, 1.4f);
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), TurnGuideLine, ESlateDrawEffect::None, FLinearColor(VelocityColor.R, VelocityColor.G, VelocityColor.B, ForwardAlpha * 0.75f), true, 1.1f);
		}

		const float SubMarkerX = GraphPos.X + 6.f;
		const float SubMarkerCenterY = Center.Y + CrossSection.SubOffsetNormalized.Y * HalfHeight;
		const FVector2D SubMarkerSize(12.f, 20.f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer + 1,
			MakePaintGeometry(FVector2D(SubMarkerX - SubMarkerSize.X * 0.5f, SubMarkerCenterY - SubMarkerSize.Y * 0.5f), SubMarkerSize),
			&WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, ForwardAlpha));

		TArray<FVector2f> OriginGuide;
		OriginGuide.Add(FVector2f(SubMarkerX + 10.f, GraphPos.Y));
		OriginGuide.Add(FVector2f(SubMarkerX + 10.f, GraphPos.Y + GraphSize.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), OriginGuide, ESlateDrawEffect::None, FLinearColor(FrameColor.R, FrameColor.G, FrameColor.B, ForwardAlpha * 0.75f), true, 1.f);

		if (ForwardView.LookaheadMeters > KINDA_SMALL_NUMBER && ForwardView.StoppingDistanceMeters >= 0.f)
		{
			const float Stop01 = FMath::Clamp(ForwardView.StoppingDistanceMeters / FMath::Max(ForwardView.LookaheadMeters, 0.01f), 0.f, 1.f);
			const float StopX = GraphPos.X + Stop01 * GraphSize.X;
			TArray<FVector2f> StopLine;
			StopLine.Add(FVector2f(StopX, GraphPos.Y));
			StopLine.Add(FVector2f(StopX, GraphPos.Y + GraphSize.Y));
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), StopLine, ESlateDrawEffect::None, FLinearColor(VelocityColor.R, VelocityColor.G, VelocityColor.B, ForwardAlpha), true, 1.2f);
		}
	}

	const FVector2D FooterPos(ContentPos.X + 8.f, ContentPos.Y + ContentSize.Y - FooterHeight + 6.f);
	DrawTextLine(FooterPos, FString::Printf(TEXT("SAFE  UP %.1fm   DN %.1fm   LF %.1fm   RT %.1fm"), CrossSection.SafeAboveCm / 100.f, CrossSection.SafeBelowCm / 100.f, CrossSection.SafeLeftCm / 100.f, CrossSection.SafeRightCm / 100.f), CrossSection.bNearWallWarning ? WarningColor : AccentColor, NextLayer + 2, SmallFont);
	DrawTextLine(FooterPos + FVector2D(0.f, 14.f), FString::Printf(TEXT("AHEAD REC %.1f km/h   STOP %.1fm   CRIT %.1fm"), ForwardView.RecommendedMaxSpeedKmh, ForwardView.StoppingDistanceMeters, ForwardView.FirstCriticalObstacleMeters), StopWarning.bWarning ? WarningColor : FrameColor, NextLayer + 2, SmallFont);
	DrawTextLine(FooterPos + FVector2D(0.f, 28.f), FString::Printf(TEXT("RECON BLEND %.0f%%   %s"), Blend * 100.f, CommitmentWarning.bWarning ? TEXT("COMMIT WARN") : TEXT("FLOW OK")), CommitmentWarning.bWarning ? WarningColor : AccentColor, NextLayer + 2, SmallFont);

	const float SliderWidth = FMath::Min(120.f, ContentSize.X * 0.32f);
	const FVector2D SliderPos(ContentPos.X + ContentSize.X - SliderWidth - 12.f, FooterPos.Y + 16.f);
	FSlateDrawElement::MakeBox(OutDrawElements, NextLayer + 1, MakePaintGeometry(SliderPos, FVector2D(SliderWidth, 6.f)), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.35f));
	FSlateDrawElement::MakeBox(OutDrawElements, NextLayer + 2, MakePaintGeometry(SliderPos, FVector2D(SliderWidth * Blend, 6.f)), &WhiteBrush, ESlateDrawEffect::None, AccentColor);
	DrawTextLine(SliderPos + FVector2D(0.f, -14.f), TEXT("HOLD + DRAG TO ROTATE RECONSTRUCTION"), FrameColor, NextLayer + 2, SmallFont);

	return NextLayer + 3;
}

FReply UReconstructionViewWidget::HandleContentMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition)
{
	if (!bEnableBlendInteraction || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	bAdjustingBlend = true;
	BlendDragStartScreenX = InMouseEvent.GetScreenSpacePosition().X;
	BlendDragStartValue = BlendAlpha01;
	return MakeHandledReply(true);
}

FReply UReconstructionViewWidget::HandleContentMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition)
{
	if (!bAdjustingBlend || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	bAdjustingBlend = false;
	return MakeHandledReply(false);
}

FReply UReconstructionViewWidget::HandleContentMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition)
{
	if (!bAdjustingBlend)
	{
		return FReply::Unhandled();
	}

	const float DeltaX = InMouseEvent.GetScreenSpacePosition().X - BlendDragStartScreenX;
	SetBlendAlpha01(BlendDragStartValue + (DeltaX / FMath::Max(BlendSensitivityPx, 1.f)));
	return MakeHandledReply(true);
}

void UReconstructionViewWidget::HandleContentMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (bAdjustingBlend && !InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		bAdjustingBlend = false;
	}
}
