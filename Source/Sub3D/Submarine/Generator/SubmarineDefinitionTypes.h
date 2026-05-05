#pragma once

#include "CoreMinimal.h"
#include "SubmarineTypes.h"
#include "SubmarineDefinitionTypes.generated.h"

// --- Enums ---------------------------------------------------------------

UENUM(BlueprintType)
enum class ESubCompartmentType : uint8
{
	Generic  UMETA(DisplayName = "Generic"),
	Helm     UMETA(DisplayName = "Helm"),
	Crew     UMETA(DisplayName = "Crew"),
	Engine   UMETA(DisplayName = "Engine"),
	Airlock  UMETA(DisplayName = "Airlock")
};

UENUM(BlueprintType)
enum class EConnectionType : uint8
{
	Door          UMETA(DisplayName = "Door"),
	Hatch         UMETA(DisplayName = "Hatch"),
	ExteriorHatch UMETA(DisplayName = "Exterior Hatch"),
	Open          UMETA(DisplayName = "Open")
};

UENUM(BlueprintType)
enum class ESpawnRole : uint8
{
	Pilot UMETA(DisplayName = "Pilot"),
	Crew  UMETA(DisplayName = "Crew")
};

UENUM(BlueprintType)
enum class EAirlockSide : uint8
{
	Port      UMETA(DisplayName = "Port"),
	Starboard UMETA(DisplayName = "Starboard"),
	Top       UMETA(DisplayName = "Top")
};

// --- Structs -------------------------------------------------------------

/** Defines the passage type for a single bulkhead in the generator spec. */
USTRUCT(BlueprintType)
struct FBulkheadPassageDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bulkhead")
	EConnectionType PassageType = EConnectionType::Door;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bulkhead", meta = (ClampMin = "60.0"))
	float DoorWidthCm = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bulkhead", meta = (ClampMin = "140.0"))
	float DoorHeightCm = 180.f;
};

/** A compartment produced by the generator. */
USTRUCT(BlueprintType)
struct FGeneratedCompartmentDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	FName CompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	ESubCompartmentType SemanticType = ESubCompartmentType::Generic;

	/** Total floodable volume in liters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	float CapacityLiters = 0.f;

	/**
	 * Compartment bounding box, submarine-local space. CONTRACT:
	 *   - Must FULLY ENCLOSE the compartment's interior geometry: hull walls,
	 *     bulkheads, ceiling, floor, props.
	 *   - Must STOP at structural boundaries: doors, bulkheads to neighbouring
	 *     compartments, decks above/below. The volume must NOT extend into
	 *     adjacent compartments (the bake's SDF + Marching Squares would then
	 *     pick up neighbour walls as if they were boundaries of THIS compartment).
	 *   - X/Y: clip exactly at door/bulkhead planes shared with adjacent compartments.
	 *   - Z:   clip exactly at the deck above/below (with a small margin like ±5 cm
	 *          to ensure floor/ceiling geometry is captured by voxelisation).
	 *
	 * This bounds drives:
	 *   1. Auto-spawn of UCompartmentVolumeComponent (crew overlap detection,
	 *      water plane sizing) — see ASubmarineBase::EnsureCompartmentVolumesFromDefinition.
	 *   2. Phase 2 offline bake voxelisation volume (cap mesh generation).
	 *
	 * Common authoring mistake: setting Z range to [WalkableFloorZCm,
	 * WalkableFloorZCm + MaxWaterHeightCm]. That captures water-volume only,
	 * which is INSUFFICIENT — the volume must include the walkable headroom
	 * above the water cap up to the ceiling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	FVector HydroBoundsMin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	FVector HydroBoundsMax = FVector::ZeroVector;

	/**
	 * Maximum water height in cm, measured from WalkableFloorZCm.
	 * This is the WATER cap, may be less than the compartment's full Z extent
	 * (HydroBoundsMax.Z - WalkableFloorZCm). Floor-to-ceiling can be > MaxWaterHeightCm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	float MaxWaterHeightCm = 0.f;

	/** Z coordinate of the walkable floor in submarine local space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compartment")
	float WalkableFloorZCm = 0.f;
};

/** A connection (door, hatch, or opening) between two compartments or to exterior. */
USTRUCT(BlueprintType)
struct FGeneratedConnectionDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FName ConnectionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FName CompartmentA = NAME_None;

	/** NAME_None if this is an exterior connection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FName CompartmentB = NAME_None;

	/** Cross-sectional area of the passage in cm2. Controls flow rate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	float FlowAreaCm2 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FTransform LocalTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	EConnectionType ConnectionType = EConnectionType::Door;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	bool bStartsClosed = true;

	/** Door opening width for mesh generation (bulkhead cutout). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	float DoorWidthCm = 90.f;

	/** Door opening height for mesh generation (bulkhead cutout). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	float DoorHeightCm = 180.f;
};

/** A station slot placed by the generator inside a compartment. */
USTRUCT(BlueprintType)
struct FGeneratedStationSlotDef
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

/** A crew spawn point placed by the generator. */
USTRUCT(BlueprintType)
struct FGeneratedSpawnPointDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FName SpawnId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FTransform LocalTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	ESpawnRole Role = ESpawnRole::Crew;
};
