#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SubCompilerTypes.h"
#include "SubmarineFunctionalGraph.generated.h"

UCLASS(BlueprintType)
class SUB3D_API USubmarineFunctionalGraph : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graph")
	TArray<FCompartmentNode> Compartments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graph")
	TArray<FPassageEdge> Passages;

	bool ValidateGraph(TArray<FLayoutValidationMessage>& OutMessages) const;
};
