#include "TacticalGraphViewWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "HelmNavigationDisplayComponent.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "SubSonarSystemComponent.h"

namespace
{
	FSlateLayoutTransform MakeLayoutTransform(const FVector2D& Position)
	{
		return FSlateLayoutTransform(FVector2f(static_cast<float>(Position.X), static_cast<float>(Position.Y)));
	}
}

UTacticalGraphViewWidget::UTacticalGraphViewWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WindowTitle = FText::FromString(TEXT("Tactical Graph"));
}

void UTacticalGraphViewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CachedDisplayComponent.IsValid() || CachedSonarSystem.IsValid())
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void UTacticalGraphViewWidget::InitForTacticalGraphSources(UHelmNavigationDisplayComponent* InDisplayComponent, USubSonarSystemComponent* InSonarSystem)
{
	CachedDisplayComponent = InDisplayComponent;
	CachedSonarSystem = InSonarSystem;
}

bool UTacticalGraphViewWidget::IsTacticalGraphViewBound() const
{
	return CachedDisplayComponent.IsValid() && CachedSonarSystem.IsValid();
}

UHelmNavigationDisplayComponent* UTacticalGraphViewWidget::GetBoundNavigationDisplay() const
{
	return CachedDisplayComponent.Get();
}

USubSonarSystemComponent* UTacticalGraphViewWidget::GetBoundSonarSystem() const
{
	return CachedSonarSystem.Get();
}

int32 UTacticalGraphViewWidget::PaintInstrumentContent(
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
			MakePaintGeometry(Position, FVector2D(360.f, 14.f)),
			Text,
			Font,
			ESlateDrawEffect::None,
			Color);
	};

	const FVector2D ContentPos(ContentRect.Left, ContentRect.Top);
	const FVector2D ContentSize(ContentRect.Right - ContentRect.Left, ContentRect.Bottom - ContentRect.Top);
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
		return NextLayer + 2;
	}

	const FHelmTacticalGraphViewData GraphView = CachedDisplayComponent->GetTacticalGraphViewData();
	const FTunnelNavProjectionResult Projection = CachedDisplayComponent->GetProjectionResult();
	if (!GraphView.bValid)
	{
		DrawTextLine(ContentPos + FVector2D(8.f, 6.f), TEXT("NO GRAPH DATA"), WarningColor, NextLayer + 1, TextFont);
		DrawTextLine(ContentPos + FVector2D(8.f, 22.f), TEXT("TunnelNavigationRuntime did not return a valid local graph window."), FrameColor, NextLayer + 1, SmallFont);
		return NextLayer + 2;
	}

	const FVector2D PlotPos = ContentPos + FVector2D(8.f, 8.f);
	const FVector2D PlotSize(ContentSize.X - 16.f, FMath::Max(60.f, ContentSize.Y - 52.f));
	const FVector2D PlotCenter = PlotPos + PlotSize * 0.5f;
	const float PlotRadius = FMath::Min(PlotSize.X, PlotSize.Y) * 0.42f;

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

		const FVector2D A = PlotCenter + FVector2D(FromNode->PositionNormalized.X, -FromNode->PositionNormalized.Y) * PlotRadius;
		const FVector2D B = PlotCenter + FVector2D(ToNode->PositionNormalized.X, -ToNode->PositionNormalized.Y) * PlotRadius;
		TArray<FVector2f> EdgeLine;
		EdgeLine.Add(FVector2f(A.X, A.Y));
		EdgeLine.Add(FVector2f(B.X, B.Y));
		const FLinearColor EdgeColor = Edge.bCurrent ? AccentColor : (Edge.bOptional ? SafeColor : FrameColor);
		FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 1, AllottedGeometry.ToPaintGeometry(), EdgeLine, ESlateDrawEffect::None, EdgeColor, true, Edge.bCurrent ? 2.f : 1.1f);
	}

	for (const FHelmTacticalGraphNodeViewData& Node : GraphView.Nodes)
	{
		const FVector2D NodePos = PlotCenter + FVector2D(Node.PositionNormalized.X, -Node.PositionNormalized.Y) * PlotRadius;
		const float NodeSize = Node.bHub ? 10.f : (Node.bCurrent ? 8.f : 6.f);
		FLinearColor NodeColor = Node.bCurrent ? AccentColor : FrameColor;
		if (Node.bHub)
		{
			NodeColor = SafeColor;
		}
		else if (Node.bSplitLike)
		{
			NodeColor = PriorityColor;
		}
		else if (Node.bOptional)
		{
			NodeColor = FLinearColor(FrameColor.R, FrameColor.G, FrameColor.B, 0.55f);
		}

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer + 2,
			MakePaintGeometry(NodePos - FVector2D(NodeSize * 0.5f, NodeSize * 0.5f), FVector2D(NodeSize, NodeSize)),
			&WhiteBrush,
			ESlateDrawEffect::None,
			NodeColor);
	}

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		NextLayer + 2,
		MakePaintGeometry(PlotCenter - FVector2D(4.f, 4.f), FVector2D(8.f, 8.f)),
		&WhiteBrush,
		ESlateDrawEffect::None,
		AccentColor);

	int32 TrackCount = 0;
	if (bDrawSonarTracks && CachedSonarSystem.IsValid() && Projection.bProjected)
	{
		const float GraphRadiusCm = FMath::Max(CachedDisplayComponent->GetTacticalGraphRadiusCm(), 1000.f);
		for (const FSonarTrack& Track : CachedSonarSystem->GetTracks())
		{
			if (Track.State == ESonarTrackState::None)
			{
				continue;
			}

			const FVector Delta = FVector(Track.EstimatedWorldLocation) - Projection.ClosestPointWorld;
			const FVector2D TrackNorm(
				FMath::Clamp(FVector::DotProduct(Delta, Projection.FrameRight) / GraphRadiusCm, -1.25f, 1.25f),
				FMath::Clamp(FVector::DotProduct(Delta, Projection.FrameForward) / GraphRadiusCm, -1.25f, 1.25f));
			if (FMath::Abs(TrackNorm.X) > 1.1f || FMath::Abs(TrackNorm.Y) > 1.1f)
			{
				continue;
			}

			++TrackCount;
			const FVector2D TrackPos = PlotCenter + FVector2D(TrackNorm.X, -TrackNorm.Y) * PlotRadius;
			const float MarkerSize = GetTrackMarkerSize(Track);
			const FLinearColor TrackColor = GetTrackColor(Track);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				NextLayer + 3,
				MakePaintGeometry(TrackPos - FVector2D(MarkerSize * 0.5f, MarkerSize * 0.5f), FVector2D(MarkerSize, MarkerSize)),
				&WhiteBrush,
				ESlateDrawEffect::None,
				TrackColor);

			if (Track.bPriority)
			{
				const float RingSize = MarkerSize + 6.f;
				TArray<FVector2f> Ring;
				Ring.Add(FVector2f(TrackPos.X - RingSize * 0.5f, TrackPos.Y - RingSize * 0.5f));
				Ring.Add(FVector2f(TrackPos.X + RingSize * 0.5f, TrackPos.Y - RingSize * 0.5f));
				Ring.Add(FVector2f(TrackPos.X + RingSize * 0.5f, TrackPos.Y + RingSize * 0.5f));
				Ring.Add(FVector2f(TrackPos.X - RingSize * 0.5f, TrackPos.Y + RingSize * 0.5f));
				Ring.Add(FVector2f(TrackPos.X - RingSize * 0.5f, TrackPos.Y - RingSize * 0.5f));
				FSlateDrawElement::MakeLines(OutDrawElements, NextLayer + 4, AllottedGeometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None, PriorityColor, true, 1.2f);
			}
		}
	}

	const FVector2D FooterPos(ContentPos.X + 8.f, ContentPos.Y + ContentSize.Y - 34.f);
	DrawTextLine(FooterPos, FString::Printf(TEXT("EDGE %d   HUB %.1fm   SPLIT %.1fm   MERGE %.1fm"), GraphView.CurrentEdgeIndex, GraphView.DistanceToNextHubMeters, GraphView.DistanceToNextSplitMeters, GraphView.DistanceToNextMergeMeters), AccentColor, NextLayer + 4, SmallFont);
	DrawTextLine(FooterPos + FVector2D(0.f, 14.f), FString::Printf(TEXT("TRACKS %d   %s"), TrackCount, GraphView.bMultipleExitsNearby ? TEXT("MULTI-EXIT") : TEXT("SINGLE FLOW")), GraphView.bMultipleExitsNearby ? PriorityColor : FrameColor, NextLayer + 4, SmallFont);

	return NextLayer + 5;
}

FLinearColor UTacticalGraphViewWidget::GetTrackColor(const FSonarTrack& Track) const
{
	FLinearColor BaseColor = FrameColor;
	switch (Track.ProbableClass)
	{
	case ESonarContactClass::StructureActive:
		BaseColor = SafeColor;
		break;
	case ESonarContactClass::MobileThreat:
		BaseColor = WarningColor;
		break;
	case ESonarContactClass::MobileNeutral:
		BaseColor = PriorityColor;
		break;
	case ESonarContactClass::Anomaly:
		BaseColor = FLinearColor(0.88f, 0.34f, 1.f, 1.f);
		break;
	case ESonarContactClass::EnvironmentStatic:
		BaseColor = FrameColor;
		break;
	case ESonarContactClass::MobileUnknown:
	case ESonarContactClass::Unknown:
	default:
		BaseColor = AccentColor;
		break;
	}

	const float Alpha = FMath::Lerp(0.35f, 1.f, FMath::Clamp(Track.Confidence, 0.f, 1.f));
	BaseColor.A = Alpha;
	return BaseColor;
}

float UTacticalGraphViewWidget::GetTrackMarkerSize(const FSonarTrack& Track) const
{
	switch (Track.State)
	{
	case ESonarTrackState::Confirmed:
		return BaseTrackMarkerSizePx + 4.f;
	case ESonarTrackState::Classified:
		return BaseTrackMarkerSizePx + 3.f;
	case ESonarTrackState::Tracked:
		return BaseTrackMarkerSizePx + 2.f;
	case ESonarTrackState::Suspected:
		return BaseTrackMarkerSizePx + 1.f;
	case ESonarTrackState::Lost:
		return BaseTrackMarkerSizePx;
	case ESonarTrackState::None:
	default:
		return BaseTrackMarkerSizePx;
	}
}
