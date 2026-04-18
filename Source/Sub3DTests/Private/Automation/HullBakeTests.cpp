#include "Misc/AutomationTest.h"
#include "Bake/SubmarineHullBakeService.h"
#include "Bake/SubmarineRingSequenceBuilder.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullBakeVertexCountTest,
    "Sub3D.Wave2.HullBakeVertexCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullBakeClosedMeshTest,
    "Sub3D.Wave2.HullBakeClosedMesh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullBakeDeterminismTest,
    "Sub3D.Wave2.HullBakeDeterminism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DCollisionProxySimplifiedTest,
    "Sub3D.Wave2.CollisionProxySimplified",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullOwnershipCountTest,
    "Sub3D.Wave2.HullOwnershipCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
static FSubmarineHullDef BuildTestHull()
{
    FSubmarineHullDef Hull;
    Hull.LengthCm = 6000.0f;
    Hull.DefaultHalfWidthCm = 200.0f;
    Hull.DefaultHalfHeightCm = 180.0f;
    Hull.DefaultSectionProfile = ESub3DSectionProfile::Ellipse;
    Hull.DefaultSectionRoundness = 0.5f;
    Hull.DefaultWallThicknessCm = 10.0f;
    return Hull;
}

static TArray<FControlRingDef> BuildControlRings()
{
    TArray<FControlRingDef> Rings;
    FControlRingDef A;
    A.ControlRingId = TEXT("A");
    A.PositionX = 0.0f;
    A.HalfWidthCm = 190.0f;
    A.HalfHeightCm = 180.0f;
    A.SectionProfile = ESub3DSectionProfile::Ellipse;
    A.WallThicknessCm = 10.0f;
    Rings.Add(A);

    FControlRingDef B = A;
    B.ControlRingId = TEXT("B");
    B.PositionX = 3000.0f;
    B.HalfWidthCm = 240.0f;
    B.HalfHeightCm = 220.0f;
    B.SectionProfile = ESub3DSectionProfile::Superellipse;
    B.SectionRoundness = 0.8f;
    Rings.Add(B);

    FControlRingDef C = A;
    C.ControlRingId = TEXT("C");
    C.PositionX = 6000.0f;
    C.HalfWidthCm = 150.0f;
    C.HalfHeightCm = 150.0f;
    Rings.Add(C);

    return Rings;
}

static bool BuildBakedHull(
    const int32 RingCount,
    const int32 RadialSegments,
    TArray<Sub3DWave2::FGeneratedRingData>& OutRings,
    FCompiledMeshSection& OutHull,
    FCompiledMeshSection& OutCollision)
{
    const FSubmarineHullDef Hull = BuildTestHull();
    const TArray<FControlRingDef> ControlRings = BuildControlRings();

    TArray<FString> Errors;
    if (!Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(Hull, ControlRings, RingCount, OutRings, Errors))
    {
        return false;
    }

    Sub3DWave2::FHullBakeSettings Settings;
    Settings.RadialSegments = RadialSegments;
    Settings.CollisionRadialSegments = 8;
    Settings.bBakeCollision = true;

    if (!Sub3DWave2::FSubmarineHullBakeService::BakeExteriorHull(Hull, OutRings, Settings, OutHull))
    {
        return false;
    }

    if (!Sub3DWave2::FSubmarineHullBakeService::BakeHullCollisionProxy(Hull, OutRings, Settings, OutCollision))
    {
        return false;
    }

    return true;
}

static uint64 MakeEdgeKey(int32 A, int32 B)
{
    if (A > B)
    {
        Swap(A, B);
    }
    return (static_cast<uint64>(static_cast<uint32>(A)) << 32) | static_cast<uint32>(B);
}

static bool IsWatertight(const FCompiledMeshSection& Section)
{
    TMap<uint64, int32> EdgeUseCount;
    for (int32 I = 0; I + 2 < Section.Indices.Num(); I += 3)
    {
        const int32 A = Section.Indices[I + 0];
        const int32 B = Section.Indices[I + 1];
        const int32 C = Section.Indices[I + 2];
        ++EdgeUseCount.FindOrAdd(MakeEdgeKey(A, B));
        ++EdgeUseCount.FindOrAdd(MakeEdgeKey(B, C));
        ++EdgeUseCount.FindOrAdd(MakeEdgeKey(C, A));
    }

    for (const TPair<uint64, int32>& Edge : EdgeUseCount)
    {
        if (Edge.Value != 2)
        {
            return false;
        }
    }

    return true;
}
} // namespace

bool FSub3DHullBakeVertexCountTest::RunTest(const FString& Parameters)
{
    TArray<Sub3DWave2::FGeneratedRingData> Rings;
    FCompiledMeshSection Hull;
    FCompiledMeshSection Collision;
    const int32 RingCount = 24;
    const int32 RadialSegments = 32;

    TestTrue(TEXT("BuildBakedHull"), BuildBakedHull(RingCount, RadialSegments, Rings, Hull, Collision));
    TestEqual(TEXT("Hull vertex count"), Hull.Positions.Num(), RingCount * RadialSegments);
    return true;
}

bool FSub3DHullBakeClosedMeshTest::RunTest(const FString& Parameters)
{
    TArray<Sub3DWave2::FGeneratedRingData> Rings;
    FCompiledMeshSection Hull;
    FCompiledMeshSection Collision;
    TestTrue(TEXT("BuildBakedHull"), BuildBakedHull(20, 24, Rings, Hull, Collision));
    TestTrue(TEXT("Exterior hull watertight"), IsWatertight(Hull));
    return true;
}

bool FSub3DHullBakeDeterminismTest::RunTest(const FString& Parameters)
{
    TArray<Sub3DWave2::FGeneratedRingData> RingsA;
    TArray<Sub3DWave2::FGeneratedRingData> RingsB;
    FCompiledMeshSection HullA;
    FCompiledMeshSection CollisionA;
    FCompiledMeshSection HullB;
    FCompiledMeshSection CollisionB;

    TestTrue(TEXT("Build hull A"), BuildBakedHull(18, 20, RingsA, HullA, CollisionA));
    TestTrue(TEXT("Build hull B"), BuildBakedHull(18, 20, RingsB, HullB, CollisionB));

    TestEqual(TEXT("Same position count"), HullA.Positions.Num(), HullB.Positions.Num());
    TestEqual(TEXT("Same index count"), HullA.Indices.Num(), HullB.Indices.Num());

    const int32 PositionCount = FMath::Min(HullA.Positions.Num(), HullB.Positions.Num());
    for (int32 Index = 0; Index < PositionCount; ++Index)
    {
        const FVector3f& A = HullA.Positions[Index];
        const FVector3f& B = HullB.Positions[Index];
        TestTrue(TEXT("Vertex deterministic"), A.Equals(B, 0.0001f));
    }

    return true;
}

bool FSub3DCollisionProxySimplifiedTest::RunTest(const FString& Parameters)
{
    TArray<Sub3DWave2::FGeneratedRingData> Rings;
    FCompiledMeshSection Hull;
    FCompiledMeshSection Collision;
    TestTrue(TEXT("BuildBakedHull"), BuildBakedHull(24, 32, Rings, Hull, Collision));
    TestTrue(TEXT("Collision has fewer vertices than render hull"), Collision.Positions.Num() < Hull.Positions.Num());
    return true;
}

bool FSub3DHullOwnershipCountTest::RunTest(const FString& Parameters)
{
    TArray<Sub3DWave2::FGeneratedRingData> Rings;
    FCompiledMeshSection Hull;
    FCompiledMeshSection Collision;
    TestTrue(TEXT("BuildBakedHull"), BuildBakedHull(24, 32, Rings, Hull, Collision));

    FCompiledHullOwnership Ownership;
    TestTrue(TEXT("BuildHullOwnership"), Sub3DWave2::FSubmarineHullBakeService::BuildHullOwnership(Rings, Hull, Ownership));
    TestEqual(TEXT("Ownership triangle count"), Ownership.TriangleOwnerIds.Num(), Hull.Indices.Num() / 3);
    return true;
}
