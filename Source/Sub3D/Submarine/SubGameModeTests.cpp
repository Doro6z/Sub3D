#include "Misc/AutomationTest.h"

#include "SubGameMode.h"
#include "SubGameState.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubRunPhaseTransitionTest,
	"Sub3D.Submarine.RunPhase.ValidTransitionSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubRunPhaseTransitionTest::RunTest(const FString& Parameters)
{
	ASubGameMode* GameMode = NewObject<ASubGameMode>(GetTransientPackage());
	TestNotNull(TEXT("GameMode should exist"), GameMode);

	if (!GameMode)
	{
		return false;
	}

	TestEqual(TEXT("Initial phase should be Boot"), GameMode->GetRunPhase(), ESubRunPhase::Boot);

	GameMode->AdvanceRunPhase(ESubRunPhase::Boarding);
	TestEqual(TEXT("Boot -> Boarding should succeed"), GameMode->GetRunPhase(), ESubRunPhase::Boarding);

	GameMode->AdvanceRunPhase(ESubRunPhase::Departure);
	TestEqual(TEXT("Boarding -> Departure should succeed"), GameMode->GetRunPhase(), ESubRunPhase::Departure);

	GameMode->AdvanceRunPhase(ESubRunPhase::Traverse);
	TestEqual(TEXT("Departure -> Traverse should succeed"), GameMode->GetRunPhase(), ESubRunPhase::Traverse);

	GameMode->AdvanceRunPhase(ESubRunPhase::Approach);
	TestEqual(TEXT("Traverse -> Approach should succeed"), GameMode->GetRunPhase(), ESubRunPhase::Approach);

	GameMode->AdvanceRunPhase(ESubRunPhase::Docking);
	TestEqual(TEXT("Approach -> Docking should succeed"), GameMode->GetRunPhase(), ESubRunPhase::Docking);

	GameMode->AdvanceRunPhase(ESubRunPhase::Success);
	TestEqual(TEXT("Docking -> Success should succeed"), GameMode->GetRunPhase(), ESubRunPhase::Success);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubRunPhaseInvalidTransitionTest,
	"Sub3D.Submarine.RunPhase.InvalidTransitionRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubRunPhaseInvalidTransitionTest::RunTest(const FString& Parameters)
{
	ASubGameMode* GameMode = NewObject<ASubGameMode>(GetTransientPackage());
	TestNotNull(TEXT("GameMode should exist"), GameMode);

	if (!GameMode)
	{
		return false;
	}

	GameMode->AdvanceRunPhase(ESubRunPhase::Success);
	TestEqual(TEXT("Boot -> Success should be rejected"), GameMode->GetRunPhase(), ESubRunPhase::Boot);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubRunTriggerBreachRejectedOutsideTraverseTest,
	"Sub3D.Submarine.RunPhase.TriggerBreachRejectedOutsideTraverse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubRunTriggerBreachRejectedOutsideTraverseTest::RunTest(const FString& Parameters)
{
	ASubGameMode* GameMode = NewObject<ASubGameMode>(GetTransientPackage());
	TestNotNull(TEXT("GameMode should exist"), GameMode);

	if (!GameMode)
	{
		return false;
	}

	TestFalse(TEXT("TriggerBreachEvent should reject outside Traverse"), GameMode->TriggerBreachEvent());
	TestEqual(TEXT("Phase should remain Boot when breach trigger is rejected"), GameMode->GetRunPhase(), ESubRunPhase::Boot);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubRunPhaseDepartureGateTransitionTest,
	"Sub3D.Submarine.RunPhase.DepartureGateTransitionsToTraverse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubRunPhaseDepartureGateTransitionTest::RunTest(const FString& Parameters)
{
	ASubGameMode* GameMode = NewObject<ASubGameMode>(GetTransientPackage());
	TestNotNull(TEXT("GameMode should exist"), GameMode);

	if (!GameMode)
	{
		return false;
	}

	GameMode->AdvanceRunPhase(ESubRunPhase::Boarding);
	GameMode->BeginDeparture();
	TestEqual(TEXT("BeginDeparture should move Boarding to Departure"), GameMode->GetRunPhase(), ESubRunPhase::Departure);

	GameMode->NotifySubmarineClearedDepartureGate();
	TestEqual(TEXT("Departure gate should move Departure to Traverse"), GameMode->GetRunPhase(), ESubRunPhase::Traverse);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubRunPhaseApproachZoneTransitionTest,
	"Sub3D.Submarine.RunPhase.ApproachZoneTransitionsToApproach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubRunPhaseApproachZoneTransitionTest::RunTest(const FString& Parameters)
{
	ASubGameMode* GameMode = NewObject<ASubGameMode>(GetTransientPackage());
	TestNotNull(TEXT("GameMode should exist"), GameMode);

	if (!GameMode)
	{
		return false;
	}

	GameMode->AdvanceRunPhase(ESubRunPhase::Boarding);
	GameMode->AdvanceRunPhase(ESubRunPhase::Departure);
	GameMode->AdvanceRunPhase(ESubRunPhase::Traverse);

	GameMode->NotifyApproachZoneStateChanged(true);

	TestEqual(TEXT("Approach zone should move Traverse to Approach when no active breach exists"), GameMode->GetRunPhase(), ESubRunPhase::Approach);
	TestTrue(TEXT("Approach zone should set the destination flag"), GameMode->bSubmarineInApproachZone);

	GameMode->NotifyApproachZoneStateChanged(false);
	TestFalse(TEXT("Leaving approach zone should clear the destination flag"), GameMode->bSubmarineInApproachZone);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubRunPhaseApproachZoneDeferredDuringBreachTest,
	"Sub3D.Submarine.RunPhase.ApproachZoneDeferredDuringBreach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubRunPhaseApproachZoneDeferredDuringBreachTest::RunTest(const FString& Parameters)
{
	ASubGameMode* GameMode = NewObject<ASubGameMode>(GetTransientPackage());
	TestNotNull(TEXT("GameMode should exist"), GameMode);

	if (!GameMode)
	{
		return false;
	}

	GameMode->AdvanceRunPhase(ESubRunPhase::Boarding);
	GameMode->AdvanceRunPhase(ESubRunPhase::Departure);
	GameMode->AdvanceRunPhase(ESubRunPhase::Traverse);
	GameMode->AdvanceRunPhase(ESubRunPhase::BreachCrisis);
	GameMode->bBreachObjectiveActive = true;
	GameMode->ActiveBreachSheetId = TEXT("Exterior_A");

	GameMode->NotifyApproachZoneStateChanged(true);

	TestEqual(TEXT("Approach zone should not skip active breach crisis"), GameMode->GetRunPhase(), ESubRunPhase::BreachCrisis);
	TestTrue(TEXT("Approach flag should still be latched during breach crisis"), GameMode->bSubmarineInApproachZone);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubRunPhaseBreachRecoveryTransitionTest,
	"Sub3D.Submarine.RunPhase.BreachRecoveryTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubRunPhaseBreachRecoveryTransitionTest::RunTest(const FString& Parameters)
{
	ASubGameMode* GameMode = NewObject<ASubGameMode>(GetTransientPackage());
	TestNotNull(TEXT("GameMode should exist"), GameMode);

	if (!GameMode)
	{
		return false;
	}

	GameMode->AdvanceRunPhase(ESubRunPhase::Boarding);
	GameMode->AdvanceRunPhase(ESubRunPhase::Departure);
	GameMode->AdvanceRunPhase(ESubRunPhase::Traverse);
	GameMode->AdvanceRunPhase(ESubRunPhase::BreachCrisis);
	TestEqual(TEXT("Traverse -> BreachCrisis should succeed"), GameMode->GetRunPhase(), ESubRunPhase::BreachCrisis);

	GameMode->AdvanceRunPhase(ESubRunPhase::Traverse);
	TestEqual(TEXT("BreachCrisis -> Traverse should succeed"), GameMode->GetRunPhase(), ESubRunPhase::Traverse);

	GameMode->AdvanceRunPhase(ESubRunPhase::Approach);
	TestEqual(TEXT("Traverse -> Approach should succeed after recovery"), GameMode->GetRunPhase(), ESubRunPhase::Approach);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubRunPhaseNotifyBreachStabilizedTraverseTest,
	"Sub3D.Submarine.RunPhase.NotifyBreachStabilizedReturnsToTraverse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubRunPhaseNotifyBreachStabilizedTraverseTest::RunTest(const FString& Parameters)
{
	ASubGameMode* GameMode = NewObject<ASubGameMode>(GetTransientPackage());
	TestNotNull(TEXT("GameMode should exist"), GameMode);

	if (!GameMode)
	{
		return false;
	}

	GameMode->AdvanceRunPhase(ESubRunPhase::Boarding);
	GameMode->AdvanceRunPhase(ESubRunPhase::Departure);
	GameMode->AdvanceRunPhase(ESubRunPhase::Traverse);
	GameMode->AdvanceRunPhase(ESubRunPhase::BreachCrisis);

	GameMode->bBreachObjectiveActive = true;
	GameMode->ActiveBreachSheetId = TEXT("Exterior_A");
	GameMode->ActiveBreachedCompartmentId = TEXT("Ballast_Fwd");
	GameMode->bSubmarineInApproachZone = false;

	GameMode->NotifyBreachStabilized(TEXT("Exterior_A"));

	TestEqual(TEXT("BreachCrisis should return to Traverse when far from destination"), GameMode->GetRunPhase(), ESubRunPhase::Traverse);
	TestFalse(TEXT("Breach objective should be cleared"), GameMode->bBreachObjectiveActive);
	TestTrue(TEXT("Active breach sheet id should be cleared"), GameMode->ActiveBreachSheetId.IsNone());
	TestTrue(TEXT("Active breached compartment id should be cleared"), GameMode->ActiveBreachedCompartmentId.IsNone());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubRunPhaseNotifyBreachStabilizedApproachTest,
	"Sub3D.Submarine.RunPhase.NotifyBreachStabilizedReturnsToApproach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubRunPhaseNotifyBreachStabilizedApproachTest::RunTest(const FString& Parameters)
{
	ASubGameMode* GameMode = NewObject<ASubGameMode>(GetTransientPackage());
	TestNotNull(TEXT("GameMode should exist"), GameMode);

	if (!GameMode)
	{
		return false;
	}

	GameMode->AdvanceRunPhase(ESubRunPhase::Boarding);
	GameMode->AdvanceRunPhase(ESubRunPhase::Departure);
	GameMode->AdvanceRunPhase(ESubRunPhase::Traverse);
	GameMode->AdvanceRunPhase(ESubRunPhase::BreachCrisis);

	GameMode->bBreachObjectiveActive = true;
	GameMode->ActiveBreachSheetId = TEXT("Exterior_A");
	GameMode->ActiveBreachedCompartmentId = TEXT("Ballast_Fwd");
	GameMode->bSubmarineInApproachZone = true;

	GameMode->NotifyBreachStabilized(TEXT("Exterior_A"));

	TestEqual(TEXT("BreachCrisis should return to Approach when already in destination zone"), GameMode->GetRunPhase(), ESubRunPhase::Approach);
	TestFalse(TEXT("Breach objective should be cleared"), GameMode->bBreachObjectiveActive);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubRunPhaseNotifyBreachStabilizedMismatchedIdRejectedTest,
	"Sub3D.Submarine.RunPhase.NotifyBreachStabilizedMismatchedIdRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubRunPhaseNotifyBreachStabilizedMismatchedIdRejectedTest::RunTest(const FString& Parameters)
{
	ASubGameMode* GameMode = NewObject<ASubGameMode>(GetTransientPackage());
	TestNotNull(TEXT("GameMode should exist"), GameMode);

	if (!GameMode)
	{
		return false;
	}

	GameMode->AdvanceRunPhase(ESubRunPhase::Boarding);
	GameMode->AdvanceRunPhase(ESubRunPhase::Departure);
	GameMode->AdvanceRunPhase(ESubRunPhase::Traverse);
	GameMode->AdvanceRunPhase(ESubRunPhase::BreachCrisis);

	GameMode->bBreachObjectiveActive = true;
	GameMode->ActiveBreachSheetId = TEXT("Exterior_A");
	GameMode->ActiveBreachedCompartmentId = TEXT("Ballast_Fwd");

	GameMode->NotifyBreachStabilized(TEXT("Exterior_B"));

	TestEqual(TEXT("Mismatched breach stabilization should keep BreachCrisis active"), GameMode->GetRunPhase(), ESubRunPhase::BreachCrisis);
	TestTrue(TEXT("Breach objective should remain active"), GameMode->bBreachObjectiveActive);
	TestEqual(TEXT("Active breach sheet should remain unchanged"), GameMode->ActiveBreachSheetId, FName(TEXT("Exterior_A")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubGameStateDefaultPhaseTest,
	"Sub3D.Submarine.RunPhase.GameStateDefaultPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubGameStateDefaultPhaseTest::RunTest(const FString& Parameters)
{
	ASubGameState* GameState = NewObject<ASubGameState>(GetTransientPackage());
	TestNotNull(TEXT("GameState should exist"), GameState);

	if (!GameState)
	{
		return false;
	}

	TestEqual(TEXT("GameState default phase should be Boot"), GameState->GetCurrentPhase(), ESubRunPhase::Boot);

	GameState->ApplyRunPhase(ESubRunPhase::Traverse);
	TestEqual(TEXT("GameState ApplyRunPhase should update CurrentPhase"), GameState->GetCurrentPhase(), ESubRunPhase::Traverse);

	return true;
}

#endif
