#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "SubmarineAuthoringActors.h"
#include "SubmarineAuthoringAssets.h"
#include "SubmarineAuthoringPipeline.h"
#include "SubHullComponent.h"
#include "SubHullVisualDamageComponent.h"
#include "SubmarineCompartmentComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubmarineAuthoringBakeBasicTest,
	"Sub3D.Submarine.Authoring.BakeBasic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubmarineAuthoringBakeDoorBulkheadTest,
	"Sub3D.Submarine.Authoring.BakeDoorBulkhead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubmarineAuthoringBakeHatchConnectorTest,
	"Sub3D.Submarine.Authoring.BakeHatchConnector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubmarineAuthoringBakeHullDecksFirstTest,
	"Sub3D.Submarine.Authoring.BakeHullDecksFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubmarineAuthoringBakedRuntimeHullBridgeTest,
	"Sub3D.Submarine.Authoring.BakedRuntimeHullBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubmarineAuthoringBakedRuntimeDebugBreachTest,
	"Sub3D.Submarine.Authoring.BakedRuntimeDebugBreach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubmarineAuthoringBakeBasicTest::RunTest(const FString& Parameters)
{
	USubmarineAuthoringAsset* AuthoringAsset = NewObject<USubmarineAuthoringAsset>(GetTransientPackage());
	UCompiledSubmarineAsset* CompiledAsset = NewObject<UCompiledSubmarineAsset>(GetTransientPackage());
	TestNotNull(TEXT("Authoring asset should exist"), AuthoringAsset);
	TestNotNull(TEXT("Compiled asset should exist"), CompiledAsset);

	if (!AuthoringAsset || !CompiledAsset)
	{
		return false;
	}

	AuthoringAsset->SubmarineId = TEXT("TestSub");
	AuthoringAsset->Hull.LengthCm = 7200.f;
	AuthoringAsset->Hull.MaxOuterDiameterCm = 760.f;
	AuthoringAsset->Hull.WallThicknessCm = 12.f;

	FSubmarineDeckAuthoring MainDeck;
	MainDeck.DeckId = TEXT("MainDeck");
	MainDeck.LocalZCm = -40.f;
	AuthoringAsset->Decks.Add(MainDeck);

	FSubmarineDeckAuthoring UpperDeck;
	UpperDeck.DeckId = TEXT("UpperDeck");
	UpperDeck.LocalZCm = 120.f;
	AuthoringAsset->Decks.Add(UpperDeck);

	FSubmarineCompartmentAuthoring Helm;
	Helm.CompartmentId = TEXT("Helm");
	Helm.Type = ECompartmentType::Helm;
	Helm.TargetLengthCm = 2200.f;
	Helm.MinLengthCm = 1600.f;
	Helm.Priority = 0;
	AuthoringAsset->Compartments.Add(Helm);

	FSubmarineCompartmentAuthoring Engine;
	Engine.CompartmentId = TEXT("Engine");
	Engine.Type = ECompartmentType::Engine;
	Engine.TargetLengthCm = 2600.f;
	Engine.MinLengthCm = 1800.f;
	Engine.Priority = 1;
	AuthoringAsset->Compartments.Add(Engine);

	FSubmarineVerticalConnectorAuthoring Ramp;
	Ramp.ConnectorId = TEXT("Ramp_A");
	Ramp.FromDeckIndex = 0;
	Ramp.ToDeckIndex = 1;
	Ramp.LocalX = 3200.f;
	AuthoringAsset->VerticalConnectors.Add(Ramp);

	FSubmarineBulkheadConnectionAuthoring Bulkhead;
	Bulkhead.BoundaryId = TEXT("Helm_Engine_Bulkhead");
	Bulkhead.CompartmentA = TEXT("Helm");
	Bulkhead.CompartmentB = TEXT("Engine");
	AuthoringAsset->BulkheadConnections.Add(Bulkhead);

	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Authoring asset should validate"), USubmarineAuthoringBakeLibrary::ValidateAuthoringAsset(AuthoringAsset, Messages));
	TestTrue(TEXT("Bake should succeed"), USubmarineAuthoringBakeLibrary::BakeToCompiledAsset(AuthoringAsset, CompiledAsset, Messages));
	TestTrue(TEXT("Bake should generate render sections"), CompiledAsset->RenderSections.Num() >= 4);
	TestEqual(TEXT("Bake should resolve compartment count"), CompiledAsset->Compartments.Num(), 2);
	TestEqual(TEXT("Bake should resolve bulkhead connection count"), CompiledAsset->BulkheadConnections.Num(), 1);
	TestEqual(TEXT("Sealed bulkhead blocker id should match boundary id"), CompiledAsset->BulkheadConnections[0].BlockerSectionId, FName(TEXT("Helm_Engine_Bulkhead")));
	TestEqual(TEXT("Sealed bulkhead should not define openings"), CompiledAsset->BulkheadConnections[0].Openings.Num(), 0);
	TestTrue(TEXT("Bake should generate structural bindings"), CompiledAsset->StructuralBindings.Num() > 0);
	TestTrue(TEXT("Bake should generate walkable decks"), CompiledAsset->Collision.WalkableDeckSections.Num() >= 1);
	TestEqual(TEXT("Bake should generate one sealed bulkhead blocker"), CompiledAsset->Collision.BulkheadBlockerSections.Num(), 1);
	TestTrue(TEXT("Bake should generate fore hull closure"), CompiledAsset->RenderSections.ContainsByPredicate([](const FCompiledSubmarineMeshSection& Section)
	{
		return Section.SectionId == FName(TEXT("HullClosure_Fore")) && Section.Positions.Num() > 0 && Section.Indices.Num() > 0;
	}));
	TestTrue(TEXT("Bake should generate aft hull closure"), CompiledAsset->RenderSections.ContainsByPredicate([](const FCompiledSubmarineMeshSection& Section)
	{
		return Section.SectionId == FName(TEXT("HullClosure_Aft")) && Section.Positions.Num() > 0 && Section.Indices.Num() > 0;
	}));

	return true;
}

bool FSubmarineAuthoringBakeDoorBulkheadTest::RunTest(const FString& Parameters)
{
	USubmarineAuthoringAsset* AuthoringAsset = NewObject<USubmarineAuthoringAsset>(GetTransientPackage());
	UCompiledSubmarineAsset* CompiledAsset = NewObject<UCompiledSubmarineAsset>(GetTransientPackage());
	TestNotNull(TEXT("Authoring asset should exist"), AuthoringAsset);
	TestNotNull(TEXT("Compiled asset should exist"), CompiledAsset);

	if (!AuthoringAsset || !CompiledAsset)
	{
		return false;
	}

	AuthoringAsset->SubmarineId = TEXT("DoorSub");
	AuthoringAsset->Hull.LengthCm = 7200.f;
	AuthoringAsset->Hull.MaxOuterDiameterCm = 760.f;
	AuthoringAsset->Hull.WallThicknessCm = 12.f;

	FSubmarineDeckAuthoring MainDeck;
	MainDeck.DeckId = TEXT("MainDeck");
	MainDeck.LocalZCm = -40.f;
	AuthoringAsset->Decks.Add(MainDeck);

	FSubmarineDeckAuthoring UpperDeck;
	UpperDeck.DeckId = TEXT("UpperDeck");
	UpperDeck.LocalZCm = 120.f;
	AuthoringAsset->Decks.Add(UpperDeck);

	FSubmarineCompartmentAuthoring Helm;
	Helm.CompartmentId = TEXT("Helm");
	Helm.Type = ECompartmentType::Helm;
	Helm.TargetLengthCm = 2200.f;
	Helm.MinLengthCm = 1600.f;
	Helm.Priority = 0;
	AuthoringAsset->Compartments.Add(Helm);

	FSubmarineCompartmentAuthoring Engine;
	Engine.CompartmentId = TEXT("Engine");
	Engine.Type = ECompartmentType::Engine;
	Engine.TargetLengthCm = 2600.f;
	Engine.MinLengthCm = 1800.f;
	Engine.Priority = 1;
	AuthoringAsset->Compartments.Add(Engine);

	FSubmarineBulkheadConnectionAuthoring Bulkhead;
	Bulkhead.BoundaryId = TEXT("Helm_Engine_Door");
	Bulkhead.CompartmentA = TEXT("Helm");
	Bulkhead.CompartmentB = TEXT("Engine");

	FSubmarineBulkheadOpeningAuthoring LowerOpening;
	LowerOpening.OpeningId = TEXT("Helm_Engine_Door_Main");
	LowerOpening.DeckIndex = 0;
	LowerOpening.DoorSizeCm = FVector2D(90.f, 190.f);
	LowerOpening.bBlockedByDefault = true;
	Bulkhead.Openings.Add(LowerOpening);

	FSubmarineBulkheadOpeningAuthoring UpperOpening;
	UpperOpening.OpeningId = TEXT("Helm_Engine_Door_Upper");
	UpperOpening.DeckIndex = 1;
	UpperOpening.DoorSizeCm = FVector2D(90.f, 190.f);
	UpperOpening.bBlockedByDefault = true;
	Bulkhead.Openings.Add(UpperOpening);

	AuthoringAsset->BulkheadConnections.Add(Bulkhead);

	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Authoring asset should validate"), USubmarineAuthoringBakeLibrary::ValidateAuthoringAsset(AuthoringAsset, Messages));
	TestTrue(TEXT("Bake should succeed"), USubmarineAuthoringBakeLibrary::BakeToCompiledAsset(AuthoringAsset, CompiledAsset, Messages));
	TestEqual(TEXT("Bake should resolve one bulkhead connection"), CompiledAsset->BulkheadConnections.Num(), 1);
	TestEqual(TEXT("Bulkhead should compile two openings"), CompiledAsset->BulkheadConnections[0].Openings.Num(), 2);
	TestEqual(TEXT("Lower opening blocker id should be compiled"), CompiledAsset->BulkheadConnections[0].Openings[0].BlockerSectionId, FName(TEXT("Helm_Engine_Door_Main_DoorBlocker")));
	TestEqual(TEXT("Upper opening blocker id should be compiled"), CompiledAsset->BulkheadConnections[0].Openings[1].BlockerSectionId, FName(TEXT("Helm_Engine_Door_Upper_DoorBlocker")));
	TestEqual(TEXT("Lower opening sill should derive from main deck"), CompiledAsset->BulkheadConnections[0].Openings[0].DoorSillZCm, -40.f);
	TestEqual(TEXT("Upper opening sill should derive from upper deck"), CompiledAsset->BulkheadConnections[0].Openings[1].DoorSillZCm, 120.f);
	TestEqual(TEXT("Bake should generate wall blocker plus two door blockers"), CompiledAsset->Collision.BulkheadBlockerSections.Num(), 3);
	TestTrue(TEXT("Door blocker section should exist"), CompiledAsset->Collision.BulkheadBlockerSections.ContainsByPredicate([](const FCompiledSubmarineMeshSection& Section)
	{
		return Section.SectionId == FName(TEXT("Helm_Engine_Door_Main_DoorBlocker")) && Section.Positions.Num() > 0 && Section.Indices.Num() > 0;
	}));
	TestTrue(TEXT("Upper door blocker section should exist"), CompiledAsset->Collision.BulkheadBlockerSections.ContainsByPredicate([](const FCompiledSubmarineMeshSection& Section)
	{
		return Section.SectionId == FName(TEXT("Helm_Engine_Door_Upper_DoorBlocker")) && Section.Positions.Num() > 0 && Section.Indices.Num() > 0;
	}));
	TestTrue(TEXT("Door frame render section should exist"), CompiledAsset->RenderSections.ContainsByPredicate([](const FCompiledSubmarineMeshSection& Section)
	{
		return Section.SectionId == FName(TEXT("Helm_Engine_Door_Main_DoorFrame")) && Section.Positions.Num() > 0 && Section.Indices.Num() > 0;
	}));
	TestTrue(TEXT("Upper door frame render section should exist"), CompiledAsset->RenderSections.ContainsByPredicate([](const FCompiledSubmarineMeshSection& Section)
	{
		return Section.SectionId == FName(TEXT("Helm_Engine_Door_Upper_DoorFrame")) && Section.Positions.Num() > 0 && Section.Indices.Num() > 0;
	}));
	TestTrue(TEXT("Door leaf render section should exist"), CompiledAsset->RenderSections.ContainsByPredicate([](const FCompiledSubmarineMeshSection& Section)
	{
		return Section.SectionId == FName(TEXT("Helm_Engine_Door_Main_DoorLeaf")) && Section.Positions.Num() > 0 && Section.Indices.Num() > 0;
	}));
	TestTrue(TEXT("Upper door leaf render section should exist"), CompiledAsset->RenderSections.ContainsByPredicate([](const FCompiledSubmarineMeshSection& Section)
	{
		return Section.SectionId == FName(TEXT("Helm_Engine_Door_Upper_DoorLeaf")) && Section.Positions.Num() > 0 && Section.Indices.Num() > 0;
	}));

	return true;
}

bool FSubmarineAuthoringBakeHatchConnectorTest::RunTest(const FString& Parameters)
{
	USubmarineAuthoringAsset* AuthoringAsset = NewObject<USubmarineAuthoringAsset>(GetTransientPackage());
	UCompiledSubmarineAsset* CompiledAsset = NewObject<UCompiledSubmarineAsset>(GetTransientPackage());
	TestNotNull(TEXT("Authoring asset should exist"), AuthoringAsset);
	TestNotNull(TEXT("Compiled asset should exist"), CompiledAsset);

	if (!AuthoringAsset || !CompiledAsset)
	{
		return false;
	}

	AuthoringAsset->SubmarineId = TEXT("HatchSub");
	AuthoringAsset->Hull.LengthCm = 7200.f;
	AuthoringAsset->Hull.MaxOuterDiameterCm = 760.f;
	AuthoringAsset->Hull.WallThicknessCm = 12.f;

	FSubmarineDeckAuthoring LowerDeck;
	LowerDeck.DeckId = TEXT("LowerDeck");
	LowerDeck.LocalZCm = -40.f;
	AuthoringAsset->Decks.Add(LowerDeck);

	FSubmarineDeckAuthoring UpperDeck;
	UpperDeck.DeckId = TEXT("UpperDeck");
	UpperDeck.LocalZCm = 120.f;
	AuthoringAsset->Decks.Add(UpperDeck);

	FSubmarineVerticalOpeningAuthoring Opening;
	Opening.OpeningId = TEXT("CentralHatchOpening");
	Opening.Type = ESubmarineVerticalOpeningType::LadderOpening;
	Opening.FromDeckIndex = 0;
	Opening.ToDeckIndex = 1;
	Opening.LocalX = 3200.f;
	Opening.WidthCm = 120.f;
	Opening.LengthCm = 140.f;
	AuthoringAsset->VerticalOpenings.Add(Opening);

	FSubmarineVerticalConnectorAuthoring Hatch;
	Hatch.ConnectorId = TEXT("CentralHatch");
	Hatch.OpeningId = TEXT("CentralHatchOpening");
	Hatch.Type = ESubmarineVerticalConnectorType::LadderAnchor;
	AuthoringAsset->VerticalConnectors.Add(Hatch);

	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Authoring asset should validate"), USubmarineAuthoringBakeLibrary::ValidateAuthoringAsset(AuthoringAsset, Messages));
	TestTrue(TEXT("Bake should succeed"), USubmarineAuthoringBakeLibrary::BakeToCompiledAsset(AuthoringAsset, CompiledAsset, Messages));
	TestEqual(TEXT("Bake should create one fallback hull compartment"), CompiledAsset->Compartments.Num(), 1);
	TestEqual(TEXT("Fallback compartment id should be HullMain"), CompiledAsset->Compartments[0].CompartmentId, FName(TEXT("HullMain")));
	TestEqual(TEXT("Bake should resolve one vertical opening"), CompiledAsset->VerticalOpenings.Num(), 1);
	TestEqual(TEXT("Vertical opening id should match authoring"), CompiledAsset->VerticalOpenings[0].OpeningId, FName(TEXT("CentralHatchOpening")));
	TestEqual(TEXT("Bake should resolve one vertical connector"), CompiledAsset->VerticalConnectors.Num(), 1);
	TestEqual(TEXT("Connector type should be ladder anchor"), CompiledAsset->VerticalConnectors[0].Type, ESubmarineVerticalConnectorType::LadderAnchor);
	TestEqual(TEXT("Connector should reference the compiled opening id"), CompiledAsset->VerticalConnectors[0].OpeningId, FName(TEXT("CentralHatchOpening")));
	TestTrue(TEXT("Hatch render section should exist"), CompiledAsset->RenderSections.ContainsByPredicate([](const FCompiledSubmarineMeshSection& Section)
	{
		return Section.SectionId == FName(TEXT("CentralHatch")) && Section.Positions.Num() > 0 && Section.Indices.Num() > 0;
	}));
	TestEqual(TEXT("Hatch should not bake walkable ramp collision"), CompiledAsset->Collision.RampSections.Num(), 0);

	return true;
}

bool FSubmarineAuthoringBakeHullDecksFirstTest::RunTest(const FString& Parameters)
{
	USubmarineAuthoringAsset* AuthoringAsset = NewObject<USubmarineAuthoringAsset>(GetTransientPackage());
	UCompiledSubmarineAsset* CompiledAsset = NewObject<UCompiledSubmarineAsset>(GetTransientPackage());
	TestNotNull(TEXT("Authoring asset should exist"), AuthoringAsset);
	TestNotNull(TEXT("Compiled asset should exist"), CompiledAsset);

	if (!AuthoringAsset || !CompiledAsset)
	{
		return false;
	}

	AuthoringAsset->SubmarineId = TEXT("HullDecksFirstSub");
	AuthoringAsset->Hull.LengthCm = 8600.f;
	AuthoringAsset->Hull.MaxOuterDiameterCm = 820.f;
	AuthoringAsset->Hull.WallThicknessCm = 14.f;

	FSubmarineDeckAuthoring LowerDeck;
	LowerDeck.DeckId = TEXT("LowerDeck");
	LowerDeck.LocalZCm = -80.f;
	AuthoringAsset->Decks.Add(LowerDeck);

	FSubmarineDeckAuthoring MainDeck;
	MainDeck.DeckId = TEXT("MainDeck");
	MainDeck.LocalZCm = 80.f;
	AuthoringAsset->Decks.Add(MainDeck);

	FSubmarineVerticalOpeningAuthoring Opening;
	Opening.OpeningId = TEXT("CentralLadderOpening");
	Opening.Type = ESubmarineVerticalOpeningType::LadderOpening;
	Opening.FromDeckIndex = 0;
	Opening.ToDeckIndex = 1;
	Opening.LocalX = 4100.f;
	Opening.WidthCm = 120.f;
	Opening.LengthCm = 140.f;
	AuthoringAsset->VerticalOpenings.Add(Opening);

	FSubmarineVerticalConnectorAuthoring Ladder;
	Ladder.ConnectorId = TEXT("CentralLadderPlaceholder");
	Ladder.OpeningId = TEXT("CentralLadderOpening");
	Ladder.Type = ESubmarineVerticalConnectorType::LadderAnchor;
	AuthoringAsset->VerticalConnectors.Add(Ladder);

	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Hull/decks-first authoring should validate without explicit compartments"), USubmarineAuthoringBakeLibrary::ValidateAuthoringAsset(AuthoringAsset, Messages));
	TestTrue(TEXT("Hull/decks-first bake should succeed"), USubmarineAuthoringBakeLibrary::BakeToCompiledAsset(AuthoringAsset, CompiledAsset, Messages));
	TestEqual(TEXT("Bake should synthesize one fallback compartment"), CompiledAsset->Compartments.Num(), 1);
	TestEqual(TEXT("Synthesized compartment should be HullMain"), CompiledAsset->Compartments[0].CompartmentId, FName(TEXT("HullMain")));
	TestEqual(TEXT("Bake should compile one vertical opening"), CompiledAsset->VerticalOpenings.Num(), 1);
	TestEqual(TEXT("Compiled opening should keep deck range"), CompiledAsset->VerticalOpenings[0].ToDeckIndex, 1);
	TestEqual(TEXT("Bake should compile one vertical connector"), CompiledAsset->VerticalConnectors.Num(), 1);
	TestEqual(TEXT("Compiled connector should reference the opening"), CompiledAsset->VerticalConnectors[0].OpeningId, FName(TEXT("CentralLadderOpening")));
	TestTrue(TEXT("Bake should still generate walkable decks"), CompiledAsset->Collision.WalkableDeckSections.Num() >= 2);
	TestTrue(TEXT("Bake should generate a hatch placeholder render section"), CompiledAsset->RenderSections.ContainsByPredicate([](const FCompiledSubmarineMeshSection& Section)
	{
		return Section.SectionId == FName(TEXT("CentralLadderPlaceholder")) && Section.Positions.Num() > 0 && Section.Indices.Num() > 0;
	}));

	return true;
}

bool FSubmarineAuthoringBakedRuntimeHullBridgeTest::RunTest(const FString& Parameters)
{
	USubmarineAuthoringAsset* AuthoringAsset = NewObject<USubmarineAuthoringAsset>(GetTransientPackage());
	UCompiledSubmarineAsset* CompiledAsset = NewObject<UCompiledSubmarineAsset>(GetTransientPackage());
	TestNotNull(TEXT("Authoring asset should exist"), AuthoringAsset);
	TestNotNull(TEXT("Compiled asset should exist"), CompiledAsset);

	if (!AuthoringAsset || !CompiledAsset)
	{
		return false;
	}

	AuthoringAsset->SubmarineId = TEXT("RuntimeBridgeSub");
	AuthoringAsset->Hull.LengthCm = 7200.f;
	AuthoringAsset->Hull.MaxOuterDiameterCm = 760.f;
	AuthoringAsset->Hull.WallThicknessCm = 12.f;

	FSubmarineDeckAuthoring MainDeck;
	MainDeck.DeckId = TEXT("MainDeck");
	MainDeck.LocalZCm = -40.f;
	AuthoringAsset->Decks.Add(MainDeck);

	FSubmarineCompartmentAuthoring Helm;
	Helm.CompartmentId = TEXT("Helm");
	Helm.Type = ECompartmentType::Helm;
	Helm.TargetLengthCm = 2200.f;
	Helm.MinLengthCm = 1600.f;
	AuthoringAsset->Compartments.Add(Helm);

	FSubmarineCompartmentAuthoring Engine;
	Engine.CompartmentId = TEXT("Engine");
	Engine.Type = ECompartmentType::Engine;
	Engine.TargetLengthCm = 2600.f;
	Engine.MinLengthCm = 1800.f;
	AuthoringAsset->Compartments.Add(Engine);

	FSubmarineBulkheadConnectionAuthoring Bulkhead;
	Bulkhead.BoundaryId = TEXT("Helm_Engine_Door");
	Bulkhead.CompartmentA = TEXT("Helm");
	Bulkhead.CompartmentB = TEXT("Engine");

	FSubmarineBulkheadOpeningAuthoring Opening;
	Opening.OpeningId = TEXT("Helm_Engine_Opening");
	Opening.DeckIndex = 0;
	Opening.DoorSizeCm = FVector2D(90.f, 190.f);
	Opening.bBlockedByDefault = true;
	Bulkhead.Openings.Add(Opening);
	AuthoringAsset->BulkheadConnections.Add(Bulkhead);

	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Bake should succeed"), USubmarineAuthoringBakeLibrary::BakeToCompiledAsset(AuthoringAsset, CompiledAsset, Messages));

	ASubmarineBakedRuntimeActor* RuntimeActor = NewObject<ASubmarineBakedRuntimeActor>(GetTransientPackage());
	TestNotNull(TEXT("Runtime actor should exist"), RuntimeActor);
	if (!RuntimeActor)
	{
		return false;
	}

	RuntimeActor->CompiledAsset = CompiledAsset;
	TestTrue(TEXT("Runtime actor should build from compiled asset"), RuntimeActor->BuildFromCompiledAsset());
	TestNotNull(TEXT("Runtime actor should keep a transient layout asset on SubHull"), RuntimeActor->SubHull ? RuntimeActor->SubHull->LayoutAsset.Get() : nullptr);
	TestTrue(TEXT("SubHull should receive structural sheets from baked asset"), RuntimeActor->SubHull && RuntimeActor->SubHull->GetStructuralSheets().Num() > 0);
	FDoorState DoorState;
	TestTrue(TEXT("Compartments should register baked opening as a door state"), RuntimeActor->Compartments && RuntimeActor->Compartments->TryGetDoorState(TEXT("Helm_Engine_Opening"), DoorState));
	TestTrue(TEXT("Baked opening should start closed when blocked by default"), DoorState.bClosed);

	return true;
}

bool FSubmarineAuthoringBakedRuntimeDebugBreachTest::RunTest(const FString& Parameters)
{
	USubmarineAuthoringAsset* AuthoringAsset = NewObject<USubmarineAuthoringAsset>(GetTransientPackage());
	UCompiledSubmarineAsset* CompiledAsset = NewObject<UCompiledSubmarineAsset>(GetTransientPackage());
	TestNotNull(TEXT("Authoring asset should exist"), AuthoringAsset);
	TestNotNull(TEXT("Compiled asset should exist"), CompiledAsset);

	if (!AuthoringAsset || !CompiledAsset)
	{
		return false;
	}

	AuthoringAsset->SubmarineId = TEXT("DebugBreachSub");
	AuthoringAsset->Hull.LengthCm = 7200.f;
	AuthoringAsset->Hull.MaxOuterDiameterCm = 760.f;
	AuthoringAsset->Hull.WallThicknessCm = 12.f;

	FSubmarineDeckAuthoring MainDeck;
	MainDeck.DeckId = TEXT("MainDeck");
	MainDeck.LocalZCm = -40.f;
	AuthoringAsset->Decks.Add(MainDeck);

	FSubmarineCompartmentAuthoring Helm;
	Helm.CompartmentId = TEXT("Helm");
	Helm.Type = ECompartmentType::Helm;
	Helm.TargetLengthCm = 2200.f;
	Helm.MinLengthCm = 1600.f;
	AuthoringAsset->Compartments.Add(Helm);

	FSubmarineCompartmentAuthoring Engine;
	Engine.CompartmentId = TEXT("Engine");
	Engine.Type = ECompartmentType::Engine;
	Engine.TargetLengthCm = 2600.f;
	Engine.MinLengthCm = 1800.f;
	AuthoringAsset->Compartments.Add(Engine);

	TArray<FLayoutValidationMessage> Messages;
	TestTrue(TEXT("Bake should succeed"), USubmarineAuthoringBakeLibrary::BakeToCompiledAsset(AuthoringAsset, CompiledAsset, Messages));

	ASubmarineBakedRuntimeActor* RuntimeActor = NewObject<ASubmarineBakedRuntimeActor>(GetTransientPackage());
	TestNotNull(TEXT("Runtime actor should exist"), RuntimeActor);
	if (!RuntimeActor)
	{
		return false;
	}

	RuntimeActor->CompiledAsset = CompiledAsset;
	TestTrue(TEXT("Runtime actor should build from compiled asset"), RuntimeActor->BuildFromCompiledAsset());
	TestTrue(TEXT("Runtime actor should create a debug breach"), RuntimeActor->CreateDebugBreachOnFirstExteriorSheet(150.f));
	TestTrue(TEXT("SubHull should now expose at least one breach cluster"), RuntimeActor->SubHull && RuntimeActor->SubHull->GetBreachClusters().Num() > 0);
	TestTrue(TEXT("Hull visual damage component should exist"), RuntimeActor->HullVisualDamage != nullptr);

	if (!RuntimeActor->HullVisualDamage)
	{
		return false;
	}

	RuntimeActor->HullVisualDamage->RefreshFromCurrentBreaches();
	TestTrue(TEXT("Hull visual damage should expose at least one active visual breach"), RuntimeActor->HullVisualDamage->GetActiveBreachVisuals().Num() > 0);

	return true;
}

#endif
