#include "SubmarineGeneratedGeometryComponent.h"

#include "Engine/CollisionProfile.h"
#include "ProceduralMeshComponent.h"
#include "Generator/SubmarineDefinition.h"

USubmarineGeneratedGeometryComponent::USubmarineGeneratedGeometryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ── Public API ───────────────────────────────────────────────────────────────

bool USubmarineGeneratedGeometryComponent::BuildFromDefinition(const USubmarineDefinition* Definition)
{
	ClearGeometry();

	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] BuildFromDefinition: null Definition"), *GetOwner()->GetName());
		return false;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[%s] [ShapeStep1] BuildFromDefinition toggles: ExteriorHull=%d Interior=%d Bulkheads=%d Airlock=%d"),
		*GetOwner()->GetName(),
		bBuildExteriorHull ? 1 : 0,
		bBuildInterior ? 1 : 0,
		bBuildBulkheads ? 1 : 0,
		bBuildAirlock ? 1 : 0);

	bool bAnyBuilt = false;
	if (bBuildExteriorHull)
	{
		bAnyBuilt |= BuildExteriorHull(Definition);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] [ShapeStep1] ExteriorHull skipped (toggle off)"), *GetOwner()->GetName());
	}

	bAnyBuilt |= BuildInteriorCompartments(Definition);

	if (bBuildBulkheads)
	{
		bAnyBuilt |= BuildBulkheads(Definition);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] [ShapeStep1] Bulkheads skipped (toggle off)"), *GetOwner()->GetName());
	}

	UE_LOG(LogTemp, Log, TEXT("[%s] BuildFromDefinition: %d render + %d collision components created"),
		*GetOwner()->GetName(), RenderComponents.Num(), CollisionComponents.Num());

	return bAnyBuilt;
}

void USubmarineGeneratedGeometryComponent::ClearGeometry()
{
	for (UProceduralMeshComponent* PMC : RenderComponents)
	{
		if (IsValid(PMC))
		{
			PMC->DestroyComponent();
		}
	}
	RenderComponents.Reset();

	for (UProceduralMeshComponent* PMC : CollisionComponents)
	{
		if (IsValid(PMC))
		{
			PMC->DestroyComponent();
		}
	}
	CollisionComponents.Reset();
	ExteriorHullCollisionComponents.Reset();
	InteriorFloorCollisionComponents.Reset();
}

// ── Exterior Hull ────────────────────────────────────────────────────────────

bool USubmarineGeneratedGeometryComponent::BuildExteriorHull(const USubmarineDefinition* Definition)
{
	const FSubmarineMeshSectionData& Mesh = Definition->ExteriorHullMesh;
	if (Mesh.Vertices.Num() == 0 || Mesh.Triangles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] BuildExteriorHull: empty mesh data"), *GetOwner()->GetName());
		return false;
	}

	// Render component — visible, no collision.
	UProceduralMeshComponent* RenderPMC = CreatePMC(
		TEXT("ExteriorHull_Render"), true, false, TEXT("GeneratedRender"));
	if (RenderPMC)
	{
		ApplySectionToPMC(RenderPMC, Mesh, false);
		if (ExteriorHullMaterial)
		{
			RenderPMC->SetMaterial(0, ExteriorHullMaterial);
		}
		RenderComponents.Add(RenderPMC);
	}

	// Collision — convex decomposition by longitudinal slices.
	// Split vertices into N slices along X axis, create one collision PMC per slice
	// with an actual convex hull (AddCollisionConvexMesh) rather than a tri-mesh.
	const int32 SliceCount = FMath::Clamp(HullCollisionSlices, 2, 16);

	// Find X bounds.
	float MinX = TNumericLimits<float>::Max();
	float MaxX = TNumericLimits<float>::Lowest();
	for (const FVector& V : Mesh.Vertices)
	{
		MinX = FMath::Min(MinX, static_cast<float>(V.X));
		MaxX = FMath::Max(MaxX, static_cast<float>(V.X));
	}

	const float SliceLength = (MaxX - MinX) / static_cast<float>(SliceCount);
	if (SliceLength < 1.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] BuildExteriorHull: hull too short for collision slices (%.1f cm)"),
			*GetOwner()->GetName(), MaxX - MinX);
		return RenderPMC != nullptr;
	}

	int32 CollisionCount = 0;
	for (int32 SliceIndex = 0; SliceIndex < SliceCount; ++SliceIndex)
	{
		const float SliceMinX = MinX + SliceIndex * SliceLength;
		const float SliceMaxX = (SliceIndex == SliceCount - 1) ? MaxX : SliceMinX + SliceLength;

		// Collect unique vertices whose X falls within this slice.
		TArray<FVector> SliceVertices;
		for (const FVector& V : Mesh.Vertices)
		{
			if (V.X >= SliceMinX && V.X <= SliceMaxX)
			{
				SliceVertices.Add(V);
			}
		}

		if (SliceVertices.Num() < 4)
		{
			continue;
		}

		const FName SliceName = FName(*FString::Printf(TEXT("ExteriorHull_Collision_%d"), SliceIndex));
		UProceduralMeshComponent* CollisionPMC = CreatePMC(
			SliceName, false, true, TEXT("GeneratedCollision"));
		if (!CollisionPMC)
		{
			continue;
		}

		CollisionPMC->SetCollisionProfileName(FName(TEXT("SubmarineHull")));
		CollisionPMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CollisionPMC->SetNotifyRigidBodyCollision(true);
		CollisionPMC->SetGenerateOverlapEvents(false);
		CollisionPMC->SetCanEverAffectNavigation(false);
		CollisionPMC->AddCollisionConvexMesh(SliceVertices);

		CollisionComponents.Add(CollisionPMC);
		ExteriorHullCollisionComponents.Add(CollisionPMC);
		++CollisionCount;
	}

	UE_LOG(LogTemp, Log, TEXT("[%s] BuildExteriorHull: 1 render + %d convex collision slices"),
		*GetOwner()->GetName(), CollisionCount);

	return true;
}

// ── Interior Compartments ────────────────────────────────────────────────────

bool USubmarineGeneratedGeometryComponent::BuildInteriorCompartments(const USubmarineDefinition* Definition)
{
	if (Definition->InteriorMeshes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] BuildInteriorCompartments: no interior meshes"), *GetOwner()->GetName());
		return false;
	}

	if (!bBuildInterior && !bBuildAirlock)
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] [ShapeStep1] Interior and Airlock both skipped (toggles off)"), *GetOwner()->GetName());
		return false;
	}

	int32 BuiltCount = 0;
	int32 SkippedInterior = 0;
	int32 SkippedAirlock = 0;

	for (int32 Idx = 0; Idx < Definition->InteriorMeshes.Num(); ++Idx)
	{
		const FSubmarineInteriorCompartmentMeshData& Interior = Definition->InteriorMeshes[Idx];
		const FString IdStr = Interior.CompartmentId.IsNone()
			? FString::Printf(TEXT("Comp_%d"), Idx)
			: Interior.CompartmentId.ToString();

		const FGeneratedCompartmentDef* Comp = Definition->FindCompartment(Interior.CompartmentId);
		const bool bIsAirlock = Comp && Comp->SemanticType == ESubCompartmentType::Airlock;

		if (bIsAirlock && !bBuildAirlock)
		{
			++SkippedAirlock;
			continue;
		}
		if (!bIsAirlock && !bBuildInterior)
		{
			++SkippedInterior;
			continue;
		}

		auto BuildRenderSection = [&](const FSubmarineMeshSectionData& Section, const TCHAR* Suffix)
		{
			if (Section.Vertices.Num() == 0 || Section.Triangles.Num() == 0)
			{
				return;
			}

			const FName Name = FName(*FString::Printf(TEXT("%s_%s_Render"), *IdStr, Suffix));
			UProceduralMeshComponent* PMC = CreatePMC(Name, true, false, TEXT("GeneratedRender"));
			if (!PMC)
			{
				return;
			}

			if (!Interior.CompartmentId.IsNone())
			{
				PMC->ComponentTags.Add(FName(*FString::Printf(TEXT("CompartmentId=%s"), *Interior.CompartmentId.ToString())));
			}

			ApplySectionToPMC(PMC, Section, false);
			if (InteriorMaterial)
			{
				PMC->SetMaterial(0, InteriorMaterial);
			}
			RenderComponents.Add(PMC);
			++BuiltCount;
		};

		BuildRenderSection(Interior.WallSection, TEXT("Walls"));
		BuildRenderSection(Interior.FloorSection, TEXT("Floor"));
		BuildRenderSection(Interior.BowCapSection, TEXT("BowCap"));
		BuildRenderSection(Interior.SternCapSection, TEXT("SternCap"));

		auto BuildInteriorCollisionSection = [&](const FSubmarineMeshSectionData& Section, const TCHAR* Suffix, bool bTrackAsFloor)
		{
			if (Section.Vertices.Num() == 0 || Section.Triangles.Num() == 0)
			{
				return;
			}

			const FName CollisionName = FName(*FString::Printf(TEXT("%s_%s_Collision"), *IdStr, Suffix));
			UProceduralMeshComponent* Collision = CreatePMC(
				CollisionName, false, true, TEXT("GeneratedCollision"));
			if (!Collision)
			{
				return;
			}

			if (!Interior.CompartmentId.IsNone())
			{
				Collision->ComponentTags.Add(FName(*FString::Printf(TEXT("CompartmentId=%s"), *Interior.CompartmentId.ToString())));
			}
			ApplySectionToPMC(Collision, Section, true);
			Collision->SetCollisionProfileName(FName(TEXT("SubInteriorWalkable")));
			CollisionComponents.Add(Collision);
			if (bTrackAsFloor)
			{
				InteriorFloorCollisionComponents.Add(Collision);
			}
		};

		// Floor collision — walkable surface, tracked so crew floor-snap traces
		// (ECC_GameTraceChannel2 / SubInterior) find it.
		BuildInteriorCollisionSection(Interior.FloorSection, TEXT("Floor"), true);
		// Walls + end caps — block the crew capsule so they can't pass through
		// the hull from inside. Not tracked as walkable; crew CMC reads only the
		// floor set for its walkable list.
		BuildInteriorCollisionSection(Interior.WallSection, TEXT("Walls"), false);
		BuildInteriorCollisionSection(Interior.BowCapSection, TEXT("BowCap"), false);
		BuildInteriorCollisionSection(Interior.SternCapSection, TEXT("SternCap"), false);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[%s] BuildInteriorCompartments: %d sections built from %d compartments (skipped interior=%d, airlock=%d)"),
		*GetOwner()->GetName(), BuiltCount, Definition->InteriorMeshes.Num(), SkippedInterior, SkippedAirlock);

	return BuiltCount > 0;
}

// ── Bulkheads ────────────────────────────────────────────────────────────────

bool USubmarineGeneratedGeometryComponent::BuildBulkheads(const USubmarineDefinition* Definition)
{
	if (Definition->BulkheadMeshes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] BuildBulkheads: no bulkhead meshes"), *GetOwner()->GetName());
		return false;
	}

	int32 BuiltCount = 0;

	for (int32 Idx = 0; Idx < Definition->BulkheadMeshes.Num(); ++Idx)
	{
		const FSubmarineBulkheadMeshData& Bulkhead = Definition->BulkheadMeshes[Idx];
		if (Bulkhead.PanelSection.Vertices.Num() == 0 || Bulkhead.PanelSection.Triangles.Num() == 0)
		{
			continue;
		}

		const FString IdStr = Bulkhead.BulkheadId.IsNone()
			? FString::Printf(TEXT("Bulkhead_%d"), Idx)
			: Bulkhead.BulkheadId.ToString();

		// Render.
		{
			const FName RenderName = FName(*FString::Printf(TEXT("%s_Render"), *IdStr));
			UProceduralMeshComponent* PMC = CreatePMC(RenderName, true, false, TEXT("GeneratedRender"));
			if (PMC)
			{
				ApplySectionToPMC(PMC, Bulkhead.PanelSection, false);
				if (BulkheadMaterial)
				{
					PMC->SetMaterial(0, BulkheadMaterial);
				}
				RenderComponents.Add(PMC);
				++BuiltCount;
			}
		}

		// Collision.
		{
			const FName CollisionName = FName(*FString::Printf(TEXT("%s_Collision"), *IdStr));
			UProceduralMeshComponent* PMC = CreatePMC(CollisionName, false, true, TEXT("GeneratedCollision"));
			if (PMC)
			{
				ApplySectionToPMC(PMC, Bulkhead.PanelSection, true);
				CollisionComponents.Add(PMC);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[%s] BuildBulkheads: %d bulkheads built from %d definitions"),
		*GetOwner()->GetName(), BuiltCount, Definition->BulkheadMeshes.Num());

	return BuiltCount > 0;
}

// ── Helpers ──────────────────────────────────────────────────────────────────

UProceduralMeshComponent* USubmarineGeneratedGeometryComponent::CreatePMC(
	FName ComponentName,
	bool bVisible,
	bool bCollision,
	FName RoleTag)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(Owner, ComponentName);
	if (!PMC)
	{
		return nullptr;
	}

	PMC->CreationMethod = EComponentCreationMethod::Instance;
	Owner->AddInstanceComponent(PMC);

	if (USceneComponent* Root = Owner->GetRootComponent())
	{
		PMC->SetupAttachment(Root);
	}

	PMC->SetMobility(EComponentMobility::Movable);
	PMC->SetVisibility(bVisible);
	PMC->SetHiddenInGame(!bVisible);

	if (bCollision)
	{
		PMC->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		PMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		PMC->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		PMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	PMC->ComponentTags.Add(RoleTag);
	PMC->SetCanEverAffectNavigation(false);

	if (Owner->GetWorld())
	{
		PMC->RegisterComponent();
	}

	return PMC;
}

void USubmarineGeneratedGeometryComponent::ApplySectionToPMC(
	UProceduralMeshComponent* PMC,
	const FSubmarineMeshSectionData& Section,
	bool bEnableCollision)
{
	if (!PMC || Section.Vertices.Num() == 0 || Section.Triangles.Num() == 0)
	{
		return;
	}

	TArray<FLinearColor> Colors;
	Colors.Init(FLinearColor::White, Section.Vertices.Num());

	PMC->CreateMeshSection_LinearColor(
		0,
		Section.Vertices,
		Section.Triangles,
		Section.Normals,
		Section.UVs,
		Colors,
		Section.Tangents,
		bEnableCollision);

	if (bEnableCollision)
	{
		PMC->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		PMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}
