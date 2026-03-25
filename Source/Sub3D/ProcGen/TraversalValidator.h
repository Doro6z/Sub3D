#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TraversalTypes.h"
#include "TraversalValidator.generated.h"

/**
 * Pass 4: Validates the generated layout against hard gameplay rules and soft scoring rules.
 */
UCLASS(BlueprintType)
class SUB3D_API UTraversalValidator : public UObject
{
    GENERATED_BODY()

public:
    // Largeur utile minimale (cm) — SubWidth(27m) + marge = 5400cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Validation")
    float MinUsableWidth = 5400.f;

    // Rayon de virage minimum (cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Validation")
    float MinTurnRadius = 3500.f;

    // Score minimum acceptable (soft rules)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Validation")
    float MinLayoutScore = 0.5f;

    // Validates a full layout.
    UFUNCTION(BlueprintCallable, Category="ProcGen")
    EValidationResult ValidateLayout(const TArray<FChunkInstance>& Instances, float& OutScore) const;

private:
    // Hard rules: width and turning radius on each connector.
    bool CheckHardRules(const FChunkInstance& Instance) const;

    // Soft rules: penalize repetitive sequences, etc.
    float ScoreSoftRules(const TArray<FChunkInstance>& Instances) const;
};
