#include "SubmarineBuildCompiler.h"

#include "SubmarineEnvelopeDef.h"
#include "Submarine/SubmarineLayoutAsset.h"
#include "Submarine/StructuralHullTypes.h"

namespace
{
void AddBuildCompilerValidationMessage(
	TArray<FLayoutValidationMessage>& OutMessages,
	ELayoutValidationSeverity Severity,
	FName RelatedId,
	const FString& Message)
{
	FLayoutValidationMessage ValidationMessage;
	ValidationMessage.Severity = Severity;
	ValidationMessage.RelatedId = RelatedId;
	ValidationMessage.Message = FText::FromString(Message);
	OutMessages.Add(MoveTemp(ValidationMessage));
}

FName MakeExteriorSheetId(FName CompartmentId, const TCHAR* Suffix)
{
	return FName(*FString::Printf(TEXT("%s_%s"), *CompartmentId.ToString(), Suffix));
}

FName MakeBulkheadSheetId(FName ForeCompartmentId, FName AftCompartmentId)
{
	return FName(*FString::Printf(TEXT("Bulkhead_%s_%s"), *ForeCompartmentId.ToString(), *AftCompartmentId.ToString()));
}

int32 ComputeGridResolution(float SizeCm)
{
	return FMath::Clamp(FMath::CeilToInt(SizeCm / 100.f), 4, 64);
}

float ComputeCompartmentCapacityLiters(const FCompartmentPlacement& Placement)
{
	const float LengthCm = FMath::Max(0.f, Placement.SpineEndCm - Placement.SpineStartCm);
	const float VolumeCm3 = LengthCm * FMath::Max(0.f, Placement.FloorWidthCm) * FMath::Max(0.f, Placement.ClearanceHeightCm);
	return FMath::Max(1.f, VolumeCm3 / 1000.f);
}

void AddExteriorSheet(
	TArray<FStructuralSheetDef>& OutSheets,
	FName SheetId,
	FName CompartmentId,
	const FVector& Origin,
	const FVector& Normal,
	const FVector& TangentX,
	const FVector& TangentY,
	const FVector2D& SizeCm)
{
	FStructuralSheetDef Sheet;
	Sheet.SheetId = SheetId;
	Sheet.ParentCompartmentId = CompartmentId;
	Sheet.LocalOrigin = Origin;
	Sheet.LocalNormal = Normal;
	Sheet.LocalTangentX = TangentX;
	Sheet.LocalTangentY = TangentY;
	Sheet.SizeCm = SizeCm;
	Sheet.ThicknessCm = 8.f;
	Sheet.MaterialStrength = 1.f;
	Sheet.GridResolutionX = ComputeGridResolution(SizeCm.X);
	Sheet.GridResolutionY = ComputeGridResolution(SizeCm.Y);
	Sheet.bCanOpenToExterior = true;
	Sheet.bSupportsVisualRupture = true;
	Sheet.ExteriorVisualMaterialSlot = 0;
	Sheet.VisualLocalOrigin = Origin;
	Sheet.VisualLocalTangentX = TangentX;
	Sheet.VisualLocalTangentY = TangentY;
	Sheet.VisualProjectionSizeCm = SizeCm;
	Sheet.MaxVisibleRuptureRadiusCm = FMath::Max(40.f, 0.35f * FMath::Min(SizeCm.X, SizeCm.Y));
	Sheet.PreferredRuptureBorderScale = 1.f;
	OutSheets.Add(Sheet);
}

FVector2D GetChartMinForSide(ESheetSide Side)
{
	switch (Side)
	{
	case ESheetSide::Starboard:
		return FVector2D(0.f, 0.875f);
	case ESheetSide::Top:
		return FVector2D(0.f, 0.125f);
	case ESheetSide::Port:
		return FVector2D(0.f, 0.375f);
	case ESheetSide::Bottom:
		return FVector2D(0.f, 0.625f);
	default:
		return FVector2D(0.f, 0.f);
	}
}

FVector2D GetChartMaxForSide(ESheetSide Side)
{
	switch (Side)
	{
	case ESheetSide::Starboard:
		return FVector2D(1.f, 1.f);
	case ESheetSide::Top:
		return FVector2D(1.f, 0.375f);
	case ESheetSide::Port:
		return FVector2D(1.f, 0.625f);
	case ESheetSide::Bottom:
		return FVector2D(1.f, 0.875f);
	default:
		return FVector2D(1.f, 1.f);
	}
}
}

USubmarineLayoutAsset* USubmarineBuildCompiler::CompileToLayoutAsset(
	const FSubmarineLayoutSolution& Solution,
	UObject* Outer,
	TArray<FLayoutValidationMessage>& OutMessages,
	const USubmarineEnvelopeDef* EnvelopeDef)
{
	OutMessages.Reset();

	if (Solution.HasErrors())
	{
		OutMessages = Solution.ValidationMessages;
		AddBuildCompilerValidationMessage(OutMessages, ELayoutValidationSeverity::Error, NAME_None, TEXT("Solution invalide : compilation refusee"));
		return nullptr;
	}

	if (Solution.Compartments.Num() == 0)
	{
		AddBuildCompilerValidationMessage(OutMessages, ELayoutValidationSeverity::Error, NAME_None, TEXT("Solution vide : aucun compartiment a compiler"));
		return nullptr;
	}

	UObject* EffectiveOuter = Outer ? Outer : GetTransientPackage();
	USubmarineLayoutAsset* LayoutAsset = NewObject<USubmarineLayoutAsset>(EffectiveOuter);
	if (!LayoutAsset)
	{
		AddBuildCompilerValidationMessage(OutMessages, ELayoutValidationSeverity::Error, NAME_None, TEXT("Creation du LayoutAsset echouee"));
		return nullptr;
	}

	LayoutAsset->Compartments.Reset();
	LayoutAsset->StructuralSheets.Reset();
	LayoutAsset->CompiledSheetBindings.Reset();
	LayoutAsset->Doors.Reset();
	LayoutAsset->StationSlots.Reset();
	LayoutAsset->WalkableSurfaces.Reset();
	LayoutAsset->Metrics = Solution.Metrics;

	const int32 EffectiveExteriorRadialSegments = FMath::Clamp(
		EnvelopeDef ? EnvelopeDef->ExteriorRadialSegments : 32,
		12,
		64);
	const int32 EffectiveExteriorLongitudinalSubdivisions = FMath::Clamp(
		EnvelopeDef ? EnvelopeDef->ExteriorLongitudinalSubdivisionsPerSpan : 6,
		1,
		16);
	const int32 EffectiveInteriorArcSegments = FMath::Clamp(
		EnvelopeDef ? EnvelopeDef->InteriorArcSegments : 24,
		8,
		48);

	const int32 ExteriorTrianglesPerSpan = EffectiveExteriorRadialSegments * 2;
	const int32 InteriorTrianglesPerCompartment = EffectiveInteriorArcSegments * 2;
	const int32 InteriorVerticesPerCompartment = (EffectiveInteriorArcSegments + 1) * 2;

	for (int32 CompartmentIndex = 0; CompartmentIndex < Solution.Compartments.Num(); ++CompartmentIndex)
	{
		const FCompartmentPlacement& Placement = Solution.Compartments[CompartmentIndex];
		FSubCompartmentDef CompartmentDef;
		CompartmentDef.CompartmentId = Placement.CompartmentId;
		CompartmentDef.DisplayName = FText::FromName(Placement.CompartmentId);
		CompartmentDef.CapacityLiters = ComputeCompartmentCapacityLiters(Placement);
		CompartmentDef.HydroBoundsMin = FVector(
			Placement.SpineStartCm,
			-Placement.FloorWidthCm * 0.5f,
			Placement.FloorOffsetCm);
		CompartmentDef.HydroBoundsMax = FVector(
			Placement.SpineEndCm,
			Placement.FloorWidthCm * 0.5f,
			Placement.FloorOffsetCm + Placement.ClearanceHeightCm);
		CompartmentDef.WalkableFloorZCm = Placement.FloorOffsetCm;
		LayoutAsset->Compartments.Add(CompartmentDef);

		const float MidX = (Placement.SpineStartCm + Placement.SpineEndCm) * 0.5f;
		const float LengthCm = FMath::Max(1.f, Placement.SpineEndCm - Placement.SpineStartCm);
		const float RadiusCm = FMath::Max(1.f, Placement.EffectiveRadiusCm);
		const FVector2D SideSize(LengthCm, RadiusCm * 2.f);
		const FVector2D CapSize(LengthCm, RadiusCm * 2.f);

		const int32 ExteriorRingStart = CompartmentIndex * EffectiveExteriorLongitudinalSubdivisions;
		const int32 ExteriorRingCount = (CompartmentIndex == Solution.Compartments.Num() - 1)
			? EffectiveExteriorLongitudinalSubdivisions + 1
			: EffectiveExteriorLongitudinalSubdivisions;
		const int32 ExteriorVertexStart = ExteriorRingStart * EffectiveExteriorRadialSegments;
		const int32 ExteriorVertexCount = ExteriorRingCount * EffectiveExteriorRadialSegments;
		const int32 ExteriorTriangleStart = CompartmentIndex * EffectiveExteriorLongitudinalSubdivisions * ExteriorTrianglesPerSpan;
		const int32 ExteriorTriangleCount = EffectiveExteriorLongitudinalSubdivisions * ExteriorTrianglesPerSpan;
		const int32 InteriorVertexStart = CompartmentIndex * InteriorVerticesPerCompartment;
		const int32 InteriorTriangleStart = CompartmentIndex * InteriorTrianglesPerCompartment;

		FWalkableSurfaceDef WalkableSurface;
		WalkableSurface.CompartmentId = Placement.CompartmentId;
		WalkableSurface.LocalTransform = FTransform(
			FRotator::ZeroRotator,
			FVector(MidX, 0.f, Placement.FloorOffsetCm));
		WalkableSurface.WidthCm = FMath::Max(1.f, Placement.FloorWidthCm);
		WalkableSurface.LengthCm = LengthCm;
		WalkableSurface.SurfaceType = FName(TEXT("CompartmentFloor"));
		WalkableSurface.CollisionProfileName = FName(TEXT("SubInteriorWalkable"));
		WalkableSurface.bSupportsCrew = true;
		LayoutAsset->WalkableSurfaces.Add(MoveTemp(WalkableSurface));

		auto AddExteriorSheetAndBinding = [&](
			const TCHAR* Suffix,
			const FVector& Origin,
			const FVector& Normal,
			const FVector& TangentX,
			const FVector& TangentY,
			const FVector2D& Size,
			ESheetSide Side)
		{
			const FName SheetId = MakeExteriorSheetId(Placement.CompartmentId, Suffix);
			AddExteriorSheet(
				LayoutAsset->StructuralSheets,
				SheetId,
				Placement.CompartmentId,
				Origin,
				Normal,
				TangentX,
				TangentY,
				Size);

			FStructuralSheetCompiledBinding Binding;
			Binding.SheetId = SheetId;
			Binding.CompartmentIndex = CompartmentIndex;
			Binding.Side = Side;
			Binding.ChartMin = GetChartMinForSide(Side);
			Binding.ChartMax = GetChartMaxForSide(Side);
			Binding.MeshRange.SectionIndexStart = ExteriorRingStart;
			Binding.MeshRange.SectionIndexEnd = ExteriorRingStart + ExteriorRingCount;
			Binding.MeshRange.ExteriorVertexStart = ExteriorVertexStart;
			Binding.MeshRange.ExteriorVertexCount = ExteriorVertexCount;
			Binding.MeshRange.ExteriorTriangleStart = ExteriorTriangleStart;
			Binding.MeshRange.ExteriorTriangleCount = ExteriorTriangleCount;
			Binding.MeshRange.InteriorVertexStart = InteriorVertexStart;
			Binding.MeshRange.InteriorVertexCount = InteriorVerticesPerCompartment;
			Binding.MeshRange.InteriorTriangleStart = InteriorTriangleStart;
			Binding.MeshRange.InteriorTriangleCount = InteriorTrianglesPerCompartment;
			Binding.MeshRange.LocalCenter = FVector(MidX, 0.f, Placement.FloorOffsetCm + Placement.ClearanceHeightCm * 0.5f);
			Binding.MeshRange.LocalNormal = Normal;
			Binding.MeshRange.LocalBounds = FBox(CompartmentDef.HydroBoundsMin, CompartmentDef.HydroBoundsMax);
			LayoutAsset->CompiledSheetBindings.Add(MoveTemp(Binding));
		};

		AddExteriorSheetAndBinding(
			TEXT("Port"),
			FVector(MidX, -RadiusCm, 0.f),
			FVector(0.f, -1.f, 0.f),
			FVector(1.f, 0.f, 0.f),
			FVector(0.f, 0.f, 1.f),
			SideSize,
			ESheetSide::Port);

		AddExteriorSheetAndBinding(
			TEXT("Starboard"),
			FVector(MidX, RadiusCm, 0.f),
			FVector(0.f, 1.f, 0.f),
			FVector(1.f, 0.f, 0.f),
			FVector(0.f, 0.f, 1.f),
			SideSize,
			ESheetSide::Starboard);

		AddExteriorSheetAndBinding(
			TEXT("Top"),
			FVector(MidX, 0.f, RadiusCm),
			FVector(0.f, 0.f, 1.f),
			FVector(1.f, 0.f, 0.f),
			FVector(0.f, 1.f, 0.f),
			CapSize,
			ESheetSide::Top);

		AddExteriorSheetAndBinding(
			TEXT("Bottom"),
			FVector(MidX, 0.f, -RadiusCm),
			FVector(0.f, 0.f, -1.f),
			FVector(1.f, 0.f, 0.f),
			FVector(0.f, 1.f, 0.f),
			CapSize,
			ESheetSide::Bottom);
	}

	for (const FBulkheadPlacement& Bulkhead : Solution.Bulkheads)
	{
		const FName BulkheadSheetId = MakeBulkheadSheetId(Bulkhead.ForeCompartmentId, Bulkhead.AftCompartmentId);

		FStructuralSheetDef BulkheadSheet;
		BulkheadSheet.SheetId = BulkheadSheetId;
		BulkheadSheet.ParentCompartmentId = Bulkhead.ForeCompartmentId;
		BulkheadSheet.AdjacentCompartmentId = Bulkhead.AftCompartmentId;
		BulkheadSheet.LocalOrigin = FVector(Bulkhead.SpinePositionCm, 0.f, 0.f);
		BulkheadSheet.LocalNormal = FVector::ForwardVector;
		BulkheadSheet.LocalTangentX = FVector::RightVector;
		BulkheadSheet.LocalTangentY = FVector::UpVector;
		BulkheadSheet.SizeCm = FVector2D(Bulkhead.RadiusCm * 2.f, Bulkhead.RadiusCm * 2.f);
		BulkheadSheet.ThicknessCm = 10.f;
		BulkheadSheet.MaterialStrength = 1.2f;
		BulkheadSheet.GridResolutionX = ComputeGridResolution(BulkheadSheet.SizeCm.X);
		BulkheadSheet.GridResolutionY = ComputeGridResolution(BulkheadSheet.SizeCm.Y);
		BulkheadSheet.bCanOpenToExterior = false;
		BulkheadSheet.bSupportsVisualRupture = false;
		BulkheadSheet.ExteriorVisualMaterialSlot = 0;
		BulkheadSheet.VisualLocalOrigin = BulkheadSheet.LocalOrigin;
		BulkheadSheet.VisualLocalTangentX = BulkheadSheet.LocalTangentX;
		BulkheadSheet.VisualLocalTangentY = BulkheadSheet.LocalTangentY;
		BulkheadSheet.VisualProjectionSizeCm = BulkheadSheet.SizeCm;
		BulkheadSheet.MaxVisibleRuptureRadiusCm = 0.f;
		BulkheadSheet.PreferredRuptureBorderScale = 1.f;
		LayoutAsset->StructuralSheets.Add(BulkheadSheet);

		FStructuralSheetCompiledBinding BulkheadBinding;
		BulkheadBinding.SheetId = BulkheadSheetId;
		BulkheadBinding.CompartmentIndex = INDEX_NONE;
		BulkheadBinding.Side = ESheetSide::Bulkhead;
		BulkheadBinding.ChartMin = FVector2D(0.f, 0.f);
		BulkheadBinding.ChartMax = FVector2D(1.f, 1.f);
		BulkheadBinding.MeshRange.SectionIndexStart = INDEX_NONE;
		BulkheadBinding.MeshRange.SectionIndexEnd = INDEX_NONE;
		BulkheadBinding.MeshRange.ExteriorVertexStart = INDEX_NONE;
		BulkheadBinding.MeshRange.ExteriorTriangleStart = INDEX_NONE;
		BulkheadBinding.MeshRange.InteriorVertexStart = INDEX_NONE;
		BulkheadBinding.MeshRange.InteriorTriangleStart = INDEX_NONE;
		BulkheadBinding.MeshRange.LocalCenter = BulkheadSheet.LocalOrigin;
		BulkheadBinding.MeshRange.LocalNormal = BulkheadSheet.LocalNormal;
		BulkheadBinding.MeshRange.LocalBounds = FBox::BuildAABB(
			BulkheadSheet.LocalOrigin,
			FVector(5.f, BulkheadSheet.SizeCm.X * 0.5f, BulkheadSheet.SizeCm.Y * 0.5f));
		LayoutAsset->CompiledSheetBindings.Add(MoveTemp(BulkheadBinding));

		if (Bulkhead.PassageType == EPassageType::SealedBulkhead)
		{
			continue;
		}

		FDoorDef DoorDef;
		DoorDef.DoorId = BulkheadSheetId;
		DoorDef.BulkheadSheetId = BulkheadSheetId;
		DoorDef.PassageType = Bulkhead.PassageType;
		DoorDef.LocalTransform = FTransform(
			FRotator::ZeroRotator,
			FVector(Bulkhead.SpinePositionCm, Bulkhead.DoorOffsetCm.X, Bulkhead.DoorOffsetCm.Y));
		DoorDef.WidthCm = Bulkhead.DoorWidthCm;
		DoorDef.HeightCm = Bulkhead.DoorHeightCm;
		LayoutAsset->Doors.Add(DoorDef);
	}

	for (int32 StationIndex = 0; StationIndex < Solution.Stations.Num(); ++StationIndex)
	{
		const FStationPlacement& Station = Solution.Stations[StationIndex];

		FStationSlotDef SlotDef;
		SlotDef.StationId = FName(*FString::Printf(TEXT("Station_%d_%s"), StationIndex, *UEnum::GetValueAsString(Station.StationType)));
		SlotDef.StationType = Station.StationType;
		SlotDef.CompartmentId = Station.CompartmentId;
		SlotDef.LocalTransform = Station.LocalTransform;
		LayoutAsset->StationSlots.Add(SlotDef);
	}

	for (const FStructuralSheetCompiledBinding& Binding : LayoutAsset->CompiledSheetBindings)
	{
		const bool bExteriorRangeValid = (Binding.MeshRange.ExteriorVertexStart == INDEX_NONE && Binding.MeshRange.ExteriorVertexCount == 0)
			|| (Binding.MeshRange.ExteriorVertexStart >= 0 && Binding.MeshRange.ExteriorVertexCount > 0);
		const bool bInteriorRangeValid = (Binding.MeshRange.InteriorVertexStart == INDEX_NONE && Binding.MeshRange.InteriorVertexCount == 0)
			|| (Binding.MeshRange.InteriorVertexStart >= 0 && Binding.MeshRange.InteriorVertexCount > 0);

		if (!bExteriorRangeValid || !bInteriorRangeValid)
		{
			AddBuildCompilerValidationMessage(
				OutMessages,
				ELayoutValidationSeverity::Warning,
				Binding.SheetId,
				FString::Printf(TEXT("Binding invalide pour %s (ranges ext/int incoherents)"), *Binding.SheetId.ToString()));
		}
	}

	return LayoutAsset;
}
