#include "Misc/AutomationTest.h"

#include "Bake/SubmarineOuterEnvelopeBakeService.h"
#include "Bake/SubmarineRingSequenceBuilder.h"
#include "Types/Sub3DCompiledTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DOuterEnvelopeDisabledTest,
    "Sub3D.Wave3.OuterEnvelopeDisabled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DOuterEnvelopeEnabledTest,
    "Sub3D.Wave3.OuterEnvelopeEnabled",
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
    Hull.DefaultWallThicknessCm = 10.0f;
    return Hull;
}

static TArray<FControlRingDef> BuildControlRings()
{
    TArray<FControlRingDef> Rings;

    FControlRingDef A;
    A.PositionX = 0.0f;
    Rings.Add(A);

    FControlRingDef B;
    B.PositionX = 3000.0f;
    B.HalfWidthCm = 220.0f;
    B.HalfHeightCm = 220.0f;
    Rings.Add(B);

    FControlRingDef C;
    C.PositionX = 6000.0f;
    C.HalfWidthCm = 180.0f;
    C.HalfHeightCm = 170.0f;
    Rings.Add(C);

    return Rings;
}

static bool BuildRingSequence(const FSubmarineHullDef& Hull, TArray<Sub3DWave2::FGeneratedRingData>& OutSequence)
{
    const TArray<FControlRingDef> ControlRings = BuildControlRings();
    TArray<FString> Errors;
    return Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(Hull, ControlRings, 24, OutSequence, Errors);
}
} // namespace

bool FSub3DOuterEnvelopeDisabledTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildTestHull();

    TArray<Sub3DWave2::FGeneratedRingData> RingSequence;
    TestTrue(TEXT("BuildRingSequence"), BuildRingSequence(Hull, RingSequence));

    FOuterEnvelopeDef OuterEnvelope;
    OuterEnvelope.bEnabled = false;

    FCompiledMeshSection Envelope;
    FCompiledMeshSection Collision;
    TArray<FString> Errors;

    TestTrue(
        TEXT("BakeOuterEnvelope disabled"),
        Sub3DWave3::FSubmarineOuterEnvelopeBakeService::BakeOuterEnvelope(Hull, RingSequence, OuterEnvelope, Envelope, Collision, Errors));
    TestEqual(TEXT("Envelope has no vertices when disabled"), Envelope.Positions.Num(), 0);

    return true;
}

bool FSub3DOuterEnvelopeEnabledTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildTestHull();

    TArray<Sub3DWave2::FGeneratedRingData> RingSequence;
    TestTrue(TEXT("BuildRingSequence"), BuildRingSequence(Hull, RingSequence));

    FOuterEnvelopeDef OuterEnvelope;
    OuterEnvelope.bEnabled = true;
    OuterEnvelope.BaseSpineAlpha = 0.35f;
    OuterEnvelope.LengthCm = 700.0f;
    OuterEnvelope.WidthCm = 300.0f;
    OuterEnvelope.HeightCm = 280.0f;
    OuterEnvelope.Shape = ESub3DEnvelopeShape::Faired;

    FCompiledMeshSection Envelope;
    FCompiledMeshSection Collision;
    TArray<FString> Errors;

    TestTrue(
        TEXT("BakeOuterEnvelope enabled"),
        Sub3DWave3::FSubmarineOuterEnvelopeBakeService::BakeOuterEnvelope(Hull, RingSequence, OuterEnvelope, Envelope, Collision, Errors));
    TestTrue(TEXT("Envelope has vertices"), Envelope.Positions.Num() > 0);
    TestTrue(TEXT("Collision is simplified"), Collision.Positions.Num() < Envelope.Positions.Num());

    return true;
}