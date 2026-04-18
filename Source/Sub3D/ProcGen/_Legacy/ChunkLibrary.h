#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TraversalTypes.h"
#include "ChunkLibrary.generated.h"

/**
 * Registry of all authorized chunk templates for procedural traversal generation.
 */
UCLASS(BlueprintType)
class SUB3D_API UChunkLibrary : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chunks")
    TArray<FChunkTemplate> Templates;

    // Returns all templates compatible with a given node type.
    UFUNCTION(BlueprintPure, Category="ProcGen")
    TArray<FChunkTemplate> GetCompatibleTemplates(ENodeType NodeType) const;

    // Checks if two connectors are geometrically and logically compatible for snapping.
    UFUNCTION(BlueprintPure, Category="ProcGen")
    bool AreConnectorsCompatible(const FChunkConnector& ConnectorOut, const FChunkConnector& ConnectorIn) const;

    // Populates the library with default Vinland Saga Proto 03 chunks.
    UFUNCTION(BlueprintCallable, CallInEditor, Category="ProcGen")
    void ResetToDefaults();
};
