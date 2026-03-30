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
