#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DCanonicalEnums.h"
#include "Sub3DFloodTypes.generated.h"

USTRUCT(BlueprintType)
struct SUB3DCORE_API FDerivedFloodVolume
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    FName VolumeId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    float CapacityLiters = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    FVector BoundsMin = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    FVector BoundsMax = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    TArray<ESub3DRoomTag> RoomTags;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FFloodGraphEdge
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    FName VolumeA = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    FName VolumeB = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    FName ClosureId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    float PassageAreaCm2 = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    bool bExteriorEdge = false;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompiledFloodGraph
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    TArray<FDerivedFloodVolume> Volumes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Derived Flood Volume")
    TArray<FFloodGraphEdge> Edges;
};