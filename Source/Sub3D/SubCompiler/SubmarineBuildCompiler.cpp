#include "SubmarineBuildCompiler.h"

#include "Submarine/SubmarineLayoutAsset.h"
#include "Submarine/StructuralHullTypes.h"

namespace
{
void AddValidationMessage(
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
	OutSheets.Add(Sheet);
}
}

USubmarineLayoutAsset* USubmarineBuildCompiler::CompileToLayoutAsset(
	const FSubmarineLayoutSolution& Solution,
	UObject* Outer,
	TArray<FLayoutValidationMessage>& OutMessages)
{
	OutMessages.Reset();

	if (Solution.HasErrors())
	{
		OutMessages = Solution.ValidationMessages;
		AddValidationMessage(OutMessages, ELayoutValidationSeverity::Error, NAME_None, TEXT("Solution invalide : compilation refusee"));
		return nullptr;
	}

	if (Solution.Compartments.Num() == 0)
	{
		AddValidationMessage(OutMessages, ELayoutValidationSeverity::Error, NAME_None, TEXT("Solution vide : aucun compartiment a compiler"));
		return nullptr;
	}

	UObject* EffectiveOuter = Outer ? Outer : GetTransientPackage();
	USubmarineLayoutAsset* LayoutAsset = NewObject<USubmarineLayoutAsset>(EffectiveOuter);
	if (!LayoutAsset)
	{
		AddValidationMessage(OutMessages, ELayoutValidationSeverity::Error, NAME_None, TEXT("Creation du LayoutAsset echouee"));
		return nullptr;
	}

	LayoutAsset->Compartments.Reset();
	LayoutAsset->StructuralSheets.Reset();
	LayoutAsset->Doors.Reset();
	LayoutAsset->StationSlots.Reset();
	LayoutAsset->Metrics = Solution.Metrics;

	for (const FCompartmentPlacement& Placement : Solution.Compartments)
	{
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

		AddExteriorSheet(
			LayoutAsset->StructuralSheets,
			MakeExteriorSheetId(Placement.CompartmentId, TEXT("Port")),
			Placement.CompartmentId,
			FVector(MidX, -RadiusCm, 0.f),
			FVector(0.f, -1.f, 0.f),
			FVector(1.f, 0.f, 0.f),
			FVector(0.f, 0.f, 1.f),
			SideSize);

		AddExteriorSheet(
			LayoutAsset->StructuralSheets,
			MakeExteriorSheetId(Placement.CompartmentId, TEXT("Starboard")),
			Placement.CompartmentId,
			FVector(MidX, RadiusCm, 0.f),
			FVector(0.f, 1.f, 0.f),
			FVector(1.f, 0.f, 0.f),
			FVector(0.f, 0.f, 1.f),
			SideSize);

		AddExteriorSheet(
			LayoutAsset->StructuralSheets,
			MakeExteriorSheetId(Placement.CompartmentId, TEXT("Top")),
			Placement.CompartmentId,
			FVector(MidX, 0.f, RadiusCm),
			FVector(0.f, 0.f, 1.f),
			FVector(1.f, 0.f, 0.f),
			FVector(0.f, 1.f, 0.f),
			CapSize);

		AddExteriorSheet(
			LayoutAsset->StructuralSheets,
			MakeExteriorSheetId(Placement.CompartmentId, TEXT("Bottom")),
			Placement.CompartmentId,
			FVector(MidX, 0.f, -RadiusCm),
			FVector(0.f, 0.f, -1.f),
			FVector(1.f, 0.f, 0.f),
			FVector(0.f, 1.f, 0.f),
			CapSize);
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
		LayoutAsset->StructuralSheets.Add(BulkheadSheet);

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

	return LayoutAsset;
}
