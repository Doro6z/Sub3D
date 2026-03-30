#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "BreachVfxManagerComponent.h"
#include "Engine/DamageEvents.h"
#include "FloodWaterVisualsComponent.h"
#include "SubCompiler/SubCompilerMvpFactory.h"
#include "SubCompiler/SubmarineCompilerActor.h"
#include "SubHullAutomationTestProbe.h"
#include "SubHullComponent.h"
#include "SubmarineCompartmentComponent.h"
#include "SubMovementComponent.h"

namespace
{
ASubmarineCompilerActor* CreateCompiledTestSubmarine()
{
	ASubmarineCompilerActor* CompilerActor = NewObject<ASubmarineCompilerActor>(GetTransientPackage());
	if (!CompilerActor)
	{
		return nullptr;
	}

	CompilerActor->EnvelopeDef = FSubCompilerMvpFactory::CreateEnvelope(CompilerActor);
	CompilerActor->FunctionalGraph = FSubCompilerMvpFactory::CreateFunctionalGraph(CompilerActor);
	if (!CompilerActor->CompileCurrentDefinitions())
	{
		return nullptr;
	}

	return CompilerActor;
}

FVector GetFirstSheetWorldImpactPoint(const ASubmarineCompilerActor* CompilerActor)
{
	if (!CompilerActor || !CompilerActor->SubHull || CompilerActor->SubHull->GetStructuralSheets().Num() == 0)
	{
		return CompilerActor ? CompilerActor->GetActorLocation() : FVector::ZeroVector;
	}

	return CompilerActor->GetActorTransform().TransformPosition(CompilerActor->SubHull->GetStructuralSheets()[0].LocalOrigin);
}

FPointDamageEvent MakePointDamageEventAt(const FVector& WorldHitPoint)
{
	FPointDamageEvent DamageEvent;
	DamageEvent.HitInfo.bBlockingHit = true;
	DamageEvent.HitInfo.ImpactPoint = WorldHitPoint;
	DamageEvent.HitInfo.Location = WorldHitPoint;
	return DamageEvent;
}

bool HasAnyDamagedCell(const TArray<FStructuralSheetRuntimeState>& SheetStates)
{
	for (const FStructuralSheetRuntimeState& SheetState : SheetStates)
	{
		for (const FStructuralCellState& Cell : SheetState.Cells)
		{
			if (Cell.Damage01 > 0.f)
			{
				return true;
			}
		}
	}

	return false;
}

int32 CountDamagedCells(const TArray<FStructuralSheetRuntimeState>& SheetStates)
{
	int32 DamagedCellCount = 0;
	for (const FStructuralSheetRuntimeState& SheetState : SheetStates)
	{
		for (const FStructuralCellState& Cell : SheetState.Cells)
		{
			if (Cell.Damage01 > 0.f)
			{
				++DamagedCellCount;
			}
		}
	}

	return DamagedCellCount;
}

bool HasFloodedCompartment(const TArray<FCompartmentRuntimeState>& CompartmentStates)
{
	for (const FCompartmentRuntimeState& CompartmentState : CompartmentStates)
	{
		if (CompartmentState.CurrentWaterLiters > 0.f)
		{
			return true;
		}
	}

	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullTakeDamagePointDamageDamagesCellsTest,
	"Sub3D.Submarine.Hull.TakeDamage.PointDamageDamagesCells",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullTakeDamagePointDamageDamagesCellsTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);

	if (!CompilerActor || !CompilerActor->SubHull)
	{
		return false;
	}

	const FVector WorldHitPoint = GetFirstSheetWorldImpactPoint(CompilerActor);
	FPointDamageEvent DamageEvent = MakePointDamageEventAt(WorldHitPoint);

	const float ReturnedDamage = CompilerActor->TakeDamage(50.f, DamageEvent, nullptr, nullptr);
	TestEqual(TEXT("TakeDamage should return the applied damage on authority"), ReturnedDamage, 50.f);
	TestTrue(TEXT("Point damage should damage at least one hull cell"), HasAnyDamagedCell(CompilerActor->SubHull->GetSheetStates()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullTakeDamageZeroDamageNoCellChangeTest,
	"Sub3D.Submarine.Hull.TakeDamage.ZeroDamageNoCellChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullTakeDamageZeroDamageNoCellChangeTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);

	if (!CompilerActor || !CompilerActor->SubHull)
	{
		return false;
	}

	const FVector WorldHitPoint = GetFirstSheetWorldImpactPoint(CompilerActor);
	FPointDamageEvent DamageEvent = MakePointDamageEventAt(WorldHitPoint);
	const int32 DamagedCellsBefore = CountDamagedCells(CompilerActor->SubHull->GetSheetStates());

	const float ReturnedDamage = CompilerActor->TakeDamage(0.f, DamageEvent, nullptr, nullptr);
	TestEqual(TEXT("TakeDamage should return 0 when damage is 0"), ReturnedDamage, 0.f);
	TestEqual(TEXT("Zero damage should not change hull cells"), CountDamagedCells(CompilerActor->SubHull->GetSheetStates()), DamagedCellsBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullTakeDamageNonAuthorityNoCellChangeTest,
	"Sub3D.Submarine.Hull.TakeDamage.NonAuthorityNoCellChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullTakeDamageNonAuthorityNoCellChangeTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);

	if (!CompilerActor || !CompilerActor->SubHull)
	{
		return false;
	}

	CompilerActor->SetRole(ROLE_SimulatedProxy);
	const FVector WorldHitPoint = GetFirstSheetWorldImpactPoint(CompilerActor);
	FPointDamageEvent DamageEvent = MakePointDamageEventAt(WorldHitPoint);

	const float ReturnedDamage = CompilerActor->TakeDamage(50.f, DamageEvent, nullptr, nullptr);
	TestEqual(TEXT("Non-authority TakeDamage should return 0"), ReturnedDamage, 0.f);
	TestFalse(TEXT("Non-authority TakeDamage should not damage hull cells"), HasAnyDamagedCell(CompilerActor->SubHull->GetSheetStates()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullOnRepBreachClustersBroadcastsDelegateTest,
	"Sub3D.Submarine.Hull.OnRep.BreachClustersBroadcastsDelegate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullOnRepBreachClustersBroadcastsDelegateTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	USubHullAutomationTestProbe* Probe = NewObject<USubHullAutomationTestProbe>(GetTransientPackage());

	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);
	TestNotNull(TEXT("Automation probe should exist"), Probe);

	if (!CompilerActor || !CompilerActor->SubHull || !Probe)
	{
		return false;
	}

	CompilerActor->SubHull->OnHullDamageUpdated.AddDynamic(Probe, &USubHullAutomationTestProbe::HandleHullDamageUpdated);
	CompilerActor->SubHull->OnBreachesUpdated.AddDynamic(Probe, &USubHullAutomationTestProbe::HandleBreachesUpdated);

	const FVector WorldHitPoint = GetFirstSheetWorldImpactPoint(CompilerActor);
	FPointDamageEvent DamageEvent = MakePointDamageEventAt(WorldHitPoint);
	CompilerActor->TakeDamage(150.f, DamageEvent, nullptr, nullptr);

	TestTrue(TEXT("Server-local hull damage delegate should fire after TakeDamage"), Probe->HullDamageUpdatedCount > 0);
	TestTrue(TEXT("Server-local breach delegate should fire after TakeDamage"), Probe->BreachesUpdatedCount > 0);
	TestTrue(TEXT("Test setup should create at least one breach cluster"), CompilerActor->SubHull->GetBreachClusters().Num() > 0);

	Probe->ResetCounters();
	UFunction* OnRepBreaches = CompilerActor->SubHull->FindFunction(TEXT("OnRep_BreachClusters"));
	TestNotNull(TEXT("OnRep_BreachClusters should be reflected"), OnRepBreaches);

	if (!OnRepBreaches)
	{
		return false;
	}

	CompilerActor->SubHull->ProcessEvent(OnRepBreaches, nullptr);
	TestEqual(TEXT("OnRep_BreachClusters should broadcast once"), Probe->BreachesUpdatedCount, 1);
	TestEqual(TEXT("OnRep_BreachClusters should broadcast the full breach array"), Probe->LastBreachesCount, CompilerActor->SubHull->GetBreachClusters().Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullOnRepCompartmentStatesBroadcastsDelegateTest,
	"Sub3D.Submarine.Hull.OnRep.CompartmentStatesBroadcastsDelegate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullOnRepCompartmentStatesBroadcastsDelegateTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	USubHullAutomationTestProbe* Probe = NewObject<USubHullAutomationTestProbe>(GetTransientPackage());

	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);
	TestNotNull(TEXT("Automation probe should exist"), Probe);

	if (!CompilerActor || !CompilerActor->SubHull || !Probe)
	{
		return false;
	}

	CompilerActor->SubHull->OnCompartmentFloodUpdated.AddDynamic(Probe, &USubHullAutomationTestProbe::HandleCompartmentFloodUpdated);

	const FVector WorldHitPoint = GetFirstSheetWorldImpactPoint(CompilerActor);
	FPointDamageEvent DamageEvent = MakePointDamageEventAt(WorldHitPoint);
	CompilerActor->TakeDamage(150.f, DamageEvent, nullptr, nullptr);
	CompilerActor->SubHull->TickComponent(1.f, LEVELTICK_All, nullptr);

	TestTrue(TEXT("Flooding tick should affect at least one compartment"), HasFloodedCompartment(CompilerActor->SubHull->GetCompartmentStates()));
	TestTrue(TEXT("Server-local flood delegate should fire during flooding tick"), Probe->CompartmentFloodUpdatedCount > 0);

	Probe->ResetCounters();
	UFunction* OnRepCompartmentStates = CompilerActor->SubHull->FindFunction(TEXT("OnRep_CompartmentStates"));
	TestNotNull(TEXT("OnRep_CompartmentStates should be reflected"), OnRepCompartmentStates);

	if (!OnRepCompartmentStates)
	{
		return false;
	}

	CompilerActor->SubHull->ProcessEvent(OnRepCompartmentStates, nullptr);
	TestEqual(TEXT("OnRep_CompartmentStates should broadcast once"), Probe->CompartmentFloodUpdatedCount, 1);
	TestEqual(TEXT("OnRep_CompartmentStates should broadcast the full compartment array"), Probe->LastCompartmentStatesCount, CompilerActor->SubHull->GetCompartmentStates().Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullDebugBreachToolCreatesDamageTest,
	"Sub3D.Submarine.Hull.DebugBreachTool.CreatesDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullDebugBreachToolCreatesDamageTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);

	if (!CompilerActor || !CompilerActor->SubHull)
	{
		return false;
	}

	TestTrue(TEXT("Debug breach tool should succeed on authority"), CompilerActor->CreateDebugBreachOnFirstExteriorSheet(150.f));
	TestTrue(TEXT("Debug breach tool should damage hull cells"), HasAnyDamagedCell(CompilerActor->SubHull->GetSheetStates()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullBreachVfxDesiredCountTest,
	"Sub3D.Submarine.Hull.BreachVfx.DesiredCountMatchesLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullBreachVfxDesiredCountTest::RunTest(const FString& Parameters)
{
	UBreachVfxManagerComponent* BreachVfxManager = NewObject<UBreachVfxManagerComponent>();
	TestNotNull(TEXT("Breach VFX manager should exist"), BreachVfxManager);

	if (!BreachVfxManager)
	{
		return false;
	}

	FBreachClusterState SmallBreach;
	SmallBreach.OpenAreaCm2 = 50.f;

	FBreachClusterState MediumBreach;
	MediumBreach.OpenAreaCm2 = 100.f;

	FBreachClusterState LargeBreach;
	LargeBreach.OpenAreaCm2 = 200.f;

	TArray<FBreachClusterState> Breaches = { SmallBreach, MediumBreach, LargeBreach };
	BreachVfxManager->MaxActiveEffects = 2;

	TestEqual(TEXT("Desired active effect count should clamp to MaxActiveEffects"), BreachVfxManager->ComputeDesiredActiveEffectCount(Breaches), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullFloodWaterSurfaceZTest,
	"Sub3D.Submarine.Hull.FloodVisuals.SurfaceZMatchesBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullFloodWaterSurfaceZTest::RunTest(const FString& Parameters)
{
	UFloodWaterVisualsComponent* FloodWaterVisuals = NewObject<UFloodWaterVisualsComponent>();
	TestNotNull(TEXT("Flood water visuals component should exist"), FloodWaterVisuals);

	if (!FloodWaterVisuals)
	{
		return false;
	}

	const FBox LocalBounds(FVector(-100.f, -50.f, -25.f), FVector(100.f, 50.f, 75.f));
	TestEqual(TEXT("Water level 0 should map to bounds min Z"), FloodWaterVisuals->ComputeSurfaceLocalZ(LocalBounds, 0.f), -25.f);
	TestEqual(TEXT("Water level 0.5 should map to the middle of the bounds"), FloodWaterVisuals->ComputeSurfaceLocalZ(LocalBounds, 0.5f), 25.f);
	TestEqual(TEXT("Water level 1 should map to bounds max Z"), FloodWaterVisuals->ComputeSurfaceLocalZ(LocalBounds, 1.f), 75.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullExportCompartmentStatesCountMatchesRuntimeStatesTest,
	"Sub3D.Submarine.Hull.ExportCompartmentStates.CountMatchesRuntimeStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullExportCompartmentStatesCountMatchesRuntimeStatesTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);

	if (!CompilerActor || !CompilerActor->SubHull)
	{
		return false;
	}

	const FVector WorldHitPoint = GetFirstSheetWorldImpactPoint(CompilerActor);
	FPointDamageEvent DamageEvent = MakePointDamageEventAt(WorldHitPoint);
	CompilerActor->TakeDamage(150.f, DamageEvent, nullptr, nullptr);
	CompilerActor->SubHull->TickComponent(1.f, LEVELTICK_All, nullptr);

	TArray<FCompartmentState> ExportedStates;
	CompilerActor->SubHull->ExportCompartmentStates(ExportedStates);

	TestEqual(
		TEXT("ExportCompartmentStates should return the same count as runtime compartment states"),
		ExportedStates.Num(),
		CompilerActor->SubHull->GetCompartmentStates().Num());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullCompartmentWaterDerivedStateTest,
	"Sub3D.Submarine.Hull.CompartmentWater.DerivedStateIsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullCompartmentWaterDerivedStateTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);

	if (!CompilerActor || !CompilerActor->SubHull || !CompilerActor->SubMovement)
	{
		return false;
	}

	CompilerActor->SubMovement->CurrentDepth = 100.f;
	const FVector WorldHitPoint = GetFirstSheetWorldImpactPoint(CompilerActor);
	FPointDamageEvent DamageEvent = MakePointDamageEventAt(WorldHitPoint);
	CompilerActor->TakeDamage(150.f, DamageEvent, nullptr, nullptr);
	CompilerActor->SubHull->TickComponent(1.f, LEVELTICK_All, nullptr);

	bool bFoundFloodedCompartment = false;
	for (const FCompartmentRuntimeState& CompartmentState : CompilerActor->SubHull->GetCompartmentStates())
	{
		TestTrue(TEXT("Max water height should stay positive"), CompartmentState.MaxWaterHeightCm > 0.f);
		TestTrue(TEXT("Water height should stay non-negative"), CompartmentState.WaterHeightCm >= 0.f);
		TestTrue(TEXT("Water level should stay normalized"), CompartmentState.WaterLevelNormalized >= 0.f && CompartmentState.WaterLevelNormalized <= 1.f);

		if (CompartmentState.CurrentWaterLiters > 0.f)
		{
			bFoundFloodedCompartment = true;
		}
	}

	TestTrue(TEXT("At least one compartment should become flooded after a breach"), bFoundFloodedCompartment);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullDoorOpenEqualizesWaterHeightsTest,
	"Sub3D.Submarine.Hull.Flooding.DoorOpenEqualizesWaterHeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullDoorOpenEqualizesWaterHeightsTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);

	if (!CompilerActor || !CompilerActor->SubHull || !CompilerActor->Compartments)
	{
		return false;
	}

	CompilerActor->SubMovement->CurrentDepth = 60.f;

	const FStructuralSheetDef* ExteriorSheet = CompilerActor->SubHull->GetStructuralSheets().FindByPredicate([](const FStructuralSheetDef& Sheet)
	{
		return Sheet.bCanOpenToExterior;
	});
	TestNotNull(TEXT("An exterior sheet should exist"), ExteriorSheet);

	const FStructuralSheetDef* BulkheadSheet = CompilerActor->SubHull->GetStructuralSheets().FindByPredicate([ExteriorSheet](const FStructuralSheetDef& Sheet)
	{
		return !Sheet.AdjacentCompartmentId.IsNone()
			&& ExteriorSheet
			&& (Sheet.ParentCompartmentId == ExteriorSheet->ParentCompartmentId || Sheet.AdjacentCompartmentId == ExteriorSheet->ParentCompartmentId);
	});
	TestNotNull(TEXT("A connected bulkhead sheet should exist"), BulkheadSheet);

	if (!ExteriorSheet || !BulkheadSheet)
	{
		return false;
	}

	FDoorState DoorState;
	DoorState.DoorId = BulkheadSheet->SheetId;
	DoorState.CompartmentA = BulkheadSheet->ParentCompartmentId;
	DoorState.CompartmentB = BulkheadSheet->AdjacentCompartmentId;
	DoorState.bClosed = false;
	CompilerActor->Compartments->RegisterDoor(DoorState);

	const FVector WorldHitPoint = CompilerActor->GetActorTransform().TransformPosition(ExteriorSheet->LocalOrigin);
	FPointDamageEvent DamageEvent = MakePointDamageEventAt(WorldHitPoint);
	CompilerActor->TakeDamage(150.f, DamageEvent, nullptr, nullptr);
	CompilerActor->SubHull->TickComponent(1.f, LEVELTICK_All, nullptr);

	const FCompartmentRuntimeState* SourceCompartment = CompilerActor->SubHull->GetCompartmentStates().FindByPredicate([ExteriorSheet](const FCompartmentRuntimeState& Candidate)
	{
		return Candidate.CompartmentId == ExteriorSheet->ParentCompartmentId;
	});
	const FName AdjacentCompartmentId = BulkheadSheet->ParentCompartmentId == ExteriorSheet->ParentCompartmentId
		? BulkheadSheet->AdjacentCompartmentId
		: BulkheadSheet->ParentCompartmentId;
	const FCompartmentRuntimeState* AdjacentCompartment = CompilerActor->SubHull->GetCompartmentStates().FindByPredicate([AdjacentCompartmentId](const FCompartmentRuntimeState& Candidate)
	{
		return Candidate.CompartmentId == AdjacentCompartmentId;
	});

	TestNotNull(TEXT("Source compartment should exist"), SourceCompartment);
	TestNotNull(TEXT("Adjacent compartment should exist"), AdjacentCompartment);

	if (!SourceCompartment || !AdjacentCompartment)
	{
		return false;
	}

	const float SourceHeightBefore = SourceCompartment->WaterHeightCm;
	const float AdjacentHeightBefore = AdjacentCompartment->WaterHeightCm;
	TestTrue(TEXT("Source compartment should contain water before equalization"), SourceHeightBefore > 0.f);
	TestTrue(TEXT("Repair should close the exterior breach before equalization-only tick"), CompilerActor->SubHull->RepairAtWorldPoint(WorldHitPoint, 10000.f, 80.f));

	CompilerActor->SubHull->TickComponent(1.f, LEVELTICK_All, nullptr);

	SourceCompartment = CompilerActor->SubHull->GetCompartmentStates().FindByPredicate([ExteriorSheet](const FCompartmentRuntimeState& Candidate)
	{
		return Candidate.CompartmentId == ExteriorSheet->ParentCompartmentId;
	});
	AdjacentCompartment = CompilerActor->SubHull->GetCompartmentStates().FindByPredicate([AdjacentCompartmentId](const FCompartmentRuntimeState& Candidate)
	{
		return Candidate.CompartmentId == AdjacentCompartmentId;
	});
	TestNotNull(TEXT("Source compartment should still exist after equalization tick"), SourceCompartment);
	TestNotNull(TEXT("Adjacent compartment should still exist after equalization tick"), AdjacentCompartment);

	if (!SourceCompartment || !AdjacentCompartment)
	{
		return false;
	}

	const float SourceHeightAfter = SourceCompartment->WaterHeightCm;
	const float AdjacentHeightAfter = AdjacentCompartment->WaterHeightCm;
	TestTrue(
		TEXT("Source water height should decrease when equalizing into an emptier compartment"),
		SourceHeightAfter < SourceHeightBefore);
	TestTrue(
		TEXT("Adjacent water height should increase when receiving equalized water"),
		AdjacentHeightAfter > AdjacentHeightBefore);
	TestTrue(
		TEXT("Open door equalization should reduce the water height gap between compartments"),
		FMath::Abs(SourceHeightAfter - AdjacentHeightAfter) < FMath::Abs(SourceHeightBefore - AdjacentHeightBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullDoorClosedBlocksTransferTest,
	"Sub3D.Submarine.Hull.Flooding.DoorClosedBlocksTransfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullDoorClosedBlocksTransferTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);

	if (!CompilerActor || !CompilerActor->SubHull || !CompilerActor->Compartments)
	{
		return false;
	}

	CompilerActor->SubMovement->CurrentDepth = 60.f;

	const FStructuralSheetDef* ExteriorSheet = CompilerActor->SubHull->GetStructuralSheets().FindByPredicate([](const FStructuralSheetDef& Sheet)
	{
		return Sheet.bCanOpenToExterior;
	});
	TestNotNull(TEXT("An exterior sheet should exist"), ExteriorSheet);

	const FStructuralSheetDef* BulkheadSheet = CompilerActor->SubHull->GetStructuralSheets().FindByPredicate([](const FStructuralSheetDef& Sheet)
	{
		return !Sheet.AdjacentCompartmentId.IsNone();
	});
	TestNotNull(TEXT("A bulkhead sheet should exist"), BulkheadSheet);

	if (!ExteriorSheet || !BulkheadSheet)
	{
		return false;
	}

	FDoorState DoorState;
	DoorState.DoorId = BulkheadSheet->SheetId;
	DoorState.CompartmentA = BulkheadSheet->ParentCompartmentId;
	DoorState.CompartmentB = BulkheadSheet->AdjacentCompartmentId;
	DoorState.bClosed = true;
	CompilerActor->Compartments->RegisterDoor(DoorState);

	const FVector WorldHitPoint = CompilerActor->GetActorTransform().TransformPosition(ExteriorSheet->LocalOrigin);
	FPointDamageEvent DamageEvent = MakePointDamageEventAt(WorldHitPoint);
	CompilerActor->TakeDamage(150.f, DamageEvent, nullptr, nullptr);
	CompilerActor->SubHull->TickComponent(1.f, LEVELTICK_All, nullptr);

	const FCompartmentRuntimeState* SourceCompartment = CompilerActor->SubHull->GetCompartmentStates().FindByPredicate([ExteriorSheet](const FCompartmentRuntimeState& Candidate)
	{
		return Candidate.CompartmentId == ExteriorSheet->ParentCompartmentId;
	});
	const FCompartmentRuntimeState* AdjacentCompartment = CompilerActor->SubHull->GetCompartmentStates().FindByPredicate([BulkheadSheet](const FCompartmentRuntimeState& Candidate)
	{
		return Candidate.CompartmentId == BulkheadSheet->AdjacentCompartmentId;
	});

	TestNotNull(TEXT("Source compartment should exist"), SourceCompartment);
	TestNotNull(TEXT("Adjacent compartment should exist"), AdjacentCompartment);

	if (!SourceCompartment || !AdjacentCompartment)
	{
		return false;
	}

	TestTrue(TEXT("Source compartment should flood"), SourceCompartment->CurrentWaterLiters > 0.f);
	TestEqual(TEXT("Closed door should block transfer to adjacent compartment"), AdjacentCompartment->CurrentWaterLiters, 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubMovementFloodedMassUsesAuthoritativeHullStateTest,
	"Sub3D.Submarine.Movement.FloodedMass.UsesAuthoritativeHullState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubMovementFloodedMassUsesAuthoritativeHullStateTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);

	if (!CompilerActor || !CompilerActor->SubHull || !CompilerActor->SubMovement)
	{
		return false;
	}

	CompilerActor->SubMovement->CurrentDepth = 60.f;
	const FVector WorldHitPoint = GetFirstSheetWorldImpactPoint(CompilerActor);
	FPointDamageEvent DamageEvent = MakePointDamageEventAt(WorldHitPoint);
	CompilerActor->TakeDamage(150.f, DamageEvent, nullptr, nullptr);
	CompilerActor->SubHull->TickComponent(1.f, LEVELTICK_All, nullptr);

	const float AuthoritativeHullWaterMassKg = CompilerActor->SubHull->GetTotalWaterLiters();
	TestTrue(TEXT("Hull should contain flooded water before movement tick"), AuthoritativeHullWaterMassKg > 0.f);

	CompilerActor->SubMovement->TickComponent(1.f / 30.f, LEVELTICK_All, nullptr);

	TestEqual(
		TEXT("Movement flooded mass should be sourced from the authoritative hull state"),
		CompilerActor->SubMovement->FloodedMassKg,
		AuthoritativeHullWaterMassKg);
	TestEqual(
		TEXT("Submarine total flooded mass getter should prefer authoritative hull state"),
		CompilerActor->GetTotalFloodWaterMassKg(),
		AuthoritativeHullWaterMassKg);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubHullCompartmentSamplingByLocationTest,
	"Sub3D.Submarine.Hull.CompartmentSampling.ByWorldLocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullCompartmentSamplingByLocationTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);

	if (!CompilerActor || !CompilerActor->SubHull)
	{
		return false;
	}

	const FStructuralSheetDef* ExteriorSheet = CompilerActor->SubHull->GetStructuralSheets().FindByPredicate([](const FStructuralSheetDef& Sheet)
	{
		return Sheet.bCanOpenToExterior;
	});

	TestNotNull(TEXT("Compiled submarine should expose at least one exterior sheet"), ExteriorSheet);
	if (!ExteriorSheet)
	{
		return false;
	}

	FCompartmentState SampledState;
	TestTrue(
		TEXT("Sampling by world location should resolve the owning compartment"),
		CompilerActor->SubHull->SampleCompartmentStateAtWorldLocation(
			CompilerActor->GetActorTransform().TransformPosition(ExteriorSheet->LocalOrigin),
			SampledState,
			nullptr));

	TestEqual(TEXT("Sampling should resolve the exterior sheet parent compartment"), SampledState.CompartmentId, ExteriorSheet->ParentCompartmentId);
	return true;
}

#endif
