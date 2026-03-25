#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TraversalTypes.h"
#include "ProceduralMeshComponent.h"
#include "TraversalGenerator.generated.h"

class UChunkLibrary;
class UTraversalGraphGenerator;
class UChunkAssembler;
class UProceduralChunkBuilder; // R5: Replaced VolumetricDeformer/Carver
class UTraversalValidator;

/**
 * ATraversalGenerator
 * Orchestrates the 4-pass procedural generation pipeline.
 */
UCLASS()
class SUB3D_API ATraversalGenerator : public AActor
{
    GENERATED_BODY()

public:
    ATraversalGenerator();

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen|Config")
    FTraversalGenSpec GenSpec;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen|Resources")
    UChunkLibrary* ChunkLibrary;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen|Config")
    int32 MaxRetryAttempts = 10;

    /** Triggers the generation pipeline. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "ProcGen")
    void GenerateTraversal();

    /** Clears all generated chunks and debug visualizations. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "ProcGen")
    void ClearTraversal();

    UPROPERTY(EditAnywhere, Category = "ProcGen|Debug")
    bool bDrawGraphDebug = true;

    UPROPERTY(EditAnywhere, Category = "ProcGen|Debug")
    bool bDrawConnectorDebug = true;

private:
    // Pipeline components
    UPROPERTY() UTraversalGraphGenerator* GraphGenerator;
    UPROPERTY() UChunkAssembler* Assembler;
    UPROPERTY() UProceduralChunkBuilder* ChunkBuilder; // R5: New builder
    UPROPERTY() UTraversalValidator* Validator;

    // Runtime state
    UPROPERTY(Transient) TArray<FChunkInstance> SpawnedInstances;
    UPROPERTY(Transient) TArray<UProceduralMeshComponent*> SpawnedMeshes; // R5: Track meshes
    UPROPERTY(Transient) TArray<FTraversalGraphNode> CurrentGraph;

    void EnsureGeneratorsCreated();
    bool TryGenerate(int32 Attempt, TArray<FChunkInstance>& OutInstances);
    
    // R5: Procedural mesh building
    void BuildGeometry(const TArray<FChunkInstance>& Instances);

    void DrawDebug(const TArray<FTraversalGraphNode>& Graph, const TArray<FChunkInstance>& Instances) const;
};
