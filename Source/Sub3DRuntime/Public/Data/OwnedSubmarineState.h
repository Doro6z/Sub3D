#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/Sub3DProductTypes.h"
#include "OwnedSubmarineState.generated.h"

UCLASS(BlueprintType)
class SUB3DRUNTIME_API UOwnedSubmarineStateAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    FOwnedSubmarineState OwnedState;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Product")
    FSubmarineEditPermissions Permissions;

    UFUNCTION(BlueprintCallable, Category="Product")
    bool CanEditStructural() const
    {
        if (OwnedState.EditContext == ESub3DEditContext::Mission)
        {
            return false;
        }

        if (OwnedState.EditContext == ESub3DEditContext::Shipyard)
        {
            return Permissions.bAllowLevelA || Permissions.bAllowLevelB || Permissions.bAllowLevelC;
        }

        return OwnedState.bAllowStructuralEdit;
    }
};
