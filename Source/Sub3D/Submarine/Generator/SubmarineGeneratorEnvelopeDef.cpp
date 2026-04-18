#include "SubmarineGeneratorEnvelopeDef.h"

float USubmarineGeneratorEnvelopeDef::ApplyCapProfile(EGenBowSternProfile Profile, float T, float Sharpness)
{
	const float Clamped = FMath::Clamp(T, 0.f, 1.f);
	const float S = FMath::Clamp(Sharpness, 0.1f, 4.f);

	switch (Profile)
	{
	case EGenBowSternProfile::Rounded:
		// sin sweep: zero at tip, full at body, smooth convex.
		return FMath::Sin(HALF_PI * Clamped);

	case EGenBowSternProfile::Needle:
		// Concave: radius stays small over most of the taper, ramps up near body.
		return FMath::Pow(Clamped, S);

	case EGenBowSternProfile::Blunt:
		// Convex: radius grows quickly from tip, flattens near body.
		return 1.f - FMath::Pow(1.f - Clamped, S);

	case EGenBowSternProfile::Bulbous:
		{
			const float Base = FMath::Sin(HALF_PI * Clamped) + 0.15f * FMath::Sin(PI * Clamped);
			return FMath::Clamp(Base, 0.f, 1.f);
		}

	case EGenBowSternProfile::Tapered:
		// Linear-biased: inverse of Needle when S>1.
		return FMath::Pow(Clamped, 1.f / S);
	}

	return Clamped;
}

namespace
{
// Shared helper that computes body boundaries from the envelope parameters.
// BodyStart and BodyEnd are normalized positions in [0, 1] that delimit the
// parallel body segment. The bow zone is [0, BodyStart] and the stern zone
// is [BodyEnd, 1]. BodyLengthFraction is the authoritative body length control;
// BowTaperFraction and SternTaperFraction are used only as weights to split
// the remaining spine (1 - BodyLengthFraction) between the two caps.
void ComputeBodyBoundaries(
	float BodyLengthFraction,
	float BowTaperFraction,
	float SternTaperFraction,
	float& OutBodyStart,
	float& OutBodyEnd)
{
	const float ClampedBody = FMath::Clamp(BodyLengthFraction, 0.1f, 0.9f);
	const float RemainingFraction = FMath::Max(0.f, 1.f - ClampedBody);
	const float WeightSum = FMath::Max(0.001f, BowTaperFraction + SternTaperFraction);
	const float BowShapeFraction = RemainingFraction * BowTaperFraction / WeightSum;
	const float SternShapeFraction = RemainingFraction * SternTaperFraction / WeightSum;

	OutBodyStart = BowShapeFraction;
	OutBodyEnd = 1.f - SternShapeFraction;
}
}

void USubmarineGeneratorEnvelopeDef::GetBodyBounds(float& OutBodyStart, float& OutBodyEnd) const
{
	ComputeBodyBoundaries(BodyLengthFraction, BowTaperFraction, SternTaperFraction, OutBodyStart, OutBodyEnd);
}

float USubmarineGeneratorEnvelopeDef::EvaluateRadius(float NormalizedPosition) const
{
	const float Pos = FMath::Clamp(NormalizedPosition, 0.f, 1.f);
	const float DefaultR = FMath::Max(1.f, DefaultRadiusCm);

	float BodyStart = 0.f;
	float BodyEnd = 1.f;
	ComputeBodyBoundaries(BodyLengthFraction, BowTaperFraction, SternTaperFraction, BodyStart, BodyEnd);

	// Body zone: constant radius.
	if (Pos >= BodyStart && Pos <= BodyEnd)
	{
		return DefaultR;
	}

	// Bow zone: [0, BodyStart]. T goes from 0 at tip to 1 at body boundary.
	if (Pos < BodyStart && BodyStart > KINDA_SMALL_NUMBER)
	{
		const float T = Pos / BodyStart;
		return DefaultR * ApplyCapProfile(BowProfile, T, BowSharpness);
	}

	// Stern zone: [BodyEnd, 1]. T goes from 0 at tip to 1 at body boundary.
	if (Pos > BodyEnd && BodyEnd < 1.f - KINDA_SMALL_NUMBER)
	{
		const float T = (1.f - Pos) / (1.f - BodyEnd);
		return DefaultR * ApplyCapProfile(SternProfile, T, SternSharpness);
	}

	return DefaultR;
}

float USubmarineGeneratorEnvelopeDef::EvaluateBowSternTaper(float NormalizedPosition) const
{
	const float Pos = FMath::Clamp(NormalizedPosition, 0.f, 1.f);

	float BodyStart = 0.f;
	float BodyEnd = 1.f;
	ComputeBodyBoundaries(BodyLengthFraction, BowTaperFraction, SternTaperFraction, BodyStart, BodyEnd);

	if (Pos >= BodyStart && Pos <= BodyEnd)
	{
		return 1.f;
	}

	if (Pos < BodyStart && BodyStart > KINDA_SMALL_NUMBER)
	{
		const float T = Pos / BodyStart;
		return ApplyCapProfile(BowProfile, T, BowSharpness);
	}

	if (Pos > BodyEnd && BodyEnd < 1.f - KINDA_SMALL_NUMBER)
	{
		const float T = (1.f - Pos) / (1.f - BodyEnd);
		return ApplyCapProfile(SternProfile, T, SternSharpness);
	}

	return 1.f;
}

float USubmarineGeneratorEnvelopeDef::EvaluateSectionHalfWidth(float LocalRadius, float VerticalOffset) const
{
	const float HalfH = FMath::Max(1.f, LocalRadius);
	const float HalfW = HalfH * FMath::Max(0.5f, WidthToHeightRatio);
	const float N = FMath::Max(0.5f, SectionExponent);

	const float AbsRatio = FMath::Abs(VerticalOffset) / HalfH;
	if (AbsRatio >= 1.f)
	{
		return 0.f;
	}

	return HalfW * FMath::Pow(FMath::Max(0.f, 1.f - FMath::Pow(AbsRatio, N)), 1.f / N);
}

void USubmarineGeneratorEnvelopeDef::EvaluateSectionPoint(float LocalRadius, float Angle, FVector2D& OutPosition, FVector2D& OutInwardNormal) const
{
	const float HalfH = FMath::Max(1.f, LocalRadius);
	const float HalfW = HalfH * FMath::Max(0.5f, WidthToHeightRatio);
	const float Exp = 2.f / FMath::Max(0.5f, SectionExponent);

	const float CosA = FMath::Cos(Angle);
	const float SinA = FMath::Sin(Angle);

	auto SuperellipsePow = [](float Base, float Power)
	{
		if (FMath::Abs(Base) < KINDA_SMALL_NUMBER) return 0.f;
		return FMath::Sign(Base) * FMath::Pow(FMath::Abs(Base), Power);
	};

	OutPosition.X = HalfW * SuperellipsePow(CosA, Exp);
	OutPosition.Y = HalfH * SuperellipsePow(SinA, Exp);

	const float N = SectionExponent;
	const float NX = -N * SuperellipsePow(CosA, N - 1.f) / FMath::Pow(HalfW, N);
	const float NY = -N * SuperellipsePow(SinA, N - 1.f) / FMath::Pow(HalfH, N);

	OutInwardNormal = FVector2D(NX, NY).GetSafeNormal();
}

float USubmarineGeneratorEnvelopeDef::GetSectionArcLength(float LocalRadius, float StartAngle, float EndAngle, int32 Samples) const
{
	if (Samples < 2) return 0.f;

	float TotalLength = 0.f;
	FVector2D PrevPos, DummyNormal;
	EvaluateSectionPoint(LocalRadius, StartAngle, PrevPos, DummyNormal);

	for (int32 i = 1; i <= Samples; ++i)
	{
		const float T = static_cast<float>(i) / static_cast<float>(Samples);
		const float CurrAngle = FMath::Lerp(StartAngle, EndAngle, T);
		FVector2D CurrPos;
		EvaluateSectionPoint(LocalRadius, CurrAngle, CurrPos, DummyNormal);

		TotalLength += FVector2D::Distance(PrevPos, CurrPos);
		PrevPos = CurrPos;
	}

	return TotalLength;
}
