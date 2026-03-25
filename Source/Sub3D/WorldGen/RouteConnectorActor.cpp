#include "RouteConnectorActor.h"

#include "ProceduralMeshComponent.h"

ARouteConnectorActor::ARouteConnectorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ConnectorMesh"));
	MeshComponent->SetupAttachment(GetRootComponent());
	MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
}

bool ARouteConnectorActor::BuildConnector(const FTransform& StartWorld, float StartRadiusCm, const FTransform& EndWorld, float EndRadiusCm)
{
	if (!MeshComponent)
	{
		return false;
	}

	FRouteConnectorMeshData MeshData;
	if (!URouteConnectorGenerator::BuildConnectorMesh(StartWorld, StartRadiusCm, EndWorld, EndRadiusCm, BuildSettings, MeshData))
	{
		ClearConnector();
		return false;
	}

	SetActorTransform(FTransform(StartWorld.GetRotation(), StartWorld.GetLocation()));

	TArray<FVector> LocalVertices;
	TArray<FVector> LocalNormals;
	LocalVertices.Reserve(MeshData.Vertices.Num());
	LocalNormals.Reserve(MeshData.Normals.Num());
	const FTransform ActorWorld = GetActorTransform();
	for (int32 Index = 0; Index < MeshData.Vertices.Num(); ++Index)
	{
		LocalVertices.Add(ActorWorld.InverseTransformPosition(MeshData.Vertices[Index]));
		LocalNormals.Add(ActorWorld.InverseTransformVectorNoScale(MeshData.Normals[Index]).GetSafeNormal());
	}

	MeshComponent->ClearAllMeshSections();
	MeshComponent->CreateMeshSection(
		0,
		LocalVertices,
		MeshData.Triangles,
		LocalNormals,
		MeshData.UVs,
		TArray<FColor>(),
		TArray<FProcMeshTangent>(),
		BuildSettings.bEnableCollision);

	if (MaterialOverride)
	{
		MeshComponent->SetMaterial(0, MaterialOverride);
	}

	return true;
}

void ARouteConnectorActor::ClearConnector()
{
	if (MeshComponent)
	{
		MeshComponent->ClearAllMeshSections();
	}
}
