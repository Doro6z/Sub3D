#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/Sub3DBayTypes.h"
#include "Types/Sub3DClosureTypes.h"
#include "Types/Sub3DCompiledTypes.h"
#include "Types/Sub3DConnectorTypes.h"
#include "Types/Sub3DEnvelopeTypes.h"
#include "Types/Sub3DFloodTypes.h"
#include "Types/Sub3DFloorTypes.h"
#include "Types/Sub3DOpeningTypes.h"
#include "Types/Sub3DPartitionTypes.h"
#include "CompiledSubmarineRuntimeAsset.generated.h"

UCLASS(BlueprintType)
class SUB3DRUNTIME_API UCompiledSubmarineRuntimeAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pressure Hull")
    float HullLengthCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pressure Hull")
    FSub3DCompiledHullData HullData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pressure Hull")
    FCompiledMeshSection ExteriorHull;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pressure Hull")
    FCompiledMeshSection InteriorHull;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pressure Hull")
    FCompiledMeshSection CollisionProxy;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pressure Hull")
    FCompiledHullOwnership HullOwnership;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pressure Hull")
    TArray<FCompiledFrameRingData> FrameRings;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Outer Envelope")
    FCompiledMeshSection OuterEnvelope;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Outer Envelope")
    FCompiledMeshSection OuterEnvelopeCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Appendages")
    FCompiledMeshSection SailMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Appendages")
    FCompiledMeshSection BowMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Appendages")
    FCompiledMeshSection SternMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structural Bays")
    TArray<FCompiledBayData> StructuralBays;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Deck Levels")
    TArray<FCompiledDeckData> CompiledDecks;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floor Regions")
    TArray<FCompiledFloorRegionData> CompiledFloorRegions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Openings")
    TArray<FCompiledOpeningData> CompiledOpenings;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connectors")
    TArray<FCompiledConnectorData> CompiledConnectors;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Closures")
    TArray<FCompiledClosureData> CompiledClosures;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Partitions")
    TArray<FCompiledPartitionData> CompiledPartitions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    FCompiledFloodGraph FloodGraph;
};
