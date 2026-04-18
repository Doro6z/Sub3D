#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SubmarineDefinitionTypes.h"
#include "SubmarineGeneratorSpec.generated.h"

class USubmarineGeneratorEnvelopeDef;

/**
 * Editor-authored input for the submarine generator.
 * Defines hull envelope, bulkhead layout, airlock placement,
 * and requested stations. Fed to USubmarineGenerator to produce
 * a USubmarineDefinition.
 */
UCLASS(BlueprintType)
class SUB3D_API USubmarineGeneratorSpec : public UDataAsset
{
	GENERATED_BODY()

public:
	// ── Hull ─────────────────────────────────────────────────────────────

	/** Hull profile (cross-section, length, bow/stern taper). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	TObjectPtr<USubmarineGeneratorEnvelopeDef> Envelope;

	// ── Bulkheads ────────────────────────────────────────────────────────

	/**
	 * Normalized positions (0..1) along the spine where bulkheads are placed.
	 * Values must be strictly increasing. Bow and stern caps are implicit.
	 * N bulkheads produce N+1 compartments (the segments between them,
	 * plus the first and last segments bounded by bow/stern).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bulkheads")
	TArray<float> BulkheadPositionsNormalized;

	/**
	 * Passage definition for each bulkhead. Must match the length of
	 * BulkheadPositionsNormalized. Each entry defines the door/hatch
	 * type and dimensions for that bulkhead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bulkheads")
	TArray<FBulkheadPassageDef> Passages;

	// ── Airlock ──────────────────────────────────────────────────────────

	/** Normalized position (0..1) along the spine for the airlock attachment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Airlock", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirlockPositionNormalized = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Airlock")
	EAirlockSide AirlockSide = EAirlockSide::Port;

	// ── Stations & Spawns ────────────────────────────────────────────────

	/** Station types the generator should place inside compartments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stations")
	TArray<ESubStationType> RequestedStations;

	// ── Config ───────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "2.0", ClampMax = "20.0"))
	float WallThicknessCm = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	float FloorDropBiasCm = 0.f;

	// ── Validation ───────────────────────────────────────────────────────

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
