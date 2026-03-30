#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SubCompilerTypes.h"
#include "SubmarineBuildCompiler.generated.h"

class USubmarineLayoutAsset;

UCLASS()
class SUB3D_API USubmarineBuildCompiler : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SubCompiler")
	USubmarineLayoutAsset* CompileToLayoutAsset(
		const FSubmarineLayoutSolution& Solution,
		UObject* Outer,
		TArray<FLayoutValidationMessage>& OutMessages);
};
