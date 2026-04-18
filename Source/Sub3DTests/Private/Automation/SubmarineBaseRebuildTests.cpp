#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include "SubDoorActor.h"
#include "SubHullComponent.h"
#include "SubmarineBase.h"
#include "SubmarineDefinition.h"
#include "SubmarineDefinitionTypes.h"
#include "SubmarineGenerator.h"
#include "SubmarineGeneratorEnvelopeDef.h"
#include "SubmarineGeneratorSpec.h"
#include "SubmarineMeshBuilder.h"
#include "SubCompiler/SubmarineGeometryBuilder.h"
#include "GeneratedGeometry/SubmarineGeneratedGeometryComponent.h"

// ── Rebuild end-to-end regression test ───────────────────────────────────────
// Loads the FirstPlayableRun GeneratorSpec, spawns an ASubmarineBase, calls
// RebuildFromSpec and asserts that the pipeline produces a non-empty Definition
// with mesh data and PMCs. This is the primary regression oracle for Step 0
// through Milestone 1.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DSubmarineBaseRebuildFromSpecTest,
    "Sub3D.FirstPlayable.SubmarineBaseRebuildFromSpec",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DSubmarineBaseRebuildFromSpecTest::RunTest(const FString& Parameters)
{
    // Load the FP spec asset. This is the same spec used by BP_Submarine_FPRun
    // in L_FP_GeneratorRun, so failures here mirror PIE failures.
    USubmarineGeneratorSpec* Spec = LoadObject<USubmarineGeneratorSpec>(
        nullptr,
        TEXT("/Game/Sub3D/FirstPlayableRun/DA_SubGenSpec_FPRun.DA_SubGenSpec_FPRun"));
    TestNotNull(TEXT("FirstPlayableRun GeneratorSpec should load"), Spec);
    if (!Spec)
    {
        return false;
    }

    TestNotNull(TEXT("GeneratorSpec.Envelope should be assigned"), Spec->Envelope.Get());
    if (!Spec->Envelope)
    {
        return false;
    }

    // Spawn a SubmarineBase in the transient package. No world, no replication.
    // This mirrors what RebuildFromSpec does when invoked from the editor button.
    ASubmarineBase* Submarine = NewObject<ASubmarineBase>(GetTransientPackage());
    TestNotNull(TEXT("ASubmarineBase should spawn in transient package"), Submarine);
    if (!Submarine)
    {
        return false;
    }

    Submarine->GeneratorSpec = Spec;

    // Invoke the generator chain directly. RebuildFromSpec is the editor helper
    // we added in Step 0 — this test exercises the same path without clicking.
    Submarine->RebuildFromSpec();

    // ── Assertions on the produced Definition ──────────────────────────────
    TestNotNull(
        TEXT("RebuildFromSpec should populate GeneratedDefinition"),
        Submarine->GeneratedDefinition.Get());
    if (!Submarine->GeneratedDefinition)
    {
        return false;
    }

    const USubmarineDefinition* Def = Submarine->GeneratedDefinition;

    TestTrue(
        TEXT("Definition should contain at least one compartment"),
        Def->Compartments.Num() > 0);
    TestTrue(
        TEXT("Definition should contain connections (bulkhead passages + airlock)"),
        Def->Connections.Num() > 0);
    TestTrue(
        TEXT("Definition should contain flood graph volumes"),
        Def->FloodGraph.Volumes.Num() > 0);
    TestEqual(
        TEXT("FloodGraph.Volumes.Num() should match Compartments.Num()"),
        Def->FloodGraph.Volumes.Num(),
        Def->Compartments.Num());

    // ── Assertions on populated mesh data ──────────────────────────────────
    TestTrue(
        TEXT("Exterior hull mesh should have vertices"),
        Def->ExteriorHullMesh.Vertices.Num() > 0);
    TestTrue(
        TEXT("Exterior hull mesh should have triangles"),
        Def->ExteriorHullMesh.Triangles.Num() > 0);
    TestTrue(
        TEXT("Interior meshes array should be non-empty"),
        Def->InteriorMeshes.Num() > 0);
    TestTrue(
        TEXT("Bulkhead meshes array should be non-empty"),
        Def->BulkheadMeshes.Num() > 0);

    // ── Assertions on materialized PMCs ────────────────────────────────────
    // Without a world, RegisterComponent is skipped but the arrays are populated.
    TestNotNull(TEXT("GeneratedGeometry component should exist"), Submarine->GeneratedGeometry);
    if (Submarine->GeneratedGeometry)
    {
        TestTrue(
            TEXT("GeneratedGeometry should have render components after rebuild"),
            Submarine->GeneratedGeometry->GetRenderComponents().Num() > 0);
    }

    return true;
}

// ── Idempotence test ─────────────────────────────────────────────────────────
// Calling RebuildFromSpec twice should produce identical counts. Guards against
// accumulator bugs where ClearGeometry misses stale state.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DSubmarineBaseRebuildIdempotentTest,
    "Sub3D.FirstPlayable.SubmarineBaseRebuildIdempotent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DSubmarineBaseRebuildIdempotentTest::RunTest(const FString& Parameters)
{
    USubmarineGeneratorSpec* Spec = LoadObject<USubmarineGeneratorSpec>(
        nullptr,
        TEXT("/Game/Sub3D/FirstPlayableRun/DA_SubGenSpec_FPRun.DA_SubGenSpec_FPRun"));
    if (!Spec)
    {
        AddError(TEXT("FirstPlayableRun GeneratorSpec should load"));
        return false;
    }

    ASubmarineBase* Submarine = NewObject<ASubmarineBase>(GetTransientPackage());
    if (!Submarine)
    {
        AddError(TEXT("ASubmarineBase should spawn in transient package"));
        return false;
    }

    Submarine->GeneratorSpec = Spec;

    // First rebuild.
    Submarine->RebuildFromSpec();
    if (!Submarine->GeneratedDefinition)
    {
        AddError(TEXT("First rebuild failed to produce Definition"));
        return false;
    }

    const int32 CompartmentsA = Submarine->GeneratedDefinition->Compartments.Num();
    const int32 ConnectionsA = Submarine->GeneratedDefinition->Connections.Num();
    const int32 ExteriorVertsA = Submarine->GeneratedDefinition->ExteriorHullMesh.Vertices.Num();
    const int32 ExteriorTrisA = Submarine->GeneratedDefinition->ExteriorHullMesh.Triangles.Num();
    const int32 RenderCompsA = Submarine->GeneratedGeometry
        ? Submarine->GeneratedGeometry->GetRenderComponents().Num()
        : 0;

    // Second rebuild.
    Submarine->RebuildFromSpec();
    if (!Submarine->GeneratedDefinition)
    {
        AddError(TEXT("Second rebuild failed to produce Definition"));
        return false;
    }

    const int32 CompartmentsB = Submarine->GeneratedDefinition->Compartments.Num();
    const int32 ConnectionsB = Submarine->GeneratedDefinition->Connections.Num();
    const int32 ExteriorVertsB = Submarine->GeneratedDefinition->ExteriorHullMesh.Vertices.Num();
    const int32 ExteriorTrisB = Submarine->GeneratedDefinition->ExteriorHullMesh.Triangles.Num();
    const int32 RenderCompsB = Submarine->GeneratedGeometry
        ? Submarine->GeneratedGeometry->GetRenderComponents().Num()
        : 0;

    TestEqual(TEXT("Compartments count stable across rebuilds"), CompartmentsB, CompartmentsA);
    TestEqual(TEXT("Connections count stable across rebuilds"), ConnectionsB, ConnectionsA);
    TestEqual(TEXT("Exterior vertices count stable across rebuilds"), ExteriorVertsB, ExteriorVertsA);
    TestEqual(TEXT("Exterior triangles count stable across rebuilds"), ExteriorTrisB, ExteriorTrisA);
    TestEqual(TEXT("Render component count stable across rebuilds"), RenderCompsB, RenderCompsA);

    return true;
}

// ── Bulkhead door cutout robustness across FloorDropBiasCm ───────────────────
// Verifies the M1.4 rewrite handles the full range {0, 40, 80} without
// degenerate bulkhead meshes. Regression oracle for the door cutout bug
// where a non-zero FloorDropBiasCm caused fan-from-pivot to collapse.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DBulkheadDoorCutoutAcrossFloorBiasTest,
    "Sub3D.FirstPlayable.BulkheadDoorCutoutAcrossFloorBias",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DBulkheadDoorCutoutAcrossFloorBiasTest::RunTest(const FString& Parameters)
{
    USubmarineGeneratorSpec* BaseSpec = LoadObject<USubmarineGeneratorSpec>(
        nullptr,
        TEXT("/Game/Sub3D/FirstPlayableRun/DA_SubGenSpec_FPRun.DA_SubGenSpec_FPRun"));
    if (!BaseSpec)
    {
        AddError(TEXT("FirstPlayableRun GeneratorSpec should load"));
        return false;
    }

    // Save and restore the original bias so the asset is not mutated.
    const float OriginalBias = BaseSpec->FloorDropBiasCm;
    ON_SCOPE_EXIT { BaseSpec->FloorDropBiasCm = OriginalBias; };

    const float TestBiases[] = { 0.f, 40.f, 80.f };
    bool bAnyBulkheadBuilt = false;

    for (const float Bias : TestBiases)
    {
        BaseSpec->FloorDropBiasCm = Bias;

        ASubmarineBase* Submarine = NewObject<ASubmarineBase>(GetTransientPackage());
        if (!Submarine)
        {
            AddError(FString::Printf(TEXT("Submarine spawn failed for bias=%.0f"), Bias));
            return false;
        }
        Submarine->GeneratorSpec = BaseSpec;
        Submarine->RebuildFromSpec();

        if (!Submarine->GeneratedDefinition)
        {
            AddError(FString::Printf(TEXT("RebuildFromSpec failed at bias=%.0f"), Bias));
            return false;
        }

        const USubmarineDefinition* Def = Submarine->GeneratedDefinition;
        const int32 BulkheadCount = Def->BulkheadMeshes.Num();

        TestTrue(
            FString::Printf(TEXT("At bias=%.0f: bulkhead meshes should be non-empty"), Bias),
            BulkheadCount > 0);

        if (BulkheadCount > 0)
        {
            bAnyBulkheadBuilt = true;
        }

        // Every bulkhead mesh must have vertices and triangles. A degenerate
        // bulkhead (like the old fan-from-pivot collapse case) would produce
        // a panel with zero triangles.
        for (int32 i = 0; i < BulkheadCount; ++i)
        {
            const FSubmarineBulkheadMeshData& Bulkhead = Def->BulkheadMeshes[i];
            const int32 VertCount = Bulkhead.PanelSection.Vertices.Num();
            const int32 TriCount = Bulkhead.PanelSection.Triangles.Num();

            TestTrue(
                FString::Printf(TEXT("At bias=%.0f: bulkhead %d has >= 6 vertices"), Bias, i),
                VertCount >= 6);
            TestTrue(
                FString::Printf(TEXT("At bias=%.0f: bulkhead %d has at least one triangle"), Bias, i),
                TriCount >= 3);
            TestTrue(
                FString::Printf(TEXT("At bias=%.0f: bulkhead %d triangle count is multiple of 3"), Bias, i),
                (TriCount % 3) == 0);
        }
    }

    TestTrue(TEXT("At least one bias variant produced a bulkhead mesh"), bAnyBulkheadBuilt);

    return true;
}

// ── SubDoorActor::InitializeFromConnectionDef correctness ───────────────────
// Verifies that the new M2.1 init method copies all relevant fields from
// FGeneratedConnectionDef into the SubDoorActor, including bStartsClosed.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DSubDoorInitializeFromConnectionDefTest,
    "Sub3D.FirstPlayable.SubDoorInitializeFromConnectionDef",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DSubDoorInitializeFromConnectionDefTest::RunTest(const FString& Parameters)
{
    ASubDoorActor* Door = NewObject<ASubDoorActor>(GetTransientPackage());
    TestNotNull(TEXT("SubDoorActor should construct"), Door);
    if (!Door)
    {
        return false;
    }

    FGeneratedConnectionDef Conn;
    Conn.ConnectionId = FName(TEXT("Door_Helm_Crew"));
    Conn.CompartmentA = FName(TEXT("Helm"));
    Conn.CompartmentB = FName(TEXT("Crew"));
    Conn.ConnectionType = EConnectionType::Door;
    Conn.bStartsClosed = true;
    Conn.DoorWidthCm = 90.f;
    Conn.DoorHeightCm = 180.f;

    Door->InitializeFromConnectionDef(Conn, nullptr);

    TestEqual(TEXT("DoorId copied from ConnectionId"), Door->DoorId, FName(TEXT("Door_Helm_Crew")));
    TestEqual(TEXT("CompartmentA copied"), Door->CompartmentA, FName(TEXT("Helm")));
    TestEqual(TEXT("CompartmentB copied"), Door->CompartmentB, FName(TEXT("Crew")));
    TestTrue(TEXT("bStartsClosed copied"), Door->bStartsClosed);

    // Second init with different values to ensure no state leaks.
    FGeneratedConnectionDef Conn2;
    Conn2.ConnectionId = FName(TEXT("Hatch_Engine_Exterior"));
    Conn2.CompartmentA = FName(TEXT("Engine"));
    Conn2.CompartmentB = NAME_None;
    Conn2.ConnectionType = EConnectionType::ExteriorHatch;
    Conn2.bStartsClosed = false;

    Door->InitializeFromConnectionDef(Conn2, nullptr);

    TestEqual(TEXT("DoorId updated on second init"), Door->DoorId, FName(TEXT("Hatch_Engine_Exterior")));
    TestEqual(TEXT("CompartmentB cleared when None"), Door->CompartmentB, FName(NAME_None));
    TestFalse(TEXT("bStartsClosed cleared"), Door->bStartsClosed);

    return true;
}

// ── SpawnDoorsFromDefinition robustness without a world ─────────────────────
// Verifies that SpawnDoorsFromDefinition returns safely when GetWorld() is
// null (transient package in automation context) instead of crashing. This
// is the guardrail the function needs to remain callable from RebuildFromSpec
// in editor preview contexts where the actor has no world.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DSpawnDoorsNoWorldSafeTest,
    "Sub3D.FirstPlayable.SpawnDoorsNoWorldSafe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DSpawnDoorsNoWorldSafeTest::RunTest(const FString& Parameters)
{
    USubmarineGeneratorSpec* Spec = LoadObject<USubmarineGeneratorSpec>(
        nullptr,
        TEXT("/Game/Sub3D/FirstPlayableRun/DA_SubGenSpec_FPRun.DA_SubGenSpec_FPRun"));
    if (!Spec)
    {
        AddError(TEXT("FirstPlayableRun GeneratorSpec should load"));
        return false;
    }

    ASubmarineBase* Submarine = NewObject<ASubmarineBase>(GetTransientPackage());
    if (!Submarine)
    {
        AddError(TEXT("ASubmarineBase should spawn in transient package"));
        return false;
    }

    Submarine->GeneratorSpec = Spec;
    Submarine->RebuildFromSpec();

    // Direct call to SpawnDoorsFromDefinition with null world and null
    // DoorActorClass. Expected: no crash, graceful early return.
    Submarine->SpawnDoorsFromDefinition();

    // Now set a GeneratorDoorActorClass and call again. Still no world → still safe.
    Submarine->GeneratorDoorActorClass = ASubDoorActor::StaticClass();
    Submarine->SpawnDoorsFromDefinition();

    // No direct assertion beyond "did not crash".
    TestTrue(TEXT("SpawnDoorsFromDefinition returned safely"), true);

    return true;
}

// ── Envelope contract propagation through the generator ────────────────────
// Regression oracle for the review issue where EvaluateRadius already baked
// in the taper but the generator kept multiplying by EvaluateBowSternTaper
// (double-apply) and DeriveCompartments still used the raw BowTaperFraction
// / SternTaperFraction as compartment boundaries (old model). If anyone
// re-introduces either bug, this test catches it:
//   (a) every non-airlock compartment's HydroBoundsMax.Z must match
//       max(10, Envelope->EvaluateRadius(MidNorm) - WallThickness)
//       within a tight tolerance;
//   (b) every non-airlock compartment must live in [BodyStart, BodyEnd]
//       as reported by the envelope's GetBodyBounds.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DGeneratorEnvelopeContractPropagationTest,
    "Sub3D.FirstPlayable.GeneratorEnvelopeContractPropagation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DGeneratorEnvelopeContractPropagationTest::RunTest(const FString& Parameters)
{
    USubmarineGeneratorSpec* Spec = LoadObject<USubmarineGeneratorSpec>(
        nullptr,
        TEXT("/Game/Sub3D/FirstPlayableRun/DA_SubGenSpec_FPRun.DA_SubGenSpec_FPRun"));
    if (!Spec || !Spec->Envelope)
    {
        AddError(TEXT("FirstPlayableRun GeneratorSpec and Envelope should load"));
        return false;
    }

    ASubmarineBase* Submarine = NewObject<ASubmarineBase>(GetTransientPackage());
    if (!Submarine)
    {
        AddError(TEXT("Submarine should spawn in transient package"));
        return false;
    }

    Submarine->GeneratorSpec = Spec;
    Submarine->RebuildFromSpec();

    USubmarineDefinition* Def = Submarine->GeneratedDefinition;
    if (!Def)
    {
        AddError(TEXT("RebuildFromSpec should populate GeneratedDefinition"));
        return false;
    }

    const USubmarineGeneratorEnvelopeDef* Envelope = Spec->Envelope;
    const float SpineLength = FMath::Max(1.f, Envelope->SpineLengthCm);
    const float WallThickness = Spec->WallThicknessCm;

    float BodyStart = 0.f;
    float BodyEnd = 1.f;
    Envelope->GetBodyBounds(BodyStart, BodyEnd);

    // Clamp epsilon: GetBodyBounds uses BodyLengthFraction clamped to [0.1, 0.9]
    // and there is a ±0.01 clamp in DeriveCompartments for bulkhead positions.
    constexpr float BoundaryEpsilon = 0.02f;
    constexpr float RadiusToleranceCm = 0.5f;

    int32 HullCompartmentsChecked = 0;
    for (const FGeneratedCompartmentDef& Comp : Def->Compartments)
    {
        if (Comp.SemanticType == ESubCompartmentType::Airlock)
        {
            continue;
        }

        // (a) radius matches the analytical envelope formula at the midpoint.
        const float MidX = (Comp.HydroBoundsMin.X + Comp.HydroBoundsMax.X) * 0.5f;
        const float MidNorm = FMath::Clamp(MidX / SpineLength, 0.f, 1.f);
        const float EnvelopeR = Envelope->EvaluateRadius(MidNorm);
        const float ExpectedInnerR = FMath::Max(10.f, EnvelopeR - WallThickness);
        const float ActualInnerR = Comp.HydroBoundsMax.Z;

        TestEqual(
            FString::Printf(
                TEXT("Compartment '%s' InnerR matches EvaluateRadius(%.3f) - WallThickness"),
                *Comp.CompartmentId.ToString(), MidNorm),
            ActualInnerR,
            ExpectedInnerR,
            RadiusToleranceCm);

        // (b) the compartment lives inside the body region [BodyStart, BodyEnd].
        const float StartNorm = Comp.HydroBoundsMin.X / SpineLength;
        const float EndNorm = Comp.HydroBoundsMax.X / SpineLength;

        TestTrue(
            FString::Printf(
                TEXT("Compartment '%s' StartNorm (%.3f) >= BodyStart (%.3f) - eps"),
                *Comp.CompartmentId.ToString(), StartNorm, BodyStart),
            StartNorm >= BodyStart - BoundaryEpsilon);

        TestTrue(
            FString::Printf(
                TEXT("Compartment '%s' EndNorm (%.3f) <= BodyEnd (%.3f) + eps"),
                *Comp.CompartmentId.ToString(), EndNorm, BodyEnd),
            EndNorm <= BodyEnd + BoundaryEpsilon);

        ++HullCompartmentsChecked;
    }

    TestTrue(
        TEXT("At least one non-airlock compartment was checked"),
        HullCompartmentsChecked > 0);

    // Cross-check: FindCompartment roundtrip still works after the refactor.
    for (const FGeneratedCompartmentDef& Comp : Def->Compartments)
    {
        const FGeneratedCompartmentDef* Found = Def->FindCompartment(Comp.CompartmentId);
        TestNotNull(
            FString::Printf(TEXT("FindCompartment('%s') must succeed"), *Comp.CompartmentId.ToString()),
            Found);
    }

    return true;
}

// ── Door lifecycle across ClearGeneratedState / RebuildFromSpec ─────────────
// Regression oracle for the review issue where RebuildFromSpec did not clear
// spawned door actors, and ClearGeneratedState had the same gap. Without a
// world the SpawnDoorsFromDefinition early-outs before creating any actors,
// so the array should remain at 0 even after the spawn call. After a clear
// the array must remain at 0. This is a structural test: if anyone adds
// direct pushes to SpawnedGeneratorDoors without routing through the
// lifecycle hooks, or if ClearGeneratedState stops calling the destroy
// helper, the count will drift and this test catches it.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DSpawnedDoorLifecycleTest,
    "Sub3D.FirstPlayable.SpawnedDoorLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DSpawnedDoorLifecycleTest::RunTest(const FString& Parameters)
{
    USubmarineGeneratorSpec* Spec = LoadObject<USubmarineGeneratorSpec>(
        nullptr,
        TEXT("/Game/Sub3D/FirstPlayableRun/DA_SubGenSpec_FPRun.DA_SubGenSpec_FPRun"));
    if (!Spec)
    {
        AddError(TEXT("FirstPlayableRun GeneratorSpec should load"));
        return false;
    }

    ASubmarineBase* Submarine = NewObject<ASubmarineBase>(GetTransientPackage());
    if (!Submarine)
    {
        AddError(TEXT("Submarine should spawn in transient package"));
        return false;
    }

    Submarine->GeneratorSpec = Spec;

    // Pre-state: no doors.
    TestEqual(
        TEXT("Fresh submarine has zero spawned doors"),
        Submarine->GetSpawnedGeneratorDoorCount(), 0);

    // First rebuild. Without a world, SpawnDoorsFromDefinition early-returns
    // without actually creating actors, so the array stays at 0 but the
    // lifecycle path has been exercised.
    Submarine->RebuildFromSpec();
    TestEqual(
        TEXT("After rebuild without world, spawned door count is 0"),
        Submarine->GetSpawnedGeneratorDoorCount(), 0);

    // Second rebuild: lifecycle must not leak any lingering entries. If
    // DestroySpawnedGeneratorDoors stopped clearing the array, the count
    // would grow on repeated rebuilds (even if the destroy calls are no-ops
    // without a world).
    Submarine->RebuildFromSpec();
    TestEqual(
        TEXT("After second rebuild, spawned door count is still 0"),
        Submarine->GetSpawnedGeneratorDoorCount(), 0);

    // ClearGeneratedState must leave the array at 0 too, and must not crash
    // when there is nothing to clear.
    Submarine->ClearGeneratedState();
    TestEqual(
        TEXT("After ClearGeneratedState, spawned door count is 0"),
        Submarine->GetSpawnedGeneratorDoorCount(), 0);

    // Double clear: no crash, still 0.
    Submarine->ClearGeneratedState();
    TestEqual(
        TEXT("After second ClearGeneratedState, spawned door count is 0"),
        Submarine->GetSpawnedGeneratorDoorCount(), 0);

    return true;
}

// ── SubHull::ClearAllBreaches / ClearBreachNearLocation ─────────────────────
// Minimal sanity test for the M2.4 repair API: calling either method on a
// freshly-constructed hull with zero breaches must return 0 and not crash.
// The methods are also exercised on a full submarine to ensure they do not
// interfere with the generator path.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSub3DSubHullRepairApiSafeTest,
    "Sub3D.FirstPlayable.SubHullRepairApiSafe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSub3DSubHullRepairApiSafeTest::RunTest(const FString& Parameters)
{
    USubmarineGeneratorSpec* Spec = LoadObject<USubmarineGeneratorSpec>(
        nullptr,
        TEXT("/Game/Sub3D/FirstPlayableRun/DA_SubGenSpec_FPRun.DA_SubGenSpec_FPRun"));
    if (!Spec)
    {
        AddError(TEXT("FirstPlayableRun GeneratorSpec should load"));
        return false;
    }

    ASubmarineBase* Submarine = NewObject<ASubmarineBase>(GetTransientPackage());
    if (!Submarine || !Submarine->SubHull)
    {
        AddError(TEXT("Submarine with SubHull should construct"));
        return false;
    }

    Submarine->GeneratorSpec = Spec;
    Submarine->RebuildFromSpec();

    // On a fresh hull, BreachClusters is empty. Both clear operations must
    // return 0 without touching any other state.
    const int32 ClearedAll = Submarine->SubHull->ClearAllBreaches();
    TestEqual(TEXT("ClearAllBreaches on empty hull returns 0"), ClearedAll, 0);

    const int32 ClearedLocal = Submarine->SubHull->ClearBreachNearLocation(
        FVector::ZeroVector, 500.f);
    TestEqual(TEXT("ClearBreachNearLocation on empty hull returns 0"), ClearedLocal, 0);

    // Negative radius is a programming error but should not crash.
    const int32 ClearedNeg = Submarine->SubHull->ClearBreachNearLocation(
        FVector::ZeroVector, -10.f);
    TestEqual(TEXT("ClearBreachNearLocation with negative radius returns 0"), ClearedNeg, 0);

    return true;
}
