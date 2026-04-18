#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Types/Sub3DHullTypes.h"
#include "SubmarineBakeSubsystem.generated.h"

class USub3DSubmarineAuthoringAsset;
class UCompiledSubmarineBaseAsset;
class UCompiledSubmarineRuntimeAsset;

UCLASS(BlueprintType)
class SUB3DBAKE_API USubmarineBakeSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Submarine|Bake")
    bool ValidateAuthoringAsset(USub3DSubmarineAuthoringAsset* Asset, TArray<FString>& OutErrors);

    UFUNCTION(BlueprintCallable, Category="Submarine|Bake")
    bool BakeBase(USub3DSubmarineAuthoringAsset* Asset);

    UFUNCTION(BlueprintCallable, Category="Submarine|Bake")
    bool BakeRuntime(USub3DSubmarineAuthoringAsset* Asset);

    UFUNCTION(BlueprintCallable, Category="Submarine|Bake")
    bool FullBake(USub3DSubmarineAuthoringAsset* Asset);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Submarine|Bake")
    TObjectPtr<UCompiledSubmarineBaseAsset> LastBakedBaseAsset = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Submarine|Bake")
    TObjectPtr<UCompiledSubmarineRuntimeAsset> LastBakedRuntimeAsset = nullptr;

private:
    static TArray<FControlRingDef> ResolveEffectiveControlRings(
        const USub3DSubmarineAuthoringAsset* Asset,
        TArray<FString>& OutErrors);
};

