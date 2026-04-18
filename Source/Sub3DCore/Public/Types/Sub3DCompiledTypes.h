#pragma once

#include "CoreMinimal.h"

#include "Types/Sub3DCanonicalEnums.h"
#include "Sub3DCompiledTypes.generated.h"

USTRUCT(BlueprintType)
struct SUB3DCORE_API FSub3DCompiledHullSection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Submarine|Hull")
    float PositionX = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Submarine|Hull")
    float HalfWidthCm = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Submarine|Hull")
    float HalfHeightCm = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Submarine|Hull")
    ESub3DSectionProfile SectionProfile = ESub3DSectionProfile::Ellipse;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Submarine|Hull")
    float SectionRoundness = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Submarine|Hull")
    float WallThicknessCm = 12.0f;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FSub3DCompiledHullData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Submarine|Hull")
    TArray<FSub3DCompiledHullSection> Sections;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Submarine|Hull")
    float LengthCm = 0.0f;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompiledMeshSection
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName SectionId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 MaterialSlotIndex = INDEX_NONE;

    UPROPERTY()
    TArray<FVector3f> Positions;

    UPROPERTY()
    TArray<FVector3f> Normals;

    UPROPERTY()
    TArray<FVector2f> UV0;

    UPROPERTY()
    TArray<int32> Indices;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompiledHullOwnership
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FName> TriangleOwnerIds;
};

struct FSub3DCompiledBaseData;
struct FSub3DCompiledRuntimeData;

