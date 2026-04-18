#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DCanonicalEnums.h"
#include "Types/Sub3DCompiledTypes.h"
#include "Sub3DPartitionTypes.generated.h"

USTRUCT(BlueprintType)
struct SUB3DCORE_API FPressureBulkheadDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition")
    FName BulkheadId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SpineAlpha = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition", meta=(ClampMin="1.0"))
    float ThicknessCm = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition", meta=(ClampMin="0.1"))
    float StrengthMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition")
    TArray<FName> OpeningIds;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FInternalWallDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition")
    FName WallId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition")
    FName ParentDeckId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition")
    FVector LocalOrigin = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition")
    FRotator LocalRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition")
    FVector2D SizeCm = FVector2D(300.0f, 250.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition", meta=(ClampMin="1.0"))
    float ThicknessCm = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Partition")
    TArray<FName> OpeningIds;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompiledPartitionData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Partition")
    FName PartitionId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Partition")
    ESub3DPartitionType Type = ESub3DPartitionType::InternalWall;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Partition")
    FCompiledMeshSection PartitionMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Partition")
    float StrengthMultiplier = 1.0f;
};