#include "SonarFieldComponent.h"

USonarFieldComponent::USonarFieldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USonarFieldComponent::InitializeFromField(const FRouteFieldModel& Field)
{
	CachedSonarField = Field.SonarField;
}

bool USonarFieldComponent::SampleOcclusionAlongRay(const FVector& Start, const FVector& End, float& OutBlockage) const
{
	// Proto 04 STUB. Implement in Proto 05 using CachedSonarField.
	OutBlockage = 0.f;
	return false;
}

void USonarFieldComponent::CollectCoarseWaterSurfacePoints(
	const FVector& Center,
	float RadiusCm,
	int32 MaxPoints,
	TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	if (CachedSonarField.Num() == 0 || RadiusCm <= 0.f || MaxPoints <= 0)
	{
		return;
	}

	const float RadiusSq = FMath::Square(RadiusCm);
	const int32 SafeMaxPoints = FMath::Max(1, MaxPoints);

	for (const TPair<FFieldChunkCoord, FRouteFieldChunkData>& Pair : CachedSonarField)
	{
		const FRouteFieldChunkData& Chunk = Pair.Value;
		const int32 S = Chunk.SamplesPerAxis;
		const int32 S1 = S + 1;
		if (S <= 0 || Chunk.OccupancySamples.Num() < S1 * S1 * S1)
		{
			continue;
		}

		const FVector ChunkExtent = FVector(Chunk.VoxelSize * static_cast<float>(S) * 0.5f);
		const FVector ChunkCenter = Chunk.ChunkOrigin + ChunkExtent;
		const float ChunkSphereRadius = ChunkExtent.Size();
		if (FVector::DistSquared(ChunkCenter, Center) > FMath::Square(RadiusCm + ChunkSphereRadius))
		{
			continue;
		}

		auto ToIndex = [S1](int32 X, int32 Y, int32 Z) -> int32
		{
			return Z * S1 * S1 + Y * S1 + X;
		};

		for (int32 Z = 1; Z < S; ++Z)
		{
			for (int32 Y = 1; Y < S; ++Y)
			{
				for (int32 X = 1; X < S; ++X)
				{
					const int32 Idx = ToIndex(X, Y, Z);
					if (!Chunk.OccupancySamples.IsValidIndex(Idx) || Chunk.OccupancySamples[Idx] != 0)
					{
						continue;
					}

					const bool bIsBoundaryWater =
						Chunk.OccupancySamples[ToIndex(X + 1, Y, Z)] != 0 ||
						Chunk.OccupancySamples[ToIndex(X - 1, Y, Z)] != 0 ||
						Chunk.OccupancySamples[ToIndex(X, Y + 1, Z)] != 0 ||
						Chunk.OccupancySamples[ToIndex(X, Y - 1, Z)] != 0 ||
						Chunk.OccupancySamples[ToIndex(X, Y, Z + 1)] != 0 ||
						Chunk.OccupancySamples[ToIndex(X, Y, Z - 1)] != 0;
					if (!bIsBoundaryWater)
					{
						continue;
					}

					const FVector WorldPos = Chunk.ChunkOrigin + FVector(X, Y, Z) * Chunk.VoxelSize;
					if (FVector::DistSquared(WorldPos, Center) > RadiusSq)
					{
						continue;
					}

					OutPoints.Add(WorldPos);
					if (OutPoints.Num() >= SafeMaxPoints)
					{
						return;
					}
				}
			}
		}
	}
}

int32 USonarFieldComponent::GetSonarVoxelCount() const
{
	int32 Total = 0;
	for (const auto& Pair : CachedSonarField)
		Total += Pair.Value.OccupancySamples.Num();
	return Total;
}
