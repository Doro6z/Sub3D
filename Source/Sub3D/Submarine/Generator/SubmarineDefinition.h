#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SubmarineDefinitionTypes.h"
#include "SubCompiler/SubmarineGeometryBuilder.h"
#include "Types/Sub3DFloodTypes.h"
#include "SubmarineDefinition.generated.h"

/**
 * Runtime single source of truth for a generated submarine.
 * Produced by USubmarineGenerator from a USubmarineGeneratorSpec.
 * All runtime systems read from this asset — no fallback, no scanning.
 */
UCLASS(BlueprintType)
class SUB3D_API USubmarineDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// ── Hull metrics ─────────────────────────────────────────────────────
	// EditAnywhere so the handmade Craniata import script (and other tooling)
	// can populate the data asset via Python `set_editor_property`. Runtime
	// remains BlueprintReadOnly: gameplay code never mutates the def at runtime.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	float HullLengthCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	float HullBeamCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	float HullHeightCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	float BaseMassKg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	float SubmergedVolumeLiters = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	float WallThicknessCm = 12.f;

	// ── Performance envelope ────────────────────────────────────────────
	// Optional authored performance caps copied onto USubMovementComponent at
	// BeginPlay. A value of 0 means "keep the movement component's default".
	// These let each submarine asset tune its own top speeds without code.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance")
	float MaxForwardSpeedCmS = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance")
	float MaxReverseSpeedCmS = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance")
	float MaxVerticalSpeedCmS = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Performance")
	float MaxThrustN = 0.f;

	// ── Layout ───────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FGeneratedCompartmentDef> Compartments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FGeneratedConnectionDef> Connections;

	// ── Flood Graph ──────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flood")
	FCompiledFloodGraph FloodGraph;

	// ── Sockets ──────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	TArray<FGeneratedStationSlotDef> StationSlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sockets")
	TArray<FGeneratedSpawnPointDef> SpawnPoints;

	// ── Mesh Data ────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	FSubmarineMeshSectionData ExteriorHullMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TArray<FSubmarineInteriorCompartmentMeshData> InteriorMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TArray<FSubmarineBulkheadMeshData> BulkheadMeshes;

	// ── Flood defaults ───────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Defaults", meta = (ClampMin = "0.0"))
	float MaxExteriorInflowLitersPerSec = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Defaults", meta = (ClampMin = "0.0"))
	float DefaultPumpRateLitersPerSec = 100.f;

	// ── Queries ──────────────────────────────────────────────────────────

	/** Find a compartment by id. Returns nullptr if not found. */
	const FGeneratedCompartmentDef* FindCompartment(FName CompartmentId) const;

	/** Find a connection by id. Returns nullptr if not found. */
	const FGeneratedConnectionDef* FindConnection(FName ConnectionId) const;

	/** Get all station slots assigned to a given compartment. */
	TArray<const FGeneratedStationSlotDef*> GetStationsInCompartment(FName CompartmentId) const;

	/**
	 * Find which compartment contains a given point in submarine local space.
	 * Tests against HydroBounds. Returns nullptr if no compartment contains the point.
	 */
	const FGeneratedCompartmentDef* FindCompartmentAtLocalLocation(const FVector& LocalPosition) const;

	/** Basic structural validation. Returns true if the definition is usable at runtime. */
	UFUNCTION(BlueprintPure, Category = "Validation")
	bool IsValid() const;
};
