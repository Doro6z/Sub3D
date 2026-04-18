#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TraversalTypes.h"
#include "Containers/Set.h"
#include "ChunkAssembler.generated.h"

class UChunkLibrary;

/**
 * Pass 2: Assembles chunk templates into a spatial sequence by snapping connectors.
 */
UCLASS(BlueprintType)
class SUB3D_API UChunkAssembler : public UObject
{
    GENERATED_BODY()

public:
    // Assembles chunks based on the graph and library.
    UFUNCTION(BlueprintCallable, Category="ProcGen")
    TArray<FChunkInstance> AssembleChunks(
        const TArray<FTraversalGraphNode>& Graph,
        const UChunkLibrary* Library,
        int32 RouteSeed) const;

    // Recursive helper to process nodes following the graph structure.
    void ProcessNode(
        int32 NodeID,
        const FTransform& ParentWorldTransform,
        const FChunkConnector& ParentConnectorOut,
        bool bIsRoot,
        const TArray<FTraversalGraphNode>& Graph,
        const UChunkLibrary* Library,
        TArray<FChunkInstance>& OutInstances,
        TSet<int32>& ProcessedNodes) const;

    // Calculates the required world transform for the next chunk to snap to the current one.
    FTransform ResolveNextTransform(
        const FTransform& CurrentWorldTransform,
        const FChunkConnector& ConnectorOut,
        const FChunkConnector& NextConnectorIn) const;

    // Selects a compatible template from the library based on node type and seed.
    const FChunkTemplate* SelectTemplate(
        ENodeType NodeType,
        const UChunkLibrary* Library,
        int32 Seed) const;
};
