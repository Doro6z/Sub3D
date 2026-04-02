#include "SubmarineEnvelopeDef.h"

#include "Curves/RichCurve.h"

namespace
{
float EvaluateProfileFunction(EBowSternProfile Profile, float T)
{
	const float ClampedT = FMath::Clamp(T, 0.f, 1.f);

	float Alpha = 2.f;
	float Beta = 0.5f;
	float Gamma = 0.f;

	switch (Profile)
	{
	case EBowSternProfile::Rounded:
		Alpha = 2.f; Beta = 0.5f;
		break;
	case EBowSternProfile::Needle:
		Alpha = 1.f; Beta = 1.f;
		break;
	case EBowSternProfile::Blunt:
		Alpha = 4.f; Beta = 0.3f;
		break;
	case EBowSternProfile::Bulbous:
		Alpha = 2.f; Beta = 0.5f; Gamma = 0.15f;
		break;
	case EBowSternProfile::Tapered:
		Alpha = 1.5f; Beta = 0.7f;
		break;
	}

	const float Base = FMath::Max(0.f, 1.f - FMath::Pow(ClampedT, Alpha));
	float Result = FMath::Pow(Base, Beta);

	if (Gamma > KINDA_SMALL_NUMBER)
	{
		Result *= (1.f + Gamma * FMath::Sin(PI * ClampedT));
	}

	return FMath::Clamp(Result, 0.f, 1.2f);
}
}

float USubmarineEnvelopeDef::EvaluateRadius(float NormalizedPosition) const
{
	const float ClampedPosition = FMath::Clamp(NormalizedPosition, 0.f, 1.f);
	const float FallbackRadius = FMath::Max(1.f, DefaultRadiusCm);
	const FRichCurve* Curve = RadiusProfile.GetRichCurveConst();

	if (!Curve || Curve->GetNumKeys() == 0)
	{
		return FallbackRadius;
	}

	return FMath::Max(1.f, Curve->Eval(ClampedPosition, FallbackRadius));
}

float USubmarineEnvelopeDef::EvaluateBowSternTaper(float NormalizedPosition) const
{
	const float Pos = FMath::Clamp(NormalizedPosition, 0.f, 1.f);

	if (Pos < BowTaperFraction && BowTaperFraction > KINDA_SMALL_NUMBER)
	{
		const float T = 1.f - (Pos / BowTaperFraction);
		return EvaluateProfileFunction(BowProfile, T);
	}

	if (Pos > (1.f - SternTaperFraction) && SternTaperFraction > KINDA_SMALL_NUMBER)
	{
		const float T = (Pos - (1.f - SternTaperFraction)) / SternTaperFraction;
		return EvaluateProfileFunction(SternProfile, T);
	}

	return 1.f;
}

float USubmarineEnvelopeDef::EvaluateSectionHalfWidth(float LocalRadius, float VerticalOffset) const
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

void USubmarineEnvelopeDef::EvaluateSectionPoint(float LocalRadius, float Angle, FVector2D& OutPosition, FVector2D& OutInwardNormal) const
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

	// The inward normal of a superellipse (x/a)^n + (y/b)^n = 1 is (-n*x^(n-1)/a^n, -n*y^(n-1)/b^n)
	// For our Exp format where Exp = 2/n:
	const float N = SectionExponent;
	const float NX = -N * SuperellipsePow(CosA, N - 1.f) / FMath::Pow(HalfW, N);
	const float NY = -N * SuperellipsePow(SinA, N - 1.f) / FMath::Pow(HalfH, N);

	OutInwardNormal = FVector2D(NX, NY).GetSafeNormal();
}

float USubmarineEnvelopeDef::GetSectionArcLength(float LocalRadius, float StartAngle, float EndAngle, int32 Samples) const
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
