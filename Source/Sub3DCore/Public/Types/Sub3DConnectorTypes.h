#pragma once

#include "CoreMinimal.h"
#include "Types/Sub3DCanonicalEnums.h"
#include "Sub3DConnectorTypes.generated.h"

USTRUCT(BlueprintType)
struct SUB3DCORE_API FConnectorDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Connector")
    FName ConnectorId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Connector")
    ESub3DConnectorType Type = ESub3DConnectorType::Ladder;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Connector")
    TArray<FName> OpeningIds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Connector")
    FName FromDeckId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Connector")
    FName ToDeckId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Connector", meta=(ClampMin="30.0"))
    float WidthCm = 80.0f;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FCompiledConnectorData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connector")
    FName ConnectorId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connector")
    ESub3DConnectorType Type = ESub3DConnectorType::Ladder;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connector")
    TArray<FName> OpeningIds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connector")
    FName FromDeckId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connector")
    FName ToDeckId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Connector")
    float WidthCm = 80.0f;
};