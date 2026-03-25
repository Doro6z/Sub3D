#include "CampaignRouteCompiler.h"

int32 UCampaignRouteCompiler::HashInts(int32 A, int32 B)
{
	const uint32 FNV_OFFSET = 2166136261U;
	const uint32 FNV_PRIME  = 16777619U;
	uint32 H = FNV_OFFSET;
	H ^= (uint32)A; H *= FNV_PRIME;
	H ^= (uint32)B; H *= FNV_PRIME;
	return (int32)H;
}

FRouteSeedCascade UCampaignRouteCompiler::DeriveSeedCascade(const FRouteGenSpec& Spec)
{
	FRouteSeedCascade Seeds;
	Seeds.RouteSeed      = HashInts(Spec.CampaignSeed, Spec.RouteID);
	Seeds.TopologySeed   = HashInts(Seeds.RouteSeed, 1);
	Seeds.VolumeSeed     = HashInts(Seeds.RouteSeed, 2);
	Seeds.OrganicSeed    = HashInts(Seeds.RouteSeed, 3);
	Seeds.SemanticSeed   = HashInts(Seeds.RouteSeed, 4);
	Seeds.PopulationSeed = HashInts(Seeds.RouteSeed, 5);
	return Seeds;
}

bool UCampaignRouteCompiler::BuildRouteSpec(int32 MasterSeed, int32 RouteID, FRouteGenSpec& OutSpec, FRouteSeedCascade& OutSeeds) const
{
	OutSpec.CampaignSeed = MasterSeed;
	OutSpec.RouteID      = RouteID;
	// Caller overrides RouteLengthMeters, BiomeID, DifficultyTier, etc.
	OutSeeds = DeriveSeedCascade(OutSpec);
	return true;
}
