#include "Misc/AutomationTest.h"
#include "Bake/SubmarineHullProfileService.h"
#include "Bake/SubmarineRingSequenceBuilder.h"
#include "Bake/SubmarineHullBakeService.h"

// --------------------------------------------------------------------------
// Profile service — radius fraction evaluation
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileMyringEndpointsTest,
    "Sub3D.HullProfile.MyringEndpointsAreZero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileMyringPeakTest,
    "Sub3D.HullProfile.MyringPeakIsOne",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileSeries58EndpointsTest,
    "Sub3D.HullProfile.Series58EndpointsAreZero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileSuperellipseSymmetryTest,
    "Sub3D.HullProfile.SuperellipseIsSymmetric",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileUniformMidbodyTest,
    "Sub3D.HullProfile.UniformMidbodyIsOne",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------
// Profile service — ring generation
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileManualRejectsTest,
    "Sub3D.HullProfile.ManualProfileRejectsGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileMyringGeneratesRingsTest,
    "Sub3D.HullProfile.MyringGeneratesRings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileRingsAreOrderedTest,
    "Sub3D.HullProfile.GeneratedRingsAreOrdered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileRingsBoundedTest,
    "Sub3D.HullProfile.GeneratedRingsWithinHullBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileRingsDimensionsBoundedTest,
    "Sub3D.HullProfile.GeneratedRingDimensionsNeverExceedDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------
// Full pipeline: profile → ring sequence → hull bake (no mesh errors)
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileMyringBakePipelineTest,
    "Sub3D.HullProfile.MyringBakePipelineSucceeds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileSeries58BakePipelineTest,
    "Sub3D.HullProfile.Series58BakePipelineSucceeds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DHullProfileSuperellipseBakePipelineTest,
    "Sub3D.HullProfile.SuperellipseBakePipelineSucceeds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
static FSubmarineHullDef BuildProfileHull(
    const ESub3DHullLongitudinalProfile Profile,
    const float HalfWidth = 200.0f,
    const float HalfHeight = 180.0f)
{
    FSubmarineHullDef Hull;
    Hull.LengthCm = 7200.0f;
    Hull.DefaultHalfWidthCm = HalfWidth;
    Hull.DefaultHalfHeightCm = HalfHeight;
    Hull.DefaultWallThicknessCm = 12.0f;
    Hull.DefaultSectionProfile = ESub3DSectionProfile::Ellipse;
    Hull.DefaultSectionRoundness = 0.5f;
    Hull.ProfileParams.Profile = Profile;
    Hull.ProfileParams.MyringNoseExponent = 2.0f;
    Hull.ProfileParams.MyringTailAngleDeg = 25.0f;
    Hull.ProfileParams.MyringNoseFraction = 0.20f;
    Hull.ProfileParams.MyringTailFraction = 0.25f;
    Hull.ProfileParams.Series58Fineness = 7.0f;
    Hull.ProfileParams.LongitudinalExponent = 2.5f;
    Hull.ProfileParams.ParallelMidbodyFraction = 0.4f;
    return Hull;
}

static bool BakeHullFromProfile(const FSubmarineHullDef& Hull, FCompiledMeshSection& OutExterior)
{
    TArray<FControlRingDef> GeneratedRings;
    TArray<FString> ProfileErrors;
    if (!Sub3DWave2::FSubmarineHullProfileService::GenerateControlRingsFromProfile(Hull, GeneratedRings, ProfileErrors))
    {
        return false;
    }

    TArray<Sub3DWave2::FGeneratedRingData> RingSequence;
    TArray<FString> SeqErrors;
    if (!Sub3DWave2::FSubmarineRingSequenceBuilder::BuildRingSequence(Hull, GeneratedRings, 32, RingSequence, SeqErrors))
    {
        return false;
    }

    Sub3DWave2::FHullBakeSettings Settings;
    Settings.RadialSegments = 24;
    Settings.bBakeCollision = false;

    return Sub3DWave2::FSubmarineHullBakeService::BakeExteriorHull(Hull, RingSequence, Settings, OutExterior);
}
} // namespace

// ==========================================================================
// Radius fraction evaluation tests
// ==========================================================================

bool FSub3DHullProfileMyringEndpointsTest::RunTest(const FString& Parameters)
{
    const FHullProfileParams Params = BuildProfileHull(ESub3DHullLongitudinalProfile::Myring).ProfileParams;

    const float AtZero = Sub3DWave2::FSubmarineHullProfileService::EvaluateRadiusFraction(
        ESub3DHullLongitudinalProfile::Myring, Params, 0.0f);
    const float AtOne = Sub3DWave2::FSubmarineHullProfileService::EvaluateRadiusFraction(
        ESub3DHullLongitudinalProfile::Myring, Params, 1.0f);

    TestTrue(TEXT("Myring radius at 0.0 is 0"), FMath::IsNearlyZero(AtZero, 0.001f));
    TestTrue(TEXT("Myring radius at 1.0 is 0"), FMath::IsNearlyZero(AtOne, 0.001f));
    return true;
}

bool FSub3DHullProfileMyringPeakTest::RunTest(const FString& Parameters)
{
    const FHullProfileParams Params = BuildProfileHull(ESub3DHullLongitudinalProfile::Myring).ProfileParams;

    // Midbody (well inside nose+tail fractions) should be 1.0
    const float AtMid = Sub3DWave2::FSubmarineHullProfileService::EvaluateRadiusFraction(
        ESub3DHullLongitudinalProfile::Myring, Params, 0.5f);

    TestTrue(TEXT("Myring midbody = 1.0"), FMath::IsNearlyEqual(AtMid, 1.0f, 0.001f));
    return true;
}

bool FSub3DHullProfileSeries58EndpointsTest::RunTest(const FString& Parameters)
{
    const FHullProfileParams Params = BuildProfileHull(ESub3DHullLongitudinalProfile::Series58).ProfileParams;

    const float AtZero = Sub3DWave2::FSubmarineHullProfileService::EvaluateRadiusFraction(
        ESub3DHullLongitudinalProfile::Series58, Params, 0.0f);
    const float AtOne = Sub3DWave2::FSubmarineHullProfileService::EvaluateRadiusFraction(
        ESub3DHullLongitudinalProfile::Series58, Params, 1.0f);

    TestTrue(TEXT("Series58 radius at 0.0 is 0"), FMath::IsNearlyZero(AtZero, 0.001f));
    TestTrue(TEXT("Series58 radius at 1.0 is 0"), FMath::IsNearlyZero(AtOne, 0.001f));
    return true;
}

bool FSub3DHullProfileSuperellipseSymmetryTest::RunTest(const FString& Parameters)
{
    const FHullProfileParams Params = BuildProfileHull(ESub3DHullLongitudinalProfile::SuperellipseLongitudinal).ProfileParams;

    const float At02 = Sub3DWave2::FSubmarineHullProfileService::EvaluateRadiusFraction(
        ESub3DHullLongitudinalProfile::SuperellipseLongitudinal, Params, 0.2f);
    const float At08 = Sub3DWave2::FSubmarineHullProfileService::EvaluateRadiusFraction(
        ESub3DHullLongitudinalProfile::SuperellipseLongitudinal, Params, 0.8f);

    TestTrue(TEXT("Superellipse is symmetric around 0.5"), FMath::IsNearlyEqual(At02, At08, 0.001f));
    return true;
}

bool FSub3DHullProfileUniformMidbodyTest::RunTest(const FString& Parameters)
{
    const FHullProfileParams Params = BuildProfileHull(ESub3DHullLongitudinalProfile::Uniform).ProfileParams;

    // Midbody fraction = 0.4 → parallel midbody from 0.3 to 0.7
    const float At04 = Sub3DWave2::FSubmarineHullProfileService::EvaluateRadiusFraction(
        ESub3DHullLongitudinalProfile::Uniform, Params, 0.4f);
    const float At06 = Sub3DWave2::FSubmarineHullProfileService::EvaluateRadiusFraction(
        ESub3DHullLongitudinalProfile::Uniform, Params, 0.6f);
    const float AtZero = Sub3DWave2::FSubmarineHullProfileService::EvaluateRadiusFraction(
        ESub3DHullLongitudinalProfile::Uniform, Params, 0.0f);

    TestTrue(TEXT("Uniform midbody at 0.4 is 1.0"), FMath::IsNearlyEqual(At04, 1.0f, 0.001f));
    TestTrue(TEXT("Uniform midbody at 0.6 is 1.0"), FMath::IsNearlyEqual(At06, 1.0f, 0.001f));
    TestTrue(TEXT("Uniform tip at 0.0 is 0"), FMath::IsNearlyZero(AtZero, 0.001f));
    return true;
}

// ==========================================================================
// Ring generation tests
// ==========================================================================

bool FSub3DHullProfileManualRejectsTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildProfileHull(ESub3DHullLongitudinalProfile::Manual);
    TArray<FControlRingDef> Rings;
    TArray<FString> Errors;
    const bool bResult = Sub3DWave2::FSubmarineHullProfileService::GenerateControlRingsFromProfile(Hull, Rings, Errors);
    TestFalse(TEXT("Manual profile must reject ring generation"), bResult);
    TestTrue(TEXT("Manual profile returns zero rings"), Rings.Num() == 0);
    return true;
}

bool FSub3DHullProfileMyringGeneratesRingsTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildProfileHull(ESub3DHullLongitudinalProfile::Myring);
    TArray<FControlRingDef> Rings;
    TArray<FString> Errors;
    TestTrue(TEXT("Myring generates rings"), Sub3DWave2::FSubmarineHullProfileService::GenerateControlRingsFromProfile(Hull, Rings, Errors));
    TestTrue(TEXT("Myring generates at least 2 rings"), Rings.Num() >= 2);
    return true;
}

bool FSub3DHullProfileRingsAreOrderedTest::RunTest(const FString& Parameters)
{
    const TArray<ESub3DHullLongitudinalProfile> Profiles = {
        ESub3DHullLongitudinalProfile::Myring,
        ESub3DHullLongitudinalProfile::Series58,
        ESub3DHullLongitudinalProfile::SuperellipseLongitudinal,
        ESub3DHullLongitudinalProfile::Uniform
    };

    for (const ESub3DHullLongitudinalProfile Profile : Profiles)
    {
        const FSubmarineHullDef Hull = BuildProfileHull(Profile);
        TArray<FControlRingDef> Rings;
        TArray<FString> Errors;
        if (!TestTrue(FString::Printf(TEXT("Profile %d generates rings"), static_cast<int32>(Profile)),
            Sub3DWave2::FSubmarineHullProfileService::GenerateControlRingsFromProfile(Hull, Rings, Errors)))
        {
            continue;
        }

        for (int32 Index = 1; Index < Rings.Num(); ++Index)
        {
            TestTrue(
                FString::Printf(TEXT("Profile %d: ring %d PositionX > ring %d"), static_cast<int32>(Profile), Index, Index - 1),
                Rings[Index].PositionX > Rings[Index - 1].PositionX);
        }
    }
    return true;
}

bool FSub3DHullProfileRingsBoundedTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildProfileHull(ESub3DHullLongitudinalProfile::Myring);
    TArray<FControlRingDef> Rings;
    TArray<FString> Errors;
    TestTrue(TEXT("Myring generates rings"), Sub3DWave2::FSubmarineHullProfileService::GenerateControlRingsFromProfile(Hull, Rings, Errors));

    for (const FControlRingDef& Ring : Rings)
    {
        TestTrue(
            FString::Printf(TEXT("Ring '%s' PositionX >= 0"), *Ring.ControlRingId.ToString()),
            Ring.PositionX >= 0.0f);
        TestTrue(
            FString::Printf(TEXT("Ring '%s' PositionX <= LengthCm"), *Ring.ControlRingId.ToString()),
            Ring.PositionX <= Hull.LengthCm);
    }
    return true;
}

bool FSub3DHullProfileRingsDimensionsBoundedTest::RunTest(const FString& Parameters)
{
    const TArray<ESub3DHullLongitudinalProfile> Profiles = {
        ESub3DHullLongitudinalProfile::Myring,
        ESub3DHullLongitudinalProfile::Series58,
        ESub3DHullLongitudinalProfile::SuperellipseLongitudinal,
        ESub3DHullLongitudinalProfile::Uniform
    };

    for (const ESub3DHullLongitudinalProfile Profile : Profiles)
    {
        const FSubmarineHullDef Hull = BuildProfileHull(Profile);
        TArray<FControlRingDef> Rings;
        TArray<FString> Errors;
        if (!Sub3DWave2::FSubmarineHullProfileService::GenerateControlRingsFromProfile(Hull, Rings, Errors))
        {
            continue;
        }

        for (const FControlRingDef& Ring : Rings)
        {
            TestTrue(
                FString::Printf(TEXT("Profile %d: Ring '%s' HalfWidth <= DefaultHalfWidth"),
                    static_cast<int32>(Profile), *Ring.ControlRingId.ToString()),
                Ring.HalfWidthCm <= Hull.DefaultHalfWidthCm + KINDA_SMALL_NUMBER);
            TestTrue(
                FString::Printf(TEXT("Profile %d: Ring '%s' HalfHeight <= DefaultHalfHeight"),
                    static_cast<int32>(Profile), *Ring.ControlRingId.ToString()),
                Ring.HalfHeightCm <= Hull.DefaultHalfHeightCm + KINDA_SMALL_NUMBER);
            TestTrue(
                FString::Printf(TEXT("Profile %d: Ring '%s' HalfWidth >= 1.0"),
                    static_cast<int32>(Profile), *Ring.ControlRingId.ToString()),
                Ring.HalfWidthCm >= 1.0f);
        }
    }
    return true;
}

// ==========================================================================
// Full pipeline tests
// ==========================================================================

bool FSub3DHullProfileMyringBakePipelineTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildProfileHull(ESub3DHullLongitudinalProfile::Myring);
    FCompiledMeshSection Exterior;
    TestTrue(TEXT("Myring full pipeline produces exterior hull"), BakeHullFromProfile(Hull, Exterior));
    TestTrue(TEXT("Myring exterior hull has vertices"), Exterior.Positions.Num() > 0);
    TestTrue(TEXT("Myring exterior hull has indices"), Exterior.Indices.Num() > 0);
    return true;
}

bool FSub3DHullProfileSeries58BakePipelineTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildProfileHull(ESub3DHullLongitudinalProfile::Series58);
    FCompiledMeshSection Exterior;
    TestTrue(TEXT("Series58 full pipeline produces exterior hull"), BakeHullFromProfile(Hull, Exterior));
    TestTrue(TEXT("Series58 exterior hull has vertices"), Exterior.Positions.Num() > 0);
    TestTrue(TEXT("Series58 exterior hull has indices"), Exterior.Indices.Num() > 0);
    return true;
}

bool FSub3DHullProfileSuperellipseBakePipelineTest::RunTest(const FString& Parameters)
{
    const FSubmarineHullDef Hull = BuildProfileHull(ESub3DHullLongitudinalProfile::SuperellipseLongitudinal);
    FCompiledMeshSection Exterior;
    TestTrue(TEXT("Superellipse full pipeline produces exterior hull"), BakeHullFromProfile(Hull, Exterior));
    TestTrue(TEXT("Superellipse exterior hull has vertices"), Exterior.Positions.Num() > 0);
    TestTrue(TEXT("Superellipse exterior hull has indices"), Exterior.Indices.Num() > 0);
    return true;
}
