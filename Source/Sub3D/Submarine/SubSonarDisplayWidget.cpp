#include "SubSonarDisplayWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "SubmarineBase.h"
#include "SubSonarComponent.h"
#include "SubSonarSystemComponent.h"

void USubSonarDisplayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CachedSonar.IsValid())
	{
		float LatestTimestamp = 0.f;
		for (const FSonarHitPoint& P : CachedSonar->SonarPoints)
		{
			LatestTimestamp = FMath::Max(LatestTimestamp, P.PingTimestamp);
		}

		if (LatestTimestamp > LastKnownPingTime + KINDA_SMALL_NUMBER)
		{
			LastKnownPingTime = LatestTimestamp;
			BP_OnNewPingReceived();
		}
	}

	if (HasActivePoints() || CachedSonarSystem.IsValid())
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

int32 USubSonarDisplayWidget::NativePaint(
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
	if (Size.X <= 2.f || Size.Y <= 2.f)
	{
		return NextLayer;
	}

	static const FSlateColorBrush WhiteBrush(FLinearColor::White);
	const auto MakePaintGeometry = [&AllottedGeometry](const FVector2D& LocalPos, const FVector2D& LocalSize)
	{
		return AllottedGeometry.ToPaintGeometry(
			FVector2f(static_cast<float>(LocalSize.X), static_cast<float>(LocalSize.Y)),
			FSlateLayoutTransform(FVector2f(static_cast<float>(LocalPos.X), static_cast<float>(LocalPos.Y))));
	};

	const FVector2D Center(Size.X * 0.5f, Size.Y * 0.5f);
	const float HalfW = Size.X * 0.5f;
	const float HalfH = Size.Y * 0.5f;
	const float MinSide = FMath::Min(Size.X, Size.Y);
	const float RangeCm = GetEffectiveDisplayRangeCm();

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		NextLayer++,
		MakePaintGeometry(FVector2D::ZeroVector, Size),
		&WhiteBrush,
		ESlateDrawEffect::None,
		BackgroundColor);

	if (NoiseOverlayTexture && NoiseOverlayAlpha > 0.f)
	{
		FSlateBrush NoiseBrush;
		NoiseBrush.SetResourceObject(NoiseOverlayTexture);
		NoiseBrush.DrawAs = ESlateBrushDrawType::Image;
		NoiseBrush.ImageSize = Size;

		FLinearColor NoiseColor = DotColor;
		NoiseColor.A = FMath::Clamp(NoiseOverlayAlpha, 0.f, 1.f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer++,
			MakePaintGeometry(FVector2D::ZeroVector, Size),
			&NoiseBrush,
			ESlateDrawEffect::None,
			NoiseColor);
	}

	if (bDrawGrid)
	{
		if (GridStepPx > 4.f)
		{
			TArray<FVector2f> LinePoints;
			LinePoints.Reserve(2);
			for (float X = 0.f; X <= Size.X; X += GridStepPx)
			{
				LinePoints.Reset();
				LinePoints.Add(FVector2f(X, 0.f));
				LinePoints.Add(FVector2f(X, Size.Y));
				FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), LinePoints, ESlateDrawEffect::None, GridColor, true, 1.0f);
			}

			for (float Y = 0.f; Y <= Size.Y; Y += GridStepPx)
			{
				LinePoints.Reset();
				LinePoints.Add(FVector2f(0.f, Y));
				LinePoints.Add(FVector2f(Size.X, Y));
				FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), LinePoints, ESlateDrawEffect::None, GridColor, true, 1.0f);
			}
		}

		const int32 RingCountInt = FMath::Max(1, FMath::RoundToInt(RingCount));
		for (int32 RingIndex = 1; RingIndex <= RingCountInt; ++RingIndex)
		{
			const float Radius = (MinSide * 0.48f) * (static_cast<float>(RingIndex) / static_cast<float>(RingCountInt));
			TArray<FVector2f> Circle;
			constexpr int32 Segments = 80;
			Circle.Reserve(Segments + 1);
			for (int32 Seg = 0; Seg <= Segments; ++Seg)
			{
				const float Angle = (2.f * PI * static_cast<float>(Seg)) / static_cast<float>(Segments);
				Circle.Add(FVector2f(Center.X + FMath::Cos(Angle) * Radius, Center.Y + FMath::Sin(Angle) * Radius));
			}
			FLinearColor RingColor = GridColor;
			RingColor.A *= 1.1f;
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				NextLayer,
				AllottedGeometry.ToPaintGeometry(),
				Circle,
				ESlateDrawEffect::None,
				RingColor,
				true,
				1.2f);
		}
		++NextLayer;
	}

	if (bDrawSweepPulse && CachedSonar.IsValid() && GetWorld() && CachedSonar->PropagationSpeedCmS > 0.f)
	{
		const TArray<float>& RecentPingTimes = CachedSonar->GetRecentPingTimestamps();
		const int32 FirstIndex = FMath::Max(0, RecentPingTimes.Num() - FMath::Max(1, MaxVisibleSweepPulses));
		for (int32 PingIndex = FirstIndex; PingIndex < RecentPingTimes.Num(); ++PingIndex)
		{
			const float PingTimestamp = RecentPingTimes[PingIndex];
			if (PingTimestamp < 0.f)
			{
				continue;
			}

			const float MaxTravelTime = FMath::Max(CachedSonar->PingMaxRangeCm / CachedSonar->PropagationSpeedCmS, KINDA_SMALL_NUMBER);
			const float Age = GetWorld()->GetTimeSeconds() - PingTimestamp;
			const float T = FMath::Clamp(Age / MaxTravelTime, 0.f, 1.25f);
			const float Radius = FMath::Min(Size.X, Size.Y) * 0.49f * T;
			const float RawAlpha = (T <= 1.f) ? (1.f - (T * 0.6f)) : FMath::Clamp(1.25f - T, 0.f, 0.4f);
			const float SweepAlpha = FMath::Pow(FMath::Clamp(RawAlpha, 0.f, 1.f), FMath::Max(0.1f, SweepFadeExponent));

			if (Radius > 1.f && SweepAlpha > KINDA_SMALL_NUMBER)
			{
				TArray<FVector2f> Circle;
				constexpr int32 Segments = 96;
				Circle.Reserve(Segments + 1);
				for (int32 Index = 0; Index <= Segments; ++Index)
				{
					const float Angle = (2.f * PI * static_cast<float>(Index)) / static_cast<float>(Segments);
					Circle.Add(FVector2f(Center.X + FMath::Cos(Angle) * Radius, Center.Y + FMath::Sin(Angle) * Radius));
				}

				FLinearColor SweepLineColor = SweepColor;
				SweepLineColor.A *= SweepAlpha;
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					NextLayer,
					AllottedGeometry.ToPaintGeometry(),
					Circle,
					ESlateDrawEffect::None,
					SweepLineColor,
					true,
					1.6f);
			}
		}
		++NextLayer;
	}

	if (bDrawTopologyWireframe && CachedSonarSystem.IsValid())
	{
		const float CellSizeCm = FMath::Max(50.f, TopologyCellSizeCm);
		for (const FSonarTopoCell& Cell : CachedSonarSystem->GetTopoCells())
		{
			const FVector CellWorld(
				(static_cast<float>(Cell.GridX) + 0.5f) * CellSizeCm,
				(static_cast<float>(Cell.GridY) + 0.5f) * CellSizeCm,
				static_cast<float>(Cell.HeightDm) * 10.f);
			const FVector2D Normalized = ProjectToDisplayNormalized(CellWorld);
			if (FMath::Abs(Normalized.X) > 1.f || FMath::Abs(Normalized.Y) > 1.f)
			{
				continue;
			}

			FVector2D CellCenter(HalfW + Normalized.X * HalfW, HalfH - Normalized.Y * HalfH);
			CellCenter = ApplyHeightParallax(CellCenter, CellWorld, Size);
			const float HalfCellPx = FMath::Max(1.0f, (CellSizeCm / FMath::Max(100.f, RangeCm)) * HalfW * 0.35f);
			TArray<FVector2f> Rect;
			Rect.Add(FVector2f(CellCenter.X - HalfCellPx, CellCenter.Y - HalfCellPx));
			Rect.Add(FVector2f(CellCenter.X + HalfCellPx, CellCenter.Y - HalfCellPx));
			Rect.Add(FVector2f(CellCenter.X + HalfCellPx, CellCenter.Y + HalfCellPx));
			Rect.Add(FVector2f(CellCenter.X - HalfCellPx, CellCenter.Y + HalfCellPx));
			Rect.Add(FVector2f(CellCenter.X - HalfCellPx, CellCenter.Y - HalfCellPx));

			FLinearColor TopoColor = DotColor;
			TopoColor = ResolveSpatialColor(
				Normalized,
				CellWorld,
				DotColor,
				FMath::Clamp(static_cast<float>(Cell.Confidence01Byte) / 255.f, 0.08f, 0.85f));
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				NextLayer,
				AllottedGeometry.ToPaintGeometry(),
				Rect,
				ESlateDrawEffect::None,
				TopoColor,
				true,
				1.0f);
		}
		++NextLayer;
	}

	if (bDrawTracks && CachedSonarSystem.IsValid())
	{
		for (const FSonarTrack& Track : CachedSonarSystem->GetTracks())
		{
			const FVector2D Normalized = ProjectToDisplayNormalized(FVector(Track.EstimatedWorldLocation));
			if (FMath::Abs(Normalized.X) > 1.f || FMath::Abs(Normalized.Y) > 1.f)
			{
				continue;
			}

			FVector2D Pos(HalfW + Normalized.X * HalfW, HalfH - Normalized.Y * HalfH);
			Pos = ApplyHeightParallax(Pos, FVector(Track.EstimatedWorldLocation), Size);
			const float SizePx = Track.bPriority ? 8.f : 6.f;
			const FLinearColor TrackColor = ResolveTrackColor(static_cast<uint8>(Track.State), Track.bPriority);

			TArray<FVector2f> CrossA;
			CrossA.Add(FVector2f(Pos.X - SizePx, Pos.Y - SizePx));
			CrossA.Add(FVector2f(Pos.X + SizePx, Pos.Y + SizePx));
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), CrossA, ESlateDrawEffect::None, TrackColor, true, 1.4f);

			TArray<FVector2f> CrossB;
			CrossB.Add(FVector2f(Pos.X - SizePx, Pos.Y + SizePx));
			CrossB.Add(FVector2f(Pos.X + SizePx, Pos.Y - SizePx));
			FSlateDrawElement::MakeLines(OutDrawElements, NextLayer, AllottedGeometry.ToPaintGeometry(), CrossB, ESlateDrawEffect::None, TrackColor, true, 1.4f);
		}
		++NextLayer;
	}

	if (CachedSonar.IsValid() && GetWorld())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		for (const FSonarHitPoint& Point : CachedSonar->SonarPoints)
		{
			const float Alpha = FMath::Clamp(ComputePointAlpha(Point, Now), 0.f, 1.f);
			if (Alpha <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FVector2D Normalized = ProjectPointNormalized(Point);
			FVector2D PointPos(HalfW + Normalized.X * HalfW, HalfH - Normalized.Y * HalfH);
			PointPos = ApplyHeightParallax(PointPos, FVector(Point.WorldLocation), Size);

			float DepthScale = 1.f;
			if (bDepthAffectsDotSize)
			{
				const float Depth01 = FMath::Clamp(Point.DistanceCm / FMath::Max(100.f, RangeCm), 0.f, 1.f);
				DepthScale = FMath::Lerp(NearDotScale, FarDotScale, Depth01);
			}

			const float CoreSize = FMath::Max(1.f, DotSizePx * DepthScale);
			const float GlowSize = FMath::Max(CoreSize, CoreSize * DotGlowScale);

			FLinearColor GlowColor = ResolveSpatialColor(
				Normalized,
				FVector(Point.WorldLocation),
				DotColor,
				Alpha * FMath::Clamp(DotGlowAlpha, 0.f, 1.f));
			const FVector2D GlowTopLeft = PointPos - FVector2D(GlowSize * 0.5f, GlowSize * 0.5f);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				NextLayer,
				MakePaintGeometry(GlowTopLeft, FVector2D(GlowSize, GlowSize)),
				&WhiteBrush,
				ESlateDrawEffect::None,
				GlowColor);

			FLinearColor CoreColor = ResolveSpatialColor(Normalized, FVector(Point.WorldLocation), DotColor, Alpha);
			const FVector2D CoreTopLeft = PointPos - FVector2D(CoreSize * 0.5f, CoreSize * 0.5f);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				NextLayer,
				MakePaintGeometry(CoreTopLeft, FVector2D(CoreSize, CoreSize)),
				&WhiteBrush,
				ESlateDrawEffect::None,
				CoreColor);
		}
		++NextLayer;
	}

	const FVector2D BaseCenterSize(
		MinSide * FMath::Clamp(CenterTextureScale, 0.05f, 1.0f),
		MinSide * FMath::Clamp(CenterTextureScale, 0.05f, 1.0f));

	if (CenterSubTexture)
	{
		FSlateBrush SubBrush;
		SubBrush.SetResourceObject(CenterSubTexture);
		SubBrush.DrawAs = ESlateBrushDrawType::Image;
		SubBrush.ImageSize = BaseCenterSize;

		const FVector2D TopLeft = Center - (BaseCenterSize * 0.5f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer++,
			MakePaintGeometry(TopLeft, BaseCenterSize),
			&SubBrush,
			ESlateDrawEffect::None,
			CenterTint);
	}

	if (CenterReticleTexture)
	{
		const FVector2D ReticleSize = BaseCenterSize * 0.95f;
		FSlateBrush ReticleBrush;
		ReticleBrush.SetResourceObject(CenterReticleTexture);
		ReticleBrush.DrawAs = ESlateBrushDrawType::Image;
		ReticleBrush.ImageSize = ReticleSize;

		const FVector2D TopLeft = Center - (ReticleSize * 0.5f);
		FLinearColor ReticleColor = DotColor;
		ReticleColor.A = 0.9f;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer++,
			MakePaintGeometry(TopLeft, ReticleSize),
			&ReticleBrush,
			ESlateDrawEffect::None,
			ReticleColor);
	}

	const AActor* RefActor = CachedSonarSystem.IsValid()
		? CachedSonarSystem->GetOwner()
		: (CachedSonar.IsValid() ? CachedSonar->GetOwner() : nullptr);
	if (RefActor)
	{
		const FVector Forward = RefActor->GetActorForwardVector().GetSafeNormal2D();
		// Sonar is north-up: world X axis maps to screen X (+right), world Y
		// axis maps to screen Y inverted (north up = -screen Y). Right of
		// world forward is (Fy, -Fx) in world XY; in screen coords with the
		// inverted Y, that becomes (Fy, +Fx). Earlier perpendicular formula
		// ((-Sf.Y, -Sf.X)) flipped both width and bow taper as the sub
		// turned, which is the "shape morphing" the user observed.
		const FVector2D ScreenForward(Forward.X, -Forward.Y);
		const FVector2D ScreenRight(Forward.Y, Forward.X);

		if (bDrawSubSilhouette)
		{
			// Resolve sub local half-extents (cm). Prefer MovementCollisionProxy
			// when assigned (it's the actual movement shape); fall back to
			// HullMesh, then to user-tunable defaults. Scales the raw mesh
			// extent by the component's relative scale so a non-unit scale on
			// HullMesh / MovementCollisionProxy is respected (review caught
			// this — GetBoundingBox alone ignores SetRelativeScale3D).
			FVector LocalHalfExtent = FallbackHullHalfExtentCm;
			if (const ASubmarineBase* Sub = Cast<ASubmarineBase>(RefActor))
			{
				const UStaticMeshComponent* ShapeSource = nullptr;
				if (Sub->MovementCollisionProxy && Sub->MovementCollisionProxy->GetStaticMesh())
				{
					ShapeSource = Sub->MovementCollisionProxy;
				}
				else if (Sub->HullMesh && Sub->HullMesh->GetStaticMesh())
				{
					ShapeSource = Sub->HullMesh;
				}
				if (ShapeSource && ShapeSource->GetStaticMesh())
				{
					const FVector MeshExtent = ShapeSource->GetStaticMesh()->GetBoundingBox().GetExtent();
					const FVector Scale = ShapeSource->GetComponentScale();
					LocalHalfExtent = FVector(MeshExtent.X * Scale.X, MeshExtent.Y * Scale.Y, MeshExtent.Z * Scale.Z);
				}
			}

			// Map cm → sonar pixels using the active range.
			const float PxPerCm = (MinSide * 0.5f) / FMath::Max(1.f, RangeCm);
			const float HalfLengthPx = LocalHalfExtent.X * PxPerCm;
			const float HalfWidthPx = LocalHalfExtent.Y * PxPerCm;

			// Sample a tear-drop / cigar polygon: ellipse with a sharper bow.
			constexpr int32 NumSegments = 32;
			TArray<FVector2f> Polygon;
			Polygon.Reserve(NumSegments + 1);
			for (int32 i = 0; i <= NumSegments; ++i)
			{
				const float Theta = 2.f * PI * static_cast<float>(i) / NumSegments;
				// Slight bow taper: shorten width at the front (cos(theta) > 0).
				const float CosT = FMath::Cos(Theta);
				const float SinT = FMath::Sin(Theta);
				const float WidthFactor = (CosT > 0.f) ? (1.f - 0.18f * CosT) : 1.f;
				const float LocalX = HalfLengthPx * CosT;
				const float LocalY = HalfWidthPx * SinT * WidthFactor;
				const FVector2D Screen = Center + ScreenForward * LocalX + ScreenRight * LocalY;
				Polygon.Add(FVector2f(static_cast<float>(Screen.X), static_cast<float>(Screen.Y)));
			}

			// Outline.
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				NextLayer,
				AllottedGeometry.ToPaintGeometry(),
				Polygon,
				ESlateDrawEffect::None,
				SubSilhouetteColor,
				true,
				1.5f);

			// Heading tick: short line from nose forward by ~15% of length.
			const FVector2D Nose = Center + ScreenForward * HalfLengthPx;
			const FVector2D NoseEnd = Nose + ScreenForward * (HalfLengthPx * 0.30f);
			TArray<FVector2f> HeadingLine;
			HeadingLine.Add(FVector2f(static_cast<float>(Nose.X), static_cast<float>(Nose.Y)));
			HeadingLine.Add(FVector2f(static_cast<float>(NoseEnd.X), static_cast<float>(NoseEnd.Y)));
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				NextLayer + 1,
				AllottedGeometry.ToPaintGeometry(),
				HeadingLine,
				ESlateDrawEffect::None,
				SubHeadingColor,
				true,
				2.0f);

			NextLayer += 2;
		}
		else
		{
			// Legacy short forward-tick fallback when silhouette is disabled.
			const FVector2D ArrowEnd = Center + ScreenForward * (MinSide * 0.16f);
			TArray<FVector2f> ArrowLine;
			ArrowLine.Add(FVector2f(static_cast<float>(Center.X), static_cast<float>(Center.Y)));
			ArrowLine.Add(FVector2f(static_cast<float>(ArrowEnd.X), static_cast<float>(ArrowEnd.Y)));
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				NextLayer++,
				AllottedGeometry.ToPaintGeometry(),
				ArrowLine,
				ESlateDrawEffect::None,
				SweepColor,
				true,
				2.0f);
		}
	}

	if (SmudgeOverlayTexture && SmudgeOverlayAlpha > 0.f)
	{
		FSlateBrush SmudgeBrush;
		SmudgeBrush.SetResourceObject(SmudgeOverlayTexture);
		SmudgeBrush.DrawAs = ESlateBrushDrawType::Image;
		SmudgeBrush.ImageSize = Size;

		FLinearColor SmudgeColor = FLinearColor::White;
		SmudgeColor.A = FMath::Clamp(SmudgeOverlayAlpha, 0.f, 1.f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			NextLayer++,
			MakePaintGeometry(FVector2D::ZeroVector, Size),
			&SmudgeBrush,
			ESlateDrawEffect::None,
			SmudgeColor);
	}

	return NextLayer;
}

void USubSonarDisplayWidget::InitForSonar(USubSonarComponent* SonarComponent)
{
	InitForSonarSources(SonarComponent, nullptr);
}

void USubSonarDisplayWidget::InitForSonarSources(USubSonarComponent* SonarComponent, USubSonarSystemComponent* SonarSystemComponent)
{
	CachedSonar = SonarComponent;
	CachedSonarSystem = SonarSystemComponent;
	LastKnownPingTime = -1000.f;
}

void USubSonarDisplayWidget::GetDisplayPoints(
	int32 CanvasWidth,
	int32 CanvasHeight,
	TArray<FVector2D>& OutPositions,
	TArray<float>& OutAlphas) const
{
	OutPositions.Reset();
	OutAlphas.Reset();

	if (!CachedSonar.IsValid() || !GetWorld())
	{
		return;
	}

	const float HalfW = CanvasWidth * 0.5f;
	const float HalfH = CanvasHeight * 0.5f;
	const float Now = GetWorld()->GetTimeSeconds();
	for (const FSonarHitPoint& Point : CachedSonar->SonarPoints)
	{
		const float Alpha = ComputePointAlpha(Point, Now);
		if (Alpha <= 0.f)
		{
			continue;
		}

		const FVector2D Normalized = ProjectPointNormalized(Point);
		OutPositions.Add(FVector2D(HalfW + Normalized.X * HalfW, HalfH - Normalized.Y * HalfH));
		OutAlphas.Add(Alpha);
	}
}

float USubSonarDisplayWidget::ComputePointAlpha(const FSonarHitPoint& Point, float CurrentTime) const
{
	if (!CachedSonar.IsValid())
	{
		return 0.f;
	}

	const float SafeSpeed = FMath::Max(CachedSonar->PropagationSpeedCmS, 1.f);
	const auto ComputeContribution = [&](float PingTimestamp, float DistanceCm) -> float
	{
		if (PingTimestamp < 0.f || DistanceCm < 0.f)
		{
			return 0.f;
		}

		const float RevealTime = PingTimestamp + DistanceCm / SafeSpeed;
		const float Age = CurrentTime - RevealTime;
		if (Age < 0.f)
		{
			return 0.f;
		}
		if (Age < CachedSonar->PointPeakDurationS)
		{
			return 1.f;
		}

		const float FadeT = (Age - CachedSonar->PointPeakDurationS) / FMath::Max(CachedSonar->PointFadeDurationS, KINDA_SMALL_NUMBER);
		return FMath::Clamp(1.f - FadeT, 0.f, 1.f);
	};

	return FMath::Max(
		ComputeContribution(Point.PingTimestamp, Point.DistanceCm),
		ComputeContribution(Point.PreviousPingTimestamp, Point.PreviousDistanceCm));
}

FVector2D USubSonarDisplayWidget::ProjectToDisplayNormalized(FVector WorldLocation) const
{
	const FVector ReferenceLocation = GetSonarReferenceLocation();
	const float RangeCm = GetEffectiveDisplayRangeCm();

	const FVector Delta = WorldLocation - ReferenceLocation;
	return FVector2D(
		FMath::Clamp(Delta.X / RangeCm, -1.f, 1.f),
		FMath::Clamp(Delta.Y / RangeCm, -1.f, 1.f));
}

bool USubSonarDisplayWidget::HasActivePoints() const
{
	if (!CachedSonar.IsValid() || !GetWorld())
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	for (const FSonarHitPoint& Point : CachedSonar->SonarPoints)
	{
		if (ComputePointAlpha(Point, Now) > 0.f)
		{
			return true;
		}
	}
	return false;
}

USubSonarSystemComponent* USubSonarDisplayWidget::GetBoundSonarSystem() const
{
	return CachedSonarSystem.Get();
}

float USubSonarDisplayWidget::GetSelfNoiseAggregate() const
{
	return CachedSonarSystem.IsValid() ? CachedSonarSystem->SelfNoiseState.AggregateNoise : 0.f;
}

float USubSonarDisplayWidget::GetAcousticClutterLevel() const
{
	return CachedSonarSystem.IsValid() ? CachedSonarSystem->GetAcousticClutterLevel() : 0.f;
}

bool USubSonarDisplayWidget::IsSignalUnstable() const
{
	return CachedSonarSystem.IsValid() ? CachedSonarSystem->IsSignalUnstable() : false;
}

FVector2D USubSonarDisplayWidget::ProjectPointNormalized(const FSonarHitPoint& Point) const
{
	return ProjectToDisplayNormalized(Point.WorldLocation);
}

FVector USubSonarDisplayWidget::GetSonarReferenceLocation() const
{
	if (CachedSonarSystem.IsValid() && CachedSonarSystem->GetOwner())
	{
		return CachedSonarSystem->GetOwner()->GetActorLocation();
	}

	if (CachedSonar.IsValid() && CachedSonar->GetOwner())
	{
		return CachedSonar->GetOwner()->GetActorLocation();
	}

	return FVector::ZeroVector;
}

float USubSonarDisplayWidget::GetEffectiveDisplayRangeCm() const
{
	if (CachedSonarSystem.IsValid())
	{
		return FMath::Max(1000.f, CachedSonarSystem->GetDisplayRangeCm());
	}
	if (CachedSonar.IsValid())
	{
		return FMath::Max(1000.f, CachedSonar->PingMaxRangeCm);
	}
	return 15000.f;
}

FVector2D USubSonarDisplayWidget::ApplyHeightParallax(const FVector2D& ScreenPos, const FVector& WorldLocation, const FVector2D& Size) const
{
	if (!bUseHeightParallax)
	{
		return ScreenPos;
	}

	const FVector ReferenceLocation = GetSonarReferenceLocation();
	const float HeightRange = FMath::Max(100.f, HeightParallaxRangeCm);
	const float Height01 = FMath::Clamp((WorldLocation.Z - ReferenceLocation.Z) / HeightRange, -1.f, 1.f);
	const float CenterDist01 = FVector2D::Distance(ScreenPos, Size * 0.5f) / FMath::Max(1.f, FMath::Min(Size.X, Size.Y) * 0.5f);
	const float CenterWeight = 1.f - FMath::Clamp(CenterDist01, 0.f, 1.f);
	return ScreenPos + FVector2D(0.f, -Height01 * HeightParallaxMaxOffsetPx * CenterWeight);
}

FLinearColor USubSonarDisplayWidget::ResolveSpatialColor(const FVector2D& Normalized, const FVector& WorldLocation, const FLinearColor& BaseColor, float Alpha) const
{
	FLinearColor OutColor = BaseColor;
	if (bEnableSpatialColorCoding)
	{
		if (SpatialColorMode == ESonarSpatialColorMode::AboveBelow)
		{
			const FVector ReferenceLocation = GetSonarReferenceLocation();
			const float HeightRange = FMath::Max(100.f, VerticalColorRangeCm);
			const float Height01 = FMath::Clamp(((WorldLocation.Z - ReferenceLocation.Z) / HeightRange + 1.f) * 0.5f, 0.f, 1.f);
			const FLinearColor SpatialGradient = FLinearColor::LerpUsingHSV(BelowColor, AboveColor, Height01);
			OutColor = FLinearColor::LerpUsingHSV(BaseColor, SpatialGradient, FMath::Clamp(SpatialColorBlend, 0.f, 1.f));
		}
		else
		{
			const float Lateral01 = FMath::Clamp((Normalized.X + 1.f) * 0.5f, 0.f, 1.f);
			const float Distance01 = FMath::Clamp(Normalized.Size(), 0.f, 1.f);
			const float Proximity01 = 1.f - Distance01;

			const float MixT = FMath::Clamp(
				(Lateral01 * FMath::Clamp(LateralColorWeight, 0.f, 1.f)) +
				(Proximity01 * FMath::Clamp(ProximityColorWeight, 0.f, 1.f)),
				0.f,
				1.f);
			const FLinearColor SpatialGradient = FLinearColor::LerpUsingHSV(LeftFarColor, RightNearColor, MixT);
			OutColor = FLinearColor::LerpUsingHSV(BaseColor, SpatialGradient, FMath::Clamp(SpatialColorBlend, 0.f, 1.f));
		}
	}

	OutColor.A = FMath::Clamp(Alpha, 0.f, 1.f);
	return OutColor;
}

FLinearColor USubSonarDisplayWidget::ResolveTrackColor(uint8 StateValue, bool bPriority) const
{
	FLinearColor Color = DotColor;
	switch (static_cast<ESonarTrackState>(StateValue))
	{
	case ESonarTrackState::Suspected:
		Color = FLinearColor(0.55f, 0.85f, 1.0f, 0.75f);
		break;
	case ESonarTrackState::Tracked:
		Color = FLinearColor(0.15f, 1.0f, 0.5f, 0.85f);
		break;
	case ESonarTrackState::Classified:
		Color = FLinearColor(1.0f, 0.95f, 0.25f, 0.90f);
		break;
	case ESonarTrackState::Confirmed:
		Color = FLinearColor(1.0f, 0.35f, 0.25f, 0.95f);
		break;
	case ESonarTrackState::Lost:
		Color = FLinearColor(0.65f, 0.65f, 0.65f, 0.45f);
		break;
	default:
		break;
	}

	if (bPriority)
	{
		Color.A = 1.f;
	}
	return Color;
}
