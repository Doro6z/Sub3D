#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DCanonicalEnums.h"
#include "Sub3DClosureTypes.generated.h"

USTRUCT(BlueprintType)
struct SUB3DCORE_API FClosureDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Closure")
    FName ClosureId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Closure")
    ESub3DClosureType Type = ESub3DClosureType::Door;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Closure")
    FName OpeningId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Closure")
    bool bClosedByDefault = true;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompiledClosureData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Closure")
    FName ClosureId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Closure")
    ESub3DClosureType Type = ESub3DClosureType::Door;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Closure")
    FName OpeningId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Closure")
    bool bClosedByDefault = true;
};