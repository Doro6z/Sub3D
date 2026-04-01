#include "HelmNavigationDisplayWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
FString SpaceModeShortLabel(ETunnelNavSpaceMode SpaceMode)
{
	switch (SpaceMode)
	{
	case ETunnelNavSpaceMode::CavernHub:
		return TEXT("HUB");
	case ETunnelNavSpaceMode::Transition:
		return TEXT("TRANS");
	case ETunnelNavSpaceMode::Corridor:
	default:
		return TEXT("CORR");
	}
}
}

void UHelmNavigationDisplayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CachedDisplayComponent.IsValid())
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

int32 UHelmNavigationDisplayWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 NextLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bEnableNativePaint)
	{
		return NextLayer;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= 10.f || Size.Y <= 10.f)
	{
		return NextLayer;
	}

	const bool bHasBoundDisplay = CachedDisplayComponent.IsValid();
	const FHelmCrossSectionViewData CrossSection = bHasBoundDisplay ? CachedDisplayComponent->GetCrossSectionViewData() : FHelmCrossSectionViewData();
	const FHelmForwardAnticipationViewData ForwardView = bHasBoundDisplay ? CachedDisplayComponent->GetForwardAnticipationViewData() : FHelmForwardAnticipationViewData();
	const FHelmTacticalGraphViewData GraphView = bHasBoundDisplay ? CachedDisplayComponent->GetTacticalGraphViewData() : FHelmTacticalGraphViewData();
	const FTunnelNavCommitmentWarning CommitmentWarning = bHasBoundDisplay ? CachedDisplayComponent->GetCommitmentWarning() : FTunnelNavCommitmentWarning();

	static const FSlateColorBrush WhiteBrush(FLinearColor::White);
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12);
	const FSlateFontInfo TextFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);

	const auto MakePaintGeometry = [&AllottedGeometry](const FVector2D& LocalPos, const FVector2D& LocalSize)
	{
		return AllottedGeometry.ToPaintGeometry(
			FVector2f(static_cast<float>(LocalSize.X), static_cast<float>(LocalSize.Y)),
			FSlateLayoutTransform(FVector2f(static_cast<float>(LocalPos.X), static_cast<float>(LocalPos.Y))));
	};

	const auto DrawPanel = [&](const FVector2D& Pos, const FVector2D& PanelSize, const FString& Title, int32 DrawLayer)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			DrawLayer,
			MakePaintGeometry(Pos, PanelSize),
			&WhiteBrush,
			ESlateDrawEffect::None,
			PanelColor);

		TArray<FVector2f> FramePoints;
		FramePoints.Add(FVector2f(Pos.X, Pos.Y));
		FramePoints.Add(FVector2f(Pos.X + PanelSize.X, Pos.Y));
		FramePoints.Add(FVector2f(Pos.X + PanelSize.X, Pos.Y + PanelSize.Y));
		FramePoints.Add(FVector2f(Pos.X, Pos.Y + PanelSize.Y));
		FramePoints.Add(FVector2f(Pos.X, Pos.Y));
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			DrawLayer + 1,
			AllottedGeometry.ToPaintGeometry(),
			FramePoints,
			ESlateDrawEffect::None,
			FrameColor,
			true,
			1.1f);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			DrawLayer + 2,
			MakePaintGeometry(Pos + FVector2D(8.f, 6.f), FVector2D(PanelSize.X - 16.f, 16.f)),
			Title,
			TitleFont,
			ESlateDrawEffect::None,
			AccentColor);
	};

	const auto DrawTextLine = [&](const FVector2D& Pos, const FString& Text, const FLinearColor& Color, int32 DrawLayer)
	{
		FSlateDrawElement::MakeText(
			OutDrawElements,
			DrawLayer,
			MakePaintGeometry(Pos, FVector2D(260.f, 14.f)),
			Text,
			TextFont,
			ESlateDrawEffect::None,
			Color);
	};

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		NextLayer++,
		MakePaintGeometry(FVector2D::ZeroVector, Size),
		&WhiteBrush,
		ESlateDrawEffect::None,
		BackgroundColor);

	const float PanelGap = FMath::Max(4.f, PanelPaddingPx);
	const float AvailableW = Size.X - PanelGap * 4.f;
	const FVector2D CrossPanelSize(AvailableW * 0.36f, Size.Y - PanelGap * 2.f);
	const FVector2D ForwardPanelSize(AvailableW * 0.36f, Size.Y - PanelGap * 2.f);
	const FVector2D GraphPanelSize(AvailableW * 0.28f, Size.Y - PanelGap * 2.f);

	const FVector2D CrossPanelPos(PanelGap, PanelGap);
	const FVector2D ForwardPanelPos(CrossPanelPos.X + CrossPanelSize.X + PanelGap, PanelGap);
	const FVector2D GraphPanelPos(ForwardPanelPos.X + ForwardPanelSize.X + PanelGap, PanelGap);

	DrawPanel(CrossPanelPos, CrossPanelSize, TEXT("A  CROSS-SECTION"), NextLayer);
	DrawPanel(ForwardPanelPos, ForwardPanelSize, TEXT("B  FORWARD ANTICIPATION"), NextLayer);
	DrawPanel(GraphPanelPos, GraphPanelSize, TEXT("C  TACTICAL GRAPH"), NextLayer);
	NextLayer += 3;

	if (!bHasBoundDisplay)
	{
		DrawTextLine(CrossPanelPos + FVector2D(10.f, 42.f), TEXT("UNBOUND"), WarningColor, NextLayer + 1);
		DrawTextLine(CrossPanelPos + FVector2D(10.f, 58.f), TEXT("Place this widget inside WBP_SubHelm"), FrameColor, NextLayer + 1);
		DrawTextLine(CrossPanelPos + FVector2D(10.f, 74.f), TEXT("and keep AutoCreate disabled."), FrameColor, NextLayer + 1);

		DrawTextLine(ForwardPanelPos + FVector2D(10.f, 42.f), TEXT("NO RUNTIME DATA"), WarningColor, NextLayer + 1);
		DrawTextLine(ForwardPanelPos + FVector2D(10.f, 58.f), TEXT("The widget needs a bound"), FrameColor, NextLayer + 1);
		DrawTextLine(ForwardPanelPos + FVector2D(10.f, 74.f), TEXT("UHelmNavigationDisplayComponent."), FrameColor, NextLayer + 1);

		DrawTextLine(GraphPanelPos + FVector2D(10.f, 42.f), TEXT("CHECK SIZE"), WarningColor, NextLayer + 1);
		DrawTextLine(GraphPanelPos + FVector2D(10.f, 58.f), TEXT("Use Canvas slot or SizeBox"), FrameColor, NextLayer + 1);
		DrawTextLine(GraphPanelPos + FVector2D(10.f, 74.f), TEXT("with explicit width/height."), FrameColor, NextLayer + 1);
		return NextLayer + 2;
	}

	if (CrossSection.bValid)
	{
		const FVector2D PanelInnerPos = CrossPanelPos + FVector2D(10.f, 28.f);
		const FVector2D PanelInnerSize(CrossPanelSize.X - 20.f, CrossPanelSize.Y - 82.f);
		const float SquareSize = FMath::Min(PanelInnerSize.X, PanelInnerSize.Y);
		const FVector2D SquarePos(
			PanelInnerPos.X + (PanelInnerSize.X - SquareSize) * 0.5f,
			PanelInnerPos.Y);
		const FVector2D SquareCenter = SquarePos + FVector2D(SquareSize * 0.5f, SquareSize * 0.5f);
		const float SquareRadius = SquareSize * 0.45f;

		TArray<FVector2f> AxisH;
		AxisH.Add(FVector2f(SquarePos.X + SquareSize * 0.05f, SquareCenter.Y));
		AxisH.Add(FVector2f(SquarePos.X + SquareSize * 0.95f, SquareCenter.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), AxisH, ESlateDrawEffect::None, FrameColor, true, 1.f);

		TArray<FVector2f> AxisV;
		AxisV.Add(FVector2f(SquareCenter.X, SquarePos.Y + SquareSize * 0.05f));
		AxisV.Add(FVector2f(SquareCenter.X, SquarePos.Y + SquareSize * 0.95f));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), AxisV, ESlateDrawEffect::None, FrameColor, true, 1.f);

		if (CrossSection.ContourPointsNormalized.Num() >= 2)
		{
			TArray<FVector2f> Contour;
			Contour.Reserve(CrossSection.ContourPointsNormalized.Num());
			for (const FVector2D& Point : CrossSection.ContourPointsNormalized)
			{
				const FVector2D P = SquareCenter + FVector2D(Point.X, Point.Y) * SquareRadius;
				Contour.Add(FVector2f(P.X, P.Y));
			}
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), Contour, ESlateDrawEffect::None, AccentColor, true, 1.6f);
		}

		const FVector2D SubPos = SquareCenter + CrossSection.SubOffsetNormalized * SquareRadius;
		const FVector2D HeadingEnd = SubPos + CrossSection.HeadingVectorNormalized * SquareRadius;
		const FVector2D VelocityEnd = SubPos + CrossSection.VelocityVectorNormalized * SquareRadius;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer + 1,
			MakePaintGeometry(SubPos - FVector2D(4.f, 4.f), FVector2D(8.f, 8.f)),
			&WhiteBrush,
			ESlateDrawEffect::None,
			CrossSection.bHardClearanceViolation ? WarningColor : AccentColor);

		TArray<FVector2f> HeadingLine;
		HeadingLine.Add(FVector2f(SubPos.X, SubPos.Y));
		HeadingLine.Add(FVector2f(HeadingEnd.X, HeadingEnd.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), HeadingLine, ESlateDrawEffect::None, AccentColor, true, 1.3f);

		TArray<FVector2f> VelocityLine;
		VelocityLine.Add(FVector2f(SubPos.X, SubPos.Y));
		VelocityLine.Add(FVector2f(VelocityEnd.X, VelocityEnd.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), VelocityLine, ESlateDrawEffect::None, VelocityColor, true, 1.5f);

		const float GaugeWidth = 10.f;
		const float GaugeHeight = SquareSize * 0.42f;
		const FVector2D GaugePos(SquarePos.X + SquareSize + 10.f, SquareCenter.Y - GaugeHeight);
		const FVector2D GaugeDownPos(GaugePos.X + 14.f, SquareCenter.Y);

		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer, MakePaintGeometry(GaugePos, FVector2D(GaugeWidth, GaugeHeight)), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.25f));
		FSlateDrawElement::MakeBox(OutDrawElements, NextLayer, MakePaintGeometry(GaugeDownPos, FVector2D(GaugeWidth, GaugeHeight)), &WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.25f));
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer + 1,
			MakePaintGeometry(GaugePos + FVector2D(0.f, GaugeHeight * (1.f - CrossSection.SafeAbove01)), FVector2D(GaugeWidth, GaugeHeight * CrossSection.SafeAbove01)),
			&WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(SafeColor.R, SafeColor.G, SafeColor.B, 0.85f));
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer + 1,
			MakePaintGeometry(GaugeDownPos, FVector2D(GaugeWidth, GaugeHeight * CrossSection.SafeBelow01)),
			&WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(SafeColor.R, SafeColor.G, SafeColor.B, 0.85f));

		DrawTextLine(CrossPanelPos + FVector2D(10.f, CrossPanelSize.Y - 48.f),
			FString::Printf(TEXT("UP %.1fm  DN %.1fm  %s"), CrossSection.SafeAboveCm / 100.f, CrossSection.SafeBelowCm / 100.f, *SpaceModeShortLabel(CrossSection.SpaceMode)),
			AccentColor,
			NextLayer + 2);
		DrawTextLine(CrossPanelPos + FVector2D(10.f, CrossPanelSize.Y - 34.f),
			FString::Printf(TEXT("LF %.1fm  RT %.1fm  OFF %.0f / %.0fcm"), CrossSection.SafeLeftCm / 100.f, CrossSection.SafeRightCm / 100.f, CrossSection.OffsetRightCm, CrossSection.OffsetUpCm),
			CrossSection.bNearWallWarning ? WarningColor : FrameColor,
			NextLayer + 2);
		DrawTextLine(CrossPanelPos + FVector2D(10.f, CrossPanelSize.Y - 20.f),
			FString::Printf(TEXT("Margin %.1fm"), CrossSection.ClearanceMarginCm / 100.f),
			CrossSection.bHardClearanceViolation ? WarningColor : AccentColor,
			NextLayer + 2);
	}

	if (ForwardView.bValid)
	{
		const FVector2D GraphPos = ForwardPanelPos + FVector2D(10.f, 28.f);
		const FVector2D GraphSize(ForwardPanelSize.X - 20.f, ForwardPanelSize.Y - 82.f);
		const FVector2D Center(GraphPos.X + GraphSize.X * 0.5f, GraphPos.Y + GraphSize.Y * 0.5f);
		const float HalfHeight = GraphSize.Y * 0.32f;
		TArray<FVector2f> CeilingLine;
		TArray<FVector2f> FloorLine;

		for (const FHelmForwardAnticipationSampleViewData& Sample : ForwardView.Samples)
		{
			const float X = GraphPos.X + Sample.Distance01 * GraphSize.X;
			const float CeilingY = Center.Y - Sample.Ceiling01 * HalfHeight;
			const float FloorY = Center.Y + Sample.Floor01 * HalfHeight;
			CeilingLine.Add(FVector2f(X, CeilingY));
			FloorLine.Add(FVector2f(X, FloorY));

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
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), CeilingLine, ESlateDrawEffect::None, AccentColor, true, 1.4f);
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), FloorLine, ESlateDrawEffect::None, AccentColor, true, 1.4f);
		}

		const float SubMarkerX = GraphPos.X + 6.f;
		const float SubMarkerCenterY = Center.Y + CrossSection.SubOffsetNormalized.Y * HalfHeight;
		const FVector2D SubMarkerSize(10.f, 18.f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer + 1,
			MakePaintGeometry(FVector2D(SubMarkerX - SubMarkerSize.X * 0.5f, SubMarkerCenterY - SubMarkerSize.Y * 0.5f), SubMarkerSize),
			&WhiteBrush,
			ESlateDrawEffect::None,
			CrossSection.bHardClearanceViolation ? WarningColor : AccentColor);

		TArray<FVector2f> OriginGuide;
		OriginGuide.Add(FVector2f(SubMarkerX + 10.f, GraphPos.Y));
		OriginGuide.Add(FVector2f(SubMarkerX + 10.f, GraphPos.Y + GraphSize.Y));
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			NextLayer + 1,
			AllottedGeometry.ToPaintGeometry(),
			OriginGuide,
			ESlateDrawEffect::None,
			FrameColor,
			true,
			1.0f);

		if (ForwardView.LookaheadMeters > KINDA_SMALL_NUMBER && ForwardView.StoppingDistanceMeters >= 0.f)
		{
			const float Stop01 = FMath::Clamp(ForwardView.StoppingDistanceMeters / FMath::Max(ForwardView.LookaheadMeters, 0.01f), 0.f, 1.f);
			const float StopX = GraphPos.X + Stop01 * GraphSize.X;
			TArray<FVector2f> StopLine;
			StopLine.Add(FVector2f(StopX, GraphPos.Y));
			StopLine.Add(FVector2f(StopX, GraphPos.Y + GraphSize.Y));
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), StopLine, ESlateDrawEffect::None, VelocityColor, true, 1.2f);
		}

		const FLinearColor SummaryColor = ForwardView.bCrashStopAlreadyLate ? WarningColor : AccentColor;
		DrawTextLine(ForwardPanelPos + FVector2D(10.f, ForwardPanelSize.Y - 48.f),
			FString::Printf(TEXT("REC %.1f km/h  STOP %.1fm"), ForwardView.RecommendedMaxSpeedKmh, ForwardView.StoppingDistanceMeters),
			SummaryColor,
			NextLayer + 2);
		DrawTextLine(ForwardPanelPos + FVector2D(10.f, ForwardPanelSize.Y - 34.f),
			FString::Printf(TEXT("CRIT %.1fm  RT-OBS %.1fm"), ForwardView.FirstCriticalObstacleMeters, ForwardView.DistanceToFirstRuntimeObstacleMeters),
			(ForwardView.FirstCriticalObstacleMeters >= 0.f) ? WarningColor : FrameColor,
			NextLayer + 2);
		DrawTextLine(ForwardPanelPos + FVector2D(10.f, ForwardPanelSize.Y - 20.f),
			FString::Printf(TEXT("%s  %s"), ForwardView.bCommitmentZone ? TEXT("COMMIT") : TEXT("OPEN"), ForwardView.bNoTurnaroundBeforeNextHub ? TEXT("NO-TURN") : TEXT("TURN OK")),
			ForwardView.bCommitmentZone ? WarningColor : AccentColor,
			NextLayer + 2);
	}

	if (GraphView.bValid)
	{
		const FVector2D PanelInnerPos = GraphPanelPos + FVector2D(10.f, 28.f);
		const FVector2D PanelInnerSize(GraphPanelSize.X - 20.f, GraphPanelSize.Y - 82.f);
		const FVector2D Center = PanelInnerPos + PanelInnerSize * 0.5f;
		const float Radius = FMath::Min(PanelInnerSize.X, PanelInnerSize.Y) * 0.42f;

		auto FindNodeById = [&](int32 NodeID) -> const FHelmTacticalGraphNodeViewData*
		{
			return GraphView.Nodes.FindByPredicate([NodeID](const FHelmTacticalGraphNodeViewData& Node)
			{
				return Node.NodeID == NodeID;
			});
		};

		for (const FHelmTacticalGraphEdgeViewData& Edge : GraphView.Edges)
		{
			const FHelmTacticalGraphNodeViewData* FromNode = FindNodeById(Edge.FromNodeID);
			const FHelmTacticalGraphNodeViewData* ToNode = FindNodeById(Edge.ToNodeID);
			if (!FromNode || !ToNode)
			{
				continue;
			}

			const FVector2D A = Center + FVector2D(FromNode->PositionNormalized.X, -FromNode->PositionNormalized.Y) * Radius;
			const FVector2D B = Center + FVector2D(ToNode->PositionNormalized.X, -ToNode->PositionNormalized.Y) * Radius;
			TArray<FVector2f> EdgeLine;
			EdgeLine.Add(FVector2f(A.X, A.Y));
			EdgeLine.Add(FVector2f(B.X, B.Y));
			const FLinearColor EdgeColor = Edge.bCurrent ? AccentColor : (Edge.bOptional ? SafeColor : FrameColor);
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), EdgeLine, ESlateDrawEffect::None, EdgeColor, true, Edge.bCurrent ? 2.f : 1.1f);
		}

		for (const FHelmTacticalGraphNodeViewData& Node : GraphView.Nodes)
		{
			const FVector2D NodePos = Center + FVector2D(Node.PositionNormalized.X, -Node.PositionNormalized.Y) * Radius;
			const float NodeSize = Node.bHub ? 9.f : (Node.bCurrent ? 8.f : 6.f);
			FLinearColor NodeColor = Node.bCurrent ? AccentColor : FrameColor;
			if (Node.bHub)
			{
				NodeColor = SafeColor;
			}
			else if (Node.bSplitLike)
			{
				NodeColor = VelocityColor;
			}
			else if (Node.bOptional)
			{
				NodeColor = FLinearColor(FrameColor.R, FrameColor.G, FrameColor.B, 0.65f);
			}

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				NextLayer + 1,
				MakePaintGeometry(NodePos - FVector2D(NodeSize * 0.5f, NodeSize * 0.5f), FVector2D(NodeSize, NodeSize)),
				&WhiteBrush,
				ESlateDrawEffect::None,
				NodeColor);
		}

		DrawTextLine(GraphPanelPos + FVector2D(10.f, GraphPanelSize.Y - 48.f),
			FString::Printf(TEXT("%s  EDGE %d"), *SpaceModeShortLabel(GraphView.SpaceMode), GraphView.CurrentEdgeIndex),
			AccentColor,
			NextLayer + 2);
		DrawTextLine(GraphPanelPos + FVector2D(10.f, GraphPanelSize.Y - 34.f),
			FString::Printf(TEXT("HUB %.1fm  SPLIT %.1fm"), GraphView.DistanceToNextHubMeters, GraphView.DistanceToNextSplitMeters),
			GraphView.bMultipleExitsNearby ? VelocityColor : FrameColor,
			NextLayer + 2);
		DrawTextLine(GraphPanelPos + FVector2D(10.f, GraphPanelSize.Y - 20.f),
			FString::Printf(TEXT("MERGE %.1fm  %s"), GraphView.DistanceToNextMergeMeters, CommitmentWarning.bWarning ? TEXT("COMMIT WARN") : TEXT("FLOW OK")),
			CommitmentWarning.bWarning ? WarningColor : AccentColor,
			NextLayer + 2);
	}

	return NextLayer + 3;
}

void UHelmNavigationDisplayWidget::InitForNavigationDisplay(UHelmNavigationDisplayComponent* InDisplayComponent)
{
	CachedDisplayComponent = InDisplayComponent;
}

bool UHelmNavigationDisplayWidget::IsNavigationDisplayBound() const
{
	return CachedDisplayComponent.IsValid();
}

UHelmNavigationDisplayComponent* UHelmNavigationDisplayWidget::GetBoundNavigationDisplay() const
{
	return CachedDisplayComponent.Get();
}

FHelmCrossSectionViewData UHelmNavigationDisplayWidget::GetCrossSectionViewData() const
{
	return CachedDisplayComponent.IsValid() ? CachedDisplayComponent->GetCrossSectionViewData() : FHelmCrossSectionViewData();
}

FHelmForwardAnticipationViewData UHelmNavigationDisplayWidget::GetForwardAnticipationViewData() const
{
	return CachedDisplayComponent.IsValid() ? CachedDisplayComponent->GetForwardAnticipationViewData() : FHelmForwardAnticipationViewData();
}

FHelmTacticalGraphViewData UHelmNavigationDisplayWidget::GetTacticalGraphViewData() const
{
	return CachedDisplayComponent.IsValid() ? CachedDisplayComponent->GetTacticalGraphViewData() : FHelmTacticalGraphViewData();
}

FTunnelNavStoppingDistanceWarning UHelmNavigationDisplayWidget::GetStoppingDistanceWarning() const
{
	return CachedDisplayComponent.IsValid() ? CachedDisplayComponent->GetStoppingDistanceWarning() : FTunnelNavStoppingDistanceWarning();
}

FTunnelNavCommitmentWarning UHelmNavigationDisplayWidget::GetCommitmentWarning() const
{
	return CachedDisplayComponent.IsValid() ? CachedDisplayComponent->GetCommitmentWarning() : FTunnelNavCommitmentWarning();
}
