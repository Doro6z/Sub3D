#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ProceduralMeshComponent.h"
#include "TraversalTypes.h"
#include "ProceduralChunkBuilder.generated.h"

class UChunkLibrary;

/**
 * UProceduralChunkBuilder
 * Pass 3: Generates UProceduralMeshComponent geometry for each chunk instance.
 * Geometry is generated from FChunkCarveParams with Simplex noise displacement.
 * All meshes face INWARD — the submarine navigates inside.
 */
UCLASS(BlueprintType)
class SUB3D_API UProceduralChunkBuilder : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Builds all chunk meshes and attaches them to ParentActor.
     * Returns created components for tracking (ClearTraversal).
     */
    UFUNCTION(BlueprintCallable, Category="ProcGen")
    TArray<UProceduralMeshComponent*> BuildAll(
        const TArray<FChunkInstance>& Instances,
        const UChunkLibrary* Library,
        AActor* ParentActor) const;

private:
    UProceduralMeshComponent* BuildChunk(
        const FChunkInstance& Instance,
        const FChunkCarveParams& Params,
        AActor* ParentActor,
        int32 SectionIndex) const;

    /** Generates a hollow oval tube along the X axis, normals pointing INWARD. */
    void GenerateOvalTube(
        const FChunkCarveParams& Params, int32 Seed,
        TArray<FVector>& OutVerts, TArray<int32>& OutTris,
        TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs) const;

    /** Generates a hollow spheroid, normals pointing INWARD. */
    void GenerateSpheroid(
        const FChunkCarveParams& Params, int32 Seed,
        TArray<FVector>& OutVerts, TArray<int32>& OutTris,
        TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs) const;

    /** Applies Simplex noise displacement along inward normals. */
    void ApplySimplex(
        TArray<FVector>& Verts,
        const TArray<FVector>& InwardNormals,
        const FTransform& ChunkWorldTransform,
        const FChunkCarveParams& Params,
        int32 Seed) const;

    /** Deterministic pseudo-noise (sine-summation with bit-mixing). */
    float Simplex3D(float X, float Y, float Z, int32 Seed) const;
};
