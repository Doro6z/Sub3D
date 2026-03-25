#include "OrganicDeformationGenerator.h"
#include "BiomeFieldProfileDataAsset.h"
#include "CampaignRouteCompiler.h"
#include "WorldGenTypes.h"

// Hash-based 3D value noise. Smooth (Hermite) interpolation. Returns [-1, 1].
float UOrganicDeformationGenerator::ValueNoise3D(const FVector& P, int32 Seed)
{
	const int32 IX = FMath::FloorToInt(P.X);
	const int32 IY = FMath::FloorToInt(P.Y);
	const int32 IZ = FMath::FloorToInt(P.Z);

	const float FX = P.X - IX;
	const float FY = P.Y - IY;
	const float FZ = P.Z - IZ;

	// Hermite smooth step
	const float UX = FX * FX * (3.f - 2.f * FX);
	const float UY = FY * FY * (3.f - 2.f * FY);
	const float UZ = FZ * FZ * (3.f - 2.f * FZ);

	// Hash lattice point to float [-1, 1]
	auto H = [Seed](int32 X, int32 Y, int32 Z) -> float
	{
		const uint32 FNV_OFFSET = 2166136261U;
		const uint32 FNV_PRIME  = 16777619U;
		uint32 h = FNV_OFFSET;
		h ^= (uint32)(X + Seed * 7); h *= FNV_PRIME;
		h ^= (uint32)(Y);            h *= FNV_PRIME;
		h ^= (uint32)(Z);            h *= FNV_PRIME;
		return (float)(int32)h / 2147483648.f;
	};

	const float V000 = H(IX,   IY,   IZ  );
	const float V100 = H(IX+1, IY,   IZ  );
	const float V010 = H(IX,   IY+1, IZ  );
	const float V110 = H(IX+1, IY+1, IZ  );
	const float V001 = H(IX,   IY,   IZ+1);
	const float V101 = H(IX+1, IY,   IZ+1);
	const float V011 = H(IX,   IY+1, IZ+1);
	const float V111 = H(IX+1, IY+1, IZ+1);

	const float X00 = FMath::Lerp(V000, V100, UX);
	const float X10 = FMath::Lerp(V010, V110, UX);
	const float X01 = FMath::Lerp(V001, V101, UX);
	const float X11 = FMath::Lerp(V011, V111, UX);
	const float Y0  = FMath::Lerp(X00,  X10,  UY);
	const float Y1  = FMath::Lerp(X01,  X11,  UY);

	return FMath::Lerp(Y0, Y1, UZ);
}

float UOrganicDeformationGenerator::EvalNoiseCave(const FVector& P, float Freq1, float Freq2,
                                                    const FVector& Offset1, const FVector& Offset2,
                                                    float Threshold)
{
	const FVector SP1 = P * Freq1 + Offset1;
	const FVector SP2 = P * Freq2 + Offset2;

	const float N1 = ValueNoise3D(SP1, 0);
	const float N2 = ValueNoise3D(SP2, 1);

	return Threshold - (N1 * N1 + N2 * N2);
}

bool UOrganicDeformationGenerator::ApplyDeformation(const FRouteGenSpec& Spec,
                                                      const FRouteSeedCascade& Seeds,
                                                      const UBiomeFieldProfileDataAsset* Biome,
                                                      FRouteFieldModel& InOutField) const
{
	if (!Biome) return true; // No biome → skip deformation, not an error

	// Derive offsets from OrganicSeed
	FRandomStream Rng(Seeds.OrganicSeed);
	const FVector Offset1 = FVector(Rng.FRandRange(-1000.f, 1000.f),
	                                Rng.FRandRange(-1000.f, 1000.f),
	                                Rng.FRandRange(-1000.f, 1000.f));
	const FVector Offset2 = FVector(Rng.FRandRange(-1000.f, 1000.f),
	                                Rng.FRandRange(-1000.f, 1000.f),
	                                Rng.FRandRange(-1000.f, 1000.f));

	const float Freq1     = Biome->NoiseFreq1;
	const float Freq2     = Biome->NoiseFreq2;
	const float Threshold = Biome->NoiseCaveThreshold;
	const float Amplitude = Biome->OrganicAmplitude;

	if (Threshold <= KINDA_SMALL_NUMBER || Amplitude <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogRouteGen, Log,
			TEXT("[C7] OrganicDeformation skipped. Threshold=%.4f Amplitude=%.4f"),
			Threshold,
			Amplitude);
		return true;
	}

	UE_LOG(LogRouteGen, Log,
	       TEXT("[C7] OrganicDeformation. Biome=%s Freq1=%.4f Freq2=%.4f Threshold=%.4f Amplitude=%.1f Chunks=%d"),
	       *Biome->GetName(), Freq1, Freq2, Threshold, Amplitude, InOutField.RenderField.Num());

	// Restrict organic deformation to chunks near the guaranteed volume.
	// Applying noise to distant rock chunks opens spurious caves far from the navigable
	// path and produces thin parasitic surfaces in the final mesh.
	TSet<FFieldChunkCoord> ActiveDeformCoords;
	for (const auto& Pair : InOutField.RenderField)
	{
		if (!Pair.Value.bContainsGuaranteedPath) continue;
		for (int32 dx = -1; dx <= 1; dx++)
		for (int32 dy = -1; dy <= 1; dy++)
		for (int32 dz = -1; dz <= 1; dz++)
		{
			FFieldChunkCoord N; N.X = Pair.Key.X + dx; N.Y = Pair.Key.Y + dy; N.Z = Pair.Key.Z + dz;
			if (InOutField.RenderField.Contains(N))
			{
				ActiveDeformCoords.Add(N);
			}
		}
	}

	// Update only the RenderField (SonarField stays guaranteed-only for reliable sonar reads)
	int32 SamplesUplifted = 0;
	int32 ChunksSkipped   = 0;
	for (auto& Pair : InOutField.RenderField)
	{
		if (!ActiveDeformCoords.Contains(Pair.Key))
		{
			ChunksSkipped++;
			continue;
		}

		FRouteFieldChunkData& Chunk = Pair.Value;
		const int32 S  = Chunk.SamplesPerAxis;
		const int32 S1 = S + 1;

		for (int32 iz = 0; iz <= S; iz++)
		for (int32 iy = 0; iy <= S; iy++)
		for (int32 ix = 0; ix <= S; ix++)
		{
			const int32   Idx   = iz * S1 * S1 + iy * S1 + ix;
			const FVector P     = Chunk.ChunkOrigin + FVector(ix, iy, iz) * Chunk.VoxelSize;
			const float   DBase = Chunk.DensitySamples[Idx];

			const float RawCave = EvalNoiseCave(P, Freq1, Freq2, Offset1, Offset2, Threshold);

			// Normalize positive cave signal from [0, Threshold] to [0, Amplitude].
			// Without this, max positive density = Threshold (0.04 cm) which produces
			// razor-thin surface shells instead of proper cave volumes.
			const float NoiseCave = (RawCave > 0.f && Threshold > KINDA_SMALL_NUMBER)
				? (RawCave / Threshold) * Amplitude
				: RawCave;

			// INVARIANT: only enlarge, never close guaranteed path
			const float NewD = FMath::Max(DBase, NoiseCave);
			Chunk.DensitySamples[Idx] = NewD;
			if (NewD > DBase) SamplesUplifted++;
		}
	}

	UE_LOG(LogRouteGen, Log, TEXT("[C7] Done. SamplesUplifted=%d ChunksSkipped=%d/%d"),
	       SamplesUplifted, ChunksSkipped, InOutField.RenderField.Num());

	return true;
}
