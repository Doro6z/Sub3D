#include "SubmarineGeometryBuilder.h"

#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "SubmarineEnvelopeDef.h"

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

void AppendBoxPrism(
	FSubmarineMeshSectionData& Section,
	const FVector& Min,
	const FVector& Max)
{
	if (Max.X <= Min.X || Max.Y <= Min.Y || Max.Z <= Min.Z)
	{
		return;
	}

	const FVector P000(Min.X, Min.Y, Min.Z);
	const FVector P001(Min.X, Min.Y, Max.Z);
	const FVector P010(Min.X, Max.Y, Min.Z);
	const FVector P011(Min.X, Max.Y, Max.Z);
	const FVector P100(Max.X, Min.Y, Min.Z);
	const FVector P101(Max.X, Min.Y, Max.Z);
	const FVector P110(Max.X, Max.Y, Min.Z);
	const FVector P111(Max.X, Max.Y, Max.Z);

	AppendQuad(Section, P100, P110, P101, P111, FVector::ForwardVector, FVector::RightVector);
	AppendQuad(Section, P010, P000, P011, P001, -FVector::ForwardVector, -FVector::RightVector);
	AppendQuad(Section, P000, P100, P001, P101, -FVector::RightVector, FVector::ForwardVector);
	AppendQuad(Section, P110, P010, P111, P011, FVector::RightVector, -FVector::ForwardVector);
	AppendQuad(Section, P001, P101, P011, P111, FVector::UpVector, FVector::ForwardVector);
	AppendQuad(Section, P000, P010, P100, P110, -FVector::UpVector, FVector::ForwardVector);
}

const FCompartmentPlacement* FindPlacementById(
	const FSubmarineLayoutSolution& Solution,
	FName CompartmentId)
{
	return Solution.Compartments.FindByPredicate([CompartmentId](const FCompartmentPlacement& Placement)
	{
		return Placement.CompartmentId == CompartmentId;
	});
}

float SuperellipsePow(float Base, float Exp)
{
	if (FMath::Abs(Base) < KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Sign(Base) * FMath::Pow(FMath::Abs(Base), Exp);
}

}

bool USubmarineGeometryBuilder::GenerateInteriorMeshData(
	const FSubmarineLayoutSolution& Solution,
	TArray<FSubmarineInteriorCompartmentMeshData>& OutMeshData,
	float SectionExponent,
	float WidthToHeightRatio,
	int32 InteriorArcSegments,
	float WallThicknessCm) const
{
	OutMeshData.Reset();

	if (!Solution.IsValid())
	{
		return false;
	}

	OutMeshData.Reserve(Solution.Compartments.Num());
	for (int32 CompartmentIndex = 0; CompartmentIndex < Solution.Compartments.Num(); ++CompartmentIndex)
	{
		const FCompartmentPlacement& Placement = Solution.Compartments[CompartmentIndex];
		FSubmarineInteriorCompartmentMeshData MeshData;
		if (!GenerateCompartmentInteriorMeshData(
			Placement,
			CompartmentIndex == 0,
			CompartmentIndex == Solution.Compartments.Num() - 1,
			SectionExponent,
			WidthToHeightRatio,
			InteriorArcSegments,
			WallThicknessCm,
			MeshData))
		{
			return false;
		}

		OutMeshData.Add(MoveTemp(MeshData));
	}

	return OutMeshData.Num() == Solution.Compartments.Num();
}

TArray<UProceduralMeshComponent*> USubmarineGeometryBuilder::BuildInteriorMeshes(
	const FSubmarineLayoutSolution& Solution,
	AActor* ParentActor,
	UMaterialInterface* WallMaterialOverride,
	UMaterialInterface* FloorMaterialOverride,
	bool bEnableCollision,
	float SectionExponent,
	float WidthToHeightRatio,
	int32 InteriorArcSegments,
	float WallThicknessCm) const
{
	TArray<UProceduralMeshComponent*> BuiltMeshes;

	if (!ParentActor || !ParentActor->GetRootComponent())
	{
		return BuiltMeshes;
	}

	TArray<FSubmarineInteriorCompartmentMeshData> MeshDataSet;
	if (!GenerateInteriorMeshData(Solution, MeshDataSet, SectionExponent, WidthToHeightRatio, InteriorArcSegments, WallThicknessCm))
	{
		return BuiltMeshes;
	}

	TArray<FSubmarineBulkheadMeshData> BulkheadMeshDataSet;
	if (!GenerateBulkheadMeshData(Solution, BulkheadMeshDataSet, SectionExponent, WidthToHeightRatio))
	{
		return BuiltMeshes;
	}

	BuiltMeshes.Reserve(MeshDataSet.Num() + BulkheadMeshDataSet.Num());
	for (int32 MeshIndex = 0; MeshIndex < MeshDataSet.Num(); ++MeshIndex)
	{
		const FSubmarineInteriorCompartmentMeshData& MeshData = MeshDataSet[MeshIndex];
		const FName VisName(*FString::Printf(TEXT("PMC_Interior_Vis_%s"), *MeshData.CompartmentId.ToString()));
		UProceduralMeshComponent* VisPMC = NewObject<UProceduralMeshComponent>(ParentActor, VisName);
		if (VisPMC)
		{
			VisPMC->RegisterComponent();
			VisPMC->AttachToComponent(ParentActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			VisPMC->SetRelativeTransform(FTransform::Identity);
			VisPMC->SetCanEverAffectNavigation(false);
			VisPMC->bUseComplexAsSimpleCollision = bEnableCollision;
			VisPMC->SetGenerateOverlapEvents(false);
			VisPMC->SetCollisionProfileName(TEXT("SubInteriorVisual"));
			VisPMC->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

			VisPMC->CreateMeshSection(
				0,
				MeshData.WallSection.Vertices,
				MeshData.WallSection.Triangles,
				MeshData.WallSection.Normals,
				MeshData.WallSection.UVs,
				TArray<FColor>(),
				MeshData.WallSection.Tangents,
				bEnableCollision);

			if (MeshData.BowCapSection.Vertices.Num() > 0)
			{
				VisPMC->CreateMeshSection(
					2,
					MeshData.BowCapSection.Vertices,
					MeshData.BowCapSection.Triangles,
					MeshData.BowCapSection.Normals,
					MeshData.BowCapSection.UVs,
					TArray<FColor>(),
					MeshData.BowCapSection.Tangents,
					bEnableCollision);
			}

			if (MeshData.SternCapSection.Vertices.Num() > 0)
			{
				VisPMC->CreateMeshSection(
					3,
					MeshData.SternCapSection.Vertices,
					MeshData.SternCapSection.Triangles,
					MeshData.SternCapSection.Normals,
					MeshData.SternCapSection.UVs,
					TArray<FColor>(),
					MeshData.SternCapSection.Tangents,
					bEnableCollision);
			}

			if (WallMaterialOverride)
			{
				VisPMC->SetMaterial(0, WallMaterialOverride);
				if (MeshData.BowCapSection.Vertices.Num() > 0)
				{
					VisPMC->SetMaterial(2, WallMaterialOverride);
				}
				if (MeshData.SternCapSection.Vertices.Num() > 0)
				{
					VisPMC->SetMaterial(3, WallMaterialOverride);
				}
			}

			BuiltMeshes.Add(VisPMC);
		}

		const FName WalkName(*FString::Printf(TEXT("PMC_Interior_Walkable_%s"), *MeshData.CompartmentId.ToString()));
		UProceduralMeshComponent* WalkPMC = NewObject<UProceduralMeshComponent>(ParentActor, WalkName);
		if (WalkPMC)
		{
			WalkPMC->RegisterComponent();
			WalkPMC->AttachToComponent(ParentActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			WalkPMC->SetRelativeTransform(FTransform::Identity);
			WalkPMC->SetCanEverAffectNavigation(false);
			WalkPMC->bUseComplexAsSimpleCollision = bEnableCollision;
			WalkPMC->SetGenerateOverlapEvents(false);
			WalkPMC->SetCollisionProfileName(TEXT("SubInteriorWalkable"));
			WalkPMC->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

			WalkPMC->CreateMeshSection(
				0,
				MeshData.FloorSection.Vertices,
				MeshData.FloorSection.Triangles,
				MeshData.FloorSection.Normals,
				MeshData.FloorSection.UVs,
				TArray<FColor>(),
				MeshData.FloorSection.Tangents,
				bEnableCollision);

			if (FloorMaterialOverride)
			{
				WalkPMC->SetMaterial(0, FloorMaterialOverride);
			}

			BuiltMeshes.Add(WalkPMC);
		}

	}

	for (int32 MeshIndex = 0; MeshIndex < BulkheadMeshDataSet.Num(); ++MeshIndex)
	{
		const FSubmarineBulkheadMeshData& MeshData = BulkheadMeshDataSet[MeshIndex];
		const FName ComponentName(*FString::Printf(TEXT("PMC_Bulkhead_%s"), *MeshData.BulkheadId.ToString()));

		UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(ParentActor, ComponentName);
		if (!PMC)
		{
			continue;
		}

		PMC->RegisterComponent();
		PMC->AttachToComponent(ParentActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		PMC->SetRelativeTransform(FTransform::Identity);
		PMC->SetCanEverAffectNavigation(false);
		PMC->bUseComplexAsSimpleCollision = bEnableCollision;
		PMC->SetGenerateOverlapEvents(false);

		// Sealed bulkheads block crew traversal; open/doored bulkheads are visual-only.
		const bool bSealedBulkhead = (MeshData.PassageType == EPassageType::SealedBulkhead);
		PMC->SetCollisionProfileName(bSealedBulkhead ? TEXT("SubInteriorWalkable") : TEXT("SubInteriorVisual"));
		PMC->SetCollisionEnabled(bEnableCollision
			? (bSealedBulkhead ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::QueryOnly)
			: ECollisionEnabled::NoCollision);

		PMC->CreateMeshSection(
			0,
			MeshData.PanelSection.Vertices,
			MeshData.PanelSection.Triangles,
			MeshData.PanelSection.Normals,
			MeshData.PanelSection.UVs,
			TArray<FColor>(),
			MeshData.PanelSection.Tangents,
			bEnableCollision);

		if (WallMaterialOverride)
		{
			PMC->SetMaterial(0, WallMaterialOverride);
		}

		BuiltMeshes.Add(PMC);
	}

	return BuiltMeshes;
}

bool USubmarineGeometryBuilder::GenerateBulkheadMeshData(
	const FSubmarineLayoutSolution& Solution,
	TArray<FSubmarineBulkheadMeshData>& OutMeshData,
	float SectionExponent,
	float WidthToHeightRatio) const
{
	OutMeshData.Reset();

	if (!Solution.IsValid())
	{
		return false;
	}

	OutMeshData.Reserve(Solution.Bulkheads.Num());
	for (const FBulkheadPlacement& Bulkhead : Solution.Bulkheads)
	{
		FSubmarineBulkheadMeshData MeshData;
		if (!GenerateSingleBulkheadMeshData(Solution, Bulkhead, SectionExponent, WidthToHeightRatio, MeshData))
		{
			return false;
		}

		OutMeshData.Add(MoveTemp(MeshData));
	}

	return OutMeshData.Num() == Solution.Bulkheads.Num();
}

bool USubmarineGeometryBuilder::GenerateExteriorMeshData(
	const FSubmarineLayoutSolution& Solution,
	FSubmarineMeshSectionData& OutMeshData,
	int32 RadialSegments,
	int32 LongitudinalSubdivisionsPerSpan,
	float SectionExponent,
	float WidthToHeightRatio,
	const USubmarineEnvelopeDef* Envelope) const
{
	OutMeshData = FSubmarineMeshSectionData();

	if (!Solution.IsValid() || Solution.Compartments.Num() == 0)
	{
		return false;
	}

	const float HullOffset = (Envelope) ? FMath::Max(0.f, Envelope->ExteriorHullOffsetCm) : 12.f;

	TArray<FExteriorRing> KnotRings;
	KnotRings.Reserve(Solution.Compartments.Num() + 1);

	const FCompartmentPlacement& FirstCompartment = Solution.Compartments[0];
	FExteriorRing FirstRing;
	FirstRing.X = FirstCompartment.SpineStartCm;
	FirstRing.Radius = FMath::Max(1.f, FirstCompartment.EffectiveRadiusCm + HullOffset);
	KnotRings.Add(FirstRing);
	for (const FCompartmentPlacement& Compartment : Solution.Compartments)
	{
		FExteriorRing Ring;
		Ring.X = Compartment.SpineEndCm;
		Ring.Radius = FMath::Max(1.f, Compartment.EffectiveRadiusCm + HullOffset);
		KnotRings.Add(Ring);
	}

	const int32 EffectiveRadialSegments = FMath::Clamp(RadialSegments, 12, 64);
	const int32 EffectiveLongitudinalSubdivisions = FMath::Clamp(LongitudinalSubdivisionsPerSpan, 1, 16);
	TArray<FExteriorRing> Rings;
	Rings.Reserve(((KnotRings.Num() - 1) * EffectiveLongitudinalSubdivisions) + 1);
	for (int32 RingIndex = 0; RingIndex + 1 < KnotRings.Num(); ++RingIndex)
	{
		const FExteriorRing& StartRing = KnotRings[RingIndex];
		const FExteriorRing& EndRing = KnotRings[RingIndex + 1];

		for (int32 StepIndex = 0; StepIndex < EffectiveLongitudinalSubdivisions; ++StepIndex)
		{
			const float T = static_cast<float>(StepIndex) / static_cast<float>(EffectiveLongitudinalSubdivisions);
			FExteriorRing SampleRing;
			SampleRing.X = FMath::Lerp(StartRing.X, EndRing.X, T);
			SampleRing.Radius = FMath::InterpEaseInOut(StartRing.Radius, EndRing.Radius, T, 2.f);
			Rings.Add(SampleRing);
		}
	}
	Rings.Add(KnotRings.Last());

	if (Envelope && Envelope->SpineLengthCm > KINDA_SMALL_NUMBER)
	{
		for (FExteriorRing& Ring : Rings)
		{
			const float NormalizedPos = FMath::Clamp(Ring.X / Envelope->SpineLengthCm, 0.f, 1.f);
			const float TaperMultiplier = Envelope->EvaluateBowSternTaper(NormalizedPos);
			Ring.Radius = FMath::Max(1.f, Ring.Radius * TaperMultiplier);
		}
	}

	OutMeshData.Vertices.Reserve(Rings.Num() * EffectiveRadialSegments + 20);
	OutMeshData.Normals.Reserve(Rings.Num() * EffectiveRadialSegments + 20);
	OutMeshData.UVs.Reserve(Rings.Num() * EffectiveRadialSegments + 20);
	OutMeshData.Tangents.Reserve(Rings.Num() * EffectiveRadialSegments + 20);
	OutMeshData.Triangles.Reserve((Rings.Num() - 1) * EffectiveRadialSegments * 6 + EffectiveRadialSegments * 12);

	const float SpineStart = Rings[0].X;
	for (int32 i = 0; i < Rings.Num(); ++i)
	{
		const float X = Rings[i].X;
		const float R = Rings[i].Radius;
		const float U = (X - SpineStart) / 100.f;

		for (int32 Seg = 0; Seg < EffectiveRadialSegments; ++Seg)
		{
			const float Theta = 2.f * PI * static_cast<float>(Seg) / static_cast<float>(EffectiveRadialSegments);
			const float CosA = FMath::Cos(Theta);
			const float SinA = FMath::Sin(Theta);
			const float Y = R * WidthToHeightRatio * SuperellipsePow(CosA, 2.f / FMath::Max(0.5f, SectionExponent));
			const float Z = R * SuperellipsePow(SinA, 2.f / FMath::Max(0.5f, SectionExponent));

			// Normal of superellipse (x/a)^n + (y/b)^n = 1
			const float N_val = FMath::Max(0.5f, SectionExponent);
			const float NY = N_val * SuperellipsePow(CosA, N_val - 1.f) / FMath::Pow(FMath::Max(1.f, R * WidthToHeightRatio), N_val);
			const float NZ = N_val * SuperellipsePow(SinA, N_val - 1.f) / FMath::Pow(FMath::Max(1.f, R), N_val);
			FVector Normal(0.f, NY, NZ);
			Normal.Normalize();

			OutMeshData.Vertices.Add(FVector(X, Y, Z));
			OutMeshData.Normals.Add(Normal);
			OutMeshData.UVs.Add(FVector2D(U, static_cast<float>(Seg) / static_cast<float>(EffectiveRadialSegments)));
			OutMeshData.Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
		}
	}

	for (int32 i = 0; i < Rings.Num() - 1; ++i)
	{
		const int32 RingBase = i * EffectiveRadialSegments;
		const int32 NextRingBase = (i + 1) * EffectiveRadialSegments;

		for (int32 SegmentIndex = 0; SegmentIndex < EffectiveRadialSegments; ++SegmentIndex)
		{
			const int32 NextSegment = (SegmentIndex + 1) % EffectiveRadialSegments;
			const int32 A0 = RingBase + SegmentIndex;
			const int32 A1 = RingBase + NextSegment;
			const int32 B0 = NextRingBase + SegmentIndex;
			const int32 B1 = NextRingBase + NextSegment;

			AppendTriangle(OutMeshData.Triangles, A0, A1, B0);
			AppendTriangle(OutMeshData.Triangles, B0, A1, B1);
		}
	}

	// Progressive bow cap.
	{
		const float BowX = Rings[0].X;
		const float BowRadius = Rings[0].Radius;
		const int32 CapRings = 6;
		const float CapLengthCm = FMath::Min(BowRadius * 0.8f, 80.f);

		TArray<int32> CapRingBases;
		CapRingBases.Add(0);

		for (int32 Ring = 1; Ring <= CapRings; ++Ring)
		{
			const float T = static_cast<float>(Ring) / static_cast<float>(CapRings + 1);
			const float ShrinkRadius = BowRadius * FMath::Cos(T * PI * 0.5f);
			const float CapXPos = BowX - T * CapLengthCm;

			const int32 RingBase = OutMeshData.Vertices.Num();
			CapRingBases.Add(RingBase);

			for (int32 Seg = 0; Seg < EffectiveRadialSegments; ++Seg)
			{
				const float Theta = 2.f * PI * static_cast<float>(Seg) / static_cast<float>(EffectiveRadialSegments);
				const float CosA = FMath::Cos(Theta);
				const float SinA = FMath::Sin(Theta);
				const float Y = ShrinkRadius * WidthToHeightRatio * SuperellipsePow(CosA, 2.f / FMath::Max(0.5f, SectionExponent));
				const float Z = ShrinkRadius * SuperellipsePow(SinA, 2.f / FMath::Max(0.5f, SectionExponent));
				FVector Normal = FVector(-T, CosA * (1.f - T), SinA * (1.f - T));
				Normal.Normalize();
				OutMeshData.Vertices.Add(FVector(CapXPos, Y, Z));
				OutMeshData.Normals.Add(Normal);
				OutMeshData.UVs.Add(FVector2D(0.5f + CosA * 0.5f * (1.f - T), 0.5f + SinA * 0.5f * (1.f - T)));
				OutMeshData.Tangents.Add(FProcMeshTangent(0.f, -SinA, CosA));
			}
		}

		for (int32 Ring = 0; Ring < CapRingBases.Num() - 1; ++Ring)
		{
			const int32 BaseA = CapRingBases[Ring];
			const int32 BaseB = CapRingBases[Ring + 1];
			for (int32 Seg = 0; Seg < EffectiveRadialSegments; ++Seg)
			{
				const int32 NextSeg = (Seg + 1) % EffectiveRadialSegments;
				const int32 A0 = BaseA + Seg;
				const int32 A1 = BaseA + NextSeg;
				const int32 B0 = BaseB + Seg;
				const int32 B1 = BaseB + NextSeg;
				AppendTriangle(OutMeshData.Triangles, A0, B0, A1);
				AppendTriangle(OutMeshData.Triangles, A1, B0, B1);
			}
		}

		const int32 BowCenterIndex = OutMeshData.Vertices.Num();
		OutMeshData.Vertices.Add(FVector(BowX - CapLengthCm, 0.f, 0.f));
		OutMeshData.Normals.Add(-FVector::ForwardVector);
		OutMeshData.UVs.Add(FVector2D(0.5f, 0.5f));
		OutMeshData.Tangents.Add(FProcMeshTangent(0.f, 1.f, 0.f));

		const int32 LastRingBase = CapRingBases.Last();
		for (int32 Seg = 0; Seg < EffectiveRadialSegments; ++Seg)
		{
			const int32 NextSeg = (Seg + 1) % EffectiveRadialSegments;
			AppendTriangle(OutMeshData.Triangles, BowCenterIndex, LastRingBase + NextSeg, LastRingBase + Seg);
		}
	}

	// Progressive stern cap.
	{
		const float SternX = Rings.Last().X;
		const float SternRadius = Rings.Last().Radius;
		const int32 CapRings = 6;
		const float CapLengthCm = FMath::Min(SternRadius * 0.8f, 80.f);
		const int32 SternRingBase = (Rings.Num() - 1) * EffectiveRadialSegments;

		TArray<int32> CapRingBases;
		CapRingBases.Add(SternRingBase);

		for (int32 Ring = 1; Ring <= CapRings; ++Ring)
		{
			const float T = static_cast<float>(Ring) / static_cast<float>(CapRings + 1);
			const float ShrinkRadius = SternRadius * FMath::Cos(T * PI * 0.5f);
			const float CapXPos = SternX + T * CapLengthCm;

			const int32 RingBase = OutMeshData.Vertices.Num();
			CapRingBases.Add(RingBase);

			for (int32 Seg = 0; Seg < EffectiveRadialSegments; ++Seg)
			{
				const float Theta = 2.f * PI * static_cast<float>(Seg) / static_cast<float>(EffectiveRadialSegments);
				const float CosA = FMath::Cos(Theta);
				const float SinA = FMath::Sin(Theta);
				const float Y = ShrinkRadius * WidthToHeightRatio * SuperellipsePow(CosA, 2.f / FMath::Max(0.5f, SectionExponent));
				const float Z = ShrinkRadius * SuperellipsePow(SinA, 2.f / FMath::Max(0.5f, SectionExponent));
				
				FVector Normal = FVector(T, CosA * (1.f - T), SinA * (1.f - T));
				Normal.Normalize();
				
				OutMeshData.Vertices.Add(FVector(CapXPos, Y, Z));
				OutMeshData.Normals.Add(Normal);
				OutMeshData.UVs.Add(FVector2D(0.5f + CosA * 0.5f * (1.f - T), 0.5f + SinA * 0.5f * (1.f - T)));
				OutMeshData.Tangents.Add(FProcMeshTangent(0.f, -SinA, CosA));
			}
		}

		for (int32 Ring = 0; Ring < CapRingBases.Num() - 1; ++Ring)
		{
			const int32 BaseA = CapRingBases[Ring];
			const int32 BaseB = CapRingBases[Ring + 1];
			for (int32 Seg = 0; Seg < EffectiveRadialSegments; ++Seg)
			{
				const int32 NextSeg = (Seg + 1) % EffectiveRadialSegments;
				const int32 A0 = BaseA + Seg;
				const int32 A1 = BaseA + NextSeg;
				const int32 B0 = BaseB + Seg;
				const int32 B1 = BaseB + NextSeg;
				AppendTriangle(OutMeshData.Triangles, A0, A1, B0);
				AppendTriangle(OutMeshData.Triangles, B0, A1, B1);
			}
		}

		const int32 SternCenterIndex = OutMeshData.Vertices.Num();
		OutMeshData.Vertices.Add(FVector(SternX + CapLengthCm, 0.f, 0.f));
		OutMeshData.Normals.Add(FVector::ForwardVector);
		OutMeshData.UVs.Add(FVector2D(0.5f, 0.5f));
		OutMeshData.Tangents.Add(FProcMeshTangent(0.f, 1.f, 0.f));

		const int32 LastRingBase = CapRingBases.Last();
		for (int32 Seg = 0; Seg < EffectiveRadialSegments; ++Seg)
		{
			const int32 NextSeg = (Seg + 1) % EffectiveRadialSegments;
			AppendTriangle(OutMeshData.Triangles, SternCenterIndex, LastRingBase + Seg, LastRingBase + NextSeg);
		}
	}

	OutMeshData.VertexCount = OutMeshData.Vertices.Num();
	OutMeshData.TriangleCount = OutMeshData.Triangles.Num();

	return OutMeshData.Vertices.Num() > 0 && OutMeshData.Triangles.Num() > 0;
}

UProceduralMeshComponent* USubmarineGeometryBuilder::BuildExteriorMesh(
	const FSubmarineLayoutSolution& Solution,
	AActor* ParentActor,
	UMaterialInterface* ExteriorMaterialOverride,
	bool bEnableCollision,
	int32 RadialSegments,
	int32 LongitudinalSubdivisionsPerSpan,
	float SectionExponent,
	float WidthToHeightRatio,
	const USubmarineEnvelopeDef* Envelope) const
{
	if (!ParentActor || !ParentActor->GetRootComponent())
	{
		return nullptr;
	}

	FSubmarineMeshSectionData MeshData;
	if (!GenerateExteriorMeshData(Solution, MeshData, RadialSegments, LongitudinalSubdivisionsPerSpan, SectionExponent, WidthToHeightRatio, Envelope))
	{
		return nullptr;
	}

	UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(ParentActor, TEXT("PMC_ExteriorHull"));
	if (!PMC)
	{
		return nullptr;
	}

	PMC->RegisterComponent();
	PMC->AttachToComponent(ParentActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	PMC->SetRelativeTransform(FTransform::Identity);
	PMC->SetCanEverAffectNavigation(false);
	PMC->bUseComplexAsSimpleCollision = bEnableCollision;
	PMC->SetGenerateOverlapEvents(false);
	PMC->SetCollisionProfileName(TEXT("SubmarineHull"));
	PMC->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	PMC->CreateMeshSection(
		0,
		MeshData.Vertices,
		MeshData.Triangles,
		MeshData.Normals,
		MeshData.UVs,
		TArray<FColor>(),
		MeshData.Tangents,
		bEnableCollision);

	if (ExteriorMaterialOverride)
	{
		PMC->SetMaterial(0, ExteriorMaterialOverride);
	}

	return PMC;
}

bool USubmarineGeometryBuilder::GenerateCompartmentInteriorMeshData(
	const FCompartmentPlacement& Placement,
	bool bGenerateBowCap,
	bool bGenerateSternCap,
	float SectionExponent,
	float WidthToHeightRatio,
	int32 InteriorArcSegments,
	float WallThicknessCm,
	FSubmarineInteriorCompartmentMeshData& OutMeshData) const
{
	constexpr float BoundaryHalfThicknessCm = 8.f;
	const float RadiusCm = FMath::Max(1.f, Placement.EffectiveRadiusCm);
	// Inset interior walls from the envelope radius by WallThicknessCm.
	const float InsetRadius = FMath::Max(10.f, RadiusCm - WallThicknessCm);
	const float HalfH = InsetRadius;
	const float HalfW = InsetRadius * FMath::Max(0.5f, WidthToHeightRatio);
	const float Exp = 2.f / FMath::Max(0.5f, SectionExponent);
	const float N = FMath::Max(0.5f, SectionExponent);
	const float FloorZ = FMath::Clamp(Placement.FloorOffsetCm, -HalfH * 0.98f, HalfH * 0.98f);
	const float WallStartX = Placement.SpineStartCm + BoundaryHalfThicknessCm;
	const float RawWallEndX = Placement.SpineEndCm - BoundaryHalfThicknessCm;
	const float WallEndX = FMath::Max(WallStartX + 1.f, RawWallEndX);

	const float AbsFloorRatio = FMath::Abs(FloorZ) / HalfH;
	const float FloorHalfWidth = (AbsFloorRatio >= 1.f) ? 0.f
		: HalfW * FMath::Pow(FMath::Max(0.f, 1.f - FMath::Pow(AbsFloorRatio, N)), 1.f / N);
	const int32 ArcSegments = FMath::Clamp(InteriorArcSegments, 8, 48);

	if (FloorHalfWidth <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float FloorRatio = FMath::Clamp(FloorZ / HalfH, -1.f, 1.f);
	const float InvExp = 1.f / FMath::Max(KINDA_SMALL_NUMBER, Exp);
	const float MappedSin = FMath::Clamp(SuperellipsePow(FloorRatio, InvExp), -1.f, 1.f);
	const float EndTheta = FMath::Asin(MappedSin);
	const float StartTheta = PI - EndTheta;

	const USubmarineEnvelopeDef* Envelope = nullptr;
	if (Placement.CompartmentId != NAME_None) // We need the envelope, but it's not passed here.
	{
		// Internal radius is already inset.
	}

	// Calculate total arc length for U normalization
	float TotalArcLength = 0.f;
	{
		auto SuperEval = [&](float Angle, FVector2D& Pos, FVector2D& Norm)
		{
			const float CosA = FMath::Cos(Angle);
			const float SinA = FMath::Sin(Angle);
			Pos.X = HalfW * SuperellipsePow(CosA, Exp);
			Pos.Y = HalfH * SuperellipsePow(SinA, Exp);
			// Approx normal for UV logic
			Norm = FVector2D(-CosA, -SinA).GetSafeNormal();
		};

		FVector2D PrevPos, Dum;
		SuperEval(StartTheta, PrevPos, Dum);
		const int32 ArcSamples = ArcSegments * 2;
		for (int32 i = 1; i <= ArcSamples; ++i)
		{
			const float T = (float)i / (float)ArcSamples;
			FVector2D CurrPos;
			SuperEval(FMath::Lerp(StartTheta, EndTheta, T), CurrPos, Dum);
			TotalArcLength += FVector2D::Distance(PrevPos, CurrPos);
			PrevPos = CurrPos;
		}
	}

	TArray<FProfilePoint> ProfilePoints;
	ProfilePoints.Reserve(ArcSegments + 1);

	float TraversedArcLength = 0.f;
	FVector2D LastPos, LastNorm;
	{
		auto SuperEval = [&](float Angle, FVector2D& Pos, FVector2D& Norm)
		{
			const float CosA = FMath::Cos(Angle);
			const float SinA = FMath::Sin(Angle);
			Pos.X = HalfW * SuperellipsePow(CosA, Exp);
			Pos.Y = HalfH * SuperellipsePow(SinA, Exp);
			const float N = 2.f / Exp;
			const float NX = -N * SuperellipsePow(CosA, N - 1.f) / FMath::Pow(HalfW, N);
			const float NY = -N * SuperellipsePow(SinA, N - 1.f) / FMath::Pow(HalfH, N);
			Norm = FVector2D(NX, NY).GetSafeNormal();
		};
		SuperEval(StartTheta, LastPos, LastNorm);
	}

	for (int32 SegmentIndex = 0; SegmentIndex <= ArcSegments; ++SegmentIndex)
	{
		const float T = static_cast<float>(SegmentIndex) / static_cast<float>(ArcSegments);
		const float Theta = FMath::Lerp(StartTheta, EndTheta, T);
		
		FVector2D Pos, Norm;
		{
			const float CosA = FMath::Cos(Theta);
			const float SinA = FMath::Sin(Theta);
			Pos.X = HalfW * SuperellipsePow(CosA, Exp);
			Pos.Y = HalfH * SuperellipsePow(SinA, Exp);
			const float N_val = 2.f / Exp;
			const float NX = -N_val * SuperellipsePow(CosA, N_val - 1.f) / FMath::Pow(HalfW, N_val);
			const float NY = -N_val * SuperellipsePow(SinA, N_val - 1.f) / FMath::Pow(HalfH, N_val);
			Norm = FVector2D(NX, NY).GetSafeNormal();
		}

		if (SegmentIndex > 0)
		{
			TraversedArcLength += FVector2D::Distance(LastPos, Pos);
		}

		FProfilePoint Point;
		Point.Position = FVector(0.f, Pos.X, Pos.Y);
		Point.InwardNormal = FVector(0.f, Norm.X, Norm.Y);
		Point.U = (TotalArcLength > KINDA_SMALL_NUMBER) ? (TraversedArcLength / TotalArcLength) : T;
		ProfilePoints.Add(Point);
		
		LastPos = Pos;
	}

	OutMeshData = FSubmarineInteriorCompartmentMeshData();
	OutMeshData.CompartmentId = Placement.CompartmentId;

	const int32 PointsPerRing = ProfilePoints.Num();
	OutMeshData.WallSection.Vertices.Reserve(PointsPerRing * 2);
	OutMeshData.WallSection.Normals.Reserve(PointsPerRing * 2);
	OutMeshData.WallSection.UVs.Reserve(PointsPerRing * 2);
	OutMeshData.WallSection.Triangles.Reserve((PointsPerRing - 1) * 6);

	for (int32 RingIndex = 0; RingIndex < 2; ++RingIndex)
	{
		const float X = (RingIndex == 0) ? WallStartX : WallEndX;
		const float V = static_cast<float>(RingIndex);

		for (const FProfilePoint& Point : ProfilePoints)
		{
			OutMeshData.WallSection.Vertices.Add(FVector(X, Point.Position.Y, Point.Position.Z));
			OutMeshData.WallSection.Normals.Add(Point.InwardNormal);
			OutMeshData.WallSection.UVs.Add(FVector2D(Point.U, V));
			OutMeshData.WallSection.Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
		}
	}

	for (int32 SegmentIndex = 0; SegmentIndex < PointsPerRing - 1; ++SegmentIndex)
	{
		const int32 FrontCurrent = SegmentIndex;
		const int32 FrontNext = SegmentIndex + 1;
		const int32 BackCurrent = PointsPerRing + SegmentIndex;
		const int32 BackNext = PointsPerRing + SegmentIndex + 1;

		AppendTriangle(OutMeshData.WallSection.Triangles, FrontCurrent, FrontNext, BackCurrent);
		AppendTriangle(OutMeshData.WallSection.Triangles, BackCurrent, FrontNext, BackNext);
	}

	OutMeshData.WallSection.VertexCount = OutMeshData.WallSection.Vertices.Num();
	OutMeshData.WallSection.TriangleCount = OutMeshData.WallSection.Triangles.Num();

	OutMeshData.FloorSection.Vertices = {
		FVector(WallStartX, -FloorHalfWidth, FloorZ),
		FVector(WallStartX, FloorHalfWidth, FloorZ),
		FVector(WallEndX, -FloorHalfWidth, FloorZ),
		FVector(WallEndX, FloorHalfWidth, FloorZ)
	};

	OutMeshData.FloorSection.Normals = {
		FVector::UpVector,
		FVector::UpVector,
		FVector::UpVector,
		FVector::UpVector
	};

	OutMeshData.FloorSection.UVs = {
		FVector2D(0.f, 0.f),
		FVector2D(1.f, 0.f),
		FVector2D(0.f, 1.f),
		FVector2D(1.f, 1.f)
	};

	AppendTriangle(OutMeshData.FloorSection.Triangles, 0, 2, 1);
	AppendTriangle(OutMeshData.FloorSection.Triangles, 1, 2, 3);

	const float CapHalfThicknessCm = 8.f;
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

	if (bGenerateBowCap)
	{
		const float CapX = Placement.SpineStartCm;
		EmitCapDisc(OutMeshData.BowCapSection, CapX - CapHalfThicknessCm, -FVector::ForwardVector);
		EmitCapDisc(OutMeshData.BowCapSection, CapX + CapHalfThicknessCm, FVector::ForwardVector);
	}

	if (bGenerateSternCap)
	{
		const float CapX = Placement.SpineEndCm;
		EmitCapDisc(OutMeshData.SternCapSection, CapX - CapHalfThicknessCm, -FVector::ForwardVector);
		EmitCapDisc(OutMeshData.SternCapSection, CapX + CapHalfThicknessCm, FVector::ForwardVector);
	}

	return OutMeshData.WallSection.Vertices.Num() > 0
		&& OutMeshData.WallSection.Triangles.Num() > 0
		&& OutMeshData.FloorSection.Vertices.Num() == 4
		&& OutMeshData.FloorSection.Triangles.Num() == 6;
}

bool USubmarineGeometryBuilder::GenerateSingleBulkheadMeshData(
	const FSubmarineLayoutSolution& Solution,
	const FBulkheadPlacement& Bulkhead,
	float SectionExponent,
	float WidthToHeightRatio,
	FSubmarineBulkheadMeshData& OutMeshData) const
{
	const FCompartmentPlacement* ForeCompartment = FindPlacementById(Solution, Bulkhead.ForeCompartmentId);
	const FCompartmentPlacement* AftCompartment = FindPlacementById(Solution, Bulkhead.AftCompartmentId);
	if (!ForeCompartment || !AftCompartment)
	{
		return false;
	}

	OutMeshData = FSubmarineBulkheadMeshData();
	OutMeshData.ForeCompartmentId = Bulkhead.ForeCompartmentId;
	OutMeshData.AftCompartmentId = Bulkhead.AftCompartmentId;
	OutMeshData.PassageType = Bulkhead.PassageType;
	OutMeshData.BulkheadId = FName(*FString::Printf(
		TEXT("Bulkhead_%s_%s"),
		*Bulkhead.ForeCompartmentId.ToString(),
		*Bulkhead.AftCompartmentId.ToString()));

	const float RadiusCm = FMath::Max(1.f, Bulkhead.RadiusCm);
	const float FloorZ = FMath::Max(ForeCompartment->FloorOffsetCm, AftCompartment->FloorOffsetCm);
	const float HalfThicknessCm = 8.f;

	// Inset bulkhead to match interior wall (Radius - WallThickness)
	// We also inset by a tiny epsilon (0.1cm) to avoid Z-fighting/bleeding through the hull.
	const float GeometryInset = 0.1f;
	const float EffectiveWallThickness = 12.0f; // TODO: Pass from envelope if available
	const float InternalRadius = FMath::Max(10.f, RadiusCm - EffectiveWallThickness - GeometryInset);

	const float N = FMath::Max(0.5f, SectionExponent);
	const float Exp = 2.f / FMath::Max(0.5f, N);
	const float HalfW = InternalRadius * FMath::Max(0.5f, WidthToHeightRatio);
	const float HalfH = InternalRadius;

	// Door cutout dimensions for passable bulkheads.
	const float DoorHalfWidth = 45.f;
	const float DoorHeight = 180.f;
	const bool bHasDoor = (Bulkhead.PassageType != EPassageType::SealedBulkhead);

	// Generate outline from floor-left through the top arc to floor-right.
	const int32 OutlineSegments = 20;
	TArray<FVector2D> OutlinePoints;
	OutlinePoints.Reserve(OutlineSegments + 3);

	const float FloorRatio = FMath::Clamp(FloorZ / HalfH, -1.f, 1.f);
	const float InvExp = 1.f / FMath::Max(KINDA_SMALL_NUMBER, Exp);
	const float MappedSin = FMath::Clamp(SuperellipsePow(FloorRatio, InvExp), -1.f, 1.f);
	const float EndTheta = FMath::Asin(MappedSin);
	const float StartTheta = PI - EndTheta;

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
		return false;
	}

	// If passable: split outline into points OUTSIDE the door rectangle.
	// Door rectangle: Y in [-DoorHalfWidth, DoorHalfWidth], Z in [FloorZ, FloorZ + DoorHeight].
	TArray<FVector2D> SolidOutline;
	if (bHasDoor)
	{
		const float DoorTop = FloorZ + DoorHeight;
		// Build solid outline: arc above door + side pillars.
		// Left pillar bottom.
		SolidOutline.Add(FVector2D(-DoorHalfWidth, FloorZ));
		SolidOutline.Add(FVector2D(-DoorHalfWidth, DoorTop));
		// Arc points above door top.
		for (const FVector2D& Pt : OutlinePoints)
		{
			if (Pt.Y > DoorTop || FMath::Abs(Pt.X) > DoorHalfWidth)
			{
				SolidOutline.Add(Pt);
			}
		}
		// Right pillar.
		SolidOutline.Add(FVector2D(DoorHalfWidth, DoorTop));
		SolidOutline.Add(FVector2D(DoorHalfWidth, FloorZ));
	}
	else
	{
		SolidOutline = OutlinePoints;
	}

	if (SolidOutline.Num() < 3)
	{
		return false;
	}

	// Remove duplicate outline points to avoid zero-area fan or side triangles.
	TArray<FVector2D> SanitizedOutline;
	SanitizedOutline.Reserve(SolidOutline.Num());
	for (const FVector2D& Pt : SolidOutline)
	{
		if (SanitizedOutline.Num() == 0 || !SanitizedOutline.Last().Equals(Pt, 0.1f))
		{
			SanitizedOutline.Add(Pt);
		}
	}
	if (SanitizedOutline.Num() > 1 && SanitizedOutline[0].Equals(SanitizedOutline.Last(), 0.1f))
	{
		SanitizedOutline.Pop();
	}
	SolidOutline = MoveTemp(SanitizedOutline);

	if (SolidOutline.Num() < 3)
	{
		return false;
	}

	auto EmitDiscFace = [&](const TArray<FVector2D>& Points, float XPos, const FVector& FaceNormal)
	{
		// Use a specific pivot for fan triangulation to correctly handle door cutout concavity.
		// If door is present, pivot is the lintel center (0, DoorTop), otherwise use hull center (0, 0).
		const float DoorTop = FloorZ + DoorHeight;
		FVector2D PivotPt(0.f, 0.f);
		if (bHasDoor)
		{
			PivotPt = FVector2D(0.f, FMath::Min(HalfH - 1.f, DoorTop + 1.f));
		}

		const int32 PivotIdx = OutMeshData.PanelSection.Vertices.Num();
		OutMeshData.PanelSection.Vertices.Add(FVector(XPos, PivotPt.X, PivotPt.Y));
		OutMeshData.PanelSection.Normals.Add(FaceNormal);
		OutMeshData.PanelSection.UVs.Add(FVector2D(0.5f, 0.5f));
		OutMeshData.PanelSection.Tangents.Add(FProcMeshTangent(0.f, 1.f, 0.f));

		for (const FVector2D& Pt : Points)
		{
			OutMeshData.PanelSection.Vertices.Add(FVector(XPos, Pt.X, Pt.Y));
			OutMeshData.PanelSection.Normals.Add(FaceNormal);
			OutMeshData.PanelSection.UVs.Add(FVector2D(
				(Pt.X / FMath::Max(1.f, HalfW) + 1.f) * 0.5f,
				(Pt.Y / FMath::Max(1.f, HalfH) + 1.f) * 0.5f));
			OutMeshData.PanelSection.Tangents.Add(FProcMeshTangent(0.f, 1.f, 0.f));
		}

		for (int32 i = 0; i < Points.Num(); ++i)
		{
			const int32 NextI = (i + 1) % Points.Num();
			// Skip triangulation across the door base gap (last segment connects floor corners).
			if (bHasDoor && i == Points.Num() - 1) continue;
			if (Points[i].Equals(Points[NextI], 0.1f))
			{
				continue;
			}

			if (FaceNormal.X < 0.f)
			{
				AppendTriangle(OutMeshData.PanelSection.Triangles, PivotIdx, PivotIdx + 1 + NextI, PivotIdx + 1 + i);
			}
			else
			{
				AppendTriangle(OutMeshData.PanelSection.Triangles, PivotIdx, PivotIdx + 1 + i, PivotIdx + 1 + NextI);
			}
		}
	};

	const float ForeX = Bulkhead.SpinePositionCm - HalfThicknessCm;
	const float AftX = Bulkhead.SpinePositionCm + HalfThicknessCm;

	OutMeshData.PanelSection.VertexStart = 0;
	OutMeshData.PanelSection.TriangleStart = 0;

	EmitDiscFace(SolidOutline, ForeX, -FVector::ForwardVector);
	EmitDiscFace(SolidOutline, AftX, FVector::ForwardVector);

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

		// Edge direction in YZ, outward normal in YZ plane.
		FVector2D EdgeDir = P1 - P0;
		FVector EdgeNormal = FVector(0.f, -EdgeDir.Y, EdgeDir.X).GetSafeNormal();
		
		// Tangent for side faces follows the longitudinal direction X.
		FVector EdgeTangent = FVector::ForwardVector;

		AppendQuad(
			OutMeshData.PanelSection,
			FVector(ForeX, P0.X, P0.Y),
			FVector(AftX, P0.X, P0.Y),
			FVector(ForeX, P1.X, P1.Y),
			FVector(AftX, P1.X, P1.Y),
			EdgeNormal,
			EdgeTangent);
	}

	OutMeshData.PanelSection.VertexCount = OutMeshData.PanelSection.Vertices.Num();
	OutMeshData.PanelSection.TriangleCount = OutMeshData.PanelSection.Triangles.Num();

	return OutMeshData.PanelSection.Vertices.Num() > 0
		&& OutMeshData.PanelSection.Triangles.Num() > 0;
}
