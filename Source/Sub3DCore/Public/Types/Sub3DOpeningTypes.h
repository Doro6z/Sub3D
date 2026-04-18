#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DCanonicalEnums.h"
#include "Sub3DOpeningTypes.generated.h"

USTRUCT(BlueprintType)
struct SUB3DCORE_API FOpeningDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Opening")
    FName OpeningId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Opening")
    ESub3DOpeningType Type = ESub3DOpeningType::Doorway;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Opening")
    FName ParentFloorRegionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Opening")
    FVector LocalPosition = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Opening")
    FVector2D SizeCm = FVector2D(90.0f, 190.0f);
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompiledOpeningData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Opening")
    FName OpeningId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Opening")
    ESub3DOpeningType Type = ESub3DOpeningType::Doorway;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Opening")
    FName ParentFloorRegionId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Opening")
    FVector LocalPosition = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Opening")
    FVector2D SizeCm = FVector2D(90.0f, 190.0f);
};