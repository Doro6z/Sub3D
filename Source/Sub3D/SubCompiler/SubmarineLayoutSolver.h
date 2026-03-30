#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SubCompilerTypes.h"
#include "SubmarineLayoutSolver.generated.h"

class USubmarineEnvelopeDef;
class USubmarineFunctionalGraph;

UCLASS()
class SUB3D_API USubmarineLayoutSolver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SubCompiler")
	bool Solve(
		const USubmarineEnvelopeDef* Envelope,
		const USubmarineFunctionalGraph* Graph,
		FSubmarineLayoutSolution& OutSolution,
		TArray<FLayoutValidationMessage>& OutMessages);
};
