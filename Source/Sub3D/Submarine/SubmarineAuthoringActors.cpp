#include "SubmarineAuthoringActors.h"

#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneComponent.h"
#include "SubHullComponent.h"
#include "SubmarineCompartmentComponent.h"
#include "SubmarineAuthoringPipeline.h"
#include "SubmarineLayoutAsset.h"

namespace
{
enum class EPreviewSectionKind : uint8
{
	ExteriorHull,
	InteriorHull,
	Deck,
	Ramp,
	Bulkhead,
	DoorFrame,
	DoorLeaf,
	Other
};

void ApplyCompiledSectionToProceduralMesh(
	UProceduralMeshComponent* ProceduralMesh,
	const FCompiledSubmarineMeshSection& Section,
	const TArray<FCompiledSubmarineMaterialSlot>& MaterialSlots,
	const bool bEnableCollision,
	const FName CollisionProfileName)
{
	if (!ProceduralMesh)
	{
		return;
	}

	TArray<FVector> Positions;
	Positions.Reserve(Section.Positions.Num());
	for (const FVector3f& Position : Section.Positions)
	{
		Positions.Add(FVector(Position));
	}

	TArray<FVector> Normals;
	Normals.Reserve(Section.Normals.Num());
	for (const FVector3f& Normal : Section.Normals)
	{
		Normals.Add(FVector(Normal));
	}

	TArray<FProcMeshTangent> Tangents;
	Tangents.Reserve(Section.Tangents.Num());
	for (const FVector4f& Tangent : Section.Tangents)
	{
		Tangents.Emplace(FVector(Tangent.X, Tangent.Y, Tangent.Z), false);
	}

	TArray<FVector2D> UV0;
	UV0.Reserve(Section.UV0.Num());
	for (const FVector2f& UV : Section.UV0)
	{
		UV0.Add(FVector2D(UV));
	}

	TArray<FLinearColor> Colors;
	Colors.Init(FLinearColor::White, Positions.Num());

	ProceduralMesh->CreateMeshSection_LinearColor(0, Positions, Section.Indices, Normals, UV0, Colors, Tangents, bEnableCollision);
	ProceduralMesh->bUseComplexAsSimpleCollision = bEnableCollision;
	ProceduralMesh->SetCollisionProfileName(CollisionProfileName);
	ProceduralMesh->SetCollisionEnabled(
		!bEnableCollision ? ECollisionEnabled::NoCollision
		: (CollisionProfileName == TEXT("SubmarineHull") ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::QueryOnly));
	ProceduralMesh->SetCanEverAffectNavigation(false);

	if (MaterialSlots.IsValidIndex(Section.MaterialSlotIndex) && !MaterialSlots[Section.MaterialSlotIndex].Material.IsNull())
	{
		if (UMaterialInterface* Material = MaterialSlots[Section.MaterialSlotIndex].Material.LoadSynchronous())
		{
			ProceduralMesh->SetMaterial(0, Material);
		}
	}
}

void DestroyProceduralMeshes(TArray<TObjectPtr<UProceduralMeshComponent>>& Components)
{
	for (UProceduralMeshComponent* Component : Components)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}

	Components.Reset();
}

EPreviewSectionKind ClassifyPreviewSection(const FName SectionId)
{
	if (SectionId == TEXT("ExteriorHull") || SectionId == TEXT("HullClosure_Fore") || SectionId == TEXT("HullClosure_Aft"))
	{
		return EPreviewSectionKind::ExteriorHull;
	}

	if (SectionId == TEXT("InteriorHull"))
	{
		return EPreviewSectionKind::InteriorHull;
	}

	const FString SectionName = SectionId.ToString();
	if (SectionName.EndsWith(TEXT("_DoorFrame")))
	{
		return EPreviewSectionKind::DoorFrame;
	}

	if (SectionName.EndsWith(TEXT("_DoorLeaf")))
	{
		return EPreviewSectionKind::DoorLeaf;
	}

	if (SectionName.StartsWith(TEXT("Ramp_")))
	{
		return EPreviewSectionKind::Ramp;
	}

	if (SectionName.StartsWith(TEXT("Hatch_")) || SectionName.Contains(TEXT("Hatch")))
	{
		return EPreviewSectionKind::Ramp;
	}

	if (SectionName.StartsWith(TEXT("Deck_")) || SectionName.Contains(TEXT("Deck")))
	{
		return EPreviewSectionKind::Deck;
	}

	if (SectionName.Contains(TEXT("Bulkhead")) || SectionName.Contains(TEXT("_Door")))
	{
		return EPreviewSectionKind::Bulkhead;
	}

	return EPreviewSectionKind::Other;
}

bool ShouldShowPreviewSection(
	const EPreviewSectionKind Kind,
	const bool bPreviewXRayMode,
	const bool bPreviewShowExteriorHull,
	const bool bPreviewShowInteriorHull,
	const bool bPreviewShowDecks,
	const bool bPreviewShowBulkheads,
	const bool bPreviewShowDoors,
	const bool bPreviewShowRamps)
{
	switch (Kind)
	{
	case EPreviewSectionKind::ExteriorHull:
		return !bPreviewXRayMode && bPreviewShowExteriorHull;
	case EPreviewSectionKind::InteriorHull:
		return bPreviewShowInteriorHull;
	case EPreviewSectionKind::Deck:
		return bPreviewShowDecks;
	case EPreviewSectionKind::Ramp:
		return bPreviewShowRamps;
	case EPreviewSectionKind::Bulkhead:
		return bPreviewShowBulkheads;
	case EPreviewSectionKind::DoorFrame:
	case EPreviewSectionKind::DoorLeaf:
		return bPreviewShowDoors;
	case EPreviewSectionKind::Other:
	default:
		return true;
	}
}

FBox BuildLocalBoundsFromMeshSection(const FCompiledSubmarineMeshSection& Section)
{
	FBox Bounds(EForceInit::ForceInit);
	for (const FVector3f& Position : Section.Positions)
	{
		Bounds += FVector(Position);
	}
	return Bounds;
}

FBox BuildLocalBoundsFromPositions(const TArray<FVector3f>& Positions)
{
	FBox Bounds(EForceInit::ForceInit);
	for (const FVector3f& Position : Positions)
	{
		Bounds += FVector(Position);
	}
	return Bounds;
}

void MakeSheetBasisFromNormal(const FVector& Normal, FVector& OutTangentX, FVector& OutTangentY)
{
	const FVector SafeNormal = Normal.GetSafeNormal();
	const FVector Reference = FMath::Abs(FVector::DotProduct(SafeNormal, FVector::ForwardVector)) < 0.95f
		? FVector::ForwardVector
		: FVector::RightVector;
	OutTangentX = FVector::CrossProduct(Reference, SafeNormal).GetSafeNormal();
	OutTangentY = FVector::CrossProduct(SafeNormal, OutTangentX).GetSafeNormal();
}

FVector2D ComputeProjectedSizeCm(const FBox& Bounds, const FVector& Origin, const FVector& TangentX, const FVector& TangentY)
{
	if (!Bounds.IsValid)
	{
		return FVector2D(200.f, 200.f);
	}

	const FVector Min = Bounds.Min;
	const FVector Max = Bounds.Max;
	const FVector Corners[8] =
	{
		FVector(Min.X, Min.Y, Min.Z),
		FVector(Min.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Min.Z),
		FVector(Min.X, Max.Y, Max.Z),
		FVector(Max.X, Min.Y, Min.Z),
		FVector(Max.X, Min.Y, Max.Z),
		FVector(Max.X, Max.Y, Min.Z),
		FVector(Max.X, Max.Y, Max.Z)
	};

	float MinX = TNumericLimits<float>::Max();
	float MaxX = TNumericLimits<float>::Lowest();
	float MinY = TNumericLimits<float>::Max();
	float MaxY = TNumericLimits<float>::Lowest();
	for (const FVector& Corner : Corners)
	{
		const FVector Offset = Corner - Origin;
		const float ProjectedX = FVector::DotProduct(Offset, TangentX);
		const float ProjectedY = FVector::DotProduct(Offset, TangentY);
		MinX = FMath::Min(MinX, ProjectedX);
		MaxX = FMath::Max(MaxX, ProjectedX);
		MinY = FMath::Min(MinY, ProjectedY);
		MaxY = FMath::Max(MaxY, ProjectedY);
	}

	return FVector2D(FMath::Max(1.f, MaxX - MinX), FMath::Max(1.f, MaxY - MinY));
}

USubmarineLayoutAsset* BuildTransientLayoutFromCompiledAsset(AActor* OuterActor, const UCompiledSubmarineAsset* CompiledAsset)
{
	if (!OuterActor || !CompiledAsset)
	{
		return nullptr;
	}

	USubmarineLayoutAsset* LayoutAsset = NewObject<USubmarineLayoutAsset>(OuterActor, NAME_None, RF_Transient);
	if (!LayoutAsset)
	{
		return nullptr;
	}

	FBox ExteriorBounds(EForceInit::ForceInit);
	if (CompiledAsset->Collision.ExteriorProxy.Positions.Num() > 0)
	{
		ExteriorBounds = BuildLocalBoundsFromMeshSection(CompiledAsset->Collision.ExteriorProxy);
	}
	else if (const FCompiledSubmarineMeshSection* ExteriorSection = CompiledAsset->RenderSections.FindByPredicate([](const FCompiledSubmarineMeshSection& Section)
	{
		return Section.SectionId == TEXT("ExteriorHull");
	}))
	{
		ExteriorBounds = BuildLocalBoundsFromMeshSection(*ExteriorSection);
	}

	const FVector HydroExtent = ExteriorBounds.IsValid ? ExteriorBounds.GetExtent() * 0.8f : FVector(200.f, 150.f, 150.f);
	for (const FCompiledSubmarineCompartmentData& Compartment : CompiledAsset->Compartments)
	{
		FSubCompartmentDef CompartmentDef;
		CompartmentDef.CompartmentId = Compartment.CompartmentId;
		CompartmentDef.DisplayName = Compartment.DisplayName;
		CompartmentDef.HydroBoundsMin = FVector(Compartment.StartXcm, -HydroExtent.Y, -HydroExtent.Z);
		CompartmentDef.HydroBoundsMax = FVector(Compartment.EndXcm, HydroExtent.Y, HydroExtent.Z);
		CompartmentDef.WalkableFloorZCm = 0.f;
		const float LengthCm = FMath::Max(1.f, Compartment.EndXcm - Compartment.StartXcm);
		CompartmentDef.CapacityLiters = FMath::Max(1000.f, (LengthCm * HydroExtent.Y * 2.f * HydroExtent.Z * 2.f) / 1000.f);
		LayoutAsset->Compartments.Add(CompartmentDef);
	}

	for (const FStructuralSheetCompiledBinding& Binding : CompiledAsset->StructuralBindings)
	{
		FStructuralSheetDef SheetDef;
		SheetDef.SheetId = Binding.SheetId;
		if (CompiledAsset->Compartments.IsValidIndex(Binding.CompartmentIndex))
		{
			SheetDef.ParentCompartmentId = CompiledAsset->Compartments[Binding.CompartmentIndex].CompartmentId;
		}
		SheetDef.LocalOrigin = Binding.MeshRange.LocalCenter;
		SheetDef.LocalNormal = Binding.MeshRange.LocalNormal.GetSafeNormal();
		MakeSheetBasisFromNormal(SheetDef.LocalNormal, SheetDef.LocalTangentX, SheetDef.LocalTangentY);
		SheetDef.SizeCm = ComputeProjectedSizeCm(Binding.MeshRange.LocalBounds, SheetDef.LocalOrigin, SheetDef.LocalTangentX, SheetDef.LocalTangentY);
		SheetDef.ThicknessCm = 12.f;
		SheetDef.GridResolutionX = 16;
		SheetDef.GridResolutionY = 16;
		SheetDef.bCanOpenToExterior = Binding.MeshRange.ExteriorVertexCount > 0;
		SheetDef.bSupportsVisualRupture = SheetDef.bCanOpenToExterior;
		SheetDef.ExteriorVisualMaterialSlot = 0;
		SheetDef.VisualLocalOrigin = SheetDef.LocalOrigin;
		SheetDef.VisualLocalTangentX = SheetDef.LocalTangentX;
		SheetDef.VisualLocalTangentY = SheetDef.LocalTangentY;
		SheetDef.VisualProjectionSizeCm = SheetDef.SizeCm;
		SheetDef.MaxVisibleRuptureRadiusCm = 0.35f * FMath::Min(SheetDef.SizeCm.X, SheetDef.SizeCm.Y);
		LayoutAsset->StructuralSheets.Add(SheetDef);
	}

	for (const FCompiledSubmarineBulkheadConnectionData& Connection : CompiledAsset->BulkheadConnections)
	{
		for (const FCompiledSubmarineBulkheadOpeningData& Opening : Connection.Openings)
		{
			FStructuralSheetDef SheetDef;
			SheetDef.SheetId = Opening.OpeningId;
			SheetDef.ParentCompartmentId = Connection.CompartmentA;
			SheetDef.AdjacentCompartmentId = Connection.CompartmentB;
			SheetDef.LocalOrigin = FVector(Connection.LocalX, 0.f, Opening.DoorSillZCm + (Opening.DoorSizeCm.Y * 0.5f));
			SheetDef.LocalNormal = FVector::ForwardVector;
			SheetDef.LocalTangentX = FVector::RightVector;
			SheetDef.LocalTangentY = FVector::UpVector;
			SheetDef.SizeCm = Opening.DoorSizeCm;
			SheetDef.ThicknessCm = 8.f;
			SheetDef.GridResolutionX = 1;
			SheetDef.GridResolutionY = 1;
			SheetDef.bCanOpenToExterior = false;
			SheetDef.bSupportsVisualRupture = false;
			LayoutAsset->StructuralSheets.Add(SheetDef);

			FDoorDef DoorDef;
			DoorDef.DoorId = Opening.OpeningId;
			DoorDef.BulkheadSheetId = Opening.OpeningId;
			DoorDef.PassageType = EPassageType::WatertightDoor;
			DoorDef.LocalTransform = FTransform(FRotator::ZeroRotator, SheetDef.LocalOrigin, FVector::OneVector);
			DoorDef.WidthCm = Opening.DoorSizeCm.X;
			DoorDef.HeightCm = Opening.DoorSizeCm.Y;
			LayoutAsset->Doors.Add(DoorDef);
		}
	}

	LayoutAsset->CompiledSheetBindings = CompiledAsset->StructuralBindings;
	return LayoutAsset;
}
}

ASubmarineAuthoringPreviewActor::ASubmarineAuthoringPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PreviewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot"));
	SetRootComponent(PreviewRoot);
}

void ASubmarineAuthoringPreviewActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bAutoRefreshPreviewOnConstruction)
	{
		RefreshPreview();
	}
}

bool ASubmarineAuthoringPreviewActor::ValidateAuthoring()
{
	LastValidationMessages.Reset();
	return USubmarineAuthoringBakeLibrary::ValidateAuthoringAsset(AuthoringAsset, LastValidationMessages);
}

bool ASubmarineAuthoringPreviewActor::BakeAuthoring()
{
	LastValidationMessages.Reset();
	UCompiledSubmarineAsset* TargetAsset = BakedAsset;
	if (!TargetAsset)
	{
		if (!TransientPreviewAsset)
		{
			TransientPreviewAsset = NewObject<UCompiledSubmarineAsset>(this, NAME_None, RF_Transient);
		}
		TargetAsset = TransientPreviewAsset;
	}

	const bool bBaked = USubmarineAuthoringBakeLibrary::BakeToCompiledAsset(AuthoringAsset, TargetAsset, LastValidationMessages);
	if (bBaked)
	{
		RefreshPreview();
	}

	return bBaked;
}

void ASubmarineAuthoringPreviewActor::ValidateAuthoringNow()
{
	ValidateAuthoring();
}

void ASubmarineAuthoringPreviewActor::BakeAuthoringNow()
{
	BakeAuthoring();
}

void ASubmarineAuthoringPreviewActor::RefreshPreview()
{
	ClearPreview();

	UCompiledSubmarineAsset* SourceAsset = BakedAsset ? BakedAsset : TransientPreviewAsset;
	if (!SourceAsset)
	{
		return;
	}

	for (int32 SectionIndex = 0; SectionIndex < SourceAsset->RenderSections.Num(); ++SectionIndex)
	{
		UProceduralMeshComponent* SectionComponent = NewObject<UProceduralMeshComponent>(this);
		SectionComponent->SetupAttachment(PreviewRoot);
		SectionComponent->RegisterComponent();
		SectionComponent->SetMobility(EComponentMobility::Movable);
		SectionComponent->ComponentTags.Add(SourceAsset->RenderSections[SectionIndex].SectionId);
		ApplyCompiledSectionToProceduralMesh(
			SectionComponent,
			SourceAsset->RenderSections[SectionIndex],
			SourceAsset->MaterialSlots,
			false,
			NAME_None);
		PreviewRenderSections.Add(SectionComponent);
	}

	ApplyPreviewVisibility();
}

void ASubmarineAuthoringPreviewActor::ClearPreview()
{
	DestroyProceduralMeshes(PreviewRenderSections);
}

void ASubmarineAuthoringPreviewActor::ApplyPreviewVisibility()
{
	for (UProceduralMeshComponent* Component : PreviewRenderSections)
	{
		if (!IsValid(Component))
		{
			continue;
		}

		const FName SectionId = Component->ComponentTags.Num() > 0 ? Component->ComponentTags[0] : NAME_None;
		const EPreviewSectionKind Kind = ClassifyPreviewSection(SectionId);
		const bool bVisible = ShouldShowPreviewSection(
			Kind,
			bPreviewXRayMode,
			bPreviewShowExteriorHull,
			bPreviewShowInteriorHull,
			bPreviewShowDecks,
			bPreviewShowBulkheads,
			bPreviewShowDoors,
			bPreviewShowRamps);
		Component->SetVisibility(bVisible);
		Component->SetHiddenInGame(!bVisible);
	}
}

ASubmarineBakedRuntimeActor::ASubmarineBakedRuntimeActor()
{
	if (HullMesh)
	{
		HullMesh->SetHiddenInGame(true);
		HullMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ASubmarineBakedRuntimeActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bBuildOnConstruction)
	{
		BuildFromCompiledAsset();
	}
}

void ASubmarineBakedRuntimeActor::BeginPlay()
{
	Super::BeginPlay();

	if (!bBuildOnConstruction)
	{
		BuildFromCompiledAsset();
	}
}

UPrimitiveComponent* ASubmarineBakedRuntimeActor::GetMovementCollisionComponent() const
{
	return ExteriorCollisionProxyComponent ? ExteriorCollisionProxyComponent : Super::GetMovementCollisionComponent();
}

TArray<UPrimitiveComponent*> ASubmarineBakedRuntimeActor::GetInteriorWalkableComponents() const
{
	TArray<UPrimitiveComponent*> Result;
	for (UProceduralMeshComponent* Component : WalkableCollisionComponents)
	{
		if (IsValid(Component))
		{
			Result.Add(Component);
		}
	}
	return Result;
}

FTransform ASubmarineBakedRuntimeActor::GetCrewEmbarkTransform() const
{
	for (UProceduralMeshComponent* Component : WalkableCollisionComponents)
	{
		if (!IsValid(Component))
		{
			continue;
		}

		const FBoxSphereBounds Bounds = Component->Bounds;
		FTransform EmbarkTransform = GetActorTransform();
		EmbarkTransform.SetLocation(FVector(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z + Bounds.BoxExtent.Z + 96.f));
		EmbarkTransform.SetRotation(GetActorQuat());
		return EmbarkTransform;
	}

	return Super::GetCrewEmbarkTransform();
}

bool ASubmarineBakedRuntimeActor::ValidateSpawnCollision() const
{
	return IsValid(ExteriorCollisionProxyComponent);
}

bool ASubmarineBakedRuntimeActor::BuildFromCompiledAsset()
{
	ClearBakedGeometry();

	if (!CompiledAsset)
	{
		return false;
	}

	const bool bCanSpawnRuntimeComponents = (GetWorld() != nullptr && GetLevel() != nullptr);

	if (HullMesh)
	{
		HullMesh->SetHiddenInGame(true);
		HullMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (bCanSpawnRuntimeComponents)
	{
		for (const FCompiledSubmarineMeshSection& Section : CompiledAsset->RenderSections)
		{
			UProceduralMeshComponent* SectionComponent = NewObject<UProceduralMeshComponent>(this, Section.SectionId);
			SectionComponent->SetupAttachment(GetRootComponent());
			SectionComponent->RegisterComponent();
			SectionComponent->SetMobility(EComponentMobility::Movable);
			SectionComponent->ComponentTags.Add(Section.SectionId);
			ApplyCompiledSectionToProceduralMesh(
				SectionComponent,
				Section,
				CompiledAsset->MaterialSlots,
				false,
				NAME_None);
			RenderSectionComponents.Add(SectionComponent);
			RenderSectionComponentMap.Add(Section.SectionId, SectionComponent);
		}

		ExteriorCollisionProxyComponent = NewObject<UProceduralMeshComponent>(this, TEXT("ExteriorCollisionProxyComponent"));
		ExteriorCollisionProxyComponent->SetupAttachment(GetRootComponent());
		ExteriorCollisionProxyComponent->RegisterComponent();
		ExteriorCollisionProxyComponent->SetHiddenInGame(true);
		ExteriorCollisionProxyComponent->SetVisibility(false);
		ApplyCompiledSectionToProceduralMesh(
			ExteriorCollisionProxyComponent,
			CompiledAsset->Collision.ExteriorProxy,
			CompiledAsset->MaterialSlots,
			true,
			TEXT("SubmarineHull"));

		for (const FCompiledSubmarineMeshSection& WalkableSection : CompiledAsset->Collision.WalkableDeckSections)
		{
			UProceduralMeshComponent* WalkableComponent = NewObject<UProceduralMeshComponent>(this);
			WalkableComponent->SetupAttachment(GetRootComponent());
			WalkableComponent->RegisterComponent();
			WalkableComponent->SetHiddenInGame(true);
			WalkableComponent->SetVisibility(false);
			ApplyCompiledSectionToProceduralMesh(
				WalkableComponent,
				WalkableSection,
				CompiledAsset->MaterialSlots,
				true,
				TEXT("SubInteriorWalkable"));
			WalkableCollisionComponents.Add(WalkableComponent);
		}

		for (const FCompiledSubmarineMeshSection& RampSection : CompiledAsset->Collision.RampSections)
		{
			UProceduralMeshComponent* WalkableComponent = NewObject<UProceduralMeshComponent>(this);
			WalkableComponent->SetupAttachment(GetRootComponent());
			WalkableComponent->RegisterComponent();
			WalkableComponent->SetHiddenInGame(true);
			WalkableComponent->SetVisibility(false);
			ApplyCompiledSectionToProceduralMesh(
				WalkableComponent,
				RampSection,
				CompiledAsset->MaterialSlots,
				true,
				TEXT("SubInteriorWalkable"));
			WalkableCollisionComponents.Add(WalkableComponent);
		}

		for (const FCompiledSubmarineMeshSection& BulkheadSection : CompiledAsset->Collision.BulkheadBlockerSections)
		{
			UProceduralMeshComponent* BulkheadComponent = NewObject<UProceduralMeshComponent>(this);
			BulkheadComponent->SetupAttachment(GetRootComponent());
			BulkheadComponent->RegisterComponent();
			BulkheadComponent->SetHiddenInGame(true);
			BulkheadComponent->SetVisibility(false);
			ApplyCompiledSectionToProceduralMesh(
				BulkheadComponent,
				BulkheadSection,
				CompiledAsset->MaterialSlots,
				true,
				TEXT("SubInteriorVisual"));
			BulkheadCollisionComponents.Add(BulkheadComponent);
			BulkheadBlockerComponentMap.Add(BulkheadSection.SectionId, BulkheadComponent);
		}
	}

	if (SubHull)
	{
		TransientCompiledLayout = BuildTransientLayoutFromCompiledAsset(this, CompiledAsset);
		SubHull->LayoutAsset = TransientCompiledLayout;
		SubHull->InitializeFromLayout(TransientCompiledLayout);
	}

	if (Compartments)
	{
		for (const FCompiledSubmarineBulkheadConnectionData& Connection : CompiledAsset->BulkheadConnections)
		{
			for (const FCompiledSubmarineBulkheadOpeningData& Opening : Connection.Openings)
			{
				FDoorState DoorState;
				DoorState.DoorId = Opening.OpeningId;
				DoorState.CompartmentA = Connection.CompartmentA;
				DoorState.CompartmentB = Connection.CompartmentB;
				DoorState.bClosed = Opening.bBlockedByDefault;
				DoorState.bLocked = false;
				Compartments->RegisterDoor(DoorState);
				Compartments->SetDoorClosed(Opening.OpeningId, Opening.bBlockedByDefault);
			}
		}
	}

	if (bCanSpawnRuntimeComponents)
	{
		for (const FCompiledSubmarineBulkheadConnectionData& Connection : CompiledAsset->BulkheadConnections)
		{
			if (Connection.Openings.Num() == 0)
			{
				if (TObjectPtr<UProceduralMeshComponent>* BlockerComponent = BulkheadBlockerComponentMap.Find(Connection.BlockerSectionId))
				{
					(*BlockerComponent)->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				}
			}

			for (const FCompiledSubmarineBulkheadOpeningData& Opening : Connection.Openings)
			{
				if (TObjectPtr<UProceduralMeshComponent>* BlockerComponent = BulkheadBlockerComponentMap.Find(Opening.BlockerSectionId))
				{
					(*BlockerComponent)->SetCollisionEnabled(Opening.bBlockedByDefault ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
				}
				if (TObjectPtr<UProceduralMeshComponent>* DoorLeafComponent = RenderSectionComponentMap.Find(Opening.DoorLeafSectionId))
				{
					(*DoorLeafComponent)->SetVisibility(Opening.bBlockedByDefault);
					(*DoorLeafComponent)->SetHiddenInGame(!Opening.bBlockedByDefault);
				}
			}
		}
	}

	RefreshMovementCollisionBinding();
	return true;
}

void ASubmarineBakedRuntimeActor::ClearBakedGeometry()
{
	DestroyProceduralMeshes(RenderSectionComponents);
	DestroyProceduralMeshes(WalkableCollisionComponents);
	DestroyProceduralMeshes(BulkheadCollisionComponents);
	RenderSectionComponentMap.Reset();
	BulkheadBlockerComponentMap.Reset();
	TransientCompiledLayout = nullptr;

	if (IsValid(ExteriorCollisionProxyComponent))
	{
		ExteriorCollisionProxyComponent->DestroyComponent();
	}
	ExteriorCollisionProxyComponent = nullptr;

	if (SubHull)
	{
		SubHull->LayoutAsset = nullptr;
	}
}

bool ASubmarineBakedRuntimeActor::SetBulkheadConnectionBlocked(const FName BoundaryId, const bool bBlocked)
{
	if (!CompiledAsset || BoundaryId.IsNone())
	{
		return false;
	}

	const FCompiledSubmarineBulkheadConnectionData* Connection = CompiledAsset->BulkheadConnections.FindByPredicate(
		[BoundaryId](const FCompiledSubmarineBulkheadConnectionData& Candidate)
		{
			return Candidate.BoundaryId == BoundaryId;
		});
	if (!Connection)
	{
		return false;
	}

	bool bChanged = false;
	for (const FCompiledSubmarineBulkheadOpeningData& Opening : Connection->Openings)
	{
		bChanged |= SetBulkheadOpeningBlocked(Opening.OpeningId, bBlocked);
	}

	return bChanged;
}

bool ASubmarineBakedRuntimeActor::IsBulkheadConnectionBlocked(const FName BoundaryId) const
{
	if (!CompiledAsset || BoundaryId.IsNone())
	{
		return false;
	}

	const FCompiledSubmarineBulkheadConnectionData* Connection = CompiledAsset->BulkheadConnections.FindByPredicate(
		[BoundaryId](const FCompiledSubmarineBulkheadConnectionData& Candidate)
		{
			return Candidate.BoundaryId == BoundaryId;
		});
	if (!Connection)
	{
		return false;
	}

	for (const FCompiledSubmarineBulkheadOpeningData& Opening : Connection->Openings)
	{
		if (IsBulkheadOpeningBlocked(Opening.OpeningId))
		{
			return true;
		}
	}

	return false;
}

bool ASubmarineBakedRuntimeActor::SetBulkheadOpeningBlocked(const FName OpeningId, const bool bBlocked)
{
	if (!CompiledAsset || OpeningId.IsNone())
	{
		return false;
	}

	const FCompiledSubmarineBulkheadOpeningData* Opening = nullptr;
	for (const FCompiledSubmarineBulkheadConnectionData& Connection : CompiledAsset->BulkheadConnections)
	{
		Opening = Connection.Openings.FindByPredicate([OpeningId](const FCompiledSubmarineBulkheadOpeningData& Candidate)
		{
			return Candidate.OpeningId == OpeningId;
		});
		if (Opening)
		{
			break;
		}
	}

	if (!Opening)
	{
		return false;
	}

	bool bChanged = false;
	if (TObjectPtr<UProceduralMeshComponent>* BlockerComponent = BulkheadBlockerComponentMap.Find(Opening->BlockerSectionId))
	{
		(*BlockerComponent)->SetCollisionEnabled(bBlocked ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		bChanged = true;
	}

	if (Compartments)
	{
		Compartments->SetDoorClosed(OpeningId, bBlocked);
	}

	if (TObjectPtr<UProceduralMeshComponent>* DoorLeafComponent = RenderSectionComponentMap.Find(Opening->DoorLeafSectionId))
	{
		(*DoorLeafComponent)->SetVisibility(bBlocked);
		(*DoorLeafComponent)->SetHiddenInGame(!bBlocked);
		bChanged = true;
	}

	return bChanged;
}

bool ASubmarineBakedRuntimeActor::IsBulkheadOpeningBlocked(const FName OpeningId) const
{
	if (!CompiledAsset || OpeningId.IsNone())
	{
		return false;
	}

	const FCompiledSubmarineBulkheadOpeningData* Opening = nullptr;
	for (const FCompiledSubmarineBulkheadConnectionData& Connection : CompiledAsset->BulkheadConnections)
	{
		Opening = Connection.Openings.FindByPredicate([OpeningId](const FCompiledSubmarineBulkheadOpeningData& Candidate)
		{
			return Candidate.OpeningId == OpeningId;
		});
		if (Opening)
		{
			break;
		}
	}

	if (!Opening)
	{
		return false;
	}

	if (const TObjectPtr<UProceduralMeshComponent>* BlockerComponent = BulkheadBlockerComponentMap.Find(Opening->BlockerSectionId))
	{
		return IsValid(*BlockerComponent) && (*BlockerComponent)->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
	}

	return false;
}
