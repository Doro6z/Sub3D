#include "Misc/AutomationTest.h"

#include "SubmarineRuntimeActor.h"
#include "Components/SubmarineBreachRuntimeComponent.h"
#include "Components/SubmarineDoorRuntimeComponent.h"
#include "Components/SubmarineFloodRuntimeComponent.h"
#include "SubHullComponent.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DRuntimeActorBuildsMeshComponentsTest,
    "Sub3D.Wave9.RuntimeActorBuildsMeshComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DRuntimeActorInitializesRuntimeSystemsTest,
    "Sub3D.Wave9.RuntimeActorInitializesRuntimeSystems",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
static FCompiledMeshSection MakeQuadSection(const FName SectionId, const float LengthCm, const float HalfWidthCm, const float HalfHeightCm)
{
    FCompiledMeshSection Section;
    Section.SectionId = SectionId;
    Section.Positions = {
        FVector3f(0.0f, -HalfWidthCm, -HalfHeightCm),
        FVector3f(0.0f, HalfWidthCm, -HalfHeightCm),
        FVector3f(LengthCm, HalfWidthCm, HalfHeightCm),
        FVector3f(LengthCm, -HalfWidthCm, HalfHeightCm)
    };
    Section.Normals = {
        FVector3f(0.0f, 0.0f, 1.0f),
        FVector3f(0.0f, 0.0f, 1.0f),
        FVector3f(0.0f, 0.0f, 1.0f),
        FVector3f(0.0f, 0.0f, 1.0f)
    };
    Section.UV0 = {
        FVector2f(0.0f, 0.0f),
        FVector2f(1.0f, 0.0f),
        FVector2f(1.0f, 1.0f),
        FVector2f(0.0f, 1.0f)
    };
    Section.Indices = {0, 1, 2, 0, 2, 3};
    return Section;
}

static UCompiledSubmarineRuntimeAsset* BuildRuntimeAsset()
{
    UCompiledSubmarineRuntimeAsset* RuntimeAsset = NewObject<UCompiledSubmarineRuntimeAsset>(GetTransientPackage());
    RuntimeAsset->HullLengthCm = 6000.0f;
    RuntimeAsset->HullData.LengthCm = RuntimeAsset->HullLengthCm;

    FSub3DCompiledHullSection& Fore = RuntimeAsset->HullData.Sections.AddDefaulted_GetRef();
    Fore.PositionX = 0.0f;
    Fore.HalfWidthCm = 200.0f;
    Fore.HalfHeightCm = 180.0f;
    Fore.WallThicknessCm = 12.0f;

    FSub3DCompiledHullSection& Aft = RuntimeAsset->HullData.Sections.AddDefaulted_GetRef();
    Aft.PositionX = RuntimeAsset->HullLengthCm;
    Aft.HalfWidthCm = 180.0f;
    Aft.HalfHeightCm = 160.0f;
    Aft.WallThicknessCm = 12.0f;

    RuntimeAsset->ExteriorHull = MakeQuadSection(TEXT("ExteriorHull"), RuntimeAsset->HullLengthCm, 220.0f, 180.0f);
    RuntimeAsset->InteriorHull = MakeQuadSection(TEXT("InteriorHull"), RuntimeAsset->HullLengthCm, 180.0f, 140.0f);
    RuntimeAsset->CollisionProxy = MakeQuadSection(TEXT("CollisionProxy"), RuntimeAsset->HullLengthCm, 230.0f, 190.0f);
    RuntimeAsset->OuterEnvelope = MakeQuadSection(TEXT("OuterEnvelope"), 1000.0f, 120.0f, 80.0f);

    FCompiledPartitionData& Partition = RuntimeAsset->CompiledPartitions.AddDefaulted_GetRef();
    Partition.PartitionId = TEXT("Bulkhead_A");
    Partition.PartitionMesh = MakeQuadSection(TEXT("Bulkhead_A"), 1.0f, 180.0f, 140.0f);

    FCompiledClosureData& Closure = RuntimeAsset->CompiledClosures.AddDefaulted_GetRef();
    Closure.ClosureId = TEXT("Door_A");
    Closure.OpeningId = TEXT("Opening_A");
    Closure.bClosedByDefault = true;

    FDerivedFloodVolume& FloodVolume = RuntimeAsset->FloodGraph.Volumes.AddDefaulted_GetRef();
    FloodVolume.VolumeId = TEXT("Bay_A");
    FloodVolume.CapacityLiters = 1000.0f;

    return RuntimeAsset;
}
}

bool FSub3DRuntimeActorBuildsMeshComponentsTest::RunTest(const FString& Parameters)
{
    ASubmarineRuntimeActor* RuntimeActor = NewObject<ASubmarineRuntimeActor>(GetTransientPackage());
    TestNotNull(TEXT("Runtime actor should exist"), RuntimeActor);
    if (!RuntimeActor)
    {
        return false;
    }

    RuntimeActor->RuntimeAsset = BuildRuntimeAsset();
    TestTrue(TEXT("InitializeFromRuntimeAsset"), RuntimeActor->InitializeFromRuntimeAsset());

    TArray<UProceduralMeshComponent*> ProceduralMeshComponents;
    RuntimeActor->GetComponents<UProceduralMeshComponent>(ProceduralMeshComponents);

    int32 RenderCount = 0;
    int32 CollisionCount = 0;
    for (const UProceduralMeshComponent* MeshComponent : ProceduralMeshComponents)
    {
        if (!MeshComponent)
        {
            continue;
        }

        if (MeshComponent->ComponentTags.Contains(TEXT("RuntimeRender")))
        {
            ++RenderCount;
        }

        if (MeshComponent->ComponentTags.Contains(TEXT("RuntimeCollision")))
        {
            ++CollisionCount;
        }
    }

    TestTrue(TEXT("Runtime actor should build render components"), RenderCount >= 3);
    TestTrue(TEXT("Runtime actor should build collision components"), CollisionCount >= 1);
    return true;
}

bool FSub3DRuntimeActorInitializesRuntimeSystemsTest::RunTest(const FString& Parameters)
{
    ASubmarineRuntimeActor* RuntimeActor = NewObject<ASubmarineRuntimeActor>(GetTransientPackage());
    TestNotNull(TEXT("Runtime actor should exist"), RuntimeActor);
    if (!RuntimeActor)
    {
        return false;
    }

    RuntimeActor->RuntimeAsset = BuildRuntimeAsset();
    TestTrue(TEXT("InitializeFromRuntimeAsset"), RuntimeActor->InitializeFromRuntimeAsset());

    TestEqual(TEXT("Hull length copied into SubHull"), RuntimeActor->SubHull->HullData.LengthCm, 6000.0f);
    TestEqual(TEXT("Hull sections copied into SubHull"), RuntimeActor->SubHull->HullData.Sections.Num(), 2);
    TestEqual(TEXT("Closures copied into DoorRuntimeComponent"), RuntimeActor->DoorRuntimeComponent->Closures.Num(), 1);
    TestEqual(TEXT("Partitions copied into BreachRuntimeComponent"), RuntimeActor->BreachRuntimeComponent->PartitionBindings.Num(), 1);
    TestEqual(TEXT("Flood graph volumes copied into FloodRuntimeComponent"), RuntimeActor->FloodRuntimeComponent->FloodGraph.Volumes.Num(), 1);
    return true;
}
