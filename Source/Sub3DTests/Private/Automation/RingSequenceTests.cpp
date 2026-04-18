#include "Misc/AutomationTest.h"
#include "Bake/SubmarineRingSequenceBuilder.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DRingSequenceDeterminismTest,
    "Sub3D.Wave2.RingSequenceDeterminism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DRingSequenceMonotonicityTest,
    "Sub3D.Wave2.RingSequenceMonotonicity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DRingSequenceIncludesControlRingsTest,
    "Sub3D.Wave2.RingSequenceIncludesControlRings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DFrameRingSequenceIntegrationTest,
    "Sub3D.Wave3.FrameRingSequenceIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
static FSubmarineHullDef BuildTestHull()
{
    FSubmarineHullDef Hull;
    Hull.LengthCm = 8000.0f;
    Hull.DefaultHalfWidthCm = 220.0f;
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
    A.ControlRingId = TEXT("CR_A");
    A.PositionX = 0.0f;
    A.HalfWidthCm = 180.0f;
    A.HalfHeightCm = 180.0f;
    A.SectionProfile = ESub3DSectionProfile::Ellipse;
    A.SectionRoundness = 0.2f;
    A.WallThicknessCm = 10.0f;
    Rings.Add(A);

    FControlRingDef B = A;
    B.ControlRingId = TEXT("CR_B");
    B.PositionX = 4000.0f;
    B.HalfWidthCm = 260.0f;
    B.HalfHeightCm = 210.0f;
    B.SectionProfile = ESub3DSectionProfile::Superellipse;
    B.SectionRoundness = 0.7f;
    Rings.Add(B);

    FControlRingDef C = A;
    C.ControlRingId = TEXT("CR_C");
    C.PositionX = 8000.0f;
    C.HalfWidthCm = 150.0f;
    C.HalfHeightCm = 160.0f;
    C.SectionProfile = ESub3DSectionProfile::Ellipse;
    C.SectionRoundness = 0.4f;
    Rings.Add(C);

    return Rings;
}
} // namespace

bool FSub3DRingSequenceDeterminismTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildTestHull();
    const TArray<FControlRingDef> ControlRings = BuildControlRings();

    TArray<Sub3DWave2::FGeneratedRingData> SeqA;
    TArray<Sub3DWave2::FGeneratedRingData> SeqB;
    TArray<FString> ErrorsA;
    TArray<FString> ErrorsB;

    TestTrue(TEXT("Build sequence A"), Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(Hull, ControlRings, 32, SeqA, ErrorsA));
    TestTrue(TEXT("Build sequence B"), Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(Hull, ControlRings, 32, SeqB, ErrorsB));
    TestEqual(TEXT("Same ring count"), SeqA.Num(), SeqB.Num());

    const int32 Count = FMath::Min(SeqA.Num(), SeqB.Num());
    for (int32 Index = 0; Index < Count; ++Index)
    {
        TestTrue(TEXT("Same alpha"), FMath::IsNearlyEqual(SeqA[Index].SpineAlpha, SeqB[Index].SpineAlpha, KINDA_SMALL_NUMBER));
        TestTrue(TEXT("Same radius"), FMath::IsNearlyEqual(SeqA[Index].RadiusCm, SeqB[Index].RadiusCm, KINDA_SMALL_NUMBER));
        TestTrue(TEXT("Same ratio"), FMath::IsNearlyEqual(SeqA[Index].WidthToHeightRatio, SeqB[Index].WidthToHeightRatio, KINDA_SMALL_NUMBER));
        TestEqual(TEXT("Same control source"), SeqA[Index].SourceControlRingIndex, SeqB[Index].SourceControlRingIndex);
    }

    return true;
}

bool FSub3DRingSequenceMonotonicityTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildTestHull();
    const TArray<FControlRingDef> ControlRings = BuildControlRings();

    TArray<Sub3DWave2::FGeneratedRingData> Sequence;
    TArray<FString> Errors;
    TestTrue(TEXT("Build sequence"), Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(Hull, ControlRings, 32, Sequence, Errors));

    for (int32 Index = 1; Index < Sequence.Num(); ++Index)
    {
        TestTrue(TEXT("SpineAlpha strictly increasing"), Sequence[Index].SpineAlpha > Sequence[Index - 1].SpineAlpha);
    }

    return true;
}

bool FSub3DRingSequenceIncludesControlRingsTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildTestHull();
    const TArray<FControlRingDef> ControlRings = BuildControlRings();

    TArray<Sub3DWave2::FGeneratedRingData> Sequence;
    TArray<FString> Errors;
    TestTrue(TEXT("Build sequence"), Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(Hull, ControlRings, 32, Sequence, Errors));

    TSet<int32> FoundControlIndices;
    for (const Sub3DWave2::FGeneratedRingData& Ring : Sequence)
    {
        if (Ring.bIsControlRing && Ring.SourceControlRingIndex != INDEX_NONE)
        {
            FoundControlIndices.Add(Ring.SourceControlRingIndex);
        }
    }

    TestEqual(TEXT("Every control ring is represented"), FoundControlIndices.Num(), ControlRings.Num());
    return true;
}

bool FSub3DFrameRingSequenceIntegrationTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildTestHull();
    const TArray<FControlRingDef> ControlRings = BuildControlRings();

    TArray<FFrameRingDef> FrameRings;
    FFrameRingDef FrameA;
    FrameA.FrameRingId = TEXT("FR_A");
    FrameA.SpineAlpha = 0.25f;
    FrameA.bIsBayBoundary = true;
    FrameRings.Add(FrameA);

    FFrameRingDef FrameB;
    FrameB.FrameRingId = TEXT("FR_B");
    FrameB.SpineAlpha = 0.75f;
    FrameB.bIsBayBoundary = true;
    FrameRings.Add(FrameB);

    TArray<Sub3DWave2::FGeneratedRingData> Sequence;
    TArray<FString> Errors;
    TestTrue(TEXT("Build sequence"), Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(Hull, ControlRings, 32, Sequence, Errors, FrameRings));

    int32 FoundFrameRingCount = 0;
    for (const Sub3DWave2::FGeneratedRingData& Ring : Sequence)
    {
        if (Ring.bIsFrameRing)
        {
            ++FoundFrameRingCount;
        }
    }

    TestEqual(TEXT("Frame-Rings mapped to generated sequence"), FoundFrameRingCount, FrameRings.Num());
    return true;
}