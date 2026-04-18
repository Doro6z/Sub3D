#include "SubmarineAuthoringPipeline.h"

#include "Curves/RichCurve.h"
#include "UObject/Package.h"

namespace
{
struct FResolvedCompartment
{
	FCompiledSubmarineCompartmentData Data;
	int32 SourceIndex = INDEX_NONE;
};

struct FRingSample
{
	float X = 0.f;
	float Alpha = 0.f;
	float OuterRadius = 0.f;
	float InnerRadius = 0.f;
	float WidthToHeightRatio = 1.f;
	TArray<FVector> OuterPositions;
	TArray<FVector> InnerPositions;
	TArray<FVector> OuterNormals;
	TArray<FVector> InnerNormals;
};

struct FBindingBuildInfo
{
	float StartX = 0.f;
	float EndX = 0.f;
	FStructuralSheetCompiledBinding Binding;
	bool bHasGeometry = false;
};

struct FDoorOpeningBuildData
{
	FName OpeningId = NAME_None;
	bool bFits = false;
	int32 DeckIndex = 0;
	bool bBlockedByDefault = true;
	float MinZ = 0.f;
	float MaxZ = 0.f;
	float DoorHalfWidth = 0.f;
	float DoorBottomZ = 0.f;
	float DoorTopZ = 0.f;
	float BottomHalfWidth = 0.f;
	float TopHalfWidth = 0.f;
};

struct FResolvedVerticalOpening
{
	FCompiledSubmarineVerticalOpeningData Data;
};

float EvaluateProfileFunction(const EBowSternProfile Profile, const float T)
{
	const float ClampedT = FMath::Clamp(T, 0.f, 1.f);

	float Alpha = 2.f;
	float Beta = 0.5f;
	float Gamma = 0.f;

	switch (Profile)
	{
	case EBowSternProfile::Rounded:
		Alpha = 2.f;
		Beta = 0.5f;
		break;
	case EBowSternProfile::Needle:
		Alpha = 1.f;
		Beta = 1.f;
		break;
	case EBowSternProfile::Blunt:
		Alpha = 4.f;
		Beta = 0.3f;
		break;
	case EBowSternProfile::Bulbous:
		Alpha = 2.f;
		Beta = 0.5f;
		Gamma = 0.15f;
		break;
	case EBowSternProfile::Tapered:
		Alpha = 1.5f;
		Beta = 0.7f;
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

float EvaluateRadiusMultiplier(const FSubmarineHullAuthoring& Hull, const float SpineAlpha)
{
	const FRichCurve* Curve = Hull.LongitudinalRadiusCurve.GetRichCurveConst();
	const float CurveValue = (Curve && Curve->GetNumKeys() > 0)
		? Curve->Eval(SpineAlpha, 1.f)
		: 1.f;

	if (SpineAlpha < Hull.BowTaperFraction && Hull.BowTaperFraction > KINDA_SMALL_NUMBER)
	{
		const float T = 1.f - (SpineAlpha / Hull.BowTaperFraction);
		return FMath::Max(0.f, CurveValue * EvaluateProfileFunction(Hull.BowProfile, T));
	}

	if (SpineAlpha > (1.f - Hull.SternTaperFraction) && Hull.SternTaperFraction > KINDA_SMALL_NUMBER)
	{
		const float T = (SpineAlpha - (1.f - Hull.SternTaperFraction)) / Hull.SternTaperFraction;
		return FMath::Max(0.f, CurveValue * EvaluateProfileFunction(Hull.SternProfile, T));
	}

	return FMath::Max(0.f, CurveValue);
}

float EvaluateWidthToHeightRatio(const FSubmarineHullAuthoring& Hull, const float SpineAlpha)
{
	const FRichCurve* Curve = Hull.WidthToHeightCurve.GetRichCurveConst();
	const float Fallback = FMath::Max(0.5f, Hull.WidthToHeightRatio);
	return FMath::Clamp((Curve && Curve->GetNumKeys() > 0) ? Curve->Eval(SpineAlpha, Fallback) : Fallback, 0.5f, 2.0f);
}

float GetEffectiveSectionExponent(const FSubmarineHullAuthoring& Hull)
{
	switch (Hull.SectionProfile)
	{
	case ESubmarineSectionProfile::Circle:
	case ESubmarineSectionProfile::Ellipse:
		return 2.f;
	case ESubmarineSectionProfile::Superellipse:
	default:
		return FMath::Clamp(Hull.SectionRoundness, 0.5f, 8.f);
	}
}

float GetEffectiveWidthToHeightRatio(const FSubmarineHullAuthoring& Hull, const float SpineAlpha)
{
	if (Hull.SectionProfile == ESubmarineSectionProfile::Circle)
	{
		return 1.f;
	}

	return EvaluateWidthToHeightRatio(Hull, SpineAlpha);
}

float EvaluateSectionHalfWidth(
	const FSubmarineHullAuthoring& Hull,
	const float LocalRadius,
	const float WidthToHeightRatio,
	const float VerticalOffset)
{
	const float HalfH = FMath::Max(0.01f, LocalRadius);
	const float HalfW = HalfH * FMath::Max(0.5f, WidthToHeightRatio);
	const float N = GetEffectiveSectionExponent(Hull);
	const float AbsRatio = FMath::Abs(VerticalOffset) / HalfH;
	if (AbsRatio >= 1.f)
	{
		return 0.f;
	}

	return HalfW * FMath::Pow(FMath::Max(0.f, 1.f - FMath::Pow(AbsRatio, N)), 1.f / N);
}

void EvaluateSectionPointInternal(
	const FSubmarineHullAuthoring& Hull,
	const float LocalRadius,
	const float WidthToHeightRatio,
	const float Angle,
	FVector2D& OutPosition,
	FVector2D& OutInwardNormal)
{
	const float HalfH = FMath::Max(0.01f, LocalRadius);
	const float HalfW = HalfH * FMath::Max(0.5f, WidthToHeightRatio);
	const float N = GetEffectiveSectionExponent(Hull);
	const float Exp = 2.f / N;
	const float CosA = FMath::Cos(Angle);
	const float SinA = FMath::Sin(Angle);

	auto SuperellipsePow = [](const float Base, const float Power)
	{
		if (FMath::Abs(Base) < KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}

		return FMath::Sign(Base) * FMath::Pow(FMath::Abs(Base), Power);
	};

	OutPosition.X = HalfW * SuperellipsePow(CosA, Exp);
	OutPosition.Y = HalfH * SuperellipsePow(SinA, Exp);

	const float NX = -N * SuperellipsePow(CosA, N - 1.f) / FMath::Pow(HalfW, N);
	const float NY = -N * SuperellipsePow(SinA, N - 1.f) / FMath::Pow(HalfH, N);
	OutInwardNormal = FVector2D(NX, NY).GetSafeNormal();
}

bool AddError(TArray<FLayoutValidationMessage>& OutMessages, const FName RelatedId, const FString& Message)
{
	FLayoutValidationMessage ValidationMessage;
	ValidationMessage.Severity = ELayoutValidationSeverity::Error;
	ValidationMessage.RelatedId = RelatedId;
	ValidationMessage.Message = FText::FromString(Message);
	OutMessages.Add(ValidationMessage);
	return false;
}

void AddWarning(TArray<FLayoutValidationMessage>& OutMessages, const FName RelatedId, const FString& Message)
{
	FLayoutValidationMessage ValidationMessage;
	ValidationMessage.Severity = ELayoutValidationSeverity::Warning;
	ValidationMessage.RelatedId = RelatedId;
	ValidationMessage.Message = FText::FromString(Message);
	OutMessages.Add(ValidationMessage);
}
}

bool USubmarineAuthoringBakeLibrary::ValidateAuthoringAsset(
	const USubmarineAuthoringAsset* AuthoringAsset,
	TArray<FLayoutValidationMessage>& OutMessages)
{
	OutMessages.Reset();

	if (!AuthoringAsset)
	{
		return AddError(OutMessages, NAME_None, TEXT("Authoring asset is null."));
	}

	if (AuthoringAsset->Hull.LengthCm < 500.f)
	{
		AddError(OutMessages, AuthoringAsset->SubmarineId, TEXT("Hull length must be at least 500 cm."));
	}

	if (AuthoringAsset->Hull.MaxOuterDiameterCm < 100.f)
	{
		AddError(OutMessages, AuthoringAsset->SubmarineId, TEXT("Max outer diameter must be at least 100 cm."));
	}

	if (AuthoringAsset->Hull.WallThicknessCm <= 0.f
		|| AuthoringAsset->Hull.WallThicknessCm >= (AuthoringAsset->Hull.MaxOuterDiameterCm * 0.5f))
	{
		AddError(OutMessages, AuthoringAsset->SubmarineId, TEXT("Wall thickness must stay below hull radius."));
	}

	if (AuthoringAsset->Decks.Num() == 0)
	{
		AddError(OutMessages, AuthoringAsset->SubmarineId, TEXT("At least one deck is required."));
	}

	if (AuthoringAsset->BakeSettings.LongitudinalSegments < 8)
	{
		AddError(OutMessages, AuthoringAsset->SubmarineId, TEXT("Longitudinal segment count must be at least 8."));
	}

	if (AuthoringAsset->BakeSettings.RadialSegments < 12)
	{
		AddError(OutMessages, AuthoringAsset->SubmarineId, TEXT("Radial segment count must be at least 12."));
	}

	TSet<FName> CompartmentIds;
	float TotalMinLength = 0.f;
	for (const FSubmarineCompartmentAuthoring& Compartment : AuthoringAsset->Compartments)
	{
		if (Compartment.CompartmentId.IsNone())
		{
			AddError(OutMessages, NAME_None, TEXT("Each compartment requires a valid id."));
			continue;
		}

		if (CompartmentIds.Contains(Compartment.CompartmentId))
		{
			AddError(OutMessages, Compartment.CompartmentId, TEXT("Duplicate compartment id."));
			continue;
		}

		CompartmentIds.Add(Compartment.CompartmentId);
		TotalMinLength += Compartment.MinLengthCm;
	}

	if (AuthoringAsset->Compartments.Num() > 0 && TotalMinLength > AuthoringAsset->Hull.LengthCm)
	{
		AddError(OutMessages, AuthoringAsset->SubmarineId, TEXT("Sum of compartment minimum lengths exceeds hull length."));
	}

	TSet<FName> DeckIds;
	for (int32 DeckIndex = 0; DeckIndex < AuthoringAsset->Decks.Num(); ++DeckIndex)
	{
		const FSubmarineDeckAuthoring& Deck = AuthoringAsset->Decks[DeckIndex];
		const FName EffectiveDeckId = Deck.DeckId.IsNone()
			? FName(*FString::Printf(TEXT("Deck_%d"), DeckIndex))
			: Deck.DeckId;

		if (DeckIds.Contains(EffectiveDeckId))
		{
			AddError(OutMessages, EffectiveDeckId, TEXT("Duplicate deck id."));
		}
		DeckIds.Add(EffectiveDeckId);

		const float InnerRadius = (AuthoringAsset->Hull.MaxOuterDiameterCm * 0.5f) - AuthoringAsset->Hull.WallThicknessCm;
		if (FMath::Abs(Deck.LocalZCm) >= InnerRadius)
		{
			AddWarning(OutMessages, EffectiveDeckId, TEXT("Deck height is close to or outside the interior hull."));
		}
	}

	for (const FSubmarineVerticalOpeningAuthoring& Opening : AuthoringAsset->VerticalOpenings)
	{
		if (!AuthoringAsset->Decks.IsValidIndex(Opening.FromDeckIndex)
			|| !AuthoringAsset->Decks.IsValidIndex(Opening.ToDeckIndex))
		{
			AddError(OutMessages, Opening.OpeningId, TEXT("Vertical opening references an invalid deck index."));
			continue;
		}

		if (Opening.FromDeckIndex == Opening.ToDeckIndex)
		{
			AddError(OutMessages, Opening.OpeningId, TEXT("Vertical opening must connect two different decks."));
		}
	}

	for (const FSubmarineBulkheadConnectionAuthoring& Connection : AuthoringAsset->BulkheadConnections)
	{
		if (AuthoringAsset->Compartments.Num() > 0
			&& (!CompartmentIds.Contains(Connection.CompartmentA) || !CompartmentIds.Contains(Connection.CompartmentB)))
		{
			AddError(OutMessages, Connection.BoundaryId, TEXT("Bulkhead connection references an unknown compartment."));
		}

		for (const FSubmarineBulkheadOpeningAuthoring& Opening : Connection.Openings)
		{
			if (!AuthoringAsset->Decks.IsValidIndex(Opening.DeckIndex))
			{
				AddError(OutMessages, Connection.BoundaryId, TEXT("Bulkhead opening references an invalid deck index."));
			}
		}
	}

	for (const FSubmarineVerticalConnectorAuthoring& Connector : AuthoringAsset->VerticalConnectors)
	{
		if (!Connector.OpeningId.IsNone())
		{
			const bool bOpeningExists = AuthoringAsset->VerticalOpenings.ContainsByPredicate([&Connector](const FSubmarineVerticalOpeningAuthoring& Opening)
			{
				return Opening.OpeningId == Connector.OpeningId;
			});
			if (!bOpeningExists)
			{
				AddError(OutMessages, Connector.ConnectorId, TEXT("Vertical connector references an unknown vertical opening."));
				continue;
			}
		}
		else if (!AuthoringAsset->Decks.IsValidIndex(Connector.FromDeckIndex)
			|| !AuthoringAsset->Decks.IsValidIndex(Connector.ToDeckIndex))
		{
			AddError(OutMessages, Connector.ConnectorId, TEXT("Vertical connector references an invalid deck index."));
			continue;
		}

		if (Connector.FromDeckIndex == Connector.ToDeckIndex)
		{
			AddError(OutMessages, Connector.ConnectorId, TEXT("Vertical connector must connect two different decks."));
		}
	}

	return !OutMessages.ContainsByPredicate([](const FLayoutValidationMessage& Message)
	{
		return Message.Severity == ELayoutValidationSeverity::Error;
	});
}

FVector USubmarineHullEvaluationLibrary::EvaluateSectionPoint(const FSubmarineHullAuthoring& Hull, const float SpineAlpha, const float ArcAlpha)
{
	const float ClampedSpine = FMath::Clamp(SpineAlpha, 0.f, 1.f);
	const float ClampedArc = FMath::Clamp(ArcAlpha, 0.f, 1.f);
	const float Radius = (Hull.MaxOuterDiameterCm * 0.5f) * EvaluateRadiusMultiplier(Hull, ClampedSpine);
	const float Ratio = GetEffectiveWidthToHeightRatio(Hull, ClampedSpine);
	FVector2D SectionPoint = FVector2D::ZeroVector;
	FVector2D InwardNormal(0.0, 1.0);
	EvaluateSectionPointInternal(Hull, Radius, Ratio, ClampedArc * UE_TWO_PI, SectionPoint, InwardNormal);
	return FVector(ClampedSpine * Hull.LengthCm, SectionPoint.X, SectionPoint.Y);
}

float USubmarineHullEvaluationLibrary::EvaluateBowSternTaper(const FSubmarineHullAuthoring& Hull, const float SpineAlpha)
{
	return EvaluateRadiusMultiplier(Hull, FMath::Clamp(SpineAlpha, 0.f, 1.f));
}

float USubmarineHullEvaluationLibrary::GetSectionArcLengthEstimate(const FSubmarineHullAuthoring& Hull, const float SpineAlpha, int32 NumSamples)
{
	const int32 Samples = FMath::Max(4, NumSamples);
	const float ClampedSpine = FMath::Clamp(SpineAlpha, 0.f, 1.f);
	const float Radius = (Hull.MaxOuterDiameterCm * 0.5f) * EvaluateRadiusMultiplier(Hull, ClampedSpine);
	const float Ratio = GetEffectiveWidthToHeightRatio(Hull, ClampedSpine);

	float Length = 0.f;
	FVector2D PreviousPoint = FVector2D::ZeroVector;
	FVector2D InwardNormal(0.0, 1.0);
	EvaluateSectionPointInternal(Hull, Radius, Ratio, 0.f, PreviousPoint, InwardNormal);

	for (int32 SampleIndex = 1; SampleIndex <= Samples; ++SampleIndex)
	{
		const float Angle = (static_cast<float>(SampleIndex) / static_cast<float>(Samples)) * UE_TWO_PI;
		FVector2D CurrentPoint = FVector2D::ZeroVector;
		EvaluateSectionPointInternal(Hull, Radius, Ratio, Angle, CurrentPoint, InwardNormal);
		Length += FVector2D::Distance(PreviousPoint, CurrentPoint);
		PreviousPoint = CurrentPoint;
	}

	return Length;
}

namespace
{
void AppendVertex(
	FCompiledSubmarineMeshSection& Section,
	const FVector& Position,
	const FVector& Normal,
	const FVector& Tangent,
	const FVector2D& UV)
{
	Section.Positions.Add(FVector3f(Position));
	Section.Normals.Add(FVector3f(Normal.GetSafeNormal()));
	Section.Tangents.Add(FVector4f(FVector3f(Tangent.GetSafeNormal()), 1.f));
	Section.UV0.Add(FVector2f(UV));
}

void AddQuad(
	FCompiledSubmarineMeshSection& Section,
	const FVector& V00,
	const FVector& V01,
	const FVector& V10,
	const FVector& V11,
	const FVector& N00,
	const FVector& N01,
	const FVector& N10,
	const FVector& N11,
	const FVector2D& UV00,
	const FVector2D& UV01,
	const FVector2D& UV10,
	const FVector2D& UV11,
	const FVector& DesiredFacingNormal)
{
	const int32 BaseIndex = Section.Positions.Num();
	const FVector Tangent = (V10 - V00).GetSafeNormal();
	AppendVertex(Section, V00, N00, Tangent, UV00);
	AppendVertex(Section, V01, N01, Tangent, UV01);
	AppendVertex(Section, V10, N10, Tangent, UV10);
	AppendVertex(Section, V11, N11, Tangent, UV11);

	const FVector CurrentFacing = FVector::CrossProduct(V01 - V00, V10 - V00).GetSafeNormal();
	const bool bFlip = FVector::DotProduct(CurrentFacing, DesiredFacingNormal.GetSafeNormal()) < 0.f;
	if (bFlip)
	{
		Section.Indices.Append({ BaseIndex + 0, BaseIndex + 2, BaseIndex + 1, BaseIndex + 2, BaseIndex + 3, BaseIndex + 1 });
	}
	else
	{
		Section.Indices.Append({ BaseIndex + 0, BaseIndex + 1, BaseIndex + 2, BaseIndex + 2, BaseIndex + 1, BaseIndex + 3 });
	}
}

TArray<FResolvedCompartment> ResolveCompartments(const USubmarineAuthoringAsset* AuthoringAsset)
{
	TArray<FResolvedCompartment> Resolved;
	if (AuthoringAsset->Compartments.Num() == 0)
	{
		FResolvedCompartment Fallback;
		Fallback.Data.CompartmentId = TEXT("HullMain");
		Fallback.Data.DisplayName = FText::FromString(TEXT("Hull Main"));
		Fallback.Data.Type = ECompartmentType::Corridor;
		Fallback.Data.StartXcm = 0.f;
		Fallback.Data.EndXcm = AuthoringAsset->Hull.LengthCm;
		Resolved.Add(Fallback);
		return Resolved;
	}

	Resolved.Reserve(AuthoringAsset->Compartments.Num());

	for (int32 Index = 0; Index < AuthoringAsset->Compartments.Num(); ++Index)
	{
		FResolvedCompartment Entry;
		Entry.SourceIndex = Index;
		Entry.Data.CompartmentId = AuthoringAsset->Compartments[Index].CompartmentId;
		Entry.Data.DisplayName = AuthoringAsset->Compartments[Index].DisplayName;
		Entry.Data.Type = AuthoringAsset->Compartments[Index].Type;
		Resolved.Add(Entry);
	}

	Resolved.StableSort([AuthoringAsset](const FResolvedCompartment& A, const FResolvedCompartment& B)
	{
		const FSubmarineCompartmentAuthoring& SourceA = AuthoringAsset->Compartments[A.SourceIndex];
		const FSubmarineCompartmentAuthoring& SourceB = AuthoringAsset->Compartments[B.SourceIndex];
		if (SourceA.Priority != SourceB.Priority)
		{
			return SourceA.Priority < SourceB.Priority;
		}
		return A.SourceIndex < B.SourceIndex;
	});

	float TotalMin = 0.f;
	float TotalDesiredExtra = 0.f;
	for (const FResolvedCompartment& Entry : Resolved)
	{
		const FSubmarineCompartmentAuthoring& Source = AuthoringAsset->Compartments[Entry.SourceIndex];
		TotalMin += Source.MinLengthCm;
		TotalDesiredExtra += FMath::Max(0.f, Source.TargetLengthCm - Source.MinLengthCm);
	}

	const float AvailableExtra = FMath::Max(0.f, AuthoringAsset->Hull.LengthCm - TotalMin);
	float Cursor = 0.f;

	for (int32 Index = 0; Index < Resolved.Num(); ++Index)
	{
		const FSubmarineCompartmentAuthoring& Source = AuthoringAsset->Compartments[Resolved[Index].SourceIndex];
		const float DesiredExtra = FMath::Max(0.f, Source.TargetLengthCm - Source.MinLengthCm);
		const float AllocatedExtra = (TotalDesiredExtra > KINDA_SMALL_NUMBER)
			? AvailableExtra * (DesiredExtra / TotalDesiredExtra)
			: AvailableExtra / FMath::Max(1, Resolved.Num());
		const float Length = (Index == Resolved.Num() - 1)
			? (AuthoringAsset->Hull.LengthCm - Cursor)
			: (Source.MinLengthCm + AllocatedExtra);
		Resolved[Index].Data.StartXcm = Cursor;
		Resolved[Index].Data.EndXcm = Cursor + Length;
		Cursor += Length;
	}

	return Resolved;
}

TArray<FResolvedVerticalOpening> ResolveVerticalOpenings(const USubmarineAuthoringAsset* AuthoringAsset)
{
	TArray<FResolvedVerticalOpening> Resolved;
	Resolved.Reserve(AuthoringAsset->VerticalOpenings.Num());

	for (const FSubmarineVerticalOpeningAuthoring& Opening : AuthoringAsset->VerticalOpenings)
	{
		FResolvedVerticalOpening Entry;
		Entry.Data.OpeningId = Opening.OpeningId.IsNone()
			? FName(*FString::Printf(TEXT("Opening_%d_%d_%.0f"), Opening.FromDeckIndex, Opening.ToDeckIndex, Opening.LocalX))
			: Opening.OpeningId;
		Entry.Data.Type = Opening.Type;
		Entry.Data.FromDeckIndex = Opening.FromDeckIndex;
		Entry.Data.ToDeckIndex = Opening.ToDeckIndex;
		Entry.Data.LocalX = Opening.LocalX;
		Entry.Data.WidthCm = Opening.WidthCm;
		Entry.Data.LengthCm = Opening.LengthCm;
		if (AuthoringAsset->Decks.IsValidIndex(Opening.FromDeckIndex) && AuthoringAsset->Decks.IsValidIndex(Opening.ToDeckIndex))
		{
			Entry.Data.LowerDeckZCm = FMath::Min(AuthoringAsset->Decks[Opening.FromDeckIndex].LocalZCm, AuthoringAsset->Decks[Opening.ToDeckIndex].LocalZCm);
			Entry.Data.UpperDeckZCm = FMath::Max(AuthoringAsset->Decks[Opening.FromDeckIndex].LocalZCm, AuthoringAsset->Decks[Opening.ToDeckIndex].LocalZCm);
		}
		Resolved.Add(Entry);
	}

	return Resolved;
}

TArray<FRingSample> BuildRings(const USubmarineAuthoringAsset* AuthoringAsset)
{
	const int32 RingCount = AuthoringAsset->BakeSettings.LongitudinalSegments + 1;
	const int32 RadialSegments = AuthoringAsset->BakeSettings.RadialSegments;
	const float MaxOuterRadius = AuthoringAsset->Hull.MaxOuterDiameterCm * 0.5f;

	TArray<FRingSample> Rings;
	Rings.Reserve(RingCount);

	for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
	{
		const float Alpha = RingCount > 1 ? static_cast<float>(RingIndex) / static_cast<float>(RingCount - 1) : 0.f;
		FRingSample Ring;
		Ring.Alpha = Alpha;
		Ring.X = Alpha * AuthoringAsset->Hull.LengthCm;
		Ring.WidthToHeightRatio = GetEffectiveWidthToHeightRatio(AuthoringAsset->Hull, Alpha);
		Ring.OuterRadius = MaxOuterRadius * EvaluateRadiusMultiplier(AuthoringAsset->Hull, Alpha);
		Ring.InnerRadius = FMath::Max(0.f, Ring.OuterRadius - AuthoringAsset->Hull.WallThicknessCm);

		Ring.OuterPositions.Reserve(RadialSegments);
		Ring.InnerPositions.Reserve(RadialSegments);
		Ring.OuterNormals.Reserve(RadialSegments);
		Ring.InnerNormals.Reserve(RadialSegments);

		for (int32 SegmentIndex = 0; SegmentIndex < RadialSegments; ++SegmentIndex)
		{
			const float Angle = (static_cast<float>(SegmentIndex) / static_cast<float>(RadialSegments)) * UE_TWO_PI;
			FVector2D SectionPoint = FVector2D::ZeroVector;
			FVector2D InwardNormal2D(0.0, 1.0);

			EvaluateSectionPointInternal(AuthoringAsset->Hull, Ring.OuterRadius, Ring.WidthToHeightRatio, Angle, SectionPoint, InwardNormal2D);
			Ring.OuterPositions.Add(FVector(Ring.X, SectionPoint.X, SectionPoint.Y));
			Ring.OuterNormals.Add(FVector(0.f, -InwardNormal2D.X, -InwardNormal2D.Y));

			EvaluateSectionPointInternal(AuthoringAsset->Hull, Ring.InnerRadius, Ring.WidthToHeightRatio, Angle, SectionPoint, InwardNormal2D);
			Ring.InnerPositions.Add(FVector(Ring.X, SectionPoint.X, SectionPoint.Y));
			Ring.InnerNormals.Add(FVector(0.f, InwardNormal2D.X, InwardNormal2D.Y));
		}

		Rings.Add(MoveTemp(Ring));
	}

	return Rings;
}

ESheetSide ClassifyShellSide(const FVector& LocalPoint)
{
	if (FMath::Abs(LocalPoint.Z) >= FMath::Abs(LocalPoint.Y))
	{
		return LocalPoint.Z >= 0.f ? ESheetSide::Top : ESheetSide::Bottom;
	}

	return LocalPoint.Y >= 0.f ? ESheetSide::Starboard : ESheetSide::Port;
}

TArray<int32> BuildSegmentSideLut(const TArray<FRingSample>& Rings)
{
	TArray<int32> SegmentSides;
	if (Rings.Num() == 0)
	{
		return SegmentSides;
	}

	const int32 SegmentCount = Rings[0].OuterPositions.Num();
	SegmentSides.SetNum(SegmentCount);
	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const int32 NextIndex = (SegmentIndex + 1) % SegmentCount;
		const FVector Mid = (Rings[0].OuterPositions[SegmentIndex] + Rings[0].OuterPositions[NextIndex]) * 0.5f;
		SegmentSides[SegmentIndex] = static_cast<int32>(ClassifyShellSide(Mid));
	}

	return SegmentSides;
}

FName MakeSheetId(const FName CompartmentId, const ESheetSide Side, const int32 SliceIndex)
{
	return FName(*FString::Printf(TEXT("%s_%s_%02d"), *CompartmentId.ToString(), *StaticEnum<ESheetSide>()->GetNameStringByValue(static_cast<int64>(Side)), SliceIndex));
}

TArray<FBindingBuildInfo> BuildBindingInfos(
	const USubmarineAuthoringAsset* AuthoringAsset,
	const TArray<FResolvedCompartment>& Compartments)
{
	TArray<FBindingBuildInfo> Infos;
	if (!AuthoringAsset->Structure.bGenerateStructuralBindings)
	{
		return Infos;
	}

	const TArray<ESheetSide> SupportedSides = { ESheetSide::Port, ESheetSide::Starboard, ESheetSide::Top, ESheetSide::Bottom };
	const int32 LongitudinalSlices = FMath::Max(1, AuthoringAsset->Structure.LongitudinalSheetsPerCompartment);

	for (int32 CompartmentIndex = 0; CompartmentIndex < Compartments.Num(); ++CompartmentIndex)
	{
		const FCompiledSubmarineCompartmentData& Compartment = Compartments[CompartmentIndex].Data;
		const float Span = FMath::Max(0.f, Compartment.EndXcm - Compartment.StartXcm);
		const float SliceLength = Span / static_cast<float>(LongitudinalSlices);

		for (int32 SliceIndex = 0; SliceIndex < LongitudinalSlices; ++SliceIndex)
		{
			const float SliceStart = Compartment.StartXcm + (SliceLength * SliceIndex);
			const float SliceEnd = (SliceIndex == LongitudinalSlices - 1)
				? Compartment.EndXcm
				: (SliceStart + SliceLength);

			for (const ESheetSide Side : SupportedSides)
			{
				FBindingBuildInfo Info;
				Info.StartX = SliceStart;
				Info.EndX = SliceEnd;
				Info.Binding.SheetId = MakeSheetId(Compartment.CompartmentId, Side, SliceIndex);
				Info.Binding.CompartmentIndex = CompartmentIndex;
				Info.Binding.Side = Side;
				Infos.Add(Info);
			}
		}
	}

	return Infos;
}

FVector GetSideNormal(const ESheetSide Side)
{
	switch (Side)
	{
	case ESheetSide::Port:
		return FVector(0.f, -1.f, 0.f);
	case ESheetSide::Starboard:
		return FVector(0.f, 1.f, 0.f);
	case ESheetSide::Top:
		return FVector(0.f, 0.f, 1.f);
	case ESheetSide::Bottom:
		return FVector(0.f, 0.f, -1.f);
	default:
		return FVector::ForwardVector;
	}
}

void BuildShellSection(
	const TArray<FRingSample>& Rings,
	const TArray<int32>& SegmentSides,
	const bool bExterior,
	FCompiledSubmarineMeshSection& OutSection,
	TArray<FBindingBuildInfo>& InOutBindings)
{
	for (FBindingBuildInfo& BindingInfo : InOutBindings)
	{
		const int32 VertexStart = OutSection.Positions.Num();
		const int32 TriangleStart = OutSection.Indices.Num() / 3;
		FBox Bounds(EForceInit::ForceInit);
		int32 UsedStartSection = INDEX_NONE;
		int32 UsedEndSection = INDEX_NONE;

		for (int32 RingIndex = 0; RingIndex < Rings.Num() - 1; ++RingIndex)
		{
			const float MidX = (Rings[RingIndex].X + Rings[RingIndex + 1].X) * 0.5f;
			if (MidX < BindingInfo.StartX || MidX > BindingInfo.EndX)
			{
				continue;
			}

			for (int32 SegmentIndex = 0; SegmentIndex < SegmentSides.Num(); ++SegmentIndex)
			{
				if (static_cast<ESheetSide>(SegmentSides[SegmentIndex]) != BindingInfo.Binding.Side)
				{
					continue;
				}

				const int32 NextSegment = (SegmentIndex + 1) % SegmentSides.Num();
				const TArray<FVector>& PositionsA = bExterior ? Rings[RingIndex].OuterPositions : Rings[RingIndex].InnerPositions;
				const TArray<FVector>& PositionsB = bExterior ? Rings[RingIndex + 1].OuterPositions : Rings[RingIndex + 1].InnerPositions;
				const TArray<FVector>& NormalsA = bExterior ? Rings[RingIndex].OuterNormals : Rings[RingIndex].InnerNormals;
				const TArray<FVector>& NormalsB = bExterior ? Rings[RingIndex + 1].OuterNormals : Rings[RingIndex + 1].InnerNormals;

				AddQuad(
					OutSection,
					PositionsA[SegmentIndex],
					PositionsA[NextSegment],
					PositionsB[SegmentIndex],
					PositionsB[NextSegment],
					NormalsA[SegmentIndex],
					NormalsA[NextSegment],
					NormalsB[SegmentIndex],
					NormalsB[NextSegment],
					FVector2D(Rings[RingIndex].Alpha, static_cast<float>(SegmentIndex) / static_cast<float>(SegmentSides.Num())),
					FVector2D(Rings[RingIndex].Alpha, static_cast<float>(NextSegment) / static_cast<float>(SegmentSides.Num())),
					FVector2D(Rings[RingIndex + 1].Alpha, static_cast<float>(SegmentIndex) / static_cast<float>(SegmentSides.Num())),
					FVector2D(Rings[RingIndex + 1].Alpha, static_cast<float>(NextSegment) / static_cast<float>(SegmentSides.Num())),
					GetSideNormal(BindingInfo.Binding.Side));

				Bounds += PositionsA[SegmentIndex];
				Bounds += PositionsA[NextSegment];
				Bounds += PositionsB[SegmentIndex];
				Bounds += PositionsB[NextSegment];
				UsedStartSection = UsedStartSection == INDEX_NONE ? RingIndex : FMath::Min(UsedStartSection, RingIndex);
				UsedEndSection = FMath::Max(UsedEndSection, RingIndex + 1);
			}
		}

		const int32 VertexCount = OutSection.Positions.Num() - VertexStart;
		const int32 TriangleCount = (OutSection.Indices.Num() / 3) - TriangleStart;
		if (VertexCount <= 0 || TriangleCount <= 0)
		{
			continue;
		}

		BindingInfo.bHasGeometry = true;
		BindingInfo.Binding.MeshRange.SectionIndexStart = UsedStartSection;
		BindingInfo.Binding.MeshRange.SectionIndexEnd = UsedEndSection;
		BindingInfo.Binding.MeshRange.LocalBounds = Bounds;
		BindingInfo.Binding.MeshRange.LocalCenter = Bounds.GetCenter();
		BindingInfo.Binding.MeshRange.LocalNormal = GetSideNormal(BindingInfo.Binding.Side);
		BindingInfo.Binding.ChartMin = FVector2D(0.f, 0.f);
		BindingInfo.Binding.ChartMax = FVector2D(1.f, 1.f);

		if (bExterior)
		{
			BindingInfo.Binding.MeshRange.ExteriorVertexStart = VertexStart;
			BindingInfo.Binding.MeshRange.ExteriorVertexCount = VertexCount;
			BindingInfo.Binding.MeshRange.ExteriorTriangleStart = TriangleStart;
			BindingInfo.Binding.MeshRange.ExteriorTriangleCount = TriangleCount;
		}
		else
		{
			BindingInfo.Binding.MeshRange.InteriorVertexStart = VertexStart;
			BindingInfo.Binding.MeshRange.InteriorVertexCount = VertexCount;
			BindingInfo.Binding.MeshRange.InteriorTriangleStart = TriangleStart;
			BindingInfo.Binding.MeshRange.InteriorTriangleCount = TriangleCount;
		}
	}
}

int32 FindFirstClosureRingIndex(const TArray<FRingSample>& Rings)
{
	for (int32 RingIndex = 0; RingIndex < Rings.Num(); ++RingIndex)
	{
		if (Rings[RingIndex].OuterRadius > KINDA_SMALL_NUMBER
			&& Rings[RingIndex].InnerRadius > KINDA_SMALL_NUMBER
			&& (Rings[RingIndex].OuterRadius - Rings[RingIndex].InnerRadius) > KINDA_SMALL_NUMBER)
		{
			return RingIndex;
		}
	}

	return INDEX_NONE;
}

int32 FindLastClosureRingIndex(const TArray<FRingSample>& Rings)
{
	for (int32 RingIndex = Rings.Num() - 1; RingIndex >= 0; --RingIndex)
	{
		if (Rings[RingIndex].OuterRadius > KINDA_SMALL_NUMBER
			&& Rings[RingIndex].InnerRadius > KINDA_SMALL_NUMBER
			&& (Rings[RingIndex].OuterRadius - Rings[RingIndex].InnerRadius) > KINDA_SMALL_NUMBER)
		{
			return RingIndex;
		}
	}

	return INDEX_NONE;
}

FCompiledSubmarineMeshSection BuildHullClosureSection(
	const TArray<FRingSample>& Rings,
	const bool bForeClosure,
	const int32 MaterialSlotIndex)
{
	FCompiledSubmarineMeshSection Section;
	Section.SectionId = bForeClosure ? TEXT("HullClosure_Fore") : TEXT("HullClosure_Aft");
	Section.MaterialSlotIndex = MaterialSlotIndex;

	const int32 RingIndex = bForeClosure ? FindFirstClosureRingIndex(Rings) : FindLastClosureRingIndex(Rings);
	if (!Rings.IsValidIndex(RingIndex))
	{
		return Section;
	}

	const FRingSample& Ring = Rings[RingIndex];
	const int32 PointCount = FMath::Min(Ring.OuterPositions.Num(), Ring.InnerPositions.Num());
	if (PointCount < 3)
	{
		return Section;
	}

	const FVector FacingNormal = bForeClosure ? -FVector::ForwardVector : FVector::ForwardVector;
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		const int32 NextIndex = (PointIndex + 1) % PointCount;
		const FVector& Outer0 = Ring.OuterPositions[PointIndex];
		const FVector& Outer1 = Ring.OuterPositions[NextIndex];
		const FVector& Inner0 = Ring.InnerPositions[PointIndex];
		const FVector& Inner1 = Ring.InnerPositions[NextIndex];

		AddQuad(
			Section,
			Outer0,
			Outer1,
			Inner0,
			Inner1,
			FacingNormal,
			FacingNormal,
			FacingNormal,
			FacingNormal,
			FVector2D(0.f, static_cast<float>(PointIndex) / static_cast<float>(PointCount)),
			FVector2D(0.f, static_cast<float>(NextIndex) / static_cast<float>(PointCount)),
			FVector2D(1.f, static_cast<float>(PointIndex) / static_cast<float>(PointCount)),
			FVector2D(1.f, static_cast<float>(NextIndex) / static_cast<float>(PointCount)),
			FacingNormal);
		AddQuad(
			Section,
			Outer0,
			Inner0,
			Outer1,
			Inner1,
			-FacingNormal,
			-FacingNormal,
			-FacingNormal,
			-FacingNormal,
			FVector2D(0.f, static_cast<float>(PointIndex) / static_cast<float>(PointCount)),
			FVector2D(1.f, static_cast<float>(PointIndex) / static_cast<float>(PointCount)),
			FVector2D(0.f, static_cast<float>(NextIndex) / static_cast<float>(PointCount)),
			FVector2D(1.f, static_cast<float>(NextIndex) / static_cast<float>(PointCount)),
			-FacingNormal);
	}

	return Section;
}

FCompiledSubmarineMeshSection BuildDeckSection(
	const USubmarineAuthoringAsset* AuthoringAsset,
	const FSubmarineDeckAuthoring& Deck,
	const int32 DeckIndex,
	const int32 MaterialSlotIndex,
	const TArray<FResolvedVerticalOpening>& VerticalOpenings,
	const TArray<FRingSample>& Rings)
{
	FCompiledSubmarineMeshSection Section;
	Section.SectionId = Deck.DeckId.IsNone()
		? FName(*FString::Printf(TEXT("Deck_%d"), DeckIndex))
		: Deck.DeckId;
	Section.MaterialSlotIndex = MaterialSlotIndex;

	const float ActiveStartX = FMath::Max(0.f, Deck.StartInsetCm);
	const float ActiveEndX = FMath::Max(ActiveStartX, AuthoringAsset->Hull.LengthCm - Deck.EndInsetCm);

	for (int32 RingIndex = 0; RingIndex < Rings.Num() - 1; ++RingIndex)
	{
		const FRingSample& RingA = Rings[RingIndex];
		const FRingSample& RingB = Rings[RingIndex + 1];
		const float SpanStartX = RingA.X;
		const float SpanEndX = RingB.X;
		const float MidX = (RingA.X + RingB.X) * 0.5f;
		if (MidX < ActiveStartX || MidX > ActiveEndX)
		{
			continue;
		}

		bool bBlockedByHatchCutout = false;
		for (const FResolvedVerticalOpening& Opening : VerticalOpenings)
		{
			if (Opening.Data.FromDeckIndex != DeckIndex && Opening.Data.ToDeckIndex != DeckIndex)
			{
				continue;
			}

			const float HatchHalfLength = FMath::Max(30.f, Opening.Data.LengthCm * 0.5f);
			const float HatchStartX = Opening.Data.LocalX - HatchHalfLength;
			const float HatchEndX = Opening.Data.LocalX + HatchHalfLength;
			if (SpanEndX > HatchStartX && SpanStartX < HatchEndX)
			{
				bBlockedByHatchCutout = true;
				break;
			}
		}

		if (bBlockedByHatchCutout)
		{
			continue;
		}

		const float WidthA = EvaluateSectionHalfWidth(AuthoringAsset->Hull, RingA.InnerRadius, RingA.WidthToHeightRatio, Deck.LocalZCm) * Deck.WidthScale;
		const float WidthB = EvaluateSectionHalfWidth(AuthoringAsset->Hull, RingB.InnerRadius, RingB.WidthToHeightRatio, Deck.LocalZCm) * Deck.WidthScale;
		if (WidthA <= KINDA_SMALL_NUMBER || WidthB <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector V00(RingA.X, -WidthA, Deck.LocalZCm);
		const FVector V01(RingA.X, WidthA, Deck.LocalZCm);
		const FVector V10(RingB.X, -WidthB, Deck.LocalZCm);
		const FVector V11(RingB.X, WidthB, Deck.LocalZCm);
		const FVector Up = FVector::UpVector;
		const FVector Down = -FVector::UpVector;
		AddQuad(
			Section,
			V00, V01, V10, V11,
			Up, Up, Up, Up,
			FVector2D(RingA.Alpha, 0.f),
			FVector2D(RingA.Alpha, 1.f),
			FVector2D(RingB.Alpha, 0.f),
			FVector2D(RingB.Alpha, 1.f),
			Up);
		AddQuad(
			Section,
			V00, V10, V01, V11,
			Down, Down, Down, Down,
			FVector2D(RingA.Alpha, 0.f),
			FVector2D(RingB.Alpha, 0.f),
			FVector2D(RingA.Alpha, 1.f),
			FVector2D(RingB.Alpha, 1.f),
			Down);
	}

	return Section;
}

float SampleHalfWidthAtX(
	const USubmarineAuthoringAsset* AuthoringAsset,
	const TArray<FRingSample>& Rings,
	const float X,
	const float LocalZ)
{
	if (Rings.Num() == 0)
	{
		return 0.f;
	}

	for (int32 RingIndex = 0; RingIndex < Rings.Num() - 1; ++RingIndex)
	{
		if (X >= Rings[RingIndex].X && X <= Rings[RingIndex + 1].X)
		{
			const float Alpha = FMath::GetMappedRangeValueClamped(
				FVector2D(Rings[RingIndex].X, Rings[RingIndex + 1].X),
				FVector2D(0.f, 1.f),
				X);
			const float InnerRadius = FMath::Lerp(Rings[RingIndex].InnerRadius, Rings[RingIndex + 1].InnerRadius, Alpha);
			const float Ratio = FMath::Lerp(Rings[RingIndex].WidthToHeightRatio, Rings[RingIndex + 1].WidthToHeightRatio, Alpha);
			return EvaluateSectionHalfWidth(AuthoringAsset->Hull, InnerRadius, Ratio, LocalZ);
		}
	}

	return EvaluateSectionHalfWidth(AuthoringAsset->Hull, Rings.Last().InnerRadius, Rings.Last().WidthToHeightRatio, LocalZ);
}

void AddDoubleSidedPlanarQuad(
	FCompiledSubmarineMeshSection& Section,
	const FVector& LeftBottom,
	const FVector& LeftTop,
	const FVector& RightBottom,
	const FVector& RightTop);

FCompiledSubmarineMeshSection BuildVerticalConnectorSection(
	const USubmarineAuthoringAsset* AuthoringAsset,
	const FSubmarineVerticalConnectorAuthoring& Connector,
	const TArray<FResolvedVerticalOpening>& VerticalOpenings,
	const TArray<FSubmarineDeckAuthoring>& Decks,
	const int32 ConnectorIndex,
	const int32 RampMaterialSlotIndex,
	const int32 HatchMaterialSlotIndex,
	const TArray<FRingSample>& Rings,
	FCompiledSubmarineVerticalConnectorData& OutCompiledConnector)
{
	FCompiledSubmarineMeshSection Section;
	const FName DefaultSectionId = Connector.Type == ESubmarineVerticalConnectorType::Ramp
		? FName(*FString::Printf(TEXT("Ramp_%d"), ConnectorIndex))
		: FName(*FString::Printf(TEXT("Hatch_%d"), ConnectorIndex));
	Section.SectionId = Connector.ConnectorId.IsNone() ? DefaultSectionId : Connector.ConnectorId;
	Section.MaterialSlotIndex = Connector.Type == ESubmarineVerticalConnectorType::Ramp
		? RampMaterialSlotIndex
		: HatchMaterialSlotIndex;

	OutCompiledConnector.ConnectorId = Section.SectionId;
	OutCompiledConnector.OpeningId = Connector.OpeningId;
	OutCompiledConnector.Type = Connector.Type;
	OutCompiledConnector.FromDeckIndex = Connector.FromDeckIndex;
	OutCompiledConnector.ToDeckIndex = Connector.ToDeckIndex;
	OutCompiledConnector.WidthCm = Connector.WidthCm;

	const FResolvedVerticalOpening* MatchedOpening = nullptr;
	if (!Connector.OpeningId.IsNone())
	{
		MatchedOpening = VerticalOpenings.FindByPredicate([&Connector](const FResolvedVerticalOpening& Candidate)
		{
			return Candidate.Data.OpeningId == Connector.OpeningId;
		});
		if (!MatchedOpening)
		{
			return Section;
		}

		OutCompiledConnector.FromDeckIndex = MatchedOpening->Data.FromDeckIndex;
		OutCompiledConnector.ToDeckIndex = MatchedOpening->Data.ToDeckIndex;
	}

	const int32 FromDeckIndex = MatchedOpening ? MatchedOpening->Data.FromDeckIndex : Connector.FromDeckIndex;
	const int32 ToDeckIndex = MatchedOpening ? MatchedOpening->Data.ToDeckIndex : Connector.ToDeckIndex;
	if (!Decks.IsValidIndex(FromDeckIndex)
		|| !Decks.IsValidIndex(ToDeckIndex))
	{
		return Section;
	}

	const float StartZ = Decks[FromDeckIndex].LocalZCm;
	const float EndZ = Decks[ToDeckIndex].LocalZCm;
	const float Rise = EndZ - StartZ;
	const float Run = FMath::Max(50.f, FMath::Abs(Rise) / FMath::Max(KINDA_SMALL_NUMBER, FMath::Tan(FMath::DegreesToRadians(Connector.SlopeDegrees))));
	const float ConnectorX = MatchedOpening ? MatchedOpening->Data.LocalX : Connector.LocalX;
	const float ConnectorWidthCm = MatchedOpening ? MatchedOpening->Data.WidthCm : Connector.WidthCm;
	const float StartX = FMath::Clamp(ConnectorX - (Run * 0.5f), 0.f, AuthoringAsset->Hull.LengthCm);
	const float EndX = FMath::Clamp(ConnectorX + (Run * 0.5f), 0.f, AuthoringAsset->Hull.LengthCm);
	const float MaxHalfWidth = FMath::Min(
		SampleHalfWidthAtX(AuthoringAsset, Rings, StartX, StartZ),
		SampleHalfWidthAtX(AuthoringAsset, Rings, EndX, EndZ));
	const float HalfWidth = FMath::Min(ConnectorWidthCm * 0.5f, MaxHalfWidth * 0.9f);
	if (HalfWidth <= KINDA_SMALL_NUMBER)
	{
		return Section;
	}

	if (Connector.Type == ESubmarineVerticalConnectorType::Ramp)
	{
		const FVector V00(StartX, -HalfWidth, StartZ);
		const FVector V01(StartX, HalfWidth, StartZ);
		const FVector V10(EndX, -HalfWidth, EndZ);
		const FVector V11(EndX, HalfWidth, EndZ);
		const FVector RampNormal = FVector::CrossProduct(V01 - V00, V10 - V00).GetSafeNormal();
		AddQuad(
			Section,
			V00, V01, V10, V11,
			RampNormal, RampNormal, RampNormal, RampNormal,
			FVector2D(0.f, 0.f),
			FVector2D(0.f, 1.f),
			FVector2D(1.f, 0.f),
			FVector2D(1.f, 1.f),
			RampNormal);

		OutCompiledConnector.StartLocal = FVector(StartX, 0.f, StartZ);
		OutCompiledConnector.EndLocal = FVector(EndX, 0.f, EndZ);
		return Section;
	}

	const float HatchX = FMath::Clamp(ConnectorX, 0.f, AuthoringAsset->Hull.LengthCm);
	const float LowerDeckZ = FMath::Min(StartZ, EndZ);
	const float UpperDeckZ = FMath::Max(StartZ, EndZ);
	AddDoubleSidedPlanarQuad(
		Section,
		FVector(HatchX, -HalfWidth, LowerDeckZ),
		FVector(HatchX, -HalfWidth, UpperDeckZ),
		FVector(HatchX, HalfWidth, LowerDeckZ),
		FVector(HatchX, HalfWidth, UpperDeckZ));

	OutCompiledConnector.StartLocal = FVector(HatchX, 0.f, LowerDeckZ);
	OutCompiledConnector.EndLocal = FVector(HatchX, 0.f, UpperDeckZ);
	return Section;
}

FRingSample InterpolateRingAtX(const TArray<FRingSample>& Rings, const float X)
{
	FRingSample Result;
	if (Rings.Num() == 0)
	{
		return Result;
	}

	if (X <= Rings[0].X)
	{
		return Rings[0];
	}

	if (X >= Rings.Last().X)
	{
		return Rings.Last();
	}

	for (int32 RingIndex = 0; RingIndex < Rings.Num() - 1; ++RingIndex)
	{
		const FRingSample& RingA = Rings[RingIndex];
		const FRingSample& RingB = Rings[RingIndex + 1];
		if (X < RingA.X || X > RingB.X)
		{
			continue;
		}

		const float Alpha = FMath::GetMappedRangeValueClamped(
			FVector2D(RingA.X, RingB.X),
			FVector2D(0.f, 1.f),
			X);

		Result.X = X;
		Result.Alpha = FMath::Lerp(RingA.Alpha, RingB.Alpha, Alpha);
		Result.OuterRadius = FMath::Lerp(RingA.OuterRadius, RingB.OuterRadius, Alpha);
		Result.InnerRadius = FMath::Lerp(RingA.InnerRadius, RingB.InnerRadius, Alpha);
		Result.WidthToHeightRatio = FMath::Lerp(RingA.WidthToHeightRatio, RingB.WidthToHeightRatio, Alpha);

		const int32 PointCount = RingA.InnerPositions.Num();
		Result.OuterPositions.Reserve(PointCount);
		Result.InnerPositions.Reserve(PointCount);
		Result.OuterNormals.Reserve(PointCount);
		Result.InnerNormals.Reserve(PointCount);

		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			Result.OuterPositions.Add(FMath::Lerp(RingA.OuterPositions[PointIndex], RingB.OuterPositions[PointIndex], Alpha));
			Result.InnerPositions.Add(FMath::Lerp(RingA.InnerPositions[PointIndex], RingB.InnerPositions[PointIndex], Alpha));
			Result.OuterNormals.Add(FMath::Lerp(RingA.OuterNormals[PointIndex], RingB.OuterNormals[PointIndex], Alpha).GetSafeNormal());
			Result.InnerNormals.Add(FMath::Lerp(RingA.InnerNormals[PointIndex], RingB.InnerNormals[PointIndex], Alpha).GetSafeNormal());
		}

		return Result;
	}

	return Rings.Last();
}

void AddTriangle(
	FCompiledSubmarineMeshSection& Section,
	const FVector& V0,
	const FVector& V1,
	const FVector& V2,
	const FVector& Normal,
	const FVector2D& UV0,
	const FVector2D& UV1,
	const FVector2D& UV2)
{
	const int32 BaseIndex = Section.Positions.Num();
	const FVector Tangent = (V1 - V0).GetSafeNormal();
	AppendVertex(Section, V0, Normal, Tangent, UV0);
	AppendVertex(Section, V1, Normal, Tangent, UV1);
	AppendVertex(Section, V2, Normal, Tangent, UV2);
	Section.Indices.Append({ BaseIndex + 0, BaseIndex + 1, BaseIndex + 2 });
}

void AddDoubleSidedPlanarQuad(
	FCompiledSubmarineMeshSection& Section,
	const FVector& LeftBottom,
	const FVector& LeftTop,
	const FVector& RightBottom,
	const FVector& RightTop);

FName MakeDoorBlockerSectionId(const FCompiledSubmarineBulkheadOpeningData& Opening)
{
	return Opening.OpeningId.IsNone()
		? NAME_None
		: FName(*FString::Printf(TEXT("%s_DoorBlocker"), *Opening.OpeningId.ToString()));
}

FName MakeDoorFrameSectionId(const FCompiledSubmarineBulkheadOpeningData& Opening)
{
	return Opening.OpeningId.IsNone()
		? NAME_None
		: FName(*FString::Printf(TEXT("%s_DoorFrame"), *Opening.OpeningId.ToString()));
}

FName MakeDoorLeafSectionId(const FCompiledSubmarineBulkheadOpeningData& Opening)
{
	return Opening.OpeningId.IsNone()
		? NAME_None
		: FName(*FString::Printf(TEXT("%s_DoorLeaf"), *Opening.OpeningId.ToString()));
}

void AddDoubleSidedPlanarQuad(
	FCompiledSubmarineMeshSection& Section,
	const FVector& LeftBottom,
	const FVector& LeftTop,
	const FVector& RightBottom,
	const FVector& RightTop)
{
	const FVector ForwardNormal = FVector::ForwardVector;
	const FVector BackwardNormal = -FVector::ForwardVector;
	const FVector2D UV00(LeftBottom.Y * 0.001f, LeftBottom.Z * 0.001f);
	const FVector2D UV01(LeftTop.Y * 0.001f, LeftTop.Z * 0.001f);
	const FVector2D UV10(RightBottom.Y * 0.001f, RightBottom.Z * 0.001f);
	const FVector2D UV11(RightTop.Y * 0.001f, RightTop.Z * 0.001f);

	AddQuad(
		Section,
		LeftBottom,
		LeftTop,
		RightBottom,
		RightTop,
		BackwardNormal,
		BackwardNormal,
		BackwardNormal,
		BackwardNormal,
		UV00,
		UV01,
		UV10,
		UV11,
		BackwardNormal);

	AddQuad(
		Section,
		LeftBottom,
		RightBottom,
		LeftTop,
		RightTop,
		ForwardNormal,
		ForwardNormal,
		ForwardNormal,
		ForwardNormal,
		UV00,
		UV10,
		UV01,
		UV11,
		ForwardNormal);
}

FDoorOpeningBuildData BuildDoorOpeningData(
	const USubmarineAuthoringAsset* AuthoringAsset,
	const float BulkheadLocalX,
	const FCompiledSubmarineBulkheadOpeningData& Opening,
	const TArray<FRingSample>& Rings,
	const FRingSample& Ring)
{
	FDoorOpeningBuildData Data;
	if (Ring.InnerPositions.Num() < 3)
	{
		return Data;
	}

	Data.OpeningId = Opening.OpeningId;
	Data.DeckIndex = Opening.DeckIndex;
	Data.bBlockedByDefault = Opening.bBlockedByDefault;

	Data.MinZ = Ring.InnerPositions[0].Z;
	Data.MaxZ = Ring.InnerPositions[0].Z;
	for (const FVector& Point : Ring.InnerPositions)
	{
		Data.MinZ = FMath::Min(Data.MinZ, Point.Z);
		Data.MaxZ = FMath::Max(Data.MaxZ, Point.Z);
	}

	Data.DoorHalfWidth = Opening.DoorSizeCm.X * 0.5f;
	Data.DoorBottomZ = Opening.DoorSillZCm;
	Data.DoorTopZ = Opening.DoorSillZCm + Opening.DoorSizeCm.Y;
	Data.BottomHalfWidth = SampleHalfWidthAtX(AuthoringAsset, Rings, BulkheadLocalX, Data.DoorBottomZ);
	Data.TopHalfWidth = SampleHalfWidthAtX(AuthoringAsset, Rings, BulkheadLocalX, Data.DoorTopZ);

	Data.bFits =
		Opening.DoorSizeCm.X > KINDA_SMALL_NUMBER &&
		Opening.DoorSizeCm.Y > KINDA_SMALL_NUMBER &&
		Data.DoorBottomZ > Data.MinZ &&
		Data.DoorTopZ < Data.MaxZ &&
		Data.DoorHalfWidth < Data.BottomHalfWidth &&
		Data.DoorHalfWidth < Data.TopHalfWidth;

	return Data;
}

FCompiledSubmarineMeshSection BuildBulkheadSection(
	const USubmarineAuthoringAsset* AuthoringAsset,
	const FCompiledSubmarineBulkheadConnectionData& Connection,
	const int32 MaterialSlotIndex,
	const TArray<FRingSample>& Rings)
{
	FCompiledSubmarineMeshSection Section;
	Section.SectionId = Connection.BoundaryId.IsNone()
		? FName(*FString::Printf(TEXT("Bulkhead_%s_%s"), *Connection.CompartmentA.ToString(), *Connection.CompartmentB.ToString()))
		: Connection.BoundaryId;
	Section.MaterialSlotIndex = MaterialSlotIndex;

	const FRingSample Ring = InterpolateRingAtX(Rings, Connection.LocalX);
	if (Ring.InnerPositions.Num() < 3)
	{
		return Section;
	}

	if (Connection.Openings.Num() == 0)
	{
		const FVector Center(Connection.LocalX, 0.f, 0.f);
		const FVector ForwardNormal = FVector::ForwardVector;
		const FVector BackwardNormal = -FVector::ForwardVector;
		const int32 PointCount = Ring.InnerPositions.Num();

		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			const int32 NextIndex = (PointIndex + 1) % PointCount;
			const FVector& P0 = Ring.InnerPositions[PointIndex];
			const FVector& P1 = Ring.InnerPositions[NextIndex];
			const FVector2D UVCenter(0.5f, 0.5f);
			const FVector2D UV0(0.5f + (P0.Y * 0.001f), 0.5f + (P0.Z * 0.001f));
			const FVector2D UV1(0.5f + (P1.Y * 0.001f), 0.5f + (P1.Z * 0.001f));

			AddTriangle(Section, Center, P0, P1, BackwardNormal, UVCenter, UV0, UV1);
			AddTriangle(Section, Center, P1, P0, ForwardNormal, UVCenter, UV1, UV0);
		}

		return Section;
	}

	float MinZ = Ring.InnerPositions[0].Z;
	float MaxZ = Ring.InnerPositions[0].Z;
	for (const FVector& Point : Ring.InnerPositions)
	{
		MinZ = FMath::Min(MinZ, Point.Z);
		MaxZ = FMath::Max(MaxZ, Point.Z);
	}

	TArray<FDoorOpeningBuildData> ValidOpenings;
	for (const FCompiledSubmarineBulkheadOpeningData& Opening : Connection.Openings)
	{
		FDoorOpeningBuildData DoorData = BuildDoorOpeningData(AuthoringAsset, Connection.LocalX, Opening, Rings, Ring);
		if (DoorData.bFits)
		{
			ValidOpenings.Add(DoorData);
		}
	}

	if (ValidOpenings.Num() == 0)
	{
		FCompiledSubmarineBulkheadConnectionData ClosedConnection = Connection;
		ClosedConnection.Openings.Reset();
		return BuildBulkheadSection(AuthoringAsset, ClosedConnection, MaterialSlotIndex, Rings);
	}

	ValidOpenings.Sort([](const FDoorOpeningBuildData& A, const FDoorOpeningBuildData& B)
	{
		return A.DoorBottomZ < B.DoorBottomZ;
	});

	float PreviousZ = MinZ;
	for (int32 OpeningIndex = 0; OpeningIndex < ValidOpenings.Num(); ++OpeningIndex)
	{
		const FDoorOpeningBuildData& DoorData = ValidOpenings[OpeningIndex];
		if (DoorData.DoorBottomZ > PreviousZ + KINDA_SMALL_NUMBER)
		{
			const float SegmentBottomZ = FMath::Lerp(PreviousZ, DoorData.DoorBottomZ, 0.05f);
			const float SegmentTopZ = FMath::Lerp(DoorData.DoorBottomZ, PreviousZ, 0.05f);
			const float BottomWidth = SampleHalfWidthAtX(AuthoringAsset, Rings, Connection.LocalX, SegmentBottomZ);
			const float TopWidth = SampleHalfWidthAtX(AuthoringAsset, Rings, Connection.LocalX, SegmentTopZ);
			AddDoubleSidedPlanarQuad(
				Section,
				FVector(Connection.LocalX, -BottomWidth, SegmentBottomZ),
				FVector(Connection.LocalX, -TopWidth, SegmentTopZ),
				FVector(Connection.LocalX, BottomWidth, SegmentBottomZ),
				FVector(Connection.LocalX, TopWidth, SegmentTopZ));
		}

		const float LeftBottomWidth = SampleHalfWidthAtX(AuthoringAsset, Rings, Connection.LocalX, DoorData.DoorBottomZ);
		const float LeftTopWidth = SampleHalfWidthAtX(AuthoringAsset, Rings, Connection.LocalX, DoorData.DoorTopZ);
		AddDoubleSidedPlanarQuad(
			Section,
			FVector(Connection.LocalX, -LeftBottomWidth, DoorData.DoorBottomZ),
			FVector(Connection.LocalX, -LeftTopWidth, DoorData.DoorTopZ),
			FVector(Connection.LocalX, -DoorData.DoorHalfWidth, DoorData.DoorBottomZ),
			FVector(Connection.LocalX, -DoorData.DoorHalfWidth, DoorData.DoorTopZ));
		AddDoubleSidedPlanarQuad(
			Section,
			FVector(Connection.LocalX, DoorData.DoorHalfWidth, DoorData.DoorBottomZ),
			FVector(Connection.LocalX, DoorData.DoorHalfWidth, DoorData.DoorTopZ),
			FVector(Connection.LocalX, LeftBottomWidth, DoorData.DoorBottomZ),
			FVector(Connection.LocalX, LeftTopWidth, DoorData.DoorTopZ));

		PreviousZ = DoorData.DoorTopZ;
	}

	if (PreviousZ < MaxZ - KINDA_SMALL_NUMBER)
	{
		const float SegmentBottomZ = FMath::Lerp(PreviousZ, MaxZ, 0.05f);
		const float SegmentTopZ = FMath::Lerp(MaxZ, PreviousZ, 0.05f);
		const float BottomWidth = SampleHalfWidthAtX(AuthoringAsset, Rings, Connection.LocalX, SegmentBottomZ);
		const float TopWidth = SampleHalfWidthAtX(AuthoringAsset, Rings, Connection.LocalX, SegmentTopZ);
		AddDoubleSidedPlanarQuad(
			Section,
			FVector(Connection.LocalX, -BottomWidth, SegmentBottomZ),
			FVector(Connection.LocalX, -TopWidth, SegmentTopZ),
			FVector(Connection.LocalX, BottomWidth, SegmentBottomZ),
			FVector(Connection.LocalX, TopWidth, SegmentTopZ));
	}

	return Section;
}

FCompiledSubmarineMeshSection BuildBulkheadSectionSafe(
	const USubmarineAuthoringAsset* AuthoringAsset,
	const FCompiledSubmarineBulkheadConnectionData& Connection,
	const int32 MaterialSlotIndex,
	const TArray<FRingSample>& Rings)
{
	FCompiledSubmarineMeshSection Section = BuildBulkheadSection(AuthoringAsset, Connection, MaterialSlotIndex, Rings);
	if (Section.Positions.Num() == 0 && Connection.Openings.Num() > 0)
	{
		FCompiledSubmarineBulkheadConnectionData ClosedConnection = Connection;
		ClosedConnection.Openings.Reset();
		Section = BuildBulkheadSection(AuthoringAsset, ClosedConnection, MaterialSlotIndex, Rings);
	}

	return Section;
}

FCompiledSubmarineMeshSection BuildDoorFrameSection(
	const USubmarineAuthoringAsset* AuthoringAsset,
	const float BulkheadLocalX,
	const FCompiledSubmarineBulkheadOpeningData& Opening,
	const int32 MaterialSlotIndex,
	const TArray<FRingSample>& Rings)
{
	FCompiledSubmarineMeshSection Section;
	if (Opening.DoorFrameSectionId.IsNone())
	{
		return Section;
	}

	const FRingSample Ring = InterpolateRingAtX(Rings, BulkheadLocalX);
	const FDoorOpeningBuildData DoorData = BuildDoorOpeningData(AuthoringAsset, BulkheadLocalX, Opening, Rings, Ring);
	if (!DoorData.bFits)
	{
		return Section;
	}

	Section.SectionId = Opening.DoorFrameSectionId;
	Section.MaterialSlotIndex = MaterialSlotIndex;

	const float FrameBorderCm = FMath::Clamp(FMath::Min(Opening.DoorSizeCm.X, Opening.DoorSizeCm.Y) * 0.12f, 6.f, 16.f);
	const float FrameOffsetCm = 0.75f;
	const float BottomOuterZ = FMath::Max(DoorData.MinZ, DoorData.DoorBottomZ - FrameBorderCm);
	const float TopOuterZ = FMath::Min(DoorData.MaxZ, DoorData.DoorTopZ + FrameBorderCm);
	const float BottomOuterWidth = SampleHalfWidthAtX(AuthoringAsset, Rings, BulkheadLocalX, BottomOuterZ);
	const float TopOuterWidth = SampleHalfWidthAtX(AuthoringAsset, Rings, BulkheadLocalX, TopOuterZ);
	const float BottomInnerWidth = DoorData.DoorHalfWidth;
	const float TopInnerWidth = DoorData.DoorHalfWidth;

	const TArray<float> XOffsets = { BulkheadLocalX - FrameOffsetCm, BulkheadLocalX + FrameOffsetCm };
	for (const float FrameX : XOffsets)
	{
		AddDoubleSidedPlanarQuad(
			Section,
			FVector(FrameX, -BottomOuterWidth, BottomOuterZ),
			FVector(FrameX, -BottomInnerWidth, DoorData.DoorBottomZ),
			FVector(FrameX, BottomOuterWidth, BottomOuterZ),
			FVector(FrameX, BottomInnerWidth, DoorData.DoorBottomZ));

		AddDoubleSidedPlanarQuad(
			Section,
			FVector(FrameX, -TopInnerWidth, DoorData.DoorTopZ),
			FVector(FrameX, -TopOuterWidth, TopOuterZ),
			FVector(FrameX, TopInnerWidth, DoorData.DoorTopZ),
			FVector(FrameX, TopOuterWidth, TopOuterZ));

		AddDoubleSidedPlanarQuad(
			Section,
			FVector(FrameX, -BottomOuterWidth, DoorData.DoorBottomZ),
			FVector(FrameX, -TopOuterWidth, DoorData.DoorTopZ),
			FVector(FrameX, -BottomInnerWidth, DoorData.DoorBottomZ),
			FVector(FrameX, -TopInnerWidth, DoorData.DoorTopZ));

		AddDoubleSidedPlanarQuad(
			Section,
			FVector(FrameX, BottomInnerWidth, DoorData.DoorBottomZ),
			FVector(FrameX, TopInnerWidth, DoorData.DoorTopZ),
			FVector(FrameX, BottomOuterWidth, DoorData.DoorBottomZ),
			FVector(FrameX, TopOuterWidth, DoorData.DoorTopZ));
	}

	return Section;
}

FCompiledSubmarineMeshSection BuildDoorwayBlockerSection(
	const USubmarineAuthoringAsset* AuthoringAsset,
	const float BulkheadLocalX,
	const FCompiledSubmarineBulkheadOpeningData& Opening,
	const TArray<FRingSample>& Rings)
{
	FCompiledSubmarineMeshSection Section;
	if (Opening.BlockerSectionId.IsNone())
	{
		return Section;
	}

	const FRingSample Ring = InterpolateRingAtX(Rings, BulkheadLocalX);
	const FDoorOpeningBuildData DoorData = BuildDoorOpeningData(AuthoringAsset, BulkheadLocalX, Opening, Rings, Ring);
	if (!DoorData.bFits)
	{
		return Section;
	}

	Section.SectionId = Opening.BlockerSectionId;
	Section.MaterialSlotIndex = INDEX_NONE;
	AddDoubleSidedPlanarQuad(
		Section,
		FVector(BulkheadLocalX, -DoorData.DoorHalfWidth, DoorData.DoorBottomZ),
		FVector(BulkheadLocalX, -DoorData.DoorHalfWidth, DoorData.DoorTopZ),
		FVector(BulkheadLocalX, DoorData.DoorHalfWidth, DoorData.DoorBottomZ),
		FVector(BulkheadLocalX, DoorData.DoorHalfWidth, DoorData.DoorTopZ));
	return Section;
}

FCompiledSubmarineMeshSection BuildDoorLeafSection(
	const USubmarineAuthoringAsset* AuthoringAsset,
	const float BulkheadLocalX,
	const FCompiledSubmarineBulkheadOpeningData& Opening,
	const int32 MaterialSlotIndex,
	const TArray<FRingSample>& Rings)
{
	FCompiledSubmarineMeshSection Section;
	if (Opening.DoorLeafSectionId.IsNone())
	{
		return Section;
	}

	const FRingSample Ring = InterpolateRingAtX(Rings, BulkheadLocalX);
	const FDoorOpeningBuildData DoorData = BuildDoorOpeningData(AuthoringAsset, BulkheadLocalX, Opening, Rings, Ring);
	if (!DoorData.bFits)
	{
		return Section;
	}

	Section.SectionId = Opening.DoorLeafSectionId;
	Section.MaterialSlotIndex = MaterialSlotIndex;

	const float LeafInsetCm = 4.f;
	const float LeafDepthOffsetCm = 1.5f;
	AddDoubleSidedPlanarQuad(
		Section,
		FVector(BulkheadLocalX + LeafDepthOffsetCm, -(DoorData.DoorHalfWidth - LeafInsetCm), DoorData.DoorBottomZ + LeafInsetCm),
		FVector(BulkheadLocalX + LeafDepthOffsetCm, -(DoorData.DoorHalfWidth - LeafInsetCm), DoorData.DoorTopZ - LeafInsetCm),
		FVector(BulkheadLocalX + LeafDepthOffsetCm, DoorData.DoorHalfWidth - LeafInsetCm, DoorData.DoorBottomZ + LeafInsetCm),
		FVector(BulkheadLocalX + LeafDepthOffsetCm, DoorData.DoorHalfWidth - LeafInsetCm, DoorData.DoorTopZ - LeafInsetCm));

	return Section;
}

FCompiledSubmarineBulkheadOpeningData ResolveBulkheadOpening(
	const FSubmarineBulkheadConnectionAuthoring& Connection,
	const FSubmarineBulkheadOpeningAuthoring& Opening,
	const TArray<FSubmarineDeckAuthoring>& Decks)
{
	FCompiledSubmarineBulkheadOpeningData Data;
	const FName EffectiveOpeningId = Opening.OpeningId.IsNone()
		? FName(*FString::Printf(TEXT("%s_Deck_%d"), *Connection.BoundaryId.ToString(), Opening.DeckIndex))
		: Opening.OpeningId;
	Data.OpeningId = EffectiveOpeningId;
	Data.DeckIndex = Opening.DeckIndex;
	Data.DoorSizeCm = Opening.DoorSizeCm;
	Data.bBlockedByDefault = Opening.bBlockedByDefault;
	Data.DoorSillZCm = Decks.IsValidIndex(Opening.DeckIndex) ? Decks[Opening.DeckIndex].LocalZCm : 0.f;
	Data.BlockerSectionId = MakeDoorBlockerSectionId(Data);
	Data.DoorFrameSectionId = MakeDoorFrameSectionId(Data);
	Data.DoorLeafSectionId = MakeDoorLeafSectionId(Data);
	return Data;
}

FCompiledSubmarineBulkheadConnectionData ResolveBulkheadConnection(
	const TArray<FResolvedCompartment>& Compartments,
	const FSubmarineBulkheadConnectionAuthoring& Connection,
	const TArray<FSubmarineDeckAuthoring>& Decks)
{
	FCompiledSubmarineBulkheadConnectionData Data;
	Data.BoundaryId = Connection.BoundaryId;
	Data.CompartmentA = Connection.CompartmentA;
	Data.CompartmentB = Connection.CompartmentB;
	Data.BlockerSectionId = Connection.BoundaryId;
	for (const FSubmarineBulkheadOpeningAuthoring& Opening : Connection.Openings)
	{
		Data.Openings.Add(ResolveBulkheadOpening(Connection, Opening, Decks));
	}

	const FResolvedCompartment* A = Compartments.FindByPredicate([&Connection](const FResolvedCompartment& Entry)
	{
		return Entry.Data.CompartmentId == Connection.CompartmentA;
	});
	const FResolvedCompartment* B = Compartments.FindByPredicate([&Connection](const FResolvedCompartment& Entry)
	{
		return Entry.Data.CompartmentId == Connection.CompartmentB;
	});

	if (A && B)
	{
		const FResolvedCompartment* Fore = A->Data.StartXcm <= B->Data.StartXcm ? A : B;
		Data.LocalX = Fore->Data.EndXcm;
	}

	return Data;
}

void BuildMaterialSlots(const USubmarineAuthoringAsset* AuthoringAsset, UCompiledSubmarineAsset* TargetAsset, TMap<FName, int32>& OutSlotMap)
{
	TargetAsset->MaterialSlots.Reset();
	OutSlotMap.Reset();

	auto AddSlot = [&TargetAsset, &OutSlotMap](const FName SlotName, const TSoftObjectPtr<UMaterialInterface>& Material)
	{
		FCompiledSubmarineMaterialSlot Slot;
		Slot.SlotName = SlotName;
		Slot.Material = Material;
		const int32 Index = TargetAsset->MaterialSlots.Add(Slot);
		OutSlotMap.Add(SlotName, Index);
	};

	AddSlot(TEXT("ExteriorHull"), AuthoringAsset->Materials.ExteriorHull);
	AddSlot(TEXT("InteriorHull"), AuthoringAsset->Materials.InteriorHull);
	AddSlot(TEXT("Deck"), AuthoringAsset->Materials.Deck);
	AddSlot(TEXT("Bulkhead"), AuthoringAsset->Materials.Bulkhead);
	AddSlot(TEXT("DoorFrame"), AuthoringAsset->Materials.DoorFrame);
	AddSlot(TEXT("Ramp"), AuthoringAsset->Materials.Ramp);
	AddSlot(TEXT("BreachRim"), AuthoringAsset->Materials.BreachRim);
}
}

bool USubmarineAuthoringBakeLibrary::BakeToCompiledAsset(
	const USubmarineAuthoringAsset* AuthoringAsset,
	UCompiledSubmarineAsset* TargetAsset,
	TArray<FLayoutValidationMessage>& OutMessages)
{
	if (!ValidateAuthoringAsset(AuthoringAsset, OutMessages))
	{
		return false;
	}

	if (!TargetAsset)
	{
		AddError(OutMessages, NAME_None, TEXT("Compiled asset target is null."));
		return false;
	}

	TargetAsset->RenderSections.Reset();
	TargetAsset->Collision = FCompiledSubmarineCollisionData();
	TargetAsset->Compartments.Reset();
	TargetAsset->BulkheadConnections.Reset();
	TargetAsset->VerticalOpenings.Reset();
	TargetAsset->VerticalConnectors.Reset();
	TargetAsset->StructuralBindings.Reset();

	TMap<FName, int32> MaterialSlotMap;
	BuildMaterialSlots(AuthoringAsset, TargetAsset, MaterialSlotMap);

	const TArray<FResolvedCompartment> Compartments = ResolveCompartments(AuthoringAsset);
	for (const FResolvedCompartment& Compartment : Compartments)
	{
		TargetAsset->Compartments.Add(Compartment.Data);
	}
	const TArray<FResolvedVerticalOpening> VerticalOpenings = ResolveVerticalOpenings(AuthoringAsset);
	for (const FResolvedVerticalOpening& Opening : VerticalOpenings)
	{
		TargetAsset->VerticalOpenings.Add(Opening.Data);
	}

	const TArray<FRingSample> Rings = BuildRings(AuthoringAsset);
	const TArray<int32> SegmentSides = BuildSegmentSideLut(Rings);
	TArray<FBindingBuildInfo> BindingInfos = BuildBindingInfos(AuthoringAsset, Compartments);

	FCompiledSubmarineMeshSection ExteriorSection;
	ExteriorSection.SectionId = TEXT("ExteriorHull");
	ExteriorSection.MaterialSlotIndex = MaterialSlotMap.FindRef(TEXT("ExteriorHull"));
	BuildShellSection(Rings, SegmentSides, true, ExteriorSection, BindingInfos);
	if (ExteriorSection.Positions.Num() > 0)
	{
		TargetAsset->RenderSections.Add(ExteriorSection);
	}

	FCompiledSubmarineMeshSection InteriorSection;
	InteriorSection.SectionId = TEXT("InteriorHull");
	InteriorSection.MaterialSlotIndex = MaterialSlotMap.FindRef(TEXT("InteriorHull"));
	BuildShellSection(Rings, SegmentSides, false, InteriorSection, BindingInfos);
	if (InteriorSection.Positions.Num() > 0)
	{
		TargetAsset->RenderSections.Add(InteriorSection);
	}

	const FCompiledSubmarineMeshSection ForeClosureSection = BuildHullClosureSection(
		Rings,
		true,
		MaterialSlotMap.FindRef(TEXT("ExteriorHull")));
	if (ForeClosureSection.Positions.Num() > 0)
	{
		TargetAsset->RenderSections.Add(ForeClosureSection);
	}

	const FCompiledSubmarineMeshSection AftClosureSection = BuildHullClosureSection(
		Rings,
		false,
		MaterialSlotMap.FindRef(TEXT("ExteriorHull")));
	if (AftClosureSection.Positions.Num() > 0)
	{
		TargetAsset->RenderSections.Add(AftClosureSection);
	}

	for (int32 DeckIndex = 0; DeckIndex < AuthoringAsset->Decks.Num(); ++DeckIndex)
	{
		const FCompiledSubmarineMeshSection DeckSection = BuildDeckSection(
			AuthoringAsset,
			AuthoringAsset->Decks[DeckIndex],
			DeckIndex,
			MaterialSlotMap.FindRef(TEXT("Deck")),
			VerticalOpenings,
			Rings);
		if (DeckSection.Positions.Num() > 0)
		{
			TargetAsset->RenderSections.Add(DeckSection);
			if (AuthoringAsset->BakeSettings.bBakeCollision && AuthoringAsset->Decks[DeckIndex].bWalkable)
			{
				TargetAsset->Collision.WalkableDeckSections.Add(DeckSection);
			}
		}
	}

	for (int32 ConnectorIndex = 0; ConnectorIndex < AuthoringAsset->VerticalConnectors.Num(); ++ConnectorIndex)
	{
		FCompiledSubmarineVerticalConnectorData CompiledConnector;
		const FCompiledSubmarineMeshSection ConnectorSection = BuildVerticalConnectorSection(
			AuthoringAsset,
			AuthoringAsset->VerticalConnectors[ConnectorIndex],
			VerticalOpenings,
			AuthoringAsset->Decks,
			ConnectorIndex,
			MaterialSlotMap.FindRef(TEXT("Ramp")),
			MaterialSlotMap.FindRef(TEXT("DoorFrame")),
			Rings,
			CompiledConnector);
		TargetAsset->VerticalConnectors.Add(CompiledConnector);
		if (ConnectorSection.Positions.Num() > 0)
		{
			TargetAsset->RenderSections.Add(ConnectorSection);
			if (AuthoringAsset->BakeSettings.bBakeCollision
				&& AuthoringAsset->VerticalConnectors[ConnectorIndex].Type == ESubmarineVerticalConnectorType::Ramp)
			{
				TargetAsset->Collision.RampSections.Add(ConnectorSection);
			}
		}
	}

	for (const FSubmarineBulkheadConnectionAuthoring& Connection : AuthoringAsset->BulkheadConnections)
	{
		const FCompiledSubmarineBulkheadConnectionData CompiledConnection = ResolveBulkheadConnection(Compartments, Connection, AuthoringAsset->Decks);
		TargetAsset->BulkheadConnections.Add(CompiledConnection);

		const FCompiledSubmarineMeshSection BulkheadSection = BuildBulkheadSectionSafe(
			AuthoringAsset,
			CompiledConnection,
			MaterialSlotMap.FindRef(TEXT("Bulkhead")),
			Rings);
		if (BulkheadSection.Positions.Num() > 0)
		{
			TargetAsset->RenderSections.Add(BulkheadSection);
			if (AuthoringAsset->BakeSettings.bBakeCollision)
			{
				TargetAsset->Collision.BulkheadBlockerSections.Add(BulkheadSection);
			}
		}

		for (const FCompiledSubmarineBulkheadOpeningData& Opening : CompiledConnection.Openings)
		{
			const FCompiledSubmarineMeshSection DoorFrameSection = BuildDoorFrameSection(
				AuthoringAsset,
				CompiledConnection.LocalX,
				Opening,
				MaterialSlotMap.FindRef(TEXT("DoorFrame")),
				Rings);
			if (DoorFrameSection.Positions.Num() > 0)
			{
				TargetAsset->RenderSections.Add(DoorFrameSection);
			}

			const FCompiledSubmarineMeshSection DoorLeafSection = BuildDoorLeafSection(
				AuthoringAsset,
				CompiledConnection.LocalX,
				Opening,
				MaterialSlotMap.FindRef(TEXT("DoorFrame")),
				Rings);
			if (DoorLeafSection.Positions.Num() > 0)
			{
				TargetAsset->RenderSections.Add(DoorLeafSection);
			}

			if (AuthoringAsset->BakeSettings.bBakeCollision)
			{
				const FCompiledSubmarineMeshSection DoorBlockerSection = BuildDoorwayBlockerSection(
					AuthoringAsset,
					CompiledConnection.LocalX,
					Opening,
					Rings);
				if (DoorBlockerSection.Positions.Num() > 0)
				{
					TargetAsset->Collision.BulkheadBlockerSections.Add(DoorBlockerSection);
				}
			}
		}
	}

	for (const FBindingBuildInfo& BindingInfo : BindingInfos)
	{
		if (BindingInfo.bHasGeometry)
		{
			TargetAsset->StructuralBindings.Add(BindingInfo.Binding);
		}
	}

	if (AuthoringAsset->BakeSettings.bBakeCollision)
	{
		TargetAsset->Collision.ExteriorProxy = ExteriorSection;
	}

	TargetAsset->MarkPackageDirty();
	return true;
}
