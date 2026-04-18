#pragma once

#include "CoreMinimal.h"
#include "Sub3DBayTypes.generated.h"

USTRUCT(BlueprintType)
struct SUB3DCORE_API FStructuralBayDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Structural Bay")
    FName BayId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Structural Bay")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Structural Bay", meta=(ClampMin="0.0", ClampMax="1.0"))
    float StartAlpha = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Structural Bay", meta=(ClampMin="0.0", ClampMax="1.0"))
    float EndAlpha = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Structural Bay", meta=(ClampMin="1"))
    int32 RequestedDeckLevels = 1;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompiledBayData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structural Bay")
    FName BayId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structural Bay")
    float StartAlpha = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structural Bay")
    float EndAlpha = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structural Bay")
    float StartX = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structural Bay")
    float EndX = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structural Bay")
    int32 MaxDeckLevels = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structural Bay")
    TArray<FName> BoundaryFrameRingIds;
};