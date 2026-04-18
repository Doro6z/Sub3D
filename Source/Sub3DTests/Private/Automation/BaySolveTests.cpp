#include "Misc/AutomationTest.h"

#include "Bake/SubmarineBaySolveService.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DStructuralBaySolveFromAuthoringTest,
    "Sub3D.Wave3.StructuralBaySolveFromAuthoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DStructuralBaySolveFromFrameBoundariesTest,
    "Sub3D.Wave3.StructuralBaySolveFromFrameBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
static FSubmarineHullDef BuildTestHull()
{
    FSubmarineHullDef Hull;
    Hull.LengthCm = 8000.0f;
    Hull.DefaultHalfHeightCm = 220.0f;
    Hull.DefaultWallThicknessCm = 12.0f;
    return Hull;
}
} // namespace

bool FSub3DStructuralBaySolveFromAuthoringTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildTestHull();

    TArray<FStructuralBayDef> Bays;
    FStructuralBayDef BayA;
    BayA.BayId = TEXT("BayA");
    BayA.StartAlpha = 0.0f;
    BayA.EndAlpha = 0.4f;
    BayA.RequestedDeckLevels = 1;
    Bays.Add(BayA);

    FStructuralBayDef BayB;
    BayB.BayId = TEXT("BayB");
    BayB.StartAlpha = 0.4f;
    BayB.EndAlpha = 1.0f;
    BayB.RequestedDeckLevels = 3;
    Bays.Add(BayB);

    TArray<FCompiledBayData> CompiledBays;
    TArray<FString> Errors;

    TestTrue(
        TEXT("SolveStructuralBays from authoring"),
        Sub3DWave3::FSubmarineBaySolveService::SolveStructuralBays(Hull, TArray<FFrameRingDef>(), Bays, CompiledBays, Errors));
    TestEqual(TEXT("Compiled bay count"), CompiledBays.Num(), 2);
    TestTrue(TEXT("MaxDeckLevels clamped and valid"), CompiledBays[1].MaxDeckLevels >= 1);

    return true;
}

bool FSub3DStructuralBaySolveFromFrameBoundariesTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildTestHull();

    TArray<FFrameRingDef> FrameRings;

    FFrameRingDef A;
    A.FrameRingId = TEXT("FR_A");
    A.SpineAlpha = 0.0f;
    A.bIsBayBoundary = true;
    FrameRings.Add(A);

    FFrameRingDef B;
    B.FrameRingId = TEXT("FR_B");
    B.SpineAlpha = 0.5f;
    B.bIsBayBoundary = true;
    FrameRings.Add(B);

    FFrameRingDef C;
    C.FrameRingId = TEXT("FR_C");
    C.SpineAlpha = 1.0f;
    C.bIsBayBoundary = true;
    FrameRings.Add(C);

    TArray<FCompiledBayData> CompiledBays;
    TArray<FString> Errors;

    TestTrue(
        TEXT("SolveStructuralBays from frame boundaries"),
        Sub3DWave3::FSubmarineBaySolveService::SolveStructuralBays(Hull, FrameRings, TArray<FStructuralBayDef>(), CompiledBays, Errors));
    TestEqual(TEXT("Derived bay count"), CompiledBays.Num(), 2);
    TestEqual(TEXT("First bay has frame boundaries"), CompiledBays[0].BoundaryFrameRingIds.Num(), 2);

    return true;
}