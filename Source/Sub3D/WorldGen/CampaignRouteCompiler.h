#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorldGenTypes.h"
#include "CampaignRouteCompiler.generated.h"

// C3 — Transforms a RouteID + MasterSeed into FRouteGenSpec + FRouteSeedCascade.
// Seeds are derived deterministically via FNV-1a. No external data needed.
UCLASS()
class SUB3D_API UCampaignRouteCompiler : public UObject
{
	GENERATED_BODY()

public:
	// Build a route spec from a master seed and route ID.
	// OutSpec is populated with defaults; caller can override per-field after this call.
	bool BuildRouteSpec(int32 MasterSeed, int32 RouteID, FRouteGenSpec& OutSpec, FRouteSeedCascade& OutSeeds) const;

	// Utility: derive seed cascade from an already-populated spec.
	static FRouteSeedCascade DeriveSeedCascade(const FRouteGenSpec& Spec);

	// FNV-1a 32-bit hash of two integers. Used across the entire WorldGen pipeline.
	static int32 HashInts(int32 A, int32 B);
};
