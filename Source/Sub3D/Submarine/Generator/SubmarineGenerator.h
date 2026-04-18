#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SubmarineDefinitionTypes.h"
#include "SubmarineGenerator.generated.h"

class USubmarineDefinition;
class USubmarineGeneratorSpec;
class USubmarineGeneratorEnvelopeDef;

/**
 * Stateless generator that reads a USubmarineGeneratorSpec and produces
 * a fully populated USubmarineDefinition. No runtime state, no mesh output.
 * Mesh generation is a separate step (Phase 5B).
 */
UCLASS(BlueprintType)
class SUB3D_API USubmarineGenerator : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Generate a complete USubmarineDefinition from a spec.
	 * Returns nullptr on failure (invalid spec, missing envelope, etc.).
	 * The returned asset is an outer-less transient UObject owned by the caller.
	 */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Generator")
	USubmarineDefinition* Generate(const USubmarineGeneratorSpec* Spec);

private:
	// Step 1: Read envelope metrics into the definition.
	bool ResolveEnvelope(const USubmarineGeneratorEnvelopeDef* Envelope, USubmarineDefinition* Def) const;

	// Step 2: Derive compartments from bulkhead positions along the spine.
	bool DeriveCompartments(
		const USubmarineGeneratorEnvelopeDef* Envelope,
		const TArray<float>& BulkheadPositions,
		float WallThicknessCm,
		float FloorDropBiasCm,
		USubmarineDefinition* Def) const;

	// Step 3: Generate connections from bulkhead passages.
	bool GenerateConnections(
		const USubmarineGeneratorSpec* Spec,
		USubmarineDefinition* Def) const;

	// Step 4: Generate the airlock compartment and its two connections.
	bool GenerateAirlock(
		const USubmarineGeneratorSpec* Spec,
		USubmarineDefinition* Def) const;

	// Step 5: Compile the flood graph from compartments and connections.
	bool BuildFloodGraph(USubmarineDefinition* Def) const;

	// Step 6: Place station slots in compartments.
	bool PlaceStations(
		const USubmarineGeneratorSpec* Spec,
		USubmarineDefinition* Def) const;

	// Step 7: Place crew spawn points.
	bool PlaceSpawns(USubmarineDefinition* Def) const;

	// Helpers
	float ComputeCompartmentVolumeLiters(
		const USubmarineGeneratorEnvelopeDef* Envelope,
		float StartNorm,
		float EndNorm,
		float FloorZLocal,
		float WallThicknessCm) const;

	float ComputeFloorZ(
		const USubmarineGeneratorEnvelopeDef* Envelope,
		float NormalizedPosition,
		float WallThicknessCm,
		float FloorDropBiasCm) const;

	ESubCompartmentType InferSemanticType(int32 CompartmentIndex, int32 TotalCompartments) const;
};
