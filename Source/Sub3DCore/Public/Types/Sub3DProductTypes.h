#pragma once

#include "CoreMinimal.h"
#include "Sub3DProductTypes.generated.h"

UENUM(BlueprintType)
enum class ESub3DEditContext : uint8
{
    Dev UMETA(DisplayName="Dev"),
    Lobby UMETA(DisplayName="Lobby"),
    Shipyard UMETA(DisplayName="Shipyard"),
    Station UMETA(DisplayName="Station"),
    Mission UMETA(DisplayName="Mission")
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FSubmarineCatalogEntryDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    FName CatalogId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    FSoftObjectPath RuntimeAssetPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    int32 Tier = 1;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FOwnedSubmarineState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    FGuid OwnedSubmarineId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    FName CatalogId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    ESub3DEditContext EditContext = ESub3DEditContext::Dev;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    bool bAllowStructuralEdit = true;
};

USTRUCT(BlueprintType)
struct SUB3DCORE_API FSubmarineEditPermissions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    ESub3DEditContext Context = ESub3DEditContext::Dev;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    bool bAllowLevelA = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    bool bAllowLevelB = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    bool bAllowLevelC = true;
};