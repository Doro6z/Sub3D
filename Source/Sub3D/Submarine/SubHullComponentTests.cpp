#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "BreachVfxManagerComponent.h"
#include "Engine/DamageEvents.h"
#include "SubCompiler/SubCompilerMvpFactory.h"
#include "SubCompiler/SubmarineCompilerActor.h"
#include "SubHullAutomationTestProbe.h"
#include "SubHullComponent.h"
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

const FStructuralSheetDef* GetFirstExteriorSheet(const ASubmarineCompilerActor* CompilerActor)
{
	if (!CompilerActor || !CompilerActor->SubHull)
	{
		return nullptr;
	}

	return CompilerActor->SubHull->GetStructuralSheets().FindByPredicate([](const FStructuralSheetDef& Sheet)
	{
		return Sheet.bCanOpenToExterior;
	});
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
	FSubHullCollisionSpeedThresholdControlsDamageTest,
	"Sub3D.Submarine.Hull.Collision.SpeedThresholdControlsDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubHullCollisionSpeedThresholdControlsDamageTest::RunTest(const FString& Parameters)
{
	ASubmarineCompilerActor* CompilerActor = CreateCompiledTestSubmarine();
	TestNotNull(TEXT("Compiled submarine should exist"), CompilerActor);

	if (!CompilerActor || !CompilerActor->SubHull || !CompilerActor->SubMovement)
	{
		return false;
	}

	const FStructuralSheetDef* ExteriorSheet = GetFirstExteriorSheet(CompilerActor);
	TestNotNull(TEXT("An exterior sheet should exist"), ExteriorSheet);
	if (!ExteriorSheet)
	{
		return false;
	}

	CompilerActor->HullCollisionDamageMinSpeedCmS = 300.f;
	CompilerActor->HullCollisionCatastrophicSpeedCmS = 600.f;
	CompilerActor->HullCollisionDamageAtCatastrophicSpeed = 200.f;
	CompilerActor->HullCollisionDamageExponent = 1.f;

	const FVector WorldHitPoint = CompilerActor->GetActorTransform().TransformPosition(ExteriorSheet->LocalOrigin);
	const FVector WorldHitNormal = CompilerActor->GetActorTransform().TransformVectorNoScale(ExteriorSheet->LocalNormal).GetSafeNormal();

	FHitResult Hit;
	Hit.bBlockingHit = true;
	Hit.ImpactPoint = WorldHitPoint;
	Hit.Location = WorldHitPoint;
	Hit.Normal = WorldHitNormal;
	Hit.ImpactNormal = WorldHitNormal;

	CompilerActor->SubMovement->Velocity = -WorldHitNormal * 250.f;
	CompilerActor->OnHullHit(CompilerActor->GetMovementCollisionComponent(), nullptr, nullptr, FVector::ZeroVector, Hit);
	TestFalse(TEXT("Low-speed hull collision should not damage cells"), HasAnyDamagedCell(CompilerActor->SubHull->GetSheetStates()));

	CompilerActor->SubMovement->Velocity = -WorldHitNormal * 600.f;
	CompilerActor->OnHullHit(CompilerActor->GetMovementCollisionComponent(), nullptr, nullptr, FVector::ZeroVector, Hit);
	TestTrue(TEXT("High-speed hull collision should damage cells"), HasAnyDamagedCell(CompilerActor->SubHull->GetSheetStates()));
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

// [Phase 1 cleanup] FSubHullFloodWaterSurfaceZTest removed: tested
// UFloodWaterVisualsComponent::ComputeSurfaceLocalZ which was a trivial
// lerp between bounds Z. Component deleted (legacy duplicate of
// UFloodWaterPlaneComponent). Equivalent surface-Z computation now lives
// in UCompartmentVolumeComponent::GetWaterSurfaceWorldLocation().

#endif
