#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/Sub3DProductTypes.h"
#include "SubmarineCatalogEntry.generated.h"

UCLASS(BlueprintType)
class SUB3DRUNTIME_API USubmarineCatalogEntry : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    FSubmarineCatalogEntryDef CatalogEntry;
};