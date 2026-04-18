#include "Misc/AutomationTest.h"

#include "SubmarineGeneratorEnvelopeDef.h"

// Unit tests for the M1.2 envelope refactor. These verify that EvaluateRadius
// and EvaluateBowSternTaper produce correct values across the new body/cap
// zoning. No curve dependency, no double application of the taper.

namespace
{
USubmarineGeneratorEnvelopeDef* MakeEnvelopeWithDefaults()
{
    USubmarineGeneratorEnvelopeDef* Env = NewObject<USubmarineGeneratorEnvelopeDef>(GetTransientPackage());
    // Defaults are set in the header. Override only what the test cares about.
    Env->SpineLengthCm = 1520.f;
    Env->DefaultRadiusCm = 180.f;
    Env->BowTaperFraction = 0.12f;
    Env->SternTaperFraction = 0.15f;
    Env->BodyLengthFraction = 0.55f;
    Env->BowProfile = EGenBowSternProfile::Rounded;
    Env->SternProfile = EGenBowSternProfile::Tapered;
    Env->BowSharpness = 1.f;
    Env->SternSharpness = 1.f;
    return Env;
}
}

// ── EvaluateRadius returns DefaultRadiusCm in the body zone ──────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DEnvelopeEvaluateRadiusBodyIsConstantTest,
    "Sub3D.FirstPlayable.EnvelopeEvaluateRadiusBodyIsConstant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DEnvelopeEvaluateRadiusBodyIsConstantTest::RunTest(const FString& Parameters)
{
    USubmarineGeneratorEnvelopeDef* Env = MakeEnvelopeWithDefaults();
    TestNotNull(TEXT("Envelope should construct"), Env);
    if (!Env)
    {
        return false;
    }

    // With BodyLengthFraction=0.55 and bow/stern weights 0.12/0.15, the body
    // occupies [0.2, 0.75] approximately. Middle of body is ~0.475. Test a few
    // points inside [0.25, 0.70] to stay safely in the body zone regardless
    // of exact boundary positioning.
    const float DefaultR = Env->DefaultRadiusCm;
    const float BodyCenter = 0.475f;
    const float BodySafeLow = 0.30f;
    const float BodySafeHigh = 0.70f;

    const float RMid = Env->EvaluateRadius(BodyCenter);
    const float RLow = Env->EvaluateRadius(BodySafeLow);
    const float RHigh = Env->EvaluateRadius(BodySafeHigh);

    TestEqual(TEXT("Body center radius equals DefaultRadiusCm"), RMid, DefaultR, 0.1f);
    TestEqual(TEXT("Body low-safe radius equals DefaultRadiusCm"), RLow, DefaultR, 0.1f);
    TestEqual(TEXT("Body high-safe radius equals DefaultRadiusCm"), RHigh, DefaultR, 0.1f);

    return true;
}

// ── EvaluateRadius tapers to zero at the tips ────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DEnvelopeEvaluateRadiusTipsTaperToZeroTest,
    "Sub3D.FirstPlayable.EnvelopeEvaluateRadiusTipsTaperToZero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DEnvelopeEvaluateRadiusTipsTaperToZeroTest::RunTest(const FString& Parameters)
{
    USubmarineGeneratorEnvelopeDef* Env = MakeEnvelopeWithDefaults();
    if (!Env)
    {
        return false;
    }

    const float DefaultR = Env->DefaultRadiusCm;

    // Bow tip (Pos=0) should be zero radius (Rounded profile: sin(0) = 0).
    const float RBowTip = Env->EvaluateRadius(0.f);
    TestTrue(
        TEXT("Bow tip radius should be <= 5% of DefaultRadiusCm"),
        RBowTip <= DefaultR * 0.05f);

    // Stern tip (Pos=1) should also taper toward zero.
    const float RSternTip = Env->EvaluateRadius(1.f);
    TestTrue(
        TEXT("Stern tip radius should be <= 5% of DefaultRadiusCm"),
        RSternTip <= DefaultR * 0.05f);

    return true;
}

// ── EvaluateRadius is monotonic in taper zones ───────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DEnvelopeEvaluateRadiusMonotonicityTest,
    "Sub3D.FirstPlayable.EnvelopeEvaluateRadiusMonotonicity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DEnvelopeEvaluateRadiusMonotonicityTest::RunTest(const FString& Parameters)
{
    USubmarineGeneratorEnvelopeDef* Env = MakeEnvelopeWithDefaults();
    if (!Env)
    {
        return false;
    }

    // Sample the bow taper zone from tip to body and verify non-decreasing radius.
    constexpr int32 Samples = 20;
    float PrevRadius = -1.f;
    bool bBowMonotonic = true;
    for (int32 i = 0; i <= Samples; ++i)
    {
        const float Pos = (static_cast<float>(i) / Samples) * 0.2f; // sample [0, 0.2]
        const float R = Env->EvaluateRadius(Pos);
        if (R + 0.01f < PrevRadius)
        {
            bBowMonotonic = false;
            break;
        }
        PrevRadius = R;
    }
    TestTrue(TEXT("Bow zone radius should be monotonically non-decreasing"), bBowMonotonic);

    // Stern zone: from body toward stern tip, radius should be non-increasing.
    PrevRadius = 1e9f;
    bool bSternMonotonic = true;
    for (int32 i = 0; i <= Samples; ++i)
    {
        const float Pos = 0.80f + (static_cast<float>(i) / Samples) * 0.2f; // sample [0.80, 1.0]
        const float R = Env->EvaluateRadius(Pos);
        if (R > PrevRadius + 0.01f)
        {
            bSternMonotonic = false;
            break;
        }
        PrevRadius = R;
    }
    TestTrue(TEXT("Stern zone radius should be monotonically non-increasing"), bSternMonotonic);

    return true;
}

// ── EvaluateBowSternTaper returns [0, 1] and matches EvaluateRadius shape ────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DEnvelopeEvaluateTaperRangeTest,
    "Sub3D.FirstPlayable.EnvelopeEvaluateTaperRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DEnvelopeEvaluateTaperRangeTest::RunTest(const FString& Parameters)
{
    USubmarineGeneratorEnvelopeDef* Env = MakeEnvelopeWithDefaults();
    if (!Env)
    {
        return false;
    }

    // Taper multiplier should stay in [0, 1] everywhere.
    bool bInRange = true;
    for (int32 i = 0; i <= 20; ++i)
    {
        const float Pos = static_cast<float>(i) / 20.f;
        const float Taper = Env->EvaluateBowSternTaper(Pos);
        if (Taper < -0.01f || Taper > 1.01f)
        {
            bInRange = false;
            break;
        }
    }
    TestTrue(TEXT("EvaluateBowSternTaper should return values in [0, 1]"), bInRange);

    // Body center should return exactly 1 (no taper).
    const float TaperMid = Env->EvaluateBowSternTaper(0.475f);
    TestEqual(TEXT("Body center taper multiplier equals 1"), TaperMid, 1.f, 0.001f);

    // Bow tip should return ~0.
    const float TaperBowTip = Env->EvaluateBowSternTaper(0.f);
    TestTrue(TEXT("Bow tip taper multiplier should be <= 0.05"), TaperBowTip <= 0.05f);

    return true;
}

// ── BodyLengthFraction controls the body extent ──────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DEnvelopeBodyLengthFractionControlsBodyTest,
    "Sub3D.FirstPlayable.EnvelopeBodyLengthFractionControlsBody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DEnvelopeBodyLengthFractionControlsBodyTest::RunTest(const FString& Parameters)
{
    // Small body fraction -> less of the spine is body.
    USubmarineGeneratorEnvelopeDef* EnvSmall = MakeEnvelopeWithDefaults();
    EnvSmall->BodyLengthFraction = 0.2f;
    // Large body fraction -> most of the spine is body.
    USubmarineGeneratorEnvelopeDef* EnvLarge = MakeEnvelopeWithDefaults();
    EnvLarge->BodyLengthFraction = 0.8f;

    const float DefaultR = EnvSmall->DefaultRadiusCm;

    // At position 0.15: with small body [0.08, 0.88]-ish centered, should be body-ish;
    // with large body [0.02, 0.98]-ish, should be clearly body.
    // A safer discriminator: at position 0.10, small body is in taper (0.10 < BodyStart)
    // while large body is in body (0.10 > BodyStart).
    // Compute expected body starts: Small: 0.8 * 0.12/(0.12+0.15) ≈ 0.355; Large: 0.2 * 0.12/0.27 ≈ 0.089.
    // So at Pos=0.15: small is in bow taper, large is in body.
    const float RSmall = EnvSmall->EvaluateRadius(0.15f);
    const float RLarge = EnvLarge->EvaluateRadius(0.15f);

    TestTrue(
        TEXT("Small body fraction produces a tapered (smaller) radius at Pos=0.15"),
        RSmall < DefaultR * 0.95f);
    TestEqual(
        TEXT("Large body fraction produces full DefaultRadius at Pos=0.15"),
        RLarge, DefaultR, 0.1f);

    return true;
}

// ── BowCapLengthCm does NOT affect EvaluateRadius (only BuildExteriorHull) ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DEnvelopeBowCapLengthDoesNotAffectRadiusTest,
    "Sub3D.FirstPlayable.EnvelopeBowCapLengthDoesNotAffectRadius",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DEnvelopeBowCapLengthDoesNotAffectRadiusTest::RunTest(const FString& Parameters)
{
    USubmarineGeneratorEnvelopeDef* EnvA = MakeEnvelopeWithDefaults();
    EnvA->BowCapLengthCm = 100.f;
    EnvA->SternCapLengthCm = 100.f;

    USubmarineGeneratorEnvelopeDef* EnvB = MakeEnvelopeWithDefaults();
    EnvB->BowCapLengthCm = 800.f;
    EnvB->SternCapLengthCm = 800.f;

    // Sample at multiple positions and assert the two envelopes give identical radii.
    bool bAllMatch = true;
    for (int32 i = 0; i <= 20; ++i)
    {
        const float Pos = static_cast<float>(i) / 20.f;
        const float RA = EnvA->EvaluateRadius(Pos);
        const float RB = EnvB->EvaluateRadius(Pos);
        if (!FMath::IsNearlyEqual(RA, RB, 0.001f))
        {
            bAllMatch = false;
            break;
        }
    }
    TestTrue(
        TEXT("BowCapLengthCm and SternCapLengthCm must not affect EvaluateRadius"),
        bAllMatch);

    return true;
}
