#include "SubmarineRuntimeActor.h"

#include "CoreMinimal.h"
#include "SubHullComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ObjectMacros.h"

namespace SubmarineRuntimeActorPrivate
{
static void DestroyProceduralMeshes(TArray<TObjectPtr<UProceduralMeshComponent>>& Components)
{
    for (UProceduralMeshComponent* Component : Components)
    {
        if (IsValid(Component))
        {
            Component->DestroyComponent();
        }
    }

    Components.Reset();
}

static void ApplyCompiledSectionToProceduralMesh(
    UProceduralMeshComponent* ProceduralMesh,
    const FCompiledMeshSection& Section,
    const bool bEnableCollision,
    const FName CollisionProfileName)
{
    if (!ProceduralMesh || Section.Positions.IsEmpty() || Section.Indices.IsEmpty())
    {
        return;
    }

    TArray<FVector> Positions;
    Positions.Reserve(Section.Positions.Num());
    for (const FVector3f& Position : Section.Positions)
    {
        Positions.Add(FVector(Position));
    }

    TArray<FVector> Normals;
    Normals.Reserve(Section.Normals.Num());
    for (const FVector3f& Normal : Section.Normals)
    {
        Normals.Add(FVector(Normal));
    }

    TArray<FVector2D> UV0;
    UV0.Reserve(Section.UV0.Num());
    for (const FVector2f& UV : Section.UV0)
    {
        UV0.Add(FVector2D(UV));
    }

    TArray<FLinearColor> Colors;
    Colors.Init(FLinearColor::White, Positions.Num());

    TArray<FProcMeshTangent> Tangents;
    ProceduralMesh->CreateMeshSection_LinearColor(0, Positions, Section.Indices, Normals, UV0, Colors, Tangents, bEnableCollision);
    ProceduralMesh->bUseComplexAsSimpleCollision = bEnableCollision;
    ProceduralMesh->SetCollisionProfileName(CollisionProfileName);
    ProceduralMesh->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    ProceduralMesh->SetCanEverAffectNavigation(false);
}

static UProceduralMeshComponent* CreateManagedMeshComponent(
    AActor& OwnerActor,
    USceneComponent* AttachParent,
    const FName ComponentName,
    const bool bVisible,
    const bool bEnableCollision,
    const FName CollisionProfileName,
    const FName RoleTag)
{
    UProceduralMeshComponent* ProceduralMesh = NewObject<UProceduralMeshComponent>(&OwnerActor, ComponentName);
    if (!ProceduralMesh)
    {
        return nullptr;
    }

    ProceduralMesh->CreationMethod = EComponentCreationMethod::Instance;
    OwnerActor.AddInstanceComponent(ProceduralMesh);

    if (AttachParent)
    {
        ProceduralMesh->SetupAttachment(AttachParent);
    }

    ProceduralMesh->SetMobility(EComponentMobility::Movable);
    ProceduralMesh->SetVisibility(bVisible);
    ProceduralMesh->SetHiddenInGame(!bVisible);
    ProceduralMesh->SetCollisionProfileName(CollisionProfileName);
    ProceduralMesh->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    ProceduralMesh->ComponentTags.Add(RoleTag);
    if (OwnerActor.GetWorld())
    {
        ProceduralMesh->RegisterComponent();
    }
    return ProceduralMesh;
}
}

// Bring the two non-conflicting helpers into file scope. DestroyProceduralMeshes
// stays inside the namespace because SubmarineAuthoringActors.cpp defines a same-signature
// global function that would clash in unity build (C2375 different-linkage).
using SubmarineRuntimeActorPrivate::ApplyCompiledSectionToProceduralMesh;
using SubmarineRuntimeActorPrivate::CreateManagedMeshComponent;

ASubmarineRuntimeActor::ASubmarineRuntimeActor()
{
    PrimaryActorTick.bCanEverTick = false;

    BreachRuntimeComponent = CreateDefaultSubobject<USubmarineBreachRuntimeComponent>(TEXT("BreachRuntimeComponent"));
    FloodRuntimeComponent = CreateDefaultSubobject<USubmarineFloodRuntimeComponent>(TEXT("FloodRuntimeComponent"));
    DoorRuntimeComponent = CreateDefaultSubobject<USubmarineDoorRuntimeComponent>(TEXT("DoorRuntimeComponent"));
}

void ASubmarineRuntimeActor::DebugTriggerBreach(FVector WorldLocation, float Damage, float Radius)
{
    if (SubHull)
    {
        SubHull->ApplyHullImpact(GetActorTransform().InverseTransformPosition(WorldLocation), Damage, Radius);
    }
}

void ASubmarineRuntimeActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (RuntimeAsset)
    {
        InitializeFromRuntimeAsset();
    }
}

void ASubmarineRuntimeActor::BeginPlay()
{
    Super::BeginPlay();

    if (RuntimeAsset)
    {
        InitializeFromRuntimeAsset();
    }
}

bool ASubmarineRuntimeActor::LoadCompiledAsset(UCompiledSubmarineRuntimeAsset* InRuntimeAsset)
{
    RuntimeAsset = InRuntimeAsset;
    if (!RuntimeAsset)
    {
        return false;
    }

    if (SubHull)
    {
        SubHull->HullData = RuntimeAsset->HullData;
    }
    return true;
}

bool ASubmarineRuntimeActor::BuildRenderComponents()
{
    if (!RuntimeAsset)
    {
        return false;
    }

    ClearBuiltGeometry();

    int32 BuiltComponentCount = 0;
    auto AddRenderSection = [this, &BuiltComponentCount](const FCompiledMeshSection& Section, const FName ComponentName)
    {
        if (Section.Positions.IsEmpty() || Section.Indices.IsEmpty())
        {
            return;
        }

        UProceduralMeshComponent* MeshComponent = CreateManagedMeshComponent(
            *this,
            GetRootComponent(),
            ComponentName,
            true,
            false,
            UCollisionProfile::NoCollision_ProfileName,
            TEXT("RuntimeRender"));
        if (!MeshComponent)
        {
            return;
        }

        MeshComponent->ComponentTags.Add(Section.SectionId);
        ApplyCompiledSectionToProceduralMesh(MeshComponent, Section, false, UCollisionProfile::NoCollision_ProfileName);
        RenderMeshComponents.Add(MeshComponent);
        ++BuiltComponentCount;
    };

    AddRenderSection(RuntimeAsset->ExteriorHull, TEXT("ExteriorHullRender"));
    AddRenderSection(RuntimeAsset->InteriorHull, TEXT("InteriorHullRender"));
    AddRenderSection(RuntimeAsset->OuterEnvelope, TEXT("OuterEnvelopeRender"));

    for (int32 FloorIndex = 0; FloorIndex < RuntimeAsset->CompiledFloorRegions.Num(); ++FloorIndex)
    {
        const FCompiledFloorRegionData& Floor = RuntimeAsset->CompiledFloorRegions[FloorIndex];
        const FName ComponentName = Floor.FloorRegionId.IsNone()
            ? FName(*FString::Printf(TEXT("FloorRender_%d"), FloorIndex))
            : FName(*FString::Printf(TEXT("%s_Render"), *Floor.FloorRegionId.ToString()));
        AddRenderSection(Floor.FloorMesh, ComponentName);
    }

    for (int32 PartitionIndex = 0; PartitionIndex < RuntimeAsset->CompiledPartitions.Num(); ++PartitionIndex)
    {
        const FCompiledPartitionData& Partition = RuntimeAsset->CompiledPartitions[PartitionIndex];
        const FName ComponentName = Partition.PartitionId.IsNone()
            ? FName(*FString::Printf(TEXT("PartitionRender_%d"), PartitionIndex))
            : FName(*FString::Printf(TEXT("%s_Render"), *Partition.PartitionId.ToString()));
        AddRenderSection(Partition.PartitionMesh, ComponentName);
    }

    return BuiltComponentCount > 0;
}

bool ASubmarineRuntimeActor::BuildCollisionComponents()
{
    if (!RuntimeAsset)
    {
        return false;
    }

    SubmarineRuntimeActorPrivate::DestroyProceduralMeshes(CollisionMeshComponents);

    int32 BuiltComponentCount = 0;

    if (!RuntimeAsset->CollisionProxy.Positions.IsEmpty() && !RuntimeAsset->CollisionProxy.Indices.IsEmpty())
    {
        UProceduralMeshComponent* CollisionProxyComponent = CreateManagedMeshComponent(
            *this,
            GetRootComponent(),
            TEXT("CollisionProxyRuntime"),
            false,
            true,
            UCollisionProfile::BlockAll_ProfileName,
            TEXT("RuntimeCollision"));
        if (CollisionProxyComponent)
        {
            CollisionProxyComponent->ComponentTags.Add(RuntimeAsset->CollisionProxy.SectionId);
            ApplyCompiledSectionToProceduralMesh(CollisionProxyComponent, RuntimeAsset->CollisionProxy, true, UCollisionProfile::BlockAll_ProfileName);
            CollisionMeshComponents.Add(CollisionProxyComponent);
            ++BuiltComponentCount;
        }
    }

    for (int32 FloorIndex = 0; FloorIndex < RuntimeAsset->CompiledFloorRegions.Num(); ++FloorIndex)
    {
        const FCompiledFloorRegionData& Floor = RuntimeAsset->CompiledFloorRegions[FloorIndex];
        if (Floor.FloorMesh.Positions.IsEmpty() || Floor.FloorMesh.Indices.IsEmpty())
        {
            continue;
        }

        const FName ComponentName = Floor.FloorRegionId.IsNone()
            ? FName(*FString::Printf(TEXT("FloorCollision_%d"), FloorIndex))
            : FName(*FString::Printf(TEXT("%s_Collision"), *Floor.FloorRegionId.ToString()));
        UProceduralMeshComponent* FloorCollisionComponent = CreateManagedMeshComponent(
            *this,
            GetRootComponent(),
            ComponentName,
            false,
            true,
            UCollisionProfile::BlockAll_ProfileName,
            TEXT("RuntimeCollision"));
        if (FloorCollisionComponent)
        {
            FloorCollisionComponent->ComponentTags.Add(Floor.FloorRegionId);
            ApplyCompiledSectionToProceduralMesh(FloorCollisionComponent, Floor.FloorMesh, true, UCollisionProfile::BlockAll_ProfileName);
            CollisionMeshComponents.Add(FloorCollisionComponent);
            ++BuiltComponentCount;
        }
    }

    for (int32 PartitionIndex = 0; PartitionIndex < RuntimeAsset->CompiledPartitions.Num(); ++PartitionIndex)
    {
        const FCompiledPartitionData& Partition = RuntimeAsset->CompiledPartitions[PartitionIndex];
        if (Partition.PartitionMesh.Positions.IsEmpty() || Partition.PartitionMesh.Indices.IsEmpty())
        {
            continue;
        }

        const FName ComponentName = Partition.PartitionId.IsNone()
            ? FName(*FString::Printf(TEXT("PartitionCollision_%d"), PartitionIndex))
            : FName(*FString::Printf(TEXT("%s_Collision"), *Partition.PartitionId.ToString()));
        UProceduralMeshComponent* PartitionCollisionComponent = CreateManagedMeshComponent(
            *this,
            GetRootComponent(),
            ComponentName,
            false,
            true,
            UCollisionProfile::BlockAll_ProfileName,
            TEXT("RuntimeCollision"));
        if (!PartitionCollisionComponent)
        {
            continue;
        }

        PartitionCollisionComponent->ComponentTags.Add(Partition.PartitionId);
        ApplyCompiledSectionToProceduralMesh(PartitionCollisionComponent, Partition.PartitionMesh, true, UCollisionProfile::BlockAll_ProfileName);
        CollisionMeshComponents.Add(PartitionCollisionComponent);
        ++BuiltComponentCount;
    }

    return BuiltComponentCount > 0;
}

bool ASubmarineRuntimeActor::InitializeRuntimeSystems()
{
    if (!RuntimeAsset)
    {
        return false;
    }

    if (SubHull)
    {
        SubHull->InitializeFromCompiledHull(RuntimeAsset);
    }

    BreachRuntimeComponent->InitializeFromCompiledPartitions(RuntimeAsset->CompiledPartitions);
    FloodRuntimeComponent->InitializeFromFloodGraph(RuntimeAsset->FloodGraph);
    DoorRuntimeComponent->InitializeFromClosures(RuntimeAsset->CompiledClosures);

    // [Phase 1 cleanup] WaterVisualsComponent (UFloodWaterVisualsComponent) removed —
    // legacy duplicate of UFloodWaterPlaneComponent. Per-compartment water rendering is
    // now spawned in ASubmarineBase::BeginPlay step 6 from CompartmentVolumeComponents.

    return true;
}

bool ASubmarineRuntimeActor::InitializeFromRuntimeAsset()
{
    if (!LoadCompiledAsset(RuntimeAsset))
    {
        return false;
    }

    const bool bBuiltRender = BuildRenderComponents();
    const bool bBuiltCollision = BuildCollisionComponents();
    const bool bInitializedSystems = InitializeRuntimeSystems();
    return bBuiltRender && bBuiltCollision && bInitializedSystems;
}

void ASubmarineRuntimeActor::ClearBuiltGeometry()
{
    SubmarineRuntimeActorPrivate::DestroyProceduralMeshes(RenderMeshComponents);
    SubmarineRuntimeActorPrivate::DestroyProceduralMeshes(CollisionMeshComponents);
}
