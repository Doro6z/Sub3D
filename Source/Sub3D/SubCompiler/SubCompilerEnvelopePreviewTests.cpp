#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "SubCompilerMvpFactory.h"
#include "SubmarineCompilerActor.h"
#include "SubCompilerTypes.h"
#include "SubmarineEnvelopeDef.h"
#include "SubmarineFunctionalGraph.h"

namespace
{
float FindMaxRadius(const FSubmarineLayoutSolution& Solution)
{
	float MaxRadius = 0.f;
	for (const FCompartmentPlacement& Placement : Solution.Compartments)
	{
		MaxRadius = FMath::Max(MaxRadius, Placement.EffectiveRadiusCm);
	}

	return MaxRadius;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerEnvelopePreviewOverrideTest,
	"Sub3D.SubCompiler.CompilerActor.PreviewOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerEnvelopePreviewOverrideTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* BaselineActor = NewObject<ASubmarineCompilerActor>(GetTransientPackage());
	TestNotNull(TEXT("Baseline actor should exist"), BaselineActor);

	if (!BaselineActor)
	{
		return false;
	}

	BaselineActor->EnvelopeDef = FSubCompilerMvpFactory::CreateEnvelope(BaselineActor);
	BaselineActor->FunctionalGraph = FSubCompilerMvpFactory::CreateFunctionalGraph(BaselineActor);
	BaselineActor->bUseEnvelopePreviewOverrides = false;
	TestTrue(TEXT("Baseline actor should compile"), BaselineActor->CompileCurrentDefinitions());

	const FSubmarineLayoutSolution BaselineSolution = BaselineActor->GetLastSolution();
	TestTrue(TEXT("Baseline solution should be valid"), BaselineSolution.IsValid());

	ASubmarineCompilerActor* PreviewActor = NewObject<ASubmarineCompilerActor>(GetTransientPackage());
	TestNotNull(TEXT("Preview actor should exist"), PreviewActor);

	if (!PreviewActor)
	{
		return false;
	}

	PreviewActor->EnvelopeDef = FSubCompilerMvpFactory::CreateEnvelope(PreviewActor);
	PreviewActor->FunctionalGraph = FSubCompilerMvpFactory::CreateFunctionalGraph(PreviewActor);
	PreviewActor->bUseEnvelopePreviewOverrides = true;
	PreviewActor->PreviewSpineLengthCm = 2400.f;
	PreviewActor->PreviewDefaultRadiusCm = 270.f;
	PreviewActor->PreviewFloorDropBiasCm = 130.f;
	PreviewActor->PreviewBowRadiusCm = 190.f;
	PreviewActor->PreviewForeShoulderRadiusCm = 255.f;
	PreviewActor->PreviewMidBodyRadiusCm = 320.f;
	PreviewActor->PreviewAftShoulderRadiusCm = 245.f;
	PreviewActor->PreviewSternRadiusCm = 170.f;
	PreviewActor->PreviewExteriorLongitudinalSubdivisionsPerSpan = 8;
	PreviewActor->PreviewExteriorRadialSegments = 40;

	TestTrue(TEXT("Preview actor should compile"), PreviewActor->CompileCurrentDefinitions());

	const FSubmarineLayoutSolution PreviewSolution = PreviewActor->GetLastSolution();
	TestTrue(TEXT("Preview solution should be valid"), PreviewSolution.IsValid());
	TestEqual(TEXT("Preview solution should use override length"), PreviewSolution.Metrics.TotalLengthCm, 2400.f);
	TestTrue(
		TEXT("Preview overrides should lower the floor compared to baseline"),
		PreviewSolution.Compartments[0].FloorOffsetCm < BaselineSolution.Compartments[0].FloorOffsetCm);
	TestTrue(
		TEXT("Preview overrides should increase the max radius compared to baseline"),
		FindMaxRadius(PreviewSolution) > FindMaxRadius(BaselineSolution));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerEnvelopePreviewSaveTest,
	"Sub3D.SubCompiler.CompilerActor.SavePreviewToEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerEnvelopePreviewSaveTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* PreviewActor = NewObject<ASubmarineCompilerActor>(GetTransientPackage());
	TestNotNull(TEXT("Preview actor should exist"), PreviewActor);

	if (!PreviewActor)
	{
		return false;
	}

	PreviewActor->EnvelopeDef = FSubCompilerMvpFactory::CreateEnvelope(PreviewActor);
	PreviewActor->FunctionalGraph = FSubCompilerMvpFactory::CreateFunctionalGraph(PreviewActor);
	PreviewActor->PreviewSpineLengthCm = 2450.f;
	PreviewActor->PreviewDefaultRadiusCm = 280.f;
	PreviewActor->PreviewFloorDropBiasCm = 140.f;
	PreviewActor->PreviewBowRadiusCm = 195.f;
	PreviewActor->PreviewForeShoulderRadiusCm = 260.f;
	PreviewActor->PreviewMidBodyRadiusCm = 330.f;
	PreviewActor->PreviewAftShoulderRadiusCm = 250.f;
	PreviewActor->PreviewSternRadiusCm = 175.f;
	PreviewActor->PreviewExteriorLongitudinalSubdivisionsPerSpan = 9;
	PreviewActor->PreviewExteriorRadialSegments = 36;

	TestTrue(TEXT("SavePreviewToEnvelope should succeed"), PreviewActor->SavePreviewToEnvelope());
	TestEqual(TEXT("Envelope length should be updated"), PreviewActor->EnvelopeDef->SpineLengthCm, 2450.f);
	TestEqual(TEXT("Envelope default radius should be updated"), PreviewActor->EnvelopeDef->DefaultRadiusCm, 280.f);
	TestEqual(TEXT("Envelope floor drop should be updated"), PreviewActor->EnvelopeDef->FloorDropBiasCm, 140.f);
	TestEqual(TEXT("Envelope longitudinal subdivisions should be updated"), PreviewActor->EnvelopeDef->ExteriorLongitudinalSubdivisionsPerSpan, 9);
	TestEqual(TEXT("Envelope radial segments should be updated"), PreviewActor->EnvelopeDef->ExteriorRadialSegments, 36);
	TestEqual(TEXT("Envelope bow radius should be updated"), PreviewActor->EnvelopeDef->EvaluateRadius(0.0f), 195.f);
	TestEqual(TEXT("Envelope mid-body radius should be updated"), PreviewActor->EnvelopeDef->EvaluateRadius(0.5f), 330.f);
	TestEqual(TEXT("Envelope stern radius should be updated"), PreviewActor->EnvelopeDef->EvaluateRadius(1.0f), 175.f);

	return true;
}

#endif
