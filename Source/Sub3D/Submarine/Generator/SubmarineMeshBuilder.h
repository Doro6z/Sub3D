#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SubmarineMeshBuilder.generated.h"

class USubmarineDefinition;
class USubmarineGeneratorEnvelopeDef;

/**
 * Stateless mesh builder that reads a USubmarineDefinition + USubmarineGeneratorEnvelopeDef
 * and populates the definition's mesh data arrays (exterior hull, interior
 * compartments, bulkheads). Superellipse geometry math is self-contained --
 * no dependency on FSubmarineLayoutSolution or the legacy compiler path.
 */
UCLASS(BlueprintType)
class SUB3D_API USubmarineMeshBuilder : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Populate the mesh data arrays on the given definition.
	 * Reads compartments, connections, hull metrics, and WallThicknessCm
	 * from the definition. Reads envelope profile for cross-section math.
	 *
	 * Returns false on failure (null inputs, zero compartments, etc.).
	 */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Generator")
	bool BuildMeshData(
		USubmarineDefinition* Definition,
		const USubmarineGeneratorEnvelopeDef* Envelope,
		int32 RadialSegments = 32,
		int32 InteriorArcSegments = 24,
		int32 LongitudinalSubdivisionsPerSpan = 6);

private:
	bool BuildExteriorHull(
		USubmarineDefinition* Definition,
		const USubmarineGeneratorEnvelopeDef* Envelope,
		int32 RadialSegments,
		int32 LongitudinalSubdivisionsPerSpan);

	bool BuildInteriorCompartments(
		USubmarineDefinition* Definition,
		const USubmarineGeneratorEnvelopeDef* Envelope,
		int32 InteriorArcSegments);

	bool BuildBulkheads(
		USubmarineDefinition* Definition,
		const USubmarineGeneratorEnvelopeDef* Envelope);
};
