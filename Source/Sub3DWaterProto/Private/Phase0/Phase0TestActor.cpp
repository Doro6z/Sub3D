#include "Phase0/Phase0TestActor.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

APhase0TestActor::APhase0TestActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProcMesh"));
    SetRootComponent(ProcMesh);

    ProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProcMesh->SetMobility(EComponentMobility::Movable);
}

void APhase0TestActor::BeginPlay()
{
    Super::BeginPlay();
    GenerateQuad();
}

void APhase0TestActor::GenerateQuad()
{
    if (!ProcMesh)
    {
        return;
    }

    const float H = QuadSize * 0.5f;

    // Quad horizontal, normales vers +Z. Winding (0,1,2,0,2,3) = front face visible from above.
    const TArray<FVector> Vertices = {
        FVector(-H, -H, 0.f),
        FVector( H, -H, 0.f),
        FVector( H,  H, 0.f),
        FVector(-H,  H, 0.f)
    };

    const TArray<int32> Triangles = { 0, 1, 2, 0, 2, 3 };

    const TArray<FVector> Normals = {
        FVector(0, 0, 1), FVector(0, 0, 1), FVector(0, 0, 1), FVector(0, 0, 1)
    };

    const TArray<FVector2D> UVs = {
        FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1)
    };

    ProcMesh->ClearAllMeshSections();
    ProcMesh->CreateMeshSection(
        /*SectionIndex*/ 0,
        Vertices, Triangles, Normals, UVs,
        TArray<FColor>(), TArray<FProcMeshTangent>(),
        /*bCreateCollision*/ false);

    if (WaterMaterial)
    {
        ProcMesh->SetMaterial(0, WaterMaterial);
    }
}
