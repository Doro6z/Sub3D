#include "SubmarineMeshBuilder.h"

#include "Algo/Reverse.h"
#include "SubmarineDefinition.h"
#include "SubmarineDefinitionTypes.h"
#include "SubmarineGeneratorEnvelopeDef.h"
#include "SubCompiler/SubmarineGeometryBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubMeshBuilder, Log, All);

// --- Shared geometry helpers (same math as SubmarineGeometryBuilder) ----------

namespace
{

struct FProfilePoint
{
	FVector Position;
	FVector InwardNormal;
	float U = 0.f;
};

struct FExteriorRing
{
	float X = 0.f;
	float Radius = 0.f;
};

float SuperellipsePow(float Base, float Exp)
{
	if (FMath::Abs(Base) < KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Sign(Base) * FMath::Pow(FMath::Abs(Base), Exp);
}

void AppendTriangle(
	TArray<int32>& Triangles,
	int32 A,
	int32 B,
	int32 C)
{
	Triangles.Add(A);
	Triangles.Add(B);
	Triangles.Add(C);
}

void AppendQuad(
	FSubmarineMeshSectionData& Section,
	const FVector& A,
	const FVector& B,
	const FVector& C,
	const FVector& D,
	const FVector& Normal,
	const FVector& TangentX = FVector(1.f, 0.f, 0.f))
{
	const int32 BaseIndex = Section.Vertices.Num();

	Section.Vertices.Add(A);
	Section.Vertices.Add(B);
	Section.Vertices.Add(C);
	Section.Vertices.Add(D);

	Section.Normals.Add(Normal);
	Section.Normals.Add(Normal);
	Section.Normals.Add(Normal);
	Section.Normals.Add(Normal);

	Section.UVs.Add(FVector2D(0.f, 0.f));
	Section.UVs.Add(FVector2D(1.f, 0.f));
	Section.UVs.Add(FVector2D(0.f, 1.f));
	Section.UVs.Add(FVector2D(1.f, 1.f));

	const FProcMeshTangent Tan(TangentX, false);
	Section.Tangents.Add(Tan);
	Section.Tangents.Add(Tan);
	Section.Tangents.Add(Tan);
	Section.Tangents.Add(Tan);

	AppendTriangle(Section.Triangles, BaseIndex, BaseIndex + 2, BaseIndex + 1);
	AppendTriangle(Section.Triangles, BaseIndex + 1, BaseIndex + 2, BaseIndex + 3);
}

} // anonymous namespace

// --- BuildMeshData -----------------------------------------------------------

bool USubmarineMeshBuilder::BuildMeshData(
	USubmarineDefinition* Definition,
	const USubmarineGeneratorEnvelopeDef* Envelope,
	int32 RadialSegments,
	int32 InteriorArcSegments,
	int32 LongitudinalSubdivisionsPerSpan)
{
	if (!Definition || !Envelope)
	{
		UE_LOG(LogSubMeshBuilder, Error, TEXT("BuildMeshData: null inputs"));
		return false;
	}

	// Filter to non-airlock compartments for hull geometry. Airlock is a box appendage.
	int32 HullCompartmentCount = 0;
	for (const FGeneratedCompartmentDef& Comp : Definition->Compartments)
	{
		if (Comp.SemanticType != ESubCompartmentType::Airlock)
		{
			++HullCompartmentCount;
		}
	}

	if (HullCompartmentCount == 0)
	{
		UE_LOG(LogSubMeshBuilder, Error, TEXT("BuildMeshData: no hull compartments"));
		return false;
	}

	Definition->ExteriorHullMesh = FSubmarineMeshSectionData();
	Definition->InteriorMeshes.Reset();
	Definition->BulkheadMeshes.Reset();

	if (!BuildExteriorHull(Definition, Envelope, RadialSegments, LongitudinalSubdivisionsPerSpan))
	{
		UE_LOG(LogSubMeshBuilder, Error, TEXT("BuildMeshData: BuildExteriorHull failed"));
		return false;
	}

	UE_LOG(LogSubMeshBuilder, Log,
		TEXT("[ShapeStep1] Exterior hull emitted: V=%d T=%d"),
		Definition->ExteriorHullMesh.Vertices.Num(),
		Definition->ExteriorHullMesh.Triangles.Num() / 3);

	if (!BuildInteriorCompartments(Definition, Envelope, InteriorArcSegments))
	{
		UE_LOG(LogSubMeshBuilder, Error, TEXT("BuildMeshData: BuildInteriorCompartments failed"));
		return false;
	}

	// Per-compartment interior / airlock split, then totals.
	{
		int32 InteriorCompartmentCount = 0;
		int32 InteriorWallV = 0, InteriorWallT = 0;
		int32 InteriorFloorV = 0, InteriorFloorT = 0;
		int32 InteriorBowCapV = 0, InteriorBowCapT = 0;
		int32 InteriorSternCapV = 0, InteriorSternCapT = 0;

		int32 AirlockCompartmentCount = 0;
		int32 AirlockWallV = 0, AirlockWallT = 0;
		int32 AirlockFloorV = 0, AirlockFloorT = 0;

		for (const FSubmarineInteriorCompartmentMeshData& Interior : Definition->InteriorMeshes)
		{
			const FGeneratedCompartmentDef* Comp = Definition->FindCompartment(Interior.CompartmentId);
			const bool bIsAirlock = Comp && Comp->SemanticType == ESubCompartmentType::Airlock;

			if (bIsAirlock)
			{
				++AirlockCompartmentCount;
				AirlockWallV += Interior.WallSection.Vertices.Num();
				AirlockWallT += Interior.WallSection.Triangles.Num() / 3;
				AirlockFloorV += Interior.FloorSection.Vertices.Num();
				AirlockFloorT += Interior.FloorSection.Triangles.Num() / 3;
			}
			else
			{
				++InteriorCompartmentCount;
				InteriorWallV += Interior.WallSection.Vertices.Num();
				InteriorWallT += Interior.WallSection.Triangles.Num() / 3;
				InteriorFloorV += Interior.FloorSection.Vertices.Num();
				InteriorFloorT += Interior.FloorSection.Triangles.Num() / 3;
				InteriorBowCapV += Interior.BowCapSection.Vertices.Num();
				InteriorBowCapT += Interior.BowCapSection.Triangles.Num() / 3;
				InteriorSternCapV += Interior.SternCapSection.Vertices.Num();
				InteriorSternCapT += Interior.SternCapSection.Triangles.Num() / 3;
			}

			UE_LOG(LogSubMeshBuilder, Verbose,
				TEXT("[ShapeStep1] Compartment %s%s: Wall V=%d T=%d | Floor V=%d T=%d | BowCap V=%d T=%d | SternCap V=%d T=%d"),
				*Interior.CompartmentId.ToString(),
				bIsAirlock ? TEXT(" (AIRLOCK)") : TEXT(""),
				Interior.WallSection.Vertices.Num(),
				Interior.WallSection.Triangles.Num() / 3,
				Interior.FloorSection.Vertices.Num(),
				Interior.FloorSection.Triangles.Num() / 3,
				Interior.BowCapSection.Vertices.Num(),
				Interior.BowCapSection.Triangles.Num() / 3,
				Interior.SternCapSection.Vertices.Num(),
				Interior.SternCapSection.Triangles.Num() / 3);
		}

		UE_LOG(LogSubMeshBuilder, Log,
			TEXT("[ShapeStep1] Interior (non-airlock) compartments=%d | Walls V=%d T=%d | Floor V=%d T=%d | BowCap V=%d T=%d | SternCap V=%d T=%d"),
			InteriorCompartmentCount,
			InteriorWallV, InteriorWallT,
			InteriorFloorV, InteriorFloorT,
			InteriorBowCapV, InteriorBowCapT,
			InteriorSternCapV, InteriorSternCapT);

		UE_LOG(LogSubMeshBuilder, Log,
			TEXT("[ShapeStep1] Airlock compartments=%d | Walls V=%d T=%d | Floor V=%d T=%d"),
			AirlockCompartmentCount,
			AirlockWallV, AirlockWallT,
			AirlockFloorV, AirlockFloorT);
	}

	if (!BuildBulkheads(Definition, Envelope))
	{
		UE_LOG(LogSubMeshBuilder, Error, TEXT("BuildMeshData: BuildBulkheads failed"));
		return false;
	}

	{
		int32 BulkheadPanelV = 0, BulkheadPanelT = 0;
		for (const FSubmarineBulkheadMeshData& Bulkhead : Definition->BulkheadMeshes)
		{
			BulkheadPanelV += Bulkhead.PanelSection.Vertices.Num();
			BulkheadPanelT += Bulkhead.PanelSection.Triangles.Num() / 3;
		}
		UE_LOG(LogSubMeshBuilder, Log,
			TEXT("[ShapeStep1] Bulkheads=%d | Panel V=%d T=%d"),
			Definition->BulkheadMeshes.Num(),
			BulkheadPanelV, BulkheadPanelT);
	}

	UE_LOG(LogSubMeshBuilder, Log,
		TEXT("[ShapeStep1] BuildMeshData complete: exterior=%d verts | %d interior meshes | %d bulkheads"),
		Definition->ExteriorHullMesh.Vertices.Num(),
		Definition->InteriorMeshes.Num(),
		Definition->BulkheadMeshes.Num());

	return true;
}

// --- Exterior Hull -----------------------------------------------------------

bool USubmarineMeshBuilder::BuildExteriorHull(
	USubmarineDefinition* Definition,
	const USubmarineGeneratorEnvelopeDef* Envelope,
	int32 RadialSegments,
	int32 LongitudinalSubdivisionsPerSpan)
{
	FSubmarineMeshSectionData& Out = Definition->ExteriorHullMesh;
	Out = FSubmarineMeshSectionData();

	const float SpineLength = FMath::Max(100.f, Envelope->SpineLengthCm);
	const float SectionExponent = FMath::Max(0.5f, Envelope->SectionExponent);
	const float WidthToHeightRatio = FMath::Max(0.5f, Envelope->WidthToHeightRatio);
	const float HullOffset = FMath::Max(0.f, Envelope->ExteriorHullOffsetCm);

	// Gather knot rings from hull compartments (non-airlock).
	TArray<FExteriorRing> KnotRings;
	for (const FGeneratedCompartmentDef& Comp : Definition->Compartments)
	{
		if (Comp.SemanticType == ESubCompartmentType::Airlock)
		{
			continue;
		}

		if (KnotRings.Num() == 0)
		{
			FExteriorRing StartRing;
			StartRing.X = Comp.HydroBoundsMin.X;
			// Use untapered radius -- taper is applied once after subdivision.
			const float StartNorm = FMath::Clamp(Comp.HydroBoundsMin.X / SpineLength, 0.f, 1.f);
			StartRing.Radius = FMath::Max(1.f, Envelope->EvaluateRadius(StartNorm) + HullOffset);
			KnotRings.Add(StartRing);
		}

		const float EndNorm = FMath::Clamp(Comp.HydroBoundsMax.X / SpineLength, 0.f, 1.f);

		FExteriorRing EndRing;
		EndRing.X = Comp.HydroBoundsMax.X;
		EndRing.Radius = FMath::Max(1.f, Envelope->EvaluateRadius(EndNorm) + HullOffset);
		KnotRings.Add(EndRing);
	}

	if (KnotRings.Num() < 2)
	{
		return false;
	}

	// Subdivide between knots.
	const int32 EffRadial = FMath::Clamp(RadialSegments, 12, 64);
	const int32 EffLongSubdiv = FMath::Clamp(LongitudinalSubdivisionsPerSpan, 1, 16);

	TArray<FExteriorRing> Rings;
	Rings.Reserve(((KnotRings.Num() - 1) * EffLongSubdiv) + 1);

	for (int32 RingIndex = 0; RingIndex + 1 < KnotRings.Num(); ++RingIndex)
	{
		const FExteriorRing& Start = KnotRings[RingIndex];
		const FExteriorRing& End = KnotRings[RingIndex + 1];

		for (int32 Step = 0; Step < EffLongSubdiv; ++Step)
		{
			const float T = static_cast<float>(Step) / static_cast<float>(EffLongSubdiv);
			FExteriorRing Sample;
			Sample.X = FMath::Lerp(Start.X, End.X, T);
			Sample.Radius = FMath::InterpEaseInOut(Start.Radius, End.Radius, T, 2.f);
			Rings.Add(Sample);
		}
	}
	Rings.Add(KnotRings.Last());

	// Note: envelope taper is already baked into EvaluateRadius since the
	// M1.2 refactor. Do not re-apply EvaluateBowSternTaper here or the hull
	// would be squared-tapered and collapse toward the caps.

	// Allocate vertex arrays.
	Out.Vertices.Reserve(Rings.Num() * EffRadial + 20);
	Out.Normals.Reserve(Rings.Num() * EffRadial + 20);
	Out.UVs.Reserve(Rings.Num() * EffRadial + 20);
	Out.Tangents.Reserve(Rings.Num() * EffRadial + 20);
	Out.Triangles.Reserve((Rings.Num() - 1) * EffRadial * 6 + EffRadial * 12);

	// Emit ring vertices.
	const float SpineStart = Rings[0].X;
	for (int32 i = 0; i < Rings.Num(); ++i)
	{
		const float X = Rings[i].X;
		const float R = Rings[i].Radius;
		const float U = (X - SpineStart) / 100.f;

		for (int32 Seg = 0; Seg < EffRadial; ++Seg)
		{
			const float Theta = 2.f * PI * static_cast<float>(Seg) / static_cast<float>(EffRadial);
			const float CosA = FMath::Cos(Theta);
			const float SinA = FMath::Sin(Theta);
			const float Y = R * WidthToHeightRatio * SuperellipsePow(CosA, 2.f / SectionExponent);
			const float Z = R * SuperellipsePow(SinA, 2.f / SectionExponent);

			const float NY = SectionExponent * SuperellipsePow(CosA, SectionExponent - 1.f)
				/ FMath::Pow(FMath::Max(1.f, R * WidthToHeightRatio), SectionExponent);
			const float NZ = SectionExponent * SuperellipsePow(SinA, SectionExponent - 1.f)
				/ FMath::Pow(FMath::Max(1.f, R), SectionExponent);
			FVector Normal(0.f, NY, NZ);
			Normal.Normalize();

			Out.Vertices.Add(FVector(X, Y, Z));
			Out.Normals.Add(Normal);
			Out.UVs.Add(FVector2D(U, static_cast<float>(Seg) / static_cast<float>(EffRadial)));
			Out.Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
		}
	}

	// Index the cylinder body.
	for (int32 i = 0; i < Rings.Num() - 1; ++i)
	{
		const int32 RingBase = i * EffRadial;
		const int32 NextRingBase = (i + 1) * EffRadial;

		for (int32 Seg = 0; Seg < EffRadial; ++Seg)
		{
			const int32 NextSeg = (Seg + 1) % EffRadial;
			const int32 A0 = RingBase + Seg;
			const int32 A1 = RingBase + NextSeg;
			const int32 B0 = NextRingBase + Seg;
			const int32 B1 = NextRingBase + NextSeg;

			AppendTriangle(Out.Triangles, A0, A1, B0);
			AppendTriangle(Out.Triangles, B0, A1, B1);
		}
	}

	// Progressive bow cap.
	{
		const float BowX = Rings[0].X;
		const float BowRadius = Rings[0].Radius;
		constexpr int32 CapRings = 6;
		const float CapLength = FMath::Max(0.f, Envelope->BowCapLengthCm);

		TArray<int32> CapRingBases;
		CapRingBases.Add(0); // First ring of body.

		for (int32 Ring = 1; Ring <= CapRings; ++Ring)
		{
			// T is the fraction along the cap from body toward tip.
			// TBody is the complementary parameter expected by ApplyCapProfile:
			// TBody=1 at the body boundary (full radius), TBody=0 at the tip (zero radius).
			const float T = static_cast<float>(Ring) / static_cast<float>(CapRings + 1);
			const float TBody = 1.f - T;
			const float ShrinkMultiplier = USubmarineGeneratorEnvelopeDef::ApplyCapProfile(
				Envelope->BowProfile, TBody, Envelope->BowSharpness);
			const float ShrinkR = BowRadius * ShrinkMultiplier;
			const float CapX = BowX - T * CapLength;

			const int32 RingBase = Out.Vertices.Num();
			CapRingBases.Add(RingBase);

			for (int32 Seg = 0; Seg < EffRadial; ++Seg)
			{
				const float Theta = 2.f * PI * static_cast<float>(Seg) / static_cast<float>(EffRadial);
				const float CosA = FMath::Cos(Theta);
				const float SinA = FMath::Sin(Theta);
				const float Y = ShrinkR * WidthToHeightRatio * SuperellipsePow(CosA, 2.f / SectionExponent);
				const float Z = ShrinkR * SuperellipsePow(SinA, 2.f / SectionExponent);
				FVector Normal = FVector(-T, CosA * (1.f - T), SinA * (1.f - T));
				Normal.Normalize();
				Out.Vertices.Add(FVector(CapX, Y, Z));
				Out.Normals.Add(Normal);
				Out.UVs.Add(FVector2D(0.5f + CosA * 0.5f * (1.f - T), 0.5f + SinA * 0.5f * (1.f - T)));
				Out.Tangents.Add(FProcMeshTangent(0.f, -SinA, CosA));
			}
		}

		for (int32 Ring = 0; Ring < CapRingBases.Num() - 1; ++Ring)
		{
			const int32 BaseA = CapRingBases[Ring];
			const int32 BaseB = CapRingBases[Ring + 1];
			for (int32 Seg = 0; Seg < EffRadial; ++Seg)
			{
				const int32 NextSeg = (Seg + 1) % EffRadial;
				AppendTriangle(Out.Triangles, BaseA + Seg, BaseB + Seg, BaseA + NextSeg);
				AppendTriangle(Out.Triangles, BaseA + NextSeg, BaseB + Seg, BaseB + NextSeg);
			}
		}

		const int32 BowCenter = Out.Vertices.Num();
		Out.Vertices.Add(FVector(BowX - CapLength, 0.f, 0.f));
		Out.Normals.Add(-FVector::ForwardVector);
		Out.UVs.Add(FVector2D(0.5f, 0.5f));
		Out.Tangents.Add(FProcMeshTangent(0.f, 1.f, 0.f));

		const int32 LastCapBase = CapRingBases.Last();
		for (int32 Seg = 0; Seg < EffRadial; ++Seg)
		{
			const int32 NextSeg = (Seg + 1) % EffRadial;
			AppendTriangle(Out.Triangles, BowCenter, LastCapBase + NextSeg, LastCapBase + Seg);
		}
	}

	// Progressive stern cap.
	{
		const float SternX = Rings.Last().X;
		const float SternRadius = Rings.Last().Radius;
		constexpr int32 CapRings = 6;
		const float CapLength = FMath::Max(0.f, Envelope->SternCapLengthCm);
		const int32 SternRingBase = (Rings.Num() - 1) * EffRadial;

		TArray<int32> CapRingBases;
		CapRingBases.Add(SternRingBase);

		for (int32 Ring = 1; Ring <= CapRings; ++Ring)
		{
			// T fraction along the cap from body toward tip (same convention as bow).
			const float T = static_cast<float>(Ring) / static_cast<float>(CapRings + 1);
			const float TBody = 1.f - T;
			const float ShrinkMultiplier = USubmarineGeneratorEnvelopeDef::ApplyCapProfile(
				Envelope->SternProfile, TBody, Envelope->SternSharpness);
			const float ShrinkR = SternRadius * ShrinkMultiplier;
			const float CapX = SternX + T * CapLength;

			const int32 RingBase = Out.Vertices.Num();
			CapRingBases.Add(RingBase);

			for (int32 Seg = 0; Seg < EffRadial; ++Seg)
			{
				const float Theta = 2.f * PI * static_cast<float>(Seg) / static_cast<float>(EffRadial);
				const float CosA = FMath::Cos(Theta);
				const float SinA = FMath::Sin(Theta);
				const float Y = ShrinkR * WidthToHeightRatio * SuperellipsePow(CosA, 2.f / SectionExponent);
				const float Z = ShrinkR * SuperellipsePow(SinA, 2.f / SectionExponent);
				FVector Normal = FVector(T, CosA * (1.f - T), SinA * (1.f - T));
				Normal.Normalize();
				Out.Vertices.Add(FVector(CapX, Y, Z));
				Out.Normals.Add(Normal);
				Out.UVs.Add(FVector2D(0.5f + CosA * 0.5f * (1.f - T), 0.5f + SinA * 0.5f * (1.f - T)));
				Out.Tangents.Add(FProcMeshTangent(0.f, -SinA, CosA));
			}
		}

		for (int32 Ring = 0; Ring < CapRingBases.Num() - 1; ++Ring)
		{
			const int32 BaseA = CapRingBases[Ring];
			const int32 BaseB = CapRingBases[Ring + 1];
			for (int32 Seg = 0; Seg < EffRadial; ++Seg)
			{
				const int32 NextSeg = (Seg + 1) % EffRadial;
				AppendTriangle(Out.Triangles, BaseA + Seg, BaseA + NextSeg, BaseB + Seg);
				AppendTriangle(Out.Triangles, BaseB + Seg, BaseA + NextSeg, BaseB + NextSeg);
			}
		}

		const int32 SternCenter = Out.Vertices.Num();
		Out.Vertices.Add(FVector(SternX + CapLength, 0.f, 0.f));
		Out.Normals.Add(FVector::ForwardVector);
		Out.UVs.Add(FVector2D(0.5f, 0.5f));
		Out.Tangents.Add(FProcMeshTangent(0.f, 1.f, 0.f));

		const int32 LastCapBase = CapRingBases.Last();
		for (int32 Seg = 0; Seg < EffRadial; ++Seg)
		{
			const int32 NextSeg = (Seg + 1) % EffRadial;
			AppendTriangle(Out.Triangles, SternCenter, LastCapBase + Seg, LastCapBase + NextSeg);
		}
	}

	Out.VertexCount = Out.Vertices.Num();
	Out.TriangleCount = Out.Triangles.Num();

	return Out.Vertices.Num() > 0 && Out.Triangles.Num() > 0;
}

// --- Interior Compartments ---------------------------------------------------

bool USubmarineMeshBuilder::BuildInteriorCompartments(
	USubmarineDefinition* Definition,
	const USubmarineGeneratorEnvelopeDef* Envelope,
	int32 InteriorArcSegments)
{
	const float SpineLength = FMath::Max(100.f, Envelope->SpineLengthCm);
	const float SectionExponent = FMath::Max(0.5f, Envelope->SectionExponent);
	const float WidthToHeightRatio = FMath::Max(0.5f, Envelope->WidthToHeightRatio);
	const float WallThickness = Definition->WallThicknessCm;
	const float Exp = 2.f / SectionExponent;
	const int32 ArcSegs = FMath::Clamp(InteriorArcSegments, 8, 48);
	constexpr float BoundaryHalfThickness = 8.f;

	// Count non-airlock compartments for first/last detection.
	TArray<int32> HullCompIndices;
	for (int32 i = 0; i < Definition->Compartments.Num(); ++i)
	{
		if (Definition->Compartments[i].SemanticType != ESubCompartmentType::Airlock)
		{
			HullCompIndices.Add(i);
		}
	}

	Definition->InteriorMeshes.Reserve(Definition->Compartments.Num());

	for (int32 HullOrderIdx = 0; HullOrderIdx < HullCompIndices.Num(); ++HullOrderIdx)
	{
		const int32 CompIdx = HullCompIndices[HullOrderIdx];
		const FGeneratedCompartmentDef& Comp = Definition->Compartments[CompIdx];
		const bool bIsFirst = (HullOrderIdx == 0);
		const bool bIsLast = (HullOrderIdx == HullCompIndices.Num() - 1);

		// Derive geometry parameters from definition + envelope.
		// EvaluateRadius already bakes in the taper since M1.2.
		const float MidX = (Comp.HydroBoundsMin.X + Comp.HydroBoundsMax.X) * 0.5f;
		const float MidNorm = FMath::Clamp(MidX / SpineLength, 0.f, 1.f);
		const float EnvR = Envelope->EvaluateRadius(MidNorm);
		const float InsetRadius = FMath::Max(10.f, EnvR - WallThickness);
		const float HalfH = InsetRadius;
		const float HalfW = InsetRadius * WidthToHeightRatio;
		const float FloorZ = FMath::Clamp(Comp.WalkableFloorZCm, -HalfH * 0.98f, HalfH * 0.98f);
		const float WallStartX = Comp.HydroBoundsMin.X + BoundaryHalfThickness;
		const float WallEndX = FMath::Max(WallStartX + 1.f, Comp.HydroBoundsMax.X - BoundaryHalfThickness);

		// Floor half-width at FloorZ using superellipse equation.
		const float AbsFloorRatio = FMath::Abs(FloorZ) / HalfH;
		const float FloorHalfWidth = (AbsFloorRatio >= 1.f) ? 0.f
			: HalfW * FMath::Pow(FMath::Max(0.f, 1.f - FMath::Pow(AbsFloorRatio, SectionExponent)), 1.f / SectionExponent);

		if (FloorHalfWidth <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogSubMeshBuilder, Warning, TEXT("BuildInterior: compartment %s has zero floor width, skipping"),
				*Comp.CompartmentId.ToString());
			continue;
		}

		// Compute arc theta range (floor-to-floor via top of cross-section).
		const float FloorRatio = FMath::Clamp(FloorZ / HalfH, -1.f, 1.f);
		const float InvExp = 1.f / FMath::Max(KINDA_SMALL_NUMBER, Exp);
		const float MappedSin = FMath::Clamp(SuperellipsePow(FloorRatio, InvExp), -1.f, 1.f);
		const float EndTheta = FMath::Asin(MappedSin);
		const float StartTheta = PI - EndTheta;

		// Compute total arc length for UV normalization.
		float TotalArcLength = 0.f;
		{
			FVector2D PrevPos;
			{
				const float CosA = FMath::Cos(StartTheta);
				const float SinA = FMath::Sin(StartTheta);
				PrevPos.X = HalfW * SuperellipsePow(CosA, Exp);
				PrevPos.Y = HalfH * SuperellipsePow(SinA, Exp);
			}
			const int32 ArcSamples = ArcSegs * 2;
			for (int32 i = 1; i <= ArcSamples; ++i)
			{
				const float T = static_cast<float>(i) / static_cast<float>(ArcSamples);
				const float Theta = FMath::Lerp(StartTheta, EndTheta, T);
				const float CosA = FMath::Cos(Theta);
				const float SinA = FMath::Sin(Theta);
				FVector2D CurrPos(HalfW * SuperellipsePow(CosA, Exp), HalfH * SuperellipsePow(SinA, Exp));
				TotalArcLength += FVector2D::Distance(PrevPos, CurrPos);
				PrevPos = CurrPos;
			}
		}

		// Build profile points along the superellipse arc.
		TArray<FProfilePoint> ProfilePoints;
		ProfilePoints.Reserve(ArcSegs + 1);
		float TraversedArc = 0.f;
		FVector2D LastPos;
		{
			const float CosA = FMath::Cos(StartTheta);
			const float SinA = FMath::Sin(StartTheta);
			LastPos.X = HalfW * SuperellipsePow(CosA, Exp);
			LastPos.Y = HalfH * SuperellipsePow(SinA, Exp);
		}

		for (int32 Seg = 0; Seg <= ArcSegs; ++Seg)
		{
			const float T = static_cast<float>(Seg) / static_cast<float>(ArcSegs);
			const float Theta = FMath::Lerp(StartTheta, EndTheta, T);
			const float CosA = FMath::Cos(Theta);
			const float SinA = FMath::Sin(Theta);

			FVector2D Pos(HalfW * SuperellipsePow(CosA, Exp), HalfH * SuperellipsePow(SinA, Exp));

			if (Seg > 0)
			{
				TraversedArc += FVector2D::Distance(LastPos, Pos);
			}

			FProfilePoint Point;
			Point.Position = FVector(0.f, Pos.X, Pos.Y);
			Point.InwardNormal = FVector(0.f, -Pos.X, -Pos.Y).GetSafeNormal();
			Point.U = (TotalArcLength > KINDA_SMALL_NUMBER) ? (TraversedArc / TotalArcLength) : T;
			ProfilePoints.Add(Point);
			LastPos = Pos;
		}

		// Build mesh data for this compartment.
		FSubmarineInteriorCompartmentMeshData MeshData;
		MeshData.CompartmentId = Comp.CompartmentId;

		// -- Wall section: two rings (front + back) connected along the arc --
		const int32 PointsPerRing = ProfilePoints.Num();
		MeshData.WallSection.Vertices.Reserve(PointsPerRing * 2);
		MeshData.WallSection.Normals.Reserve(PointsPerRing * 2);
		MeshData.WallSection.UVs.Reserve(PointsPerRing * 2);
		MeshData.WallSection.Tangents.Reserve(PointsPerRing * 2);
		MeshData.WallSection.Triangles.Reserve((PointsPerRing - 1) * 6);

		for (int32 RingIdx = 0; RingIdx < 2; ++RingIdx)
		{
			const float X = (RingIdx == 0) ? WallStartX : WallEndX;
			const float V = static_cast<float>(RingIdx);

			for (const FProfilePoint& Pt : ProfilePoints)
			{
				MeshData.WallSection.Vertices.Add(FVector(X, Pt.Position.Y, Pt.Position.Z));
				MeshData.WallSection.Normals.Add(Pt.InwardNormal);
				MeshData.WallSection.UVs.Add(FVector2D(Pt.U, V));
				MeshData.WallSection.Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
			}
		}

		for (int32 Seg = 0; Seg < PointsPerRing - 1; ++Seg)
		{
			const int32 FC = Seg;
			const int32 FN = Seg + 1;
			const int32 BC = PointsPerRing + Seg;
			const int32 BN = PointsPerRing + Seg + 1;
			AppendTriangle(MeshData.WallSection.Triangles, FC, FN, BC);
			AppendTriangle(MeshData.WallSection.Triangles, BC, FN, BN);
		}

		MeshData.WallSection.VertexCount = MeshData.WallSection.Vertices.Num();
		MeshData.WallSection.TriangleCount = MeshData.WallSection.Triangles.Num();

		// -- Floor section: simple quad --
		MeshData.FloorSection.Vertices = {
			FVector(WallStartX, -FloorHalfWidth, FloorZ),
			FVector(WallStartX, FloorHalfWidth, FloorZ),
			FVector(WallEndX, -FloorHalfWidth, FloorZ),
			FVector(WallEndX, FloorHalfWidth, FloorZ)
		};
		MeshData.FloorSection.Normals = {
			FVector::UpVector, FVector::UpVector,
			FVector::UpVector, FVector::UpVector
		};
		MeshData.FloorSection.UVs = {
			FVector2D(0.f, 0.f), FVector2D(1.f, 0.f),
			FVector2D(0.f, 1.f), FVector2D(1.f, 1.f)
		};
		MeshData.FloorSection.Tangents = {
			FProcMeshTangent(1.f, 0.f, 0.f), FProcMeshTangent(1.f, 0.f, 0.f),
			FProcMeshTangent(1.f, 0.f, 0.f), FProcMeshTangent(1.f, 0.f, 0.f)
		};
		AppendTriangle(MeshData.FloorSection.Triangles, 0, 2, 1);
		AppendTriangle(MeshData.FloorSection.Triangles, 1, 2, 3);

		// -- Bow/Stern cap discs (fan triangulation from centroid) --
		FVector2D CapCentroid(0.f, 0.f);
		for (const FProfilePoint& Pt : ProfilePoints)
		{
			CapCentroid += FVector2D(Pt.Position.Y, Pt.Position.Z);
		}
		CapCentroid /= static_cast<float>(ProfilePoints.Num());

		auto EmitCapDisc = [&](FSubmarineMeshSectionData& Section, float XPos, const FVector& FaceNormal)
		{
			const int32 CI = Section.Vertices.Num();
			Section.Vertices.Add(FVector(XPos, CapCentroid.X, CapCentroid.Y));
			Section.Normals.Add(FaceNormal);
			Section.UVs.Add(FVector2D(0.5f, 0.5f));
			Section.Tangents.Add(FProcMeshTangent(0.f, 1.f, 0.f));

			for (const FProfilePoint& Pt : ProfilePoints)
			{
				Section.Vertices.Add(FVector(XPos, Pt.Position.Y, Pt.Position.Z));
				Section.Normals.Add(FaceNormal);
				Section.UVs.Add(FVector2D(Pt.U, (Pt.Position.Z - FloorZ) / FMath::Max(1.f, HalfH - FloorZ)));
				Section.Tangents.Add(FProcMeshTangent(0.f, 1.f, 0.f));
			}

			const int32 TotalPts = ProfilePoints.Num();
			for (int32 i = 0; i < TotalPts; ++i)
			{
				const int32 Next = (i + 1) % TotalPts;
				if (FaceNormal.X < 0.f)
				{
					AppendTriangle(Section.Triangles, CI, CI + 1 + Next, CI + 1 + i);
				}
				else
				{
					AppendTriangle(Section.Triangles, CI, CI + 1 + i, CI + 1 + Next);
				}
			}

			Section.VertexCount = Section.Vertices.Num();
			Section.TriangleCount = Section.Triangles.Num();
		};

		constexpr float CapHalfThickness = 8.f;

		if (bIsFirst)
		{
			const float CapX = Comp.HydroBoundsMin.X;
			EmitCapDisc(MeshData.BowCapSection, CapX - CapHalfThickness, -FVector::ForwardVector);
			EmitCapDisc(MeshData.BowCapSection, CapX + CapHalfThickness, FVector::ForwardVector);
		}

		if (bIsLast)
		{
			const float CapX = Comp.HydroBoundsMax.X;
			EmitCapDisc(MeshData.SternCapSection, CapX - CapHalfThickness, -FVector::ForwardVector);
			EmitCapDisc(MeshData.SternCapSection, CapX + CapHalfThickness, FVector::ForwardVector);
		}

		Definition->InteriorMeshes.Add(MoveTemp(MeshData));
	}

	// Generate airlock compartments as simple box geometry.
	for (const FGeneratedCompartmentDef& Comp : Definition->Compartments)
	{
		if (Comp.SemanticType != ESubCompartmentType::Airlock)
		{
			continue;
		}

		FSubmarineInteriorCompartmentMeshData MeshData;
		MeshData.CompartmentId = Comp.CompartmentId;

		const FVector Min = Comp.HydroBoundsMin;
		const FVector Max = Comp.HydroBoundsMax;
		if (Max.X <= Min.X || Max.Y <= Min.Y || Max.Z <= Min.Z)
		{
			continue;
		}

		const FVector P000(Min.X, Min.Y, Min.Z);
		const FVector P001(Min.X, Min.Y, Max.Z);
		const FVector P010(Min.X, Max.Y, Min.Z);
		const FVector P011(Min.X, Max.Y, Max.Z);
		const FVector P100(Max.X, Min.Y, Min.Z);
		const FVector P101(Max.X, Min.Y, Max.Z);
		const FVector P110(Max.X, Max.Y, Min.Z);
		const FVector P111(Max.X, Max.Y, Max.Z);

		// Walls: 4 vertical faces (inward-facing normals for interior view).
		AppendQuad(MeshData.WallSection, P010, P000, P011, P001, -FVector::ForwardVector, -FVector::RightVector);
		AppendQuad(MeshData.WallSection, P100, P110, P101, P111, FVector::ForwardVector, FVector::RightVector);
		AppendQuad(MeshData.WallSection, P000, P100, P001, P101, -FVector::RightVector, FVector::ForwardVector);
		AppendQuad(MeshData.WallSection, P110, P010, P111, P011, FVector::RightVector, -FVector::ForwardVector);
		// Ceiling.
		AppendQuad(MeshData.WallSection, P001, P101, P011, P111, FVector::UpVector, FVector::ForwardVector);

		MeshData.WallSection.VertexCount = MeshData.WallSection.Vertices.Num();
		MeshData.WallSection.TriangleCount = MeshData.WallSection.Triangles.Num();

		// Floor.
		MeshData.FloorSection.Vertices = {P000, P010, P100, P110};
		MeshData.FloorSection.Normals = {FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector};
		MeshData.FloorSection.UVs = {FVector2D(0.f, 0.f), FVector2D(1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f)};
		MeshData.FloorSection.Tangents = {
			FProcMeshTangent(1.f, 0.f, 0.f), FProcMeshTangent(1.f, 0.f, 0.f),
			FProcMeshTangent(1.f, 0.f, 0.f), FProcMeshTangent(1.f, 0.f, 0.f)
		};
		AppendTriangle(MeshData.FloorSection.Triangles, 0, 2, 1);
		AppendTriangle(MeshData.FloorSection.Triangles, 1, 2, 3);

		Definition->InteriorMeshes.Add(MoveTemp(MeshData));
	}

	return Definition->InteriorMeshes.Num() > 0;
}

// --- Bulkheads ---------------------------------------------------------------

bool USubmarineMeshBuilder::BuildBulkheads(
	USubmarineDefinition* Definition,
	const USubmarineGeneratorEnvelopeDef* Envelope)
{
	const float SpineLength = FMath::Max(100.f, Envelope->SpineLengthCm);
	const float SectionExponent = FMath::Max(0.5f, Envelope->SectionExponent);
	const float WidthToHeightRatio = FMath::Max(0.5f, Envelope->WidthToHeightRatio);
	const float WallThickness = Definition->WallThicknessCm;
	const float Exp = 2.f / SectionExponent;
	constexpr float HalfThickness = 8.f;
	constexpr float GeometryInset = 0.1f;
	constexpr int32 OutlineSegments = 20;

	// Interior bulkhead connections. Exterior hatches (CompartmentB == None) are skipped.
	for (const FGeneratedConnectionDef& Conn : Definition->Connections)
	{
		// Skip exterior connections (outer hatch has no hull-shaped bulkhead).
		if (Conn.CompartmentB.IsNone())
		{
			continue;
		}

		const FGeneratedCompartmentDef* CompA = Definition->FindCompartment(Conn.CompartmentA);
		const FGeneratedCompartmentDef* CompB = Definition->FindCompartment(Conn.CompartmentB);
		if (!CompA || !CompB)
		{
			continue;
		}

		// Derive radius from envelope at the bulkhead X position.
		// EvaluateRadius already bakes in the taper since M1.2.
		const float BulkheadX = Conn.LocalTransform.GetLocation().X;
		const float NormPos = FMath::Clamp(BulkheadX / SpineLength, 0.f, 1.f);
		const float EnvR = Envelope->EvaluateRadius(NormPos);
		const float InternalRadius = FMath::Max(10.f, EnvR - WallThickness - GeometryInset);
		const float HalfH = InternalRadius;
		const float HalfW = InternalRadius * WidthToHeightRatio;
		const float FloorZ = FMath::Max(CompA->WalkableFloorZCm, CompB->WalkableFloorZCm);

		const bool bHasDoor = (Conn.ConnectionType != EConnectionType::Open)
			&& (Conn.ConnectionType != EConnectionType::ExteriorHatch);
		const float DoorHalfWidth = Conn.DoorWidthCm * 0.5f;
		const float DoorHeight = Conn.DoorHeightCm;

		// Generate outline from floor-left through top arc to floor-right.
		const float FloorRatio = FMath::Clamp(FloorZ / HalfH, -1.f, 1.f);
		const float InvExp = 1.f / FMath::Max(KINDA_SMALL_NUMBER, Exp);
		const float MappedSin = FMath::Clamp(SuperellipsePow(FloorRatio, InvExp), -1.f, 1.f);
		const float EndTheta = FMath::Asin(MappedSin);
		const float StartTheta = PI - EndTheta;

		TArray<FVector2D> OutlinePoints;
		OutlinePoints.Reserve(OutlineSegments + 3);
		for (int32 Seg = 0; Seg <= OutlineSegments; ++Seg)
		{
			const float T = static_cast<float>(Seg) / static_cast<float>(OutlineSegments);
			const float Theta = FMath::Lerp(StartTheta, EndTheta, T);
			const float CosA = FMath::Cos(Theta);
			const float SinA = FMath::Sin(Theta);
			const float Y = HalfW * SuperellipsePow(CosA, Exp);
			const float Z = HalfH * SuperellipsePow(SinA, Exp);
			OutlinePoints.Add(FVector2D(Y, Z));
		}

		// Close the floor edge.
		if (OutlinePoints.Num() >= 2)
		{
			OutlinePoints.Add(FVector2D(OutlinePoints.Last().X, FloorZ));
			OutlinePoints.Add(FVector2D(OutlinePoints[0].X, FloorZ));
		}

		if (OutlinePoints.Num() < 3)
		{
			continue;
		}

		// Build SolidOutline: arc sweep + optional door notch or floor closure.
		// Walks the silhouette boundary without the old fan-from-pivot filter that
		// dropped arc points when FloorDropBiasCm > 0. Door notch is inserted as
		// four explicit corner points; the polygon remains a simple (non-self-
		// intersecting) polygon regardless of FloorZ, suitable for ear clipping.
		TArray<FVector2D> SolidOutline;
		SolidOutline.Reserve(OutlinePoints.Num() + 4);
		SolidOutline.Append(OutlinePoints);

		if (bHasDoor && DoorHalfWidth > 0.f && DoorHeight > 0.f)
		{
			const float DoorTopZ = FloorZ + DoorHeight;
			// From the right arc endpoint (last point of OutlinePoints, at FloorZ),
			// walk the floor line toward the right door corner, up the right
			// door edge, across the door top, down the left door edge, and along
			// the floor back to the left arc endpoint (implicit polygon closure).
			SolidOutline.Add(FVector2D(DoorHalfWidth, FloorZ));
			SolidOutline.Add(FVector2D(DoorHalfWidth, DoorTopZ));
			SolidOutline.Add(FVector2D(-DoorHalfWidth, DoorTopZ));
			SolidOutline.Add(FVector2D(-DoorHalfWidth, FloorZ));
		}
		else
		{
			// No door: close the polygon with a straight floor segment.
			SolidOutline.Add(FVector2D(OutlinePoints.Last().X, FloorZ));
			SolidOutline.Add(FVector2D(OutlinePoints[0].X, FloorZ));
		}

		// Remove duplicate consecutive points and close-loop duplicates.
		TArray<FVector2D> Sanitized;
		Sanitized.Reserve(SolidOutline.Num());
		for (const FVector2D& Pt : SolidOutline)
		{
			if (Sanitized.Num() == 0 || !Sanitized.Last().Equals(Pt, 0.1f))
			{
				Sanitized.Add(Pt);
			}
		}
		if (Sanitized.Num() > 1 && Sanitized[0].Equals(Sanitized.Last(), 0.1f))
		{
			Sanitized.Pop();
		}
		SolidOutline = MoveTemp(Sanitized);

		if (SolidOutline.Num() < 3)
		{
			continue;
		}

		// Build bulkhead mesh data.
		FSubmarineBulkheadMeshData MeshData;
		MeshData.ForeCompartmentId = Conn.CompartmentA;
		MeshData.AftCompartmentId = Conn.CompartmentB;
		MeshData.BulkheadId = FName(*FString::Printf(TEXT("Bulkhead_%s_%s"),
			*Conn.CompartmentA.ToString(), *Conn.CompartmentB.ToString()));

		// Map EConnectionType to EPassageType for mesh data.
		switch (Conn.ConnectionType)
		{
		case EConnectionType::Door:
			MeshData.PassageType = EPassageType::WatertightDoor;
			break;
		case EConnectionType::Hatch:
		case EConnectionType::ExteriorHatch:
			MeshData.PassageType = EPassageType::Hatch;
			break;
		case EConnectionType::Open:
			MeshData.PassageType = EPassageType::Open;
			break;
		}

		// Emit front and back disc faces.
		const float ForeX = BulkheadX - HalfThickness;
		const float AftX = BulkheadX + HalfThickness;

		// Ear clipping on SolidOutline. Works for any simple polygon (convex or
		// concave with notches) and replaces the old fan-from-pivot triangulation
		// which only handled star-shaped polygons. Polygon orientation is
		// detected via signed area; the algorithm operates on a CCW copy so the
		// convex-vertex test and the "no other point inside ear" test have a
		// single sign convention.
		auto ComputeSignedArea = [](const TArray<FVector2D>& Poly) -> float
		{
			float Area = 0.f;
			for (int32 i = 0; i < Poly.Num(); ++i)
			{
				const FVector2D& P0 = Poly[i];
				const FVector2D& P1 = Poly[(i + 1) % Poly.Num()];
				Area += P0.X * P1.Y - P1.X * P0.Y;
			}
			return Area * 0.5f;
		};

		const float PolySignedArea = ComputeSignedArea(SolidOutline);
		const bool bPolyIsCCW = PolySignedArea > 0.f;

		TArray<FVector2D> CCWPoly = SolidOutline;
		TArray<int32> IndexMap;
		IndexMap.SetNum(CCWPoly.Num());
		for (int32 i = 0; i < IndexMap.Num(); ++i)
		{
			IndexMap[i] = i;
		}
		if (!bPolyIsCCW)
		{
			Algo::Reverse(CCWPoly);
			Algo::Reverse(IndexMap);
		}

		// Ear clipping on the CCW polygon.
		TArray<FIntVector> EarTriangles;
		{
			auto Cross2D = [](const FVector2D& A, const FVector2D& B)
			{
				return A.X * B.Y - A.Y * B.X;
			};
			auto PointInTriangle = [](const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C) -> bool
			{
				auto Sign = [](const FVector2D& P1, const FVector2D& P2, const FVector2D& P3)
				{
					return (P1.X - P3.X) * (P2.Y - P3.Y) - (P2.X - P3.X) * (P1.Y - P3.Y);
				};
				const float D1 = Sign(P, A, B);
				const float D2 = Sign(P, B, C);
				const float D3 = Sign(P, C, A);
				const bool HasNeg = (D1 < 0.f) || (D2 < 0.f) || (D3 < 0.f);
				const bool HasPos = (D1 > 0.f) || (D2 > 0.f) || (D3 > 0.f);
				return !(HasNeg && HasPos);
			};

			TArray<int32> Indices;
			Indices.Reserve(CCWPoly.Num());
			for (int32 i = 0; i < CCWPoly.Num(); ++i)
			{
				Indices.Add(i);
			}

			int32 Guard = Indices.Num() * Indices.Num() + 16;
			while (Indices.Num() > 3 && Guard-- > 0)
			{
				bool bEarFound = false;
				for (int32 i = 0; i < Indices.Num(); ++i)
				{
					const int32 PrevI = Indices[(i - 1 + Indices.Num()) % Indices.Num()];
					const int32 CurrI = Indices[i];
					const int32 NextI = Indices[(i + 1) % Indices.Num()];
					const FVector2D& Prev = CCWPoly[PrevI];
					const FVector2D& Curr = CCWPoly[CurrI];
					const FVector2D& Next = CCWPoly[NextI];

					// Convex vertex (for CCW polygon): cross(edge1, edge2) > 0
					const FVector2D Edge1 = Curr - Prev;
					const FVector2D Edge2 = Next - Curr;
					if (Cross2D(Edge1, Edge2) <= 0.f)
					{
						continue;
					}

					// Ear validity: no other polygon vertex must be inside the triangle.
					bool bContainsOther = false;
					for (int32 j = 0; j < Indices.Num(); ++j)
					{
						const int32 OtherI = Indices[j];
						if (OtherI == PrevI || OtherI == CurrI || OtherI == NextI)
						{
							continue;
						}
						if (PointInTriangle(CCWPoly[OtherI], Prev, Curr, Next))
						{
							bContainsOther = true;
							break;
						}
					}
					if (bContainsOther)
					{
						continue;
					}

					EarTriangles.Add(FIntVector(PrevI, CurrI, NextI));
					Indices.RemoveAt(i);
					bEarFound = true;
					break;
				}
				if (!bEarFound)
				{
					break;
				}
			}
			if (Indices.Num() == 3)
			{
				EarTriangles.Add(FIntVector(Indices[0], Indices[1], Indices[2]));
			}
		}

		// Remap ear triangle indices back to SolidOutline (original polygon).
		for (FIntVector& Tri : EarTriangles)
		{
			Tri = FIntVector(IndexMap[Tri.X], IndexMap[Tri.Y], IndexMap[Tri.Z]);
		}

		// Emit front face (-X normal) and back face (+X normal). Triangles are
		// CCW in CCWPoly space; after remapping to SolidOutline indices, the
		// spatial winding of (Prev, Curr, Next) in 2D is preserved (CCW triangles
		// in Y-Z plane face +X via the right-hand rule). The back face uses them
		// directly, the front face reverses the winding.
		auto EmitFace = [&](float XPos, const FVector& FaceNormal, bool bReverseWinding)
		{
			const int32 Base = MeshData.PanelSection.Vertices.Num();
			for (const FVector2D& Pt : SolidOutline)
			{
				MeshData.PanelSection.Vertices.Add(FVector(XPos, Pt.X, Pt.Y));
				MeshData.PanelSection.Normals.Add(FaceNormal);
				MeshData.PanelSection.UVs.Add(FVector2D(
					(Pt.X / FMath::Max(1.f, HalfW) + 1.f) * 0.5f,
					(Pt.Y / FMath::Max(1.f, HalfH) + 1.f) * 0.5f));
				MeshData.PanelSection.Tangents.Add(FProcMeshTangent(0.f, 1.f, 0.f));
			}

			for (const FIntVector& Tri : EarTriangles)
			{
				if (bReverseWinding)
				{
					AppendTriangle(MeshData.PanelSection.Triangles,
						Base + Tri.X, Base + Tri.Z, Base + Tri.Y);
				}
				else
				{
					AppendTriangle(MeshData.PanelSection.Triangles,
						Base + Tri.X, Base + Tri.Y, Base + Tri.Z);
				}
			}
		};

		// After remapping, EarTriangles reference the same spatial points with
		// CCW winding in the Y-Z plane (guaranteed by ear clipping). CCW
		// triangles in the Y-Z plane face +X via the right-hand rule (Y×Z=X),
		// so the back face (+X normal) uses the triangles directly and the
		// front face (-X normal) reverses each triangle winding.
		EmitFace(ForeX, -FVector::ForwardVector, true);
		EmitFace(AftX, FVector::ForwardVector, false);

		// Side faces: extrude quads between front and back outlines.
		for (int32 i = 0; i < SolidOutline.Num(); ++i)
		{
			const int32 NextI = (i + 1) % SolidOutline.Num();
			const FVector2D& P0 = SolidOutline[i];
			const FVector2D& P1 = SolidOutline[NextI];
			if (P0.Equals(P1, 0.1f))
			{
				continue;
			}

			FVector2D EdgeDir = P1 - P0;
			FVector EdgeNormal = FVector(0.f, -EdgeDir.Y, EdgeDir.X).GetSafeNormal();

			AppendQuad(
				MeshData.PanelSection,
				FVector(ForeX, P0.X, P0.Y),
				FVector(AftX, P0.X, P0.Y),
				FVector(ForeX, P1.X, P1.Y),
				FVector(AftX, P1.X, P1.Y),
				EdgeNormal,
				FVector::ForwardVector);
		}

		MeshData.PanelSection.VertexCount = MeshData.PanelSection.Vertices.Num();
		MeshData.PanelSection.TriangleCount = MeshData.PanelSection.Triangles.Num();

		if (MeshData.PanelSection.Vertices.Num() > 0 && MeshData.PanelSection.Triangles.Num() > 0)
		{
			Definition->BulkheadMeshes.Add(MoveTemp(MeshData));
		}
	}

	// Bulkheads are not required (a single-compartment sub has none).
	return true;
}
