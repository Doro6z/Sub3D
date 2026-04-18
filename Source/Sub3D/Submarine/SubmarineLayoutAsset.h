#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StructuralHullTypes.h"
#include "SubCompiler/SubCompilerTypes.h"
#include "SubmarineLayoutAsset.generated.h"

USTRUCT(BlueprintType)
struct FWalkableSurfaceDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walkable", meta = (Categories = "Submarine.Compartment"))
	FName CompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walkable")
	FTransform LocalTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walkable", meta = (ClampMin = "0.0"))
	float WidthCm = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walkable", meta = (ClampMin = "0.0"))
	float LengthCm = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walkable")
	FName SurfaceType = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walkable")
	FName CollisionProfileName = FName("SubInteriorWalkable");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walkable")
	bool bSupportsCrew = true;
};

USTRUCT(BlueprintType)
struct FDoorDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FName DoorId = NAME_None;

	// Proto04A contract: keep this identical to DoorId until the flooding
	// runtime decouples door lookup from sheet ids.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FName BulkheadSheetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	EPassageType PassageType = EPassageType::WatertightDoor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FTransform LocalTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = "0.0"))
	float WidthCm = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = "0.0"))
	float HeightCm = 180.f;
};

USTRUCT(BlueprintType)
struct FStationSlotDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FName StationId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	ESubStationType StationType = ESubStationType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FName CompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FTransform LocalTransform = FTransform::Identity;
};

// LEGACY (Phase 7A, 2026-04-10) — Temporary proto fallback path only.
// USubmarineLayoutAsset is not deprecated compile-time because it is still the
// real runtime source of truth until the SubmarineGenerator path (Phase 5D)
// becomes the default. Do not add new consumers. Remove once generator path is
// the only init route, then promote to UE_DEPRECATED in Phase 7B.
UCLASS(BlueprintType)
class SUB3D_API USubmarineLayoutAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FSubCompartmentDef> Compartments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FStructuralSheetDef> StructuralSheets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Compiled")
	TArray<FStructuralSheetCompiledBinding> CompiledSheetBindings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Doors")
	TArray<FDoorDef> Doors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Stations")
	TArray<FStationSlotDef> StationSlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Walkable")
	TArray<FWalkableSurfaceDef> WalkableSurfaces;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Metrics")
	FSubmarineBuildMetrics Metrics;
};
