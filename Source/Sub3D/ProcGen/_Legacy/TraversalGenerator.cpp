#include "TraversalGenerator.h"
#include "ChunkLibrary.h"
#include "TraversalGraphGenerator.h"
#include "ChunkAssembler.h"
#include "ProceduralChunkBuilder.h" // R6: New builder
#include "TraversalValidator.h"
#include "DrawDebugHelpers.h"

ATraversalGenerator::ATraversalGenerator()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Root component for transforms
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ATraversalGenerator::BeginPlay()
{
    Super::BeginPlay();
    EnsureGeneratorsCreated();
}

void ATraversalGenerator::EnsureGeneratorsCreated()
{
    if (!GraphGenerator) GraphGenerator = NewObject<UTraversalGraphGenerator>(this);
    if (!Assembler) Assembler = NewObject<UChunkAssembler>(this);
    if (!ChunkBuilder) ChunkBuilder = NewObject<UProceduralChunkBuilder>(this); // R6
    if (!Validator) Validator = NewObject<UTraversalValidator>(this);
}

void ATraversalGenerator::GenerateTraversal()
{
    UE_LOG(LogTemp, Display, TEXT("ATraversalGenerator: Starting generation pipeline..."));
    
    ClearTraversal();
    EnsureGeneratorsCreated();

    if (!ChunkLibrary)
    {
        UE_LOG(LogTemp, Error, TEXT("ATraversalGenerator: ChunkLibrary is missing!"));
        return;
    }

    TArray<FChunkInstance> FinalInstances;
    bool bSuccess = false;

    for (int32 i = 0; i < MaxRetryAttempts; ++i)
    {
        if (TryGenerate(i, FinalInstances))
        {
            bSuccess = true;
            break;
        }
    }

    if (bSuccess)
    {
        SpawnedInstances = FinalInstances;
        BuildGeometry(FinalInstances); // R6
        
        if (bDrawGraphDebug || bDrawConnectorDebug)
        {
            DrawDebug(CurrentGraph, FinalInstances);
        }
        
        UE_LOG(LogTemp, Display, TEXT("ATraversalGenerator: PIPELINE SUCCESS. Generated %d chunks."), FinalInstances.Num());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ATraversalGenerator: PIPELINE FAILED after %d attempts."), MaxRetryAttempts);
    }
}

bool ATraversalGenerator::TryGenerate(int32 Attempt, TArray<FChunkInstance>& OutInstances)
{
    UE_LOG(LogTemp, Log, TEXT("--- ATraversalGenerator: Attempt %d ---"), Attempt + 1);

    // NodeSeed cascade: Combine base seed with attempt count for variety
    int32 RouteSeed = GenSpec.CampaignSeed + GenSpec.RouteID + Attempt;

    // Pass 1: Graph
    CurrentGraph = GraphGenerator->GenerateGraph(GenSpec);

    // Pass 2: Assembler
    OutInstances = Assembler->AssembleChunks(CurrentGraph, ChunkLibrary, RouteSeed);

    // Pass 4: Validator (R6: No deformer pass call here)
    float Score = 0.0f;
    EValidationResult Result = Validator->ValidateLayout(OutInstances, Score);

    if (Result != EValidationResult::HardFail)
    {
        UE_LOG(LogTemp, Log, TEXT("ATraversalGenerator: Validation Result = %s (Score: %.2f)"), 
            (Result == EValidationResult::Valid ? TEXT("Valid") : TEXT("SoftFail")), Score);
        return true;
    }

    return false;
}

void ATraversalGenerator::BuildGeometry(const TArray<FChunkInstance>& Instances)
{
    // R6: ChunkBuilder returns components attached to this actor
    SpawnedMeshes = ChunkBuilder->BuildAll(Instances, ChunkLibrary, this);
    UE_LOG(LogTemp, Display, TEXT("ATraversalGenerator: Built %d procedural mesh chunks."), SpawnedMeshes.Num());
}

void ATraversalGenerator::ClearTraversal()
{
    // R6: Cleanup mesh components
    for (UProceduralMeshComponent* Mesh : SpawnedMeshes)
    {
        if (Mesh)
        {
            Mesh->DestroyComponent();
        }
    }
    SpawnedMeshes.Empty();
    SpawnedInstances.Empty();
    CurrentGraph.Empty();
    
    FlushPersistentDebugLines(GetWorld());
    
    UE_LOG(LogTemp, Log, TEXT("ATraversalGenerator: Traversal cleared."));
}

void ATraversalGenerator::DrawDebug(const TArray<FTraversalGraphNode>& Graph, const TArray<FChunkInstance>& Instances) const
{
    UWorld* World = GetWorld();
    if (!World) return;

    for (const FChunkInstance& Instance : Instances)
    {
        // Draw World Transform basis
        DrawDebugCoordinateSystem(World, Instance.WorldTransform.GetLocation(), Instance.WorldTransform.Rotator(), 500.f, false, 30.f);

        if (bDrawConnectorDebug)
        {
            // Draw Connectors as boxes
            FVector InLoc = Instance.WorldTransform.TransformPosition(Instance.ConnectorIn.LocalTransform.GetLocation());
            FVector OutLoc = Instance.WorldTransform.TransformPosition(Instance.ConnectorOut.LocalTransform.GetLocation());
            
            DrawDebugBox(World, InLoc, FVector(100, Instance.ConnectorIn.UsableWidth/2, Instance.ConnectorIn.UsableHeight/2), 
                Instance.WorldTransform.GetRotation(), FColor::Green, false, 30.f);
            DrawDebugBox(World, OutLoc, FVector(100, Instance.ConnectorOut.UsableWidth/2, Instance.ConnectorOut.UsableHeight/2), 
                Instance.WorldTransform.GetRotation(), FColor::Red, false, 30.f);
        }
    }
}
