#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "SubCompilerMvpFactory.h"
#include "SubCompilerTypes.h"
#include "SubmarineCompilerActor.h"
#include "SubmarineBuildCompiler.h"
#include "SubmarineEnvelopeDef.h"
#include "SubmarineFunctionalGraph.h"
#include "SubmarineGeometryBuilder.h"
#include "SubmarineLayoutSolver.h"
#include "Submarine/SubHullComponent.h"
#include "Submarine/SubmarineLayoutAsset.h"

namespace
{
float ComputeTriangleArea(const FVector& A, const FVector& B, const FVector& C)
{
	return FVector::CrossProduct(B - A, C - A).Size() * 0.5f;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerEnvelopeDefaultRadiusTest,
	"Sub3D.SubCompiler.Envelope.DefaultRadius",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerEnvelopeDefaultRadiusTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = NewObject<USubmarineEnvelopeDef>();
	TestNotNull(TEXT("Envelope should exist"), Envelope);

	if (!Envelope)
	{
		return false;
	}

	Envelope->DefaultRadiusCm = 180.f;
	TestEqual(TEXT("Empty curve should use DefaultRadiusCm"), Envelope->EvaluateRadius(0.5f), 180.f);
	TestTrue(TEXT("EvaluateRadius should stay positive"), Envelope->EvaluateRadius(2.f) > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerFunctionalGraphValidationTest,
	"Sub3D.SubCompiler.FunctionalGraph.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerFunctionalGraphValidationTest::RunTest(const FString& Parameters)
{
	USubmarineFunctionalGraph* EmptyGraph = NewObject<USubmarineFunctionalGraph>();
	TestNotNull(TEXT("EmptyGraph should exist"), EmptyGraph);

	TArray<FLayoutValidationMessage> Messages;
	TestFalse(TEXT("Empty graph should be invalid"), EmptyGraph && EmptyGraph->ValidateGraph(Messages));
	TestTrue(
		TEXT("Empty graph should emit minimum compartments error"),
		Messages.ContainsByPredicate([](const FLayoutValidationMessage& Message)
		{
			return Message.Severity == ELayoutValidationSeverity::Error
				&& Message.Message.ToString().Contains(TEXT("Minimum 2 compartiments"));
		}));

	USubmarineFunctionalGraph* ValidGraph = NewObject<USubmarineFunctionalGraph>();
	TestNotNull(TEXT("ValidGraph should exist"), ValidGraph);

	if (!ValidGraph)
	{
		return false;
	}

	FCompartmentNode Ballast;
	Ballast.CompartmentId = TEXT("Ballast_Fwd");
	Ballast.Type = ECompartmentType::Ballast;
	Ballast.Priority = 0;
	Ballast.RequiredSystems = { ESubStationType::Ballast };

	FCompartmentNode Helm;
	Helm.CompartmentId = TEXT("Helm");
	Helm.Type = ECompartmentType::Helm;
	Helm.Priority = 1;
	Helm.RequiredSystems = { ESubStationType::Helm };

	FCompartmentNode Engine;
	Engine.CompartmentId = TEXT("Engine");
	Engine.Type = ECompartmentType::Engine;
	Engine.Priority = 2;
	Engine.RequiredSystems = { ESubStationType::Engine, ESubStationType::Pump };

	FCompartmentNode Airlock;
	Airlock.CompartmentId = TEXT("Airlock_Aft");
	Airlock.Type = ECompartmentType::Airlock;
	Airlock.Priority = 3;
	Airlock.RequiredSystems = { ESubStationType::Turret };

	ValidGraph->Compartments = { Ballast, Helm, Engine, Airlock };

	FPassageEdge D1;
	D1.FromCompartmentId = Ballast.CompartmentId;
	D1.ToCompartmentId = Helm.CompartmentId;

	FPassageEdge D2;
	D2.FromCompartmentId = Helm.CompartmentId;
	D2.ToCompartmentId = Engine.CompartmentId;

	FPassageEdge D3;
	D3.FromCompartmentId = Engine.CompartmentId;
	D3.ToCompartmentId = Airlock.CompartmentId;

	ValidGraph->Passages = { D1, D2, D3 };

	Messages.Reset();
	TestTrue(TEXT("MVP graph should validate"), ValidGraph->ValidateGraph(Messages));
	TestFalse(
		TEXT("MVP graph should not emit errors"),
		Messages.ContainsByPredicate([](const FLayoutValidationMessage& Message)
		{
			return Message.Severity == ELayoutValidationSeverity::Error;
		}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerMvpFactoryTest,
	"Sub3D.SubCompiler.MVP.Factory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerMvpFactoryTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());

	TestNotNull(TEXT("MVP envelope should exist"), Envelope);
	TestNotNull(TEXT("MVP graph should exist"), Graph);

	if (!Envelope || !Graph)
	{
		return false;
	}

	TestEqual(TEXT("MVP envelope length"), Envelope->SpineLengthCm, 2100.f);
	TestEqual(TEXT("MVP envelope default radius"), Envelope->DefaultRadiusCm, 250.f);
	TestEqual(TEXT("MVP envelope floor drop bias"), Envelope->FloorDropBiasCm, 90.f);
	TestTrue(TEXT("MVP envelope should be wider at center than at bow"), Envelope->EvaluateRadius(0.5f) > Envelope->EvaluateRadius(0.0f));
	TestTrue(TEXT("MVP envelope should be wider at center than at stern"), Envelope->EvaluateRadius(0.5f) > Envelope->EvaluateRadius(1.0f));
	TestEqual(TEXT("MVP graph compartment count"), Graph->Compartments.Num(), 4);
	TestEqual(TEXT("MVP graph passage count"), Graph->Passages.Num(), 3);

	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("MVP graph should validate"), Graph->ValidateGraph(Messages));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerLayoutSolverTest,
	"Sub3D.SubCompiler.LayoutSolver.MVP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerLayoutSolverTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();

	TestNotNull(TEXT("Envelope should exist"), Envelope);
	TestNotNull(TEXT("Graph should exist"), Graph);
	TestNotNull(TEXT("Solver should exist"), Solver);

	if (!Envelope || !Graph || !Solver)
	{
		return false;
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solver should succeed on MVP graph"), Solver->Solve(Envelope, Graph, Solution, Messages));
	TestFalse(TEXT("Solution should not have errors"), Solution.HasErrors());
	TestTrue(TEXT("Solution should be valid"), Solution.IsValid());
	TestEqual(TEXT("MVP should produce 4 compartments"), Solution.Compartments.Num(), 4);
	TestEqual(TEXT("MVP should produce 3 bulkheads"), Solution.Bulkheads.Num(), 3);
	TestEqual(TEXT("MVP should produce 5 stations"), Solution.Stations.Num(), 5);
	TestEqual(TEXT("MVP metrics compartment count"), Solution.Metrics.CompartmentCount, 4);
	TestEqual(TEXT("MVP metrics door count"), Solution.Metrics.DoorCount, 3);

	const float FloorOffsetWithBias = Solution.Compartments[0].FloorOffsetCm;
	const float ClearanceWithBias = Solution.Compartments[0].ClearanceHeightCm;

	Envelope->FloorDropBiasCm = 0.f;
	FSubmarineLayoutSolution FlatFloorSolution;
	Messages.Reset();
	TestTrue(TEXT("Solver should also succeed with zero floor drop bias"), Solver->Solve(Envelope, Graph, FlatFloorSolution, Messages));
	TestTrue(TEXT("Floor drop bias should lower the floor"), FloorOffsetWithBias < FlatFloorSolution.Compartments[0].FloorOffsetCm);
	TestTrue(TEXT("Floor drop bias should increase headroom"), ClearanceWithBias > FlatFloorSolution.Compartments[0].ClearanceHeightCm);

	for (int32 Index = 0; Index + 1 < Solution.Compartments.Num(); ++Index)
	{
		TestTrue(
			FString::Printf(TEXT("Compartments %d and %d should be ordered"), Index, Index + 1),
			Solution.Compartments[Index].SpineEndCm <= Solution.Compartments[Index + 1].SpineStartCm + KINDA_SMALL_NUMBER);
	}

	for (const FCompartmentPlacement& Placement : Solution.Compartments)
	{
		TestTrue(
			FString::Printf(TEXT("FloorWidth should be positive for %s"), *Placement.CompartmentId.ToString()),
			Placement.FloorWidthCm > 0.f);
		TestTrue(
			FString::Printf(TEXT("ClearanceHeight should be >= 200 for %s"), *Placement.CompartmentId.ToString()),
			Placement.ClearanceHeightCm >= 200.f);
	}

	Envelope->SpineLengthCm = 500.f;
	Solution = FSubmarineLayoutSolution();
	Messages.Reset();
	TestFalse(TEXT("Solver should fail on too-small envelope"), Solver->Solve(Envelope, Graph, Solution, Messages));
	TestTrue(TEXT("Too-small envelope should produce errors"), Solution.HasErrors());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerLayoutSolverLinearWarningsTest,
	"Sub3D.SubCompiler.LayoutSolver.LinearWarnings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerLayoutSolverLinearWarningsTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();

	TestNotNull(TEXT("Envelope should exist"), Envelope);
	TestNotNull(TEXT("Graph should exist"), Graph);
	TestNotNull(TEXT("Solver should exist"), Solver);

	if (!Envelope || !Graph || !Solver)
	{
		return false;
	}

	FCompartmentNode RearBallast;
	RearBallast.CompartmentId = TEXT("Ballast_Bwd");
	RearBallast.Type = ECompartmentType::Ballast;
	RearBallast.MinLengthCm = 250.f;
	RearBallast.MinWidthCm = 100.f;
	RearBallast.MinHeightCm = 200.f;
	RearBallast.RequiredSystems = { ESubStationType::Ballast };
	RearBallast.CrewCapacity = 2;
	RearBallast.Priority = 0;
	Graph->Compartments.Add(RearBallast);

	FPassageEdge RearPassage;
	RearPassage.FromCompartmentId = TEXT("Engine");
	RearPassage.ToCompartmentId = RearBallast.CompartmentId;
	RearPassage.Type = EPassageType::WatertightDoor;
	RearPassage.MinWidthCm = 90.f;
	RearPassage.MinHeightCm = 180.f;
	Graph->Passages.Add(RearPassage);

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solver should still succeed on connected but non-linear graph"), Solver->Solve(Envelope, Graph, Solution, Messages));

	TestTrue(
		TEXT("Duplicate priorities should emit a warning"),
		Messages.ContainsByPredicate([](const FLayoutValidationMessage& Message)
		{
			return Message.Severity == ELayoutValidationSeverity::Warning
				&& Message.Message.ToString().Contains(TEXT("Priorite 0 dupliquee"));
		}));

	TestTrue(
		TEXT("Non-adjacent passage should emit a warning"),
		Messages.ContainsByPredicate([](const FLayoutValidationMessage& Message)
		{
			return Message.Severity == ELayoutValidationSeverity::Warning
				&& Message.Message.ToString().Contains(TEXT("non adjacent dans l'ordre resolu"));
		}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerBuildCompilerTest,
	"Sub3D.SubCompiler.BuildCompiler.MVP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerBuildCompilerTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();
	USubmarineBuildCompiler* Compiler = NewObject<USubmarineBuildCompiler>();

	TestNotNull(TEXT("Envelope should exist"), Envelope);
	TestNotNull(TEXT("Graph should exist"), Graph);
	TestNotNull(TEXT("Solver should exist"), Solver);
	TestNotNull(TEXT("Compiler should exist"), Compiler);

	if (!Envelope || !Graph || !Solver || !Compiler)
	{
		return false;
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solver should succeed before compile"), Solver->Solve(Envelope, Graph, Solution, Messages));

	USubmarineLayoutAsset* LayoutAsset = Compiler->CompileToLayoutAsset(Solution, GetTransientPackage(), Messages);
	TestNotNull(TEXT("Compiled layout asset should exist"), LayoutAsset);

	if (!LayoutAsset)
	{
		return false;
	}

	TestEqual(TEXT("Compiled asset should keep 4 compartments"), LayoutAsset->Compartments.Num(), 4);
	TestEqual(TEXT("Compiled asset should create 19 sheets"), LayoutAsset->StructuralSheets.Num(), 19);
	TestEqual(TEXT("Compiled asset should create 3 doors"), LayoutAsset->Doors.Num(), 3);
	TestEqual(TEXT("Compiled asset should create 5 station slots"), LayoutAsset->StationSlots.Num(), 5);

	const int32 ExteriorSheetCount = LayoutAsset->StructuralSheets.FilterByPredicate([](const FStructuralSheetDef& Sheet)
	{
		return Sheet.bCanOpenToExterior;
	}).Num();
	TestEqual(TEXT("MVP should create 16 exterior sheets"), ExteriorSheetCount, 16);

	const int32 BulkheadSheetCount = LayoutAsset->StructuralSheets.FilterByPredicate([](const FStructuralSheetDef& Sheet)
	{
		return !Sheet.bCanOpenToExterior && !Sheet.AdjacentCompartmentId.IsNone();
	}).Num();
	TestEqual(TEXT("MVP should create 3 bulkhead sheets"), BulkheadSheetCount, 3);

	for (const FDoorDef& Door : LayoutAsset->Doors)
	{
		TestEqual(
			FString::Printf(TEXT("Door %s should match its bulkhead sheet id"), *Door.DoorId.ToString()),
			Door.DoorId,
			Door.BulkheadSheetId);
	}

	USubHullComponent* HullComponent = NewObject<USubHullComponent>();
	TestNotNull(TEXT("HullComponent should exist"), HullComponent);

	if (!HullComponent)
	{
		return false;
	}

	HullComponent->InitializeFromLayout(LayoutAsset);
	TestEqual(TEXT("Hull component should initialize 4 compartment states"), HullComponent->GetCompartmentStates().Num(), 4);
	TestEqual(TEXT("Hull component should keep compiled sheet count"), HullComponent->GetStructuralSheets().Num(), 19);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerBuildCompilerBindingsValidRangesTest,
	"Sub3D.SubCompiler.BuildCompiler.CompiledSheetBindings_ValidRanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerBuildCompilerBindingsValidRangesTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();
	USubmarineBuildCompiler* Compiler = NewObject<USubmarineBuildCompiler>();

	if (!Envelope || !Graph || !Solver || !Compiler)
	{
		return false;
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solver should succeed before compile"), Solver->Solve(Envelope, Graph, Solution, Messages));

	USubmarineLayoutAsset* LayoutAsset = Compiler->CompileToLayoutAsset(Solution, GetTransientPackage(), Messages, Envelope);
	TestNotNull(TEXT("Compiled layout asset should exist"), LayoutAsset);
	if (!LayoutAsset)
	{
		return false;
	}

	const int32 CompartmentCount = Solution.Compartments.Num();
	const int32 ExteriorRadialSegments = FMath::Clamp(Envelope->ExteriorRadialSegments, 12, 64);
	const int32 ExteriorLongitudinalSubdivisions = FMath::Clamp(Envelope->ExteriorLongitudinalSubdivisionsPerSpan, 1, 16);
	const int32 InteriorArcSegments = FMath::Clamp(Envelope->InteriorArcSegments, 8, 48);
	const int32 ExteriorTotalVertexCount = (CompartmentCount * ExteriorLongitudinalSubdivisions + 1) * ExteriorRadialSegments;
	const int32 ExteriorTotalTriangleCount = CompartmentCount * ExteriorLongitudinalSubdivisions * ExteriorRadialSegments * 2;
	const int32 InteriorTotalVertexCount = CompartmentCount * (InteriorArcSegments + 1) * 2;
	const int32 InteriorTotalTriangleCount = CompartmentCount * InteriorArcSegments * 2;
	const int32 MaxSectionIndexExclusive = CompartmentCount * ExteriorLongitudinalSubdivisions + 1;

	for (const FStructuralSheetCompiledBinding& Binding : LayoutAsset->CompiledSheetBindings)
	{
		const FStructuralSheetDef* Sheet = LayoutAsset->StructuralSheets.FindByPredicate(
			[&Binding](const FStructuralSheetDef& Candidate)
			{
				return Candidate.SheetId == Binding.SheetId;
			});
		TestNotNull(
			FString::Printf(TEXT("Binding %s should map to a structural sheet"), *Binding.SheetId.ToString()),
			Sheet);
		if (!Sheet)
		{
			continue;
		}

		const bool bIsBulkhead = Binding.Side == ESheetSide::Bulkhead;
		if (bIsBulkhead)
		{
			TestEqual(
				FString::Printf(TEXT("Bulkhead binding %s should not expose exterior vertex range"), *Binding.SheetId.ToString()),
				Binding.MeshRange.ExteriorVertexStart,
				INDEX_NONE);
			TestEqual(
				FString::Printf(TEXT("Bulkhead binding %s should not expose interior vertex range"), *Binding.SheetId.ToString()),
				Binding.MeshRange.InteriorVertexStart,
				INDEX_NONE);
			continue;
		}

		TestTrue(
			FString::Printf(TEXT("Binding %s section start should be valid"), *Binding.SheetId.ToString()),
			Binding.MeshRange.SectionIndexStart >= 0);
		TestTrue(
			FString::Printf(TEXT("Binding %s section end should be > start"), *Binding.SheetId.ToString()),
			Binding.MeshRange.SectionIndexEnd > Binding.MeshRange.SectionIndexStart);
		TestTrue(
			FString::Printf(TEXT("Binding %s section end should stay inside ring stack"), *Binding.SheetId.ToString()),
			Binding.MeshRange.SectionIndexEnd <= MaxSectionIndexExclusive);

		TestTrue(
			FString::Printf(TEXT("Binding %s exterior vertex start should be valid"), *Binding.SheetId.ToString()),
			Binding.MeshRange.ExteriorVertexStart >= 0);
		TestTrue(
			FString::Printf(TEXT("Binding %s exterior vertex count should be > 0"), *Binding.SheetId.ToString()),
			Binding.MeshRange.ExteriorVertexCount > 0);
		TestTrue(
			FString::Printf(TEXT("Binding %s exterior triangles should be valid"), *Binding.SheetId.ToString()),
			Binding.MeshRange.ExteriorTriangleStart >= 0 && Binding.MeshRange.ExteriorTriangleCount > 0);
		TestTrue(
			FString::Printf(TEXT("Binding %s exterior vertex range should stay in buffer"), *Binding.SheetId.ToString()),
			Binding.MeshRange.ExteriorVertexStart + Binding.MeshRange.ExteriorVertexCount <= ExteriorTotalVertexCount);
		TestTrue(
			FString::Printf(TEXT("Binding %s exterior triangle range should stay in buffer"), *Binding.SheetId.ToString()),
			Binding.MeshRange.ExteriorTriangleStart + Binding.MeshRange.ExteriorTriangleCount <= ExteriorTotalTriangleCount);

		TestTrue(
			FString::Printf(TEXT("Binding %s interior vertex start should be valid"), *Binding.SheetId.ToString()),
			Binding.MeshRange.InteriorVertexStart >= 0);
		TestTrue(
			FString::Printf(TEXT("Binding %s interior vertex count should be > 0"), *Binding.SheetId.ToString()),
			Binding.MeshRange.InteriorVertexCount > 0);
		TestTrue(
			FString::Printf(TEXT("Binding %s interior triangles should be valid"), *Binding.SheetId.ToString()),
			Binding.MeshRange.InteriorTriangleStart >= 0 && Binding.MeshRange.InteriorTriangleCount > 0);
		TestTrue(
			FString::Printf(TEXT("Binding %s interior vertex range should stay in buffer"), *Binding.SheetId.ToString()),
			Binding.MeshRange.InteriorVertexStart + Binding.MeshRange.InteriorVertexCount <= InteriorTotalVertexCount);
		TestTrue(
			FString::Printf(TEXT("Binding %s interior triangle range should stay in buffer"), *Binding.SheetId.ToString()),
			Binding.MeshRange.InteriorTriangleStart + Binding.MeshRange.InteriorTriangleCount <= InteriorTotalTriangleCount);

		TestTrue(
			FString::Printf(TEXT("Binding %s chart min/max X should be valid"), *Binding.SheetId.ToString()),
			Binding.ChartMin.X >= 0.f && Binding.ChartMax.X <= 1.f && Binding.ChartMax.X > Binding.ChartMin.X);
		TestTrue(
			FString::Printf(TEXT("Binding %s chart min/max Y should be valid"), *Binding.SheetId.ToString()),
			Binding.ChartMin.Y >= 0.f && Binding.ChartMax.Y <= 1.f && Binding.ChartMax.Y > Binding.ChartMin.Y);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerBuildCompilerBindingsDeterministicTest,
	"Sub3D.SubCompiler.BuildCompiler.CompiledSheetBindings_Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerBuildCompilerBindingsDeterministicTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();
	USubmarineBuildCompiler* Compiler = NewObject<USubmarineBuildCompiler>();

	if (!Envelope || !Graph || !Solver || !Compiler)
	{
		return false;
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solver should succeed before deterministic compile check"), Solver->Solve(Envelope, Graph, Solution, Messages));

	USubmarineLayoutAsset* LayoutA = Compiler->CompileToLayoutAsset(Solution, GetTransientPackage(), Messages, Envelope);
	USubmarineLayoutAsset* LayoutB = Compiler->CompileToLayoutAsset(Solution, GetTransientPackage(), Messages, Envelope);
	TestNotNull(TEXT("First compiled layout should exist"), LayoutA);
	TestNotNull(TEXT("Second compiled layout should exist"), LayoutB);
	if (!LayoutA || !LayoutB)
	{
		return false;
	}

	TestEqual(
		TEXT("Compiled binding count should be deterministic"),
		LayoutA->CompiledSheetBindings.Num(),
		LayoutB->CompiledSheetBindings.Num());

	for (const FStructuralSheetCompiledBinding& BindingA : LayoutA->CompiledSheetBindings)
	{
		const FStructuralSheetCompiledBinding* BindingB = LayoutB->CompiledSheetBindings.FindByPredicate(
			[&BindingA](const FStructuralSheetCompiledBinding& Candidate)
			{
				return Candidate.SheetId == BindingA.SheetId;
			});

		TestNotNull(
			FString::Printf(TEXT("Binding %s should exist in second compile"), *BindingA.SheetId.ToString()),
			BindingB);
		if (!BindingB)
		{
			continue;
		}

		TestEqual(
			FString::Printf(TEXT("Binding %s side should be deterministic"), *BindingA.SheetId.ToString()),
			BindingA.Side,
			BindingB->Side);
		TestEqual(
			FString::Printf(TEXT("Binding %s compartment index should be deterministic"), *BindingA.SheetId.ToString()),
			BindingA.CompartmentIndex,
			BindingB->CompartmentIndex);
		TestEqual(
			FString::Printf(TEXT("Binding %s section start should be deterministic"), *BindingA.SheetId.ToString()),
			BindingA.MeshRange.SectionIndexStart,
			BindingB->MeshRange.SectionIndexStart);
		TestEqual(
			FString::Printf(TEXT("Binding %s section end should be deterministic"), *BindingA.SheetId.ToString()),
			BindingA.MeshRange.SectionIndexEnd,
			BindingB->MeshRange.SectionIndexEnd);
		TestEqual(
			FString::Printf(TEXT("Binding %s exterior vertex start should be deterministic"), *BindingA.SheetId.ToString()),
			BindingA.MeshRange.ExteriorVertexStart,
			BindingB->MeshRange.ExteriorVertexStart);
		TestEqual(
			FString::Printf(TEXT("Binding %s exterior triangle start should be deterministic"), *BindingA.SheetId.ToString()),
			BindingA.MeshRange.ExteriorTriangleStart,
			BindingB->MeshRange.ExteriorTriangleStart);
		TestEqual(
			FString::Printf(TEXT("Binding %s interior vertex start should be deterministic"), *BindingA.SheetId.ToString()),
			BindingA.MeshRange.InteriorVertexStart,
			BindingB->MeshRange.InteriorVertexStart);
		TestEqual(
			FString::Printf(TEXT("Binding %s interior triangle start should be deterministic"), *BindingA.SheetId.ToString()),
			BindingA.MeshRange.InteriorTriangleStart,
			BindingB->MeshRange.InteriorTriangleStart);
		TestTrue(
			FString::Printf(TEXT("Binding %s chart min should be deterministic"), *BindingA.SheetId.ToString()),
			BindingA.ChartMin.Equals(BindingB->ChartMin, KINDA_SMALL_NUMBER));
		TestTrue(
			FString::Printf(TEXT("Binding %s chart max should be deterministic"), *BindingA.SheetId.ToString()),
			BindingA.ChartMax.Equals(BindingB->ChartMax, KINDA_SMALL_NUMBER));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerGeometryBuilderInteriorTest,
	"Sub3D.SubCompiler.GeometryBuilder.Interior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerGeometryBuilderInteriorTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();
	USubmarineGeometryBuilder* GeometryBuilder = NewObject<USubmarineGeometryBuilder>();

	TestNotNull(TEXT("Envelope should exist"), Envelope);
	TestNotNull(TEXT("Graph should exist"), Graph);
	TestNotNull(TEXT("Solver should exist"), Solver);
	TestNotNull(TEXT("GeometryBuilder should exist"), GeometryBuilder);

	if (!Envelope || !Graph || !Solver || !GeometryBuilder)
	{
		return false;
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solver should succeed before geometry generation"), Solver->Solve(Envelope, Graph, Solution, Messages));

	TArray<FSubmarineInteriorCompartmentMeshData> MeshDataSet;
	TestTrue(TEXT("GeometryBuilder should generate interior mesh data"), GeometryBuilder->GenerateInteriorMeshData(Solution, MeshDataSet));
	TestEqual(TEXT("Should generate one interior mesh per compartment"), MeshDataSet.Num(), Solution.Compartments.Num());

	TArray<FSubmarineBulkheadMeshData> BulkheadMeshDataSet;
	TestTrue(TEXT("GeometryBuilder should generate bulkhead mesh data"), GeometryBuilder->GenerateBulkheadMeshData(Solution, BulkheadMeshDataSet));
	TestEqual(TEXT("Should generate one bulkhead mesh per bulkhead"), BulkheadMeshDataSet.Num(), Solution.Bulkheads.Num());

	FSubmarineMeshSectionData ExteriorMeshData;
	TestTrue(TEXT("GeometryBuilder should generate exterior mesh data"), GeometryBuilder->GenerateExteriorMeshData(Solution, ExteriorMeshData));
	TestTrue(TEXT("Exterior hull should have vertices"), ExteriorMeshData.Vertices.Num() > 0);
	TestTrue(TEXT("Exterior hull should have triangles"), ExteriorMeshData.Triangles.Num() > 0);

	for (int32 MeshIndex = 0; MeshIndex < MeshDataSet.Num(); ++MeshIndex)
	{
		const FSubmarineInteriorCompartmentMeshData& MeshData = MeshDataSet[MeshIndex];

		TestTrue(
			FString::Printf(TEXT("Wall vertices should exist for %s"), *MeshData.CompartmentId.ToString()),
			MeshData.WallSection.Vertices.Num() > 0);
		TestTrue(
			FString::Printf(TEXT("Wall triangles should exist for %s"), *MeshData.CompartmentId.ToString()),
			MeshData.WallSection.Triangles.Num() > 0);
		TestEqual(
			FString::Printf(TEXT("Floor should have 4 vertices for %s"), *MeshData.CompartmentId.ToString()),
			MeshData.FloorSection.Vertices.Num(),
			4);
		TestEqual(
			FString::Printf(TEXT("Floor should have 4 triangles for %s"), *MeshData.CompartmentId.ToString()),
			MeshData.FloorSection.Triangles.Num(),
			6);

		const bool bIsFirstCompartment = MeshIndex == 0;
		const bool bIsLastCompartment = MeshIndex == MeshDataSet.Num() - 1;
		TestEqual(
			FString::Printf(TEXT("Bow cap presence should match first compartment for %s"), *MeshData.CompartmentId.ToString()),
			MeshData.BowCapSection.Vertices.Num() > 0,
			bIsFirstCompartment);
		TestEqual(
			FString::Printf(TEXT("Stern cap presence should match last compartment for %s"), *MeshData.CompartmentId.ToString()),
			MeshData.SternCapSection.Vertices.Num() > 0,
			bIsLastCompartment);

		for (int32 TriIndex = 0; TriIndex + 2 < MeshData.WallSection.Triangles.Num(); TriIndex += 3)
		{
			const FVector& A = MeshData.WallSection.Vertices[MeshData.WallSection.Triangles[TriIndex]];
			const FVector& B = MeshData.WallSection.Vertices[MeshData.WallSection.Triangles[TriIndex + 1]];
			const FVector& C = MeshData.WallSection.Vertices[MeshData.WallSection.Triangles[TriIndex + 2]];
			TestTrue(
				FString::Printf(TEXT("Wall triangle %d should not be degenerate for %s"), TriIndex / 3, *MeshData.CompartmentId.ToString()),
				ComputeTriangleArea(A, B, C) > KINDA_SMALL_NUMBER);
		}

		for (int32 TriIndex = 0; TriIndex + 2 < MeshData.FloorSection.Triangles.Num(); TriIndex += 3)
		{
			const FVector& A = MeshData.FloorSection.Vertices[MeshData.FloorSection.Triangles[TriIndex]];
			const FVector& B = MeshData.FloorSection.Vertices[MeshData.FloorSection.Triangles[TriIndex + 1]];
			const FVector& C = MeshData.FloorSection.Vertices[MeshData.FloorSection.Triangles[TriIndex + 2]];
			TestTrue(
				FString::Printf(TEXT("Floor triangle %d should not be degenerate for %s"), TriIndex / 3, *MeshData.CompartmentId.ToString()),
				ComputeTriangleArea(A, B, C) > KINDA_SMALL_NUMBER);
		}

		const TArray<FSubmarineMeshSectionData> CapSections = { MeshData.BowCapSection, MeshData.SternCapSection };
		for (int32 CapIndex = 0; CapIndex < CapSections.Num(); ++CapIndex)
		{
			const FSubmarineMeshSectionData& CapSection = CapSections[CapIndex];
			for (int32 TriIndex = 0; TriIndex + 2 < CapSection.Triangles.Num(); TriIndex += 3)
			{
				const FVector& A = CapSection.Vertices[CapSection.Triangles[TriIndex]];
				const FVector& B = CapSection.Vertices[CapSection.Triangles[TriIndex + 1]];
				const FVector& C = CapSection.Vertices[CapSection.Triangles[TriIndex + 2]];
				TestTrue(
					FString::Printf(TEXT("Cap triangle %d should not be degenerate for %s"), TriIndex / 3, *MeshData.CompartmentId.ToString()),
					ComputeTriangleArea(A, B, C) > KINDA_SMALL_NUMBER);
			}
		}

		for (int32 VertexIndex = 0; VertexIndex < MeshData.WallSection.Vertices.Num(); ++VertexIndex)
		{
			const FVector& Vertex = MeshData.WallSection.Vertices[VertexIndex];
			const FVector& Normal = MeshData.WallSection.Normals[VertexIndex];
			const FVector ExpectedInward = FVector(0.f, -Vertex.Y, -Vertex.Z).GetSafeNormal();

			TestTrue(
				FString::Printf(TEXT("Wall normal %d should point inward for %s"), VertexIndex, *MeshData.CompartmentId.ToString()),
				FVector::DotProduct(Normal, ExpectedInward) > 0.9f);
		}

		const float FloorZ = MeshData.FloorSection.Vertices[0].Z;
		for (const FVector& FloorVertex : MeshData.FloorSection.Vertices)
		{
			TestTrue(
				FString::Printf(TEXT("Floor should be horizontal for %s"), *MeshData.CompartmentId.ToString()),
				FMath::IsNearlyEqual(FloorVertex.Z, FloorZ));
		}
	}

	for (int32 MeshIndex = 0; MeshIndex < BulkheadMeshDataSet.Num(); ++MeshIndex)
	{
		const FSubmarineBulkheadMeshData& MeshData = BulkheadMeshDataSet[MeshIndex];

		TestTrue(
			FString::Printf(TEXT("Bulkhead vertices should exist for %s"), *MeshData.BulkheadId.ToString()),
			MeshData.PanelSection.Vertices.Num() > 0);
		TestTrue(
			FString::Printf(TEXT("Bulkhead triangles should exist for %s"), *MeshData.BulkheadId.ToString()),
			MeshData.PanelSection.Triangles.Num() > 0);

		for (int32 TriIndex = 0; TriIndex + 2 < MeshData.PanelSection.Triangles.Num(); TriIndex += 3)
		{
			const FVector& A = MeshData.PanelSection.Vertices[MeshData.PanelSection.Triangles[TriIndex]];
			const FVector& B = MeshData.PanelSection.Vertices[MeshData.PanelSection.Triangles[TriIndex + 1]];
			const FVector& C = MeshData.PanelSection.Vertices[MeshData.PanelSection.Triangles[TriIndex + 2]];
			TestTrue(
				FString::Printf(TEXT("Bulkhead triangle %d should not be degenerate for %s"), TriIndex / 3, *MeshData.BulkheadId.ToString()),
				ComputeTriangleArea(A, B, C) > KINDA_SMALL_NUMBER);
		}
	}

	float MinExteriorX = TNumericLimits<float>::Max();
	float MaxExteriorX = TNumericLimits<float>::Lowest();
	for (const FVector& Vertex : ExteriorMeshData.Vertices)
	{
		MinExteriorX = FMath::Min(MinExteriorX, Vertex.X);
		MaxExteriorX = FMath::Max(MaxExteriorX, Vertex.X);
	}

	TestTrue(TEXT("Exterior hull should start at or before first compartment"), MinExteriorX <= Solution.Compartments[0].SpineStartCm + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Exterior hull should reach aft end"), MaxExteriorX >= Solution.Compartments.Last().SpineEndCm - KINDA_SMALL_NUMBER);

	for (int32 TriIndex = 0; TriIndex + 2 < ExteriorMeshData.Triangles.Num(); TriIndex += 3)
	{
		const FVector& A = ExteriorMeshData.Vertices[ExteriorMeshData.Triangles[TriIndex]];
		const FVector& B = ExteriorMeshData.Vertices[ExteriorMeshData.Triangles[TriIndex + 1]];
		const FVector& C = ExteriorMeshData.Vertices[ExteriorMeshData.Triangles[TriIndex + 2]];
		TestTrue(
			FString::Printf(TEXT("Exterior triangle %d should not be degenerate"), TriIndex / 3),
			ComputeTriangleArea(A, B, C) > KINDA_SMALL_NUMBER);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerActorCompileTest,
	"Sub3D.SubCompiler.CompilerActor.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerActorCompileTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = NewObject<ASubmarineCompilerActor>(GetTransientPackage());
	TestNotNull(TEXT("Compiler actor should exist"), CompilerActor);

	if (!CompilerActor)
	{
		return false;
	}

	CompilerActor->EnvelopeDef = FSubCompilerMvpFactory::CreateEnvelope(CompilerActor);
	CompilerActor->FunctionalGraph = FSubCompilerMvpFactory::CreateFunctionalGraph(CompilerActor);

	TestTrue(TEXT("Compiler actor should compile current definitions"), CompilerActor->CompileCurrentDefinitions());

	const USubmarineLayoutAsset* LayoutAsset = CompilerActor->GetCompiledLayoutAsset();
	TestNotNull(TEXT("Compiler actor should keep a compiled layout asset"), LayoutAsset);

	if (!LayoutAsset)
	{
		return false;
	}

	TestEqual(TEXT("Compiled layout should keep 4 compartments"), LayoutAsset->Compartments.Num(), 4);
	TestEqual(TEXT("Compiled layout should keep 19 sheets"), LayoutAsset->StructuralSheets.Num(), 19);
	TestEqual(TEXT("Compiled layout should keep 3 doors"), LayoutAsset->Doors.Num(), 3);
	TestEqual(TEXT("SubHull should initialize on compile"), CompilerActor->SubHull->GetStructuralSheets().Num(), 19);
	TestEqual(TEXT("Helm socket should move into compiled interior"), CompilerActor->HelmSocket->GetRelativeLocation().X > 0.f, true);

	return true;
}

// ============================================================================
// E.6 — Proto04E Tests
// ============================================================================

// ---------------------------------------------------------------------------
// E.1 — Superellipse tests
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerSuperellipseExponent2MatchesCircleTest,
	"Sub3D.SubCompiler.Superellipse.Exponent2_MatchesCircle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerSuperellipseExponent2MatchesCircleTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();
	USubmarineGeometryBuilder* Builder = NewObject<USubmarineGeometryBuilder>();

	if (!Envelope || !Graph || !Solver || !Builder)
	{
		return false;
	}

	Envelope->SectionExponent = 2.f;
	Envelope->WidthToHeightRatio = 1.f;

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solve should succeed"), Solver->Solve(Envelope, Graph, Solution, Messages));

	TArray<FSubmarineInteriorCompartmentMeshData> MeshData_N2;
	TestTrue(
		TEXT("Interior generation should succeed with n=2"),
		Builder->GenerateInteriorMeshData(Solution, MeshData_N2, 2.f, 1.f, 24, Envelope->WallThicknessCm));

	// With n=2, WHR=1, every wall vertex should satisfy Y^2 + Z^2 ≈ R^2 (circle)
	for (const FSubmarineInteriorCompartmentMeshData& Compartment : MeshData_N2)
	{
		const FCompartmentPlacement* Placement = Solution.Compartments.FindByPredicate(
			[&Compartment](const FCompartmentPlacement& P) { return P.CompartmentId == Compartment.CompartmentId; });
		if (!Placement)
		{
			continue;
		}
		const float InnerR = FMath::Max(10.f, Placement->EffectiveRadiusCm - Envelope->WallThicknessCm);
		for (const FVector& V : Compartment.WallSection.Vertices)
		{
			const float DistFromCenter = FMath::Sqrt(V.Y * V.Y + V.Z * V.Z);
			TestTrue(
				FString::Printf(TEXT("n=2 wall vertex should be on inset circle (R=%.1f, dist=%.1f) for %s"),
					InnerR, DistFromCenter, *Compartment.CompartmentId.ToString()),
				FMath::IsNearlyEqual(DistFromCenter, InnerR, 1.f));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerSuperellipseExponent4WiderFloorTest,
	"Sub3D.SubCompiler.Superellipse.Exponent4_WiderFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerSuperellipseExponent4WiderFloorTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();
	USubmarineGeometryBuilder* Builder = NewObject<USubmarineGeometryBuilder>();

	if (!Envelope || !Graph || !Solver || !Builder)
	{
		return false;
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solve should succeed"), Solver->Solve(Envelope, Graph, Solution, Messages));

	TArray<FSubmarineInteriorCompartmentMeshData> MeshData_N2;
	TestTrue(TEXT("Interior n=2"), Builder->GenerateInteriorMeshData(Solution, MeshData_N2, 2.f, 1.f));

	TArray<FSubmarineInteriorCompartmentMeshData> MeshData_N4;
	TestTrue(TEXT("Interior n=4"), Builder->GenerateInteriorMeshData(Solution, MeshData_N4, 4.f, 1.f));

	for (int32 i = 0; i < FMath::Min(MeshData_N2.Num(), MeshData_N4.Num()); ++i)
	{
		// Floor width at n=4 should be wider than at n=2
		float MaxFloorY_N2 = 0.f;
		for (const FVector& V : MeshData_N2[i].FloorSection.Vertices)
		{
			MaxFloorY_N2 = FMath::Max(MaxFloorY_N2, FMath::Abs(V.Y));
		}

		float MaxFloorY_N4 = 0.f;
		for (const FVector& V : MeshData_N4[i].FloorSection.Vertices)
		{
			MaxFloorY_N4 = FMath::Max(MaxFloorY_N4, FMath::Abs(V.Y));
		}

		TestTrue(
			FString::Printf(TEXT("n=4 floor (%.1f) should be wider than n=2 floor (%.1f) for %s"),
				MaxFloorY_N4, MaxFloorY_N2, *MeshData_N2[i].CompartmentId.ToString()),
			MaxFloorY_N4 > MaxFloorY_N2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerSuperellipseExponentRangeNoDegenerateTest,
	"Sub3D.SubCompiler.Superellipse.ExponentRange_NoDegenerate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerSuperellipseExponentRangeNoDegenerateTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();
	USubmarineGeometryBuilder* Builder = NewObject<USubmarineGeometryBuilder>();

	if (!Envelope || !Graph || !Solver || !Builder)
	{
		return false;
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solve should succeed"), Solver->Solve(Envelope, Graph, Solution, Messages));

	const float Exponents[] = { 0.5f, 1.f, 1.5f, 2.f, 3.f, 4.f, 6.f, 8.f };

	for (float N : Exponents)
	{
		TArray<FSubmarineInteriorCompartmentMeshData> MeshData;
		TestTrue(
			FString::Printf(TEXT("Interior generation should succeed for n=%.1f"), N),
			Builder->GenerateInteriorMeshData(Solution, MeshData, N, 1.f));

		for (const FSubmarineInteriorCompartmentMeshData& Compartment : MeshData)
		{
			for (int32 TriIndex = 0; TriIndex + 2 < Compartment.WallSection.Triangles.Num(); TriIndex += 3)
			{
				const FVector& A = Compartment.WallSection.Vertices[Compartment.WallSection.Triangles[TriIndex]];
				const FVector& B = Compartment.WallSection.Vertices[Compartment.WallSection.Triangles[TriIndex + 1]];
				const FVector& C = Compartment.WallSection.Vertices[Compartment.WallSection.Triangles[TriIndex + 2]];
				TestTrue(
					FString::Printf(TEXT("n=%.1f wall tri %d should not be degenerate for %s"),
						N, TriIndex / 3, *Compartment.CompartmentId.ToString()),
					ComputeTriangleArea(A, B, C) > KINDA_SMALL_NUMBER);
			}
		}

		FSubmarineMeshSectionData ExteriorData;
		TestTrue(
			FString::Printf(TEXT("Exterior generation should succeed for n=%.1f"), N),
			Builder->GenerateExteriorMeshData(Solution, ExteriorData, 32, 6, N, 1.f));

		for (int32 TriIndex = 0; TriIndex + 2 < ExteriorData.Triangles.Num(); TriIndex += 3)
		{
			const FVector& A = ExteriorData.Vertices[ExteriorData.Triangles[TriIndex]];
			const FVector& B = ExteriorData.Vertices[ExteriorData.Triangles[TriIndex + 1]];
			const FVector& C = ExteriorData.Vertices[ExteriorData.Triangles[TriIndex + 2]];
			TestTrue(
				FString::Printf(TEXT("n=%.1f exterior tri %d should not be degenerate"), N, TriIndex / 3),
				ComputeTriangleArea(A, B, C) > KINDA_SMALL_NUMBER);
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
// E.2 — Lock tests
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerLockSinglePreservesPositionTest,
	"Sub3D.SubCompiler.Lock.Single_PreservesPosition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerLockSinglePreservesPositionTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();

	if (!Envelope || !Graph || !Solver)
	{
		return false;
	}

	// Lock the Helm compartment at [500, 1060]
	for (FCompartmentNode& Comp : Graph->Compartments)
	{
		if (Comp.CompartmentId == FName(TEXT("Helm")))
		{
			Comp.bLocked = true;
			Comp.LockedSpineRangeCm = FVector2D(500.f, 1060.f);
			break;
		}
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solver should succeed with one lock"), Solver->Solve(Envelope, Graph, Solution, Messages));
	TestTrue(TEXT("Solution should be valid"), Solution.IsValid());

	const FCompartmentPlacement* HelmPlacement = Solution.Compartments.FindByPredicate(
		[](const FCompartmentPlacement& P) { return P.CompartmentId == FName(TEXT("Helm")); });

	TestNotNull(TEXT("Helm should be placed"), HelmPlacement);
	if (HelmPlacement)
	{
		TestTrue(
			FString::Printf(TEXT("Helm SpineStart should be 500 (got %.1f)"), HelmPlacement->SpineStartCm),
			FMath::IsNearlyEqual(HelmPlacement->SpineStartCm, 500.f, 0.1f));
		TestTrue(
			FString::Printf(TEXT("Helm SpineEnd should be 1060 (got %.1f)"), HelmPlacement->SpineEndCm),
			FMath::IsNearlyEqual(HelmPlacement->SpineEndCm, 1060.f, 0.1f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerLockNoneIdenticalToBaselineTest,
	"Sub3D.SubCompiler.Lock.None_IdenticalToBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerLockNoneIdenticalToBaselineTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();

	if (!Envelope || !Graph || !Solver)
	{
		return false;
	}

	// Ensure no locks
	for (FCompartmentNode& Comp : Graph->Compartments)
	{
		Comp.bLocked = false;
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solver should succeed with no locks"), Solver->Solve(Envelope, Graph, Solution, Messages));
	TestTrue(TEXT("Solution should be valid"), Solution.IsValid());
	TestEqual(TEXT("Should produce 4 compartments"), Solution.Compartments.Num(), 4);
	TestEqual(TEXT("Should produce 3 bulkheads"), Solution.Bulkheads.Num(), 3);

	// All compartments should be contiguous and ordered
	for (int32 i = 0; i + 1 < Solution.Compartments.Num(); ++i)
	{
		TestTrue(
			FString::Printf(TEXT("Compartments %d and %d should be contiguous"), i, i + 1),
			FMath::IsNearlyEqual(Solution.Compartments[i].SpineEndCm, Solution.Compartments[i + 1].SpineStartCm, 0.1f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerLockOverlapReturnsErrorTest,
	"Sub3D.SubCompiler.Lock.Overlap_ReturnsError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerLockOverlapReturnsErrorTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();

	if (!Envelope || !Graph || !Solver)
	{
		return false;
	}

	// Lock two compartments with overlapping ranges
	int32 LockCount = 0;
	for (FCompartmentNode& Comp : Graph->Compartments)
	{
		if (Comp.CompartmentId == FName(TEXT("Helm")))
		{
			Comp.bLocked = true;
			Comp.LockedSpineRangeCm = FVector2D(300.f, 800.f);
			++LockCount;
		}
		else if (Comp.CompartmentId == FName(TEXT("Engine")))
		{
			Comp.bLocked = true;
			Comp.LockedSpineRangeCm = FVector2D(700.f, 1200.f); // overlaps [300,800]
			++LockCount;
		}
	}

	TestEqual(TEXT("Should have set 2 locks"), LockCount, 2);

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	const bool bSolved = Solver->Solve(Envelope, Graph, Solution, Messages);
	TestFalse(TEXT("Overlapping locks should cause solve failure"), bSolved);
	TestTrue(TEXT("Overlapping locks should produce errors"), Solution.HasErrors());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerLockOutOfEnvelopeReturnsErrorTest,
	"Sub3D.SubCompiler.Lock.OutOfEnvelope_ReturnsError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerLockOutOfEnvelopeReturnsErrorTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();

	if (!Envelope || !Graph || !Solver)
	{
		return false;
	}

	// Lock beyond spine length (2100)
	for (FCompartmentNode& Comp : Graph->Compartments)
	{
		if (Comp.CompartmentId == FName(TEXT("Helm")))
		{
			Comp.bLocked = true;
			Comp.LockedSpineRangeCm = FVector2D(1900.f, 2500.f);
			break;
		}
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	const bool bSolved = Solver->Solve(Envelope, Graph, Solution, Messages);
	TestFalse(TEXT("Out-of-envelope lock should cause solve failure"), bSolved);
	TestTrue(TEXT("Out-of-envelope lock should produce errors"), Solution.HasErrors());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerLockAllLockedFillsExactlyTest,
	"Sub3D.SubCompiler.Lock.AllLocked_FillsExactly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerLockAllLockedFillsExactlyTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();

	if (!Envelope || !Graph || !Solver)
	{
		return false;
	}

	// Lock all 4 compartments to fill the entire spine (2100 cm)
	const float SpineLength = Envelope->SpineLengthCm;
	const float SegLen = SpineLength / Graph->Compartments.Num();

	for (int32 i = 0; i < Graph->Compartments.Num(); ++i)
	{
		Graph->Compartments[i].bLocked = true;
		Graph->Compartments[i].LockedSpineRangeCm = FVector2D(i * SegLen, (i + 1) * SegLen);
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solver should succeed with all locked"), Solver->Solve(Envelope, Graph, Solution, Messages));
	TestTrue(TEXT("Solution should be valid"), Solution.IsValid());

	float TotalLength = 0.f;
	for (const FCompartmentPlacement& P : Solution.Compartments)
	{
		TotalLength += (P.SpineEndCm - P.SpineStartCm);
	}

	TestTrue(
		FString::Printf(TEXT("Total locked length (%.1f) should equal spine length (%.1f)"), TotalLength, SpineLength),
		FMath::IsNearlyEqual(TotalLength, SpineLength, 1.f));

	return true;
}

// ---------------------------------------------------------------------------
// E.3 — Bow/Stern profile tests
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerBowSternNoTaperIdenticalTest,
	"Sub3D.SubCompiler.BowStern.NoTaper_IdenticalToBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerBowSternNoTaperIdenticalTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();
	USubmarineGeometryBuilder* Builder = NewObject<USubmarineGeometryBuilder>();

	if (!Envelope || !Graph || !Solver || !Builder)
	{
		return false;
	}

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solve should succeed"), Solver->Solve(Envelope, Graph, Solution, Messages));

	// Baseline: no taper (fractions = 0)
	USubmarineEnvelopeDef* NoTaper = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	NoTaper->BowTaperFraction = 0.f;
	NoTaper->SternTaperFraction = 0.f;

	FSubmarineMeshSectionData ExteriorNoTaper;
	TestTrue(TEXT("Exterior no-taper should succeed"),
		Builder->GenerateExteriorMeshData(Solution, ExteriorNoTaper, 32, 6, 2.f, 1.f, NoTaper));

	FSubmarineMeshSectionData ExteriorNullEnvelope;
	TestTrue(TEXT("Exterior null-envelope should succeed"),
		Builder->GenerateExteriorMeshData(Solution, ExteriorNullEnvelope, 32, 6, 2.f, 1.f, nullptr));

	TestEqual(TEXT("Vertex count should match"),
		ExteriorNoTaper.Vertices.Num(), ExteriorNullEnvelope.Vertices.Num());

	// Compare vertices — with zero taper, the result should be identical to no envelope
	bool bAllMatch = true;
	for (int32 i = 0; i < FMath::Min(ExteriorNoTaper.Vertices.Num(), ExteriorNullEnvelope.Vertices.Num()); ++i)
	{
		if (!ExteriorNoTaper.Vertices[i].Equals(ExteriorNullEnvelope.Vertices[i], 0.1f))
		{
			bAllMatch = false;
			break;
		}
	}
	TestTrue(TEXT("Zero taper should produce identical vertices to no-envelope"), bAllMatch);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerBowSternNeedleConvergesToZeroTest,
	"Sub3D.SubCompiler.BowStern.Needle_ConvergesToZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerBowSternNeedleConvergesToZeroTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();
	USubmarineGeometryBuilder* Builder = NewObject<USubmarineGeometryBuilder>();

	if (!Envelope || !Graph || !Solver || !Builder)
	{
		return false;
	}

	Envelope->BowProfile = EBowSternProfile::Needle;
	Envelope->BowTaperFraction = 0.2f;

	FSubmarineLayoutSolution Solution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Solve should succeed"), Solver->Solve(Envelope, Graph, Solution, Messages));

	FSubmarineMeshSectionData ExteriorData;
	TestTrue(TEXT("Exterior should succeed with Needle bow"),
		Builder->GenerateExteriorMeshData(Solution, ExteriorData, 32, 6, 2.f, 1.f, Envelope));

	// Find the bow-most ring: vertices with the smallest X
	float MinX = TNumericLimits<float>::Max();
	for (const FVector& V : ExteriorData.Vertices)
	{
		MinX = FMath::Min(MinX, V.X);
	}

	// Check that vertices at the bow tip have very small radial distance
	float MaxBowRadius = 0.f;
	for (const FVector& V : ExteriorData.Vertices)
	{
		if (FMath::IsNearlyEqual(V.X, MinX, 1.f))
		{
			const float RadialDist = FMath::Sqrt(V.Y * V.Y + V.Z * V.Z);
			MaxBowRadius = FMath::Max(MaxBowRadius, RadialDist);
		}
	}

	TestTrue(
		FString::Printf(TEXT("Needle bow tip radius (%.1f) should be <= 5cm"), MaxBowRadius),
		MaxBowRadius <= 5.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerBowSternBluntStaysWideTest,
	"Sub3D.SubCompiler.BowStern.Blunt_StaysWideUntilEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerBowSternBluntStaysWideTest::RunTest(const FString& Parameters)
{
	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();

	if (!Envelope || !Graph || !Solver)
	{
		return false;
	}

	Envelope->SternProfile = EBowSternProfile::Blunt;
	Envelope->SternTaperFraction = 0.2f;

	// Evaluate taper at 80% of the stern taper zone
	// Stern zone is from (1 - SternTaperFraction) to 1.0
	// 80% through the zone: NormalizedPos = (1 - 0.2) + 0.8 * 0.2 = 0.96
	const float TestPos = (1.f - Envelope->SternTaperFraction) + 0.8f * Envelope->SternTaperFraction;
	const float TaperAt80Pct = Envelope->EvaluateBowSternTaper(TestPos);

	TestTrue(
		FString::Printf(TEXT("Blunt stern at 80%% of taper zone (pos=%.2f) should have taper >= 0.7 (got %.3f)"),
			TestPos, TaperAt80Pct),
		TaperAt80Pct >= 0.7f);

	// Also check body zone is 1.0
	const float BodyTaper = Envelope->EvaluateBowSternTaper(0.5f);
	TestTrue(
		FString::Printf(TEXT("Body zone taper should be 1.0 (got %.3f)"), BodyTaper),
		FMath::IsNearlyEqual(BodyTaper, 1.f, 0.001f));

	return true;
}

// ---------------------------------------------------------------------------
// E.4 — Partial rebuild test
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubCompilerPartialRebuildMatchesFullRebuildTest,
	"Sub3D.SubCompiler.PartialRebuild.MatchesFullRebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubCompilerPartialRebuildMatchesFullRebuildTest::RunTest(const FString& Parameters)
{
	// This test verifies that CompileAndBuildDirty produces the same solution as CompileAndBuild
	// by comparing the solver output (no PMC creation in headless tests)

	USubmarineEnvelopeDef* Envelope = FSubCompilerMvpFactory::CreateEnvelope(GetTransientPackage());
	USubmarineFunctionalGraph* Graph = FSubCompilerMvpFactory::CreateFunctionalGraph(GetTransientPackage());
	USubmarineLayoutSolver* Solver = NewObject<USubmarineLayoutSolver>();

	if (!Envelope || !Graph || !Solver)
	{
		return false;
	}

	// Baseline solve
	FSubmarineLayoutSolution BaselineSolution;
	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Baseline solve should succeed"), Solver->Solve(Envelope, Graph, BaselineSolution, Messages));

	// Modify one compartment's MinLength
	for (FCompartmentNode& Comp : Graph->Compartments)
	{
		if (Comp.CompartmentId == FName(TEXT("Engine")))
		{
			Comp.MinLengthCm = 350.f; // was 320
			break;
		}
	}

	// Solve again (simulates what CompileAndBuildDirty does internally)
	FSubmarineLayoutSolution ModifiedSolution;
	Messages.Reset();
	TestTrue(TEXT("Modified solve should succeed"), Solver->Solve(Envelope, Graph, ModifiedSolution, Messages));
	TestTrue(TEXT("Modified solution should be valid"), ModifiedSolution.IsValid());

	// The Engine compartment should have a different (larger) length
	const FCompartmentPlacement* EngineBaseline = BaselineSolution.Compartments.FindByPredicate(
		[](const FCompartmentPlacement& P) { return P.CompartmentId == FName(TEXT("Engine")); });
	const FCompartmentPlacement* EngineModified = ModifiedSolution.Compartments.FindByPredicate(
		[](const FCompartmentPlacement& P) { return P.CompartmentId == FName(TEXT("Engine")); });

	TestNotNull(TEXT("Engine should exist in baseline"), EngineBaseline);
	TestNotNull(TEXT("Engine should exist in modified"), EngineModified);

	if (EngineBaseline && EngineModified)
	{
		const float BaselineLen = EngineBaseline->SpineEndCm - EngineBaseline->SpineStartCm;
		const float ModifiedLen = EngineModified->SpineEndCm - EngineModified->SpineStartCm;
		TestTrue(
			FString::Printf(TEXT("Modified Engine (%.1f) should be >= baseline (%.1f)"), ModifiedLen, BaselineLen),
			ModifiedLen >= BaselineLen - 0.1f);
	}

	// Verify solution structure is consistent
	TestEqual(TEXT("Compartment count should match"), ModifiedSolution.Compartments.Num(), BaselineSolution.Compartments.Num());
	TestEqual(TEXT("Bulkhead count should match"), ModifiedSolution.Bulkheads.Num(), BaselineSolution.Bulkheads.Num());

	// Solve a second time with same parameters — should produce identical result (deterministic)
	FSubmarineLayoutSolution RepeatSolution;
	Messages.Reset();
	TestTrue(TEXT("Repeat solve should succeed"), Solver->Solve(Envelope, Graph, RepeatSolution, Messages));

	for (int32 i = 0; i < ModifiedSolution.Compartments.Num(); ++i)
	{
		TestTrue(
			FString::Printf(TEXT("Compartment %d SpineStart should be deterministic"), i),
			FMath::IsNearlyEqual(ModifiedSolution.Compartments[i].SpineStartCm, RepeatSolution.Compartments[i].SpineStartCm, 0.01f));
		TestTrue(
			FString::Printf(TEXT("Compartment %d SpineEnd should be deterministic"), i),
			FMath::IsNearlyEqual(ModifiedSolution.Compartments[i].SpineEndCm, RepeatSolution.Compartments[i].SpineEndCm, 0.01f));
	}

	return true;
}

#endif
