#include "SubmarineGeometryBuilder.h"

#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "SubmarineEnvelopeDef.h"

namespace
{
constexpr float ExteriorHullOffsetCm = 5.f;

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
	const FVector& Normal)
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

	AppendQuad(Section, P100, P110, P101, P111, FVector::ForwardVector);
	AppendQuad(Section, P010, P000, P011, P001, -FVector::ForwardVector);
	AppendQuad(Section, P000, P100, P001, P101, -FVector::RightVector);
	AppendQuad(Section, P110, P010, P111, P011, FVector::RightVector);
	AppendQuad(Section, P001, P101, P011, P111, FVector::UpVector);
	AppendQuad(Section, P000, P010, P100, P110, -FVector::UpVector);
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

void AppendExteriorRing(
	FSubmarineMeshSectionData& Section,
	const FExteriorRing& Ring,
	int32 RadialSegments,
	float SectionExponent = 2.f,
	float WidthToHeightRatio = 1.f)
{
	const float Exp = 2.f / FMath::Max(0.5f, SectionExponent);
	const float HalfW = Ring.Radius * FMath::Max(0.5f, WidthToHeightRatio);
	const float HalfH = Ring.Radius;

	for (int32 SegmentIndex = 0; SegmentIndex < RadialSegments; ++SegmentIndex)
	{
		const float T = static_cast<float>(SegmentIndex) / static_cast<float>(RadialSegments);
		const float Angle = T * PI * 2.f;
		const float CosA = FMath::Cos(Angle);
		const float SinA = FMath::Sin(Angle);

		const float Y = HalfW * SuperellipsePow(CosA, Exp);
		const float Z = HalfH * SuperellipsePow(SinA, Exp);
		const FVector OutwardNormal = FVector(0.f, Y, Z).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0.f, CosA, SinA));

		Section.Vertices.Add(FVector(Ring.X, Y, Z));
		Section.Normals.Add(OutwardNormal);
		Section.UVs.Add(FVector2D(T, Ring.X * 0.01f));
	}
}
}

bool USubmarineGeometryBuilder::GenerateInteriorMeshData(
	const FSubmarineLayoutSolution& Solution,
	TArray<FSubmarineInteriorCompartmentMeshData>& OutMeshData,
	float SectionExponent,
	float WidthToHeightRatio) const
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
	UMaterialInterface* MaterialOverride,
	bool bEnableCollision,
	float SectionExponent,
	float WidthToHeightRatio) const
{
	TArray<UProceduralMeshComponent*> BuiltMeshes;

	if (!ParentActor || !ParentActor->GetRootComponent())
	{
		return BuiltMeshes;
	}

	TArray<FSubmarineInteriorCompartmentMeshData> MeshDataSet;
	if (!GenerateInteriorMeshData(Solution, MeshDataSet, SectionExponent, WidthToHeightRatio))
	{
		return BuiltMeshes;
	}

	TArray<FSubmarineBulkheadMeshData> BulkheadMeshDataSet;
	if (!GenerateBulkheadMeshData(Solution, BulkheadMeshDataSet))
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
				TArray<FProcMeshTangent>(),
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
					TArray<FProcMeshTangent>(),
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
					TArray<FProcMeshTangent>(),
					bEnableCollision);
			}

			if (MaterialOverride)
			{
				VisPMC->SetMaterial(0, MaterialOverride);
				if (MeshData.BowCapSection.Vertices.Num() > 0)
				{
					VisPMC->SetMaterial(2, MaterialOverride);
				}
				if (MeshData.SternCapSection.Vertices.Num() > 0)
				{
					VisPMC->SetMaterial(3, MaterialOverride);
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
				1,
				MeshData.FloorSection.Vertices,
				MeshData.FloorSection.Triangles,
				MeshData.FloorSection.Normals,
				MeshData.FloorSection.UVs,
				TArray<FColor>(),
				TArray<FProcMeshTangent>(),
				bEnableCollision);

			if (MaterialOverride)
			{
				WalkPMC->SetMaterial(1, MaterialOverride);
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
		PMC->SetCollisionProfileName(TEXT("SubInteriorVisual"));
		PMC->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

		PMC->CreateMeshSection(
			0,
			MeshData.PanelSection.Vertices,
			MeshData.PanelSection.Triangles,
			MeshData.PanelSection.Normals,
			MeshData.PanelSection.UVs,
			TArray<FColor>(),
			TArray<FProcMeshTangent>(),
			bEnableCollision);

		if (MaterialOverride)
		{
			PMC->SetMaterial(0, MaterialOverride);
		}

		BuiltMeshes.Add(PMC);
	}

	return BuiltMeshes;
}

bool USubmarineGeometryBuilder::GenerateBulkheadMeshData(
	const FSubmarineLayoutSolution& Solution,
	TArray<FSubmarineBulkheadMeshData>& OutMeshData) const
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
		if (!GenerateSingleBulkheadMeshData(Solution, Bulkhead, MeshData))
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

	TArray<FExteriorRing> KnotRings;
	KnotRings.Reserve(Solution.Compartments.Num() + 1);

	const FCompartmentPlacement& FirstCompartment = Solution.Compartments[0];
	FExteriorRing FirstRing;
	FirstRing.X = FirstCompartment.SpineStartCm;
	FirstRing.Radius = FMath::Max(1.f, FirstCompartment.EffectiveRadiusCm + ExteriorHullOffsetCm);
	KnotRings.Add(FirstRing);
	for (const FCompartmentPlacement& Compartment : Solution.Compartments)
	{
		FExteriorRing Ring;
		Ring.X = Compartment.SpineEndCm;
		Ring.Radius = FMath::Max(1.f, Compartment.EffectiveRadiusCm + ExteriorHullOffsetCm);
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

	OutMeshData.Vertices.Reserve(Rings.Num() * RadialSegments + 2);
	OutMeshData.Normals.Reserve(Rings.Num() * RadialSegments + 2);
	OutMeshData.UVs.Reserve(Rings.Num() * RadialSegments + 2);
	OutMeshData.Triangles.Reserve((Rings.Num() - 1) * RadialSegments * 6 + RadialSegments * 6);

	for (const FExteriorRing& Ring : Rings)
	{
		AppendExteriorRing(OutMeshData, Ring, EffectiveRadialSegments, SectionExponent, WidthToHeightRatio);
	}

	for (int32 RingIndex = 0; RingIndex + 1 < Rings.Num(); ++RingIndex)
	{
		const int32 RingBase = RingIndex * EffectiveRadialSegments;
		const int32 NextRingBase = (RingIndex + 1) * EffectiveRadialSegments;

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

	const int32 BowCenterIndex = OutMeshData.Vertices.Num();
	OutMeshData.Vertices.Add(FVector(Rings[0].X, 0.f, 0.f));
	OutMeshData.Normals.Add(-FVector::ForwardVector);
	OutMeshData.UVs.Add(FVector2D(0.5f, 0.5f));

	for (int32 SegmentIndex = 0; SegmentIndex < EffectiveRadialSegments; ++SegmentIndex)
	{
		const int32 NextSegment = (SegmentIndex + 1) % EffectiveRadialSegments;
		AppendTriangle(OutMeshData.Triangles, BowCenterIndex, NextSegment, SegmentIndex);
	}

	const int32 SternCenterIndex = OutMeshData.Vertices.Num();
	const int32 SternRingBase = (Rings.Num() - 1) * EffectiveRadialSegments;
	OutMeshData.Vertices.Add(FVector(Rings.Last().X, 0.f, 0.f));
	OutMeshData.Normals.Add(FVector::ForwardVector);
	OutMeshData.UVs.Add(FVector2D(0.5f, 0.5f));

	for (int32 SegmentIndex = 0; SegmentIndex < EffectiveRadialSegments; ++SegmentIndex)
	{
		const int32 NextSegment = (SegmentIndex + 1) % EffectiveRadialSegments;
		AppendTriangle(OutMeshData.Triangles, SternCenterIndex, SternRingBase + SegmentIndex, SternRingBase + NextSegment);
	}

	return OutMeshData.Vertices.Num() > 0 && OutMeshData.Triangles.Num() > 0;
}

UProceduralMeshComponent* USubmarineGeometryBuilder::BuildExteriorMesh(
	const FSubmarineLayoutSolution& Solution,
	AActor* ParentActor,
	UMaterialInterface* MaterialOverride,
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
		TArray<FProcMeshTangent>(),
		bEnableCollision);

	if (MaterialOverride)
	{
		PMC->SetMaterial(0, MaterialOverride);
	}

	return PMC;
}

bool USubmarineGeometryBuilder::GenerateCompartmentInteriorMeshData(
	const FCompartmentPlacement& Placement,
	bool bGenerateBowCap,
	bool bGenerateSternCap,
	float SectionExponent,
	float WidthToHeightRatio,
	FSubmarineInteriorCompartmentMeshData& OutMeshData) const
{
	const float RadiusCm = FMath::Max(1.f, Placement.EffectiveRadiusCm);
	const float HalfH = RadiusCm;
	const float HalfW = RadiusCm * FMath::Max(0.5f, WidthToHeightRatio);
	const float Exp = 2.f / FMath::Max(0.5f, SectionExponent);
	const float N = FMath::Max(0.5f, SectionExponent);
	const float FloorZ = FMath::Clamp(Placement.FloorOffsetCm, -HalfH * 0.98f, HalfH * 0.98f);
	const float LengthCm = FMath::Max(1.f, Placement.SpineEndCm - Placement.SpineStartCm);

	const float AbsFloorRatio = FMath::Abs(FloorZ) / HalfH;
	const float FloorHalfWidth = (AbsFloorRatio >= 1.f) ? 0.f
		: HalfW * FMath::Pow(FMath::Max(0.f, 1.f - FMath::Pow(AbsFloorRatio, N)), 1.f / N);
	const int32 ArcSegments = 20;

	if (FloorHalfWidth <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float FloorRatio = FMath::Clamp(FloorZ / HalfH, -1.f, 1.f);
	const float InvExp = 1.f / FMath::Max(KINDA_SMALL_NUMBER, Exp);
	const float MappedSin = FMath::Clamp(SuperellipsePow(FloorRatio, InvExp), -1.f, 1.f);
	const float EndTheta = FMath::Asin(MappedSin);
	const float StartTheta = PI - EndTheta;

	TArray<FProfilePoint> ProfilePoints;
	ProfilePoints.Reserve(ArcSegments + 1);

	for (int32 SegmentIndex = 0; SegmentIndex <= ArcSegments; ++SegmentIndex)
	{
		const float T = static_cast<float>(SegmentIndex) / static_cast<float>(ArcSegments);
		const float Theta = FMath::Lerp(StartTheta, EndTheta, T);
		const float CosA = FMath::Cos(Theta);
		const float SinA = FMath::Sin(Theta);
		const float Y = HalfW * SuperellipsePow(CosA, Exp);
		const float Z = HalfH * SuperellipsePow(SinA, Exp);
		const FVector Position(0.f, Y, Z);
		const FVector InwardNormal = FVector(0.f, -Y, -Z).GetSafeNormal();

		FProfilePoint Point;
		Point.Position = Position;
		Point.InwardNormal = InwardNormal;
		Point.U = T;
		ProfilePoints.Add(Point);
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
		const float X = (RingIndex == 0) ? Placement.SpineStartCm : Placement.SpineEndCm;
		const float V = static_cast<float>(RingIndex);

		for (const FProfilePoint& Point : ProfilePoints)
		{
			OutMeshData.WallSection.Vertices.Add(FVector(X, Point.Position.Y, Point.Position.Z));
			OutMeshData.WallSection.Normals.Add(Point.InwardNormal);
			OutMeshData.WallSection.UVs.Add(FVector2D(Point.U, V));
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

	OutMeshData.FloorSection.Vertices = {
		FVector(Placement.SpineStartCm, -FloorHalfWidth, FloorZ),
		FVector(Placement.SpineStartCm, FloorHalfWidth, FloorZ),
		FVector(Placement.SpineEndCm, -FloorHalfWidth, FloorZ),
		FVector(Placement.SpineEndCm, FloorHalfWidth, FloorZ)
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
	AppendTriangle(OutMeshData.FloorSection.Triangles, 0, 1, 2);
	AppendTriangle(OutMeshData.FloorSection.Triangles, 1, 3, 2);

	const float CapHalfThicknessCm = 4.f;
	if (bGenerateBowCap)
	{
		AppendBoxPrism(
			OutMeshData.BowCapSection,
			FVector(Placement.SpineStartCm - CapHalfThicknessCm, -FloorHalfWidth, FloorZ),
			FVector(Placement.SpineStartCm + CapHalfThicknessCm, FloorHalfWidth, RadiusCm));
	}

	if (bGenerateSternCap)
	{
		AppendBoxPrism(
			OutMeshData.SternCapSection,
			FVector(Placement.SpineEndCm - CapHalfThicknessCm, -FloorHalfWidth, FloorZ),
			FVector(Placement.SpineEndCm + CapHalfThicknessCm, FloorHalfWidth, RadiusCm));
	}

	return OutMeshData.WallSection.Vertices.Num() > 0
		&& OutMeshData.WallSection.Triangles.Num() > 0
		&& OutMeshData.FloorSection.Vertices.Num() == 4
		&& OutMeshData.FloorSection.Triangles.Num() == 12
		&& (!bGenerateBowCap || (OutMeshData.BowCapSection.Vertices.Num() > 0 && OutMeshData.BowCapSection.Triangles.Num() > 0))
		&& (!bGenerateSternCap || (OutMeshData.SternCapSection.Vertices.Num() > 0 && OutMeshData.SternCapSection.Triangles.Num() > 0));
}

bool USubmarineGeometryBuilder::GenerateSingleBulkheadMeshData(
	const FSubmarineLayoutSolution& Solution,
	const FBulkheadPlacement& Bulkhead,
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
	OutMeshData.BulkheadId = FName(*FString::Printf(
		TEXT("Bulkhead_%s_%s"),
		*Bulkhead.ForeCompartmentId.ToString(),
		*Bulkhead.AftCompartmentId.ToString()));

	const float RadiusCm = FMath::Max(1.f, Bulkhead.RadiusCm);
	const float FloorZ = FMath::Max(ForeCompartment->FloorOffsetCm, AftCompartment->FloorOffsetCm);
	const float CeilingZ = RadiusCm;
	const float HalfThicknessCm = 4.f;
	const float XMin = Bulkhead.SpinePositionCm - HalfThicknessCm;
	const float XMax = Bulkhead.SpinePositionCm + HalfThicknessCm;

	if (Bulkhead.PassageType == EPassageType::SealedBulkhead
		|| Bulkhead.DoorWidthCm <= KINDA_SMALL_NUMBER
		|| Bulkhead.DoorHeightCm <= KINDA_SMALL_NUMBER)
	{
		AppendBoxPrism(
			OutMeshData.PanelSection,
			FVector(XMin, -RadiusCm, FloorZ),
			FVector(XMax, RadiusCm, CeilingZ));
		return OutMeshData.PanelSection.Vertices.Num() > 0
			&& OutMeshData.PanelSection.Triangles.Num() > 0;
	}

	const float DoorCenterY = Bulkhead.DoorOffsetCm.X;
	const float DoorCenterZ = Bulkhead.DoorOffsetCm.Y;
	const float DoorHalfWidthCm = Bulkhead.DoorWidthCm * 0.5f;
	const float DoorHalfHeightCm = Bulkhead.DoorHeightCm * 0.5f;
	const float DoorMinY = FMath::Clamp(DoorCenterY - DoorHalfWidthCm, -RadiusCm, RadiusCm);
	const float DoorMaxY = FMath::Clamp(DoorCenterY + DoorHalfWidthCm, -RadiusCm, RadiusCm);
	const float DoorBottomZ = FMath::Clamp(DoorCenterZ - DoorHalfHeightCm, FloorZ, CeilingZ);
	const float DoorTopZ = FMath::Clamp(DoorCenterZ + DoorHalfHeightCm, FloorZ, CeilingZ);

	AppendBoxPrism(
		OutMeshData.PanelSection,
		FVector(XMin, -RadiusCm, FloorZ),
		FVector(XMax, DoorMinY, CeilingZ));

	AppendBoxPrism(
		OutMeshData.PanelSection,
		FVector(XMin, DoorMaxY, FloorZ),
		FVector(XMax, RadiusCm, CeilingZ));

	AppendBoxPrism(
		OutMeshData.PanelSection,
		FVector(XMin, DoorMinY, DoorTopZ),
		FVector(XMax, DoorMaxY, CeilingZ));

	return OutMeshData.PanelSection.Vertices.Num() > 0
		&& OutMeshData.PanelSection.Triangles.Num() > 0
		&& DoorBottomZ >= FloorZ - KINDA_SMALL_NUMBER;
}
