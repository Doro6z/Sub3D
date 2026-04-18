#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TraversalTypes.h"
#include "TraversalGraphGenerator.generated.h"

/**
 * Pass 1: Generates an abstract oriented graph of nodes representing the mission flow.
 */
UCLASS(BlueprintType)
class SUB3D_API UTraversalGraphGenerator : public UObject
{
    GENERATED_BODY()

public:
    // Generates the graph based on the specification.
    UFUNCTION(BlueprintCallable, Category="ProcGen")
    TArray<FTraversalGraphNode> GenerateGraph(const FTraversalGenSpec& Spec) const;

private:
    // Selects a node type based on the mission pacing position (0.0 to 1.0) and a seed.
    ENodeType SelectNodeTypeForPacing(float PacingPos, int32 Seed) const;

    // Deterministic hash for two integers (FNV-1a 32-bit).
    int32 HashInts(int32 A, int32 B) const;
};
