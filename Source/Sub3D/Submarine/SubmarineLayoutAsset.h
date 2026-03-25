#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StructuralHullTypes.h"
#include "SubmarineLayoutAsset.generated.h"

UCLASS(BlueprintType)
class SUB3D_API USubmarineLayoutAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FSubCompartmentDef> Compartments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FStructuralSheetDef> StructuralSheets;
};

