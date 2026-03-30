#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "SonarContactTrackerComponent.h"

namespace
{
FSonarDetectionSample MakeSample(const FVector& Location, float ConfidenceDelta, ESonarContactClass ContactClass = ESonarContactClass::MobileUnknown)
{
	FSonarDetectionSample Sample;
	Sample.EstimatedWorldLocation = Location;
	Sample.BearingDeg = 0.f;
	Sample.EstimatedDistanceCm = Location.Size();
	Sample.RawStrength = ConfidenceDelta;
	Sample.ConfidenceDelta = ConfidenceDelta;
	Sample.SuggestedClass = ContactClass;
	Sample.Timestamp = 1.f;
	return Sample;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubSonarTrackerStateTransitionsTest,
	"Sub3D.Submarine.SonarTracker.StateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubSonarTrackerStateTransitionsTest::RunTest(const FString& Parameters)
{
	USonarContactTrackerComponent* Tracker = NewObject<USonarContactTrackerComponent>();
	TestNotNull(TEXT("Tracker should be created"), Tracker);
	if (!Tracker)
	{
		return false;
	}

	TArray<FSonarDetectionSample> Samples;
	Samples.Add(MakeSample(FVector(2000.f, 0.f, 0.f), 0.7f, ESonarContactClass::MobileThreat));
	Tracker->ConsumeSamples(Samples, 1.f, FVector::ZeroVector);

	const TArray<FSonarTrack>& Tracks = Tracker->GetTracks();
	TestEqual(TEXT("One sample should create one track"), Tracks.Num(), 1);
	if (Tracks.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("High confidence sample should produce Confirmed state"), Tracks[0].State, ESonarTrackState::Confirmed);
	TestTrue(TEXT("MobileThreat sample should flag likely hostile"), Tracks[0].bLikelyHostile);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubSonarTrackerDecayToLostTest,
	"Sub3D.Submarine.SonarTracker.DecayToLost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubSonarTrackerDecayToLostTest::RunTest(const FString& Parameters)
{
	USonarContactTrackerComponent* Tracker = NewObject<USonarContactTrackerComponent>();
	TestNotNull(TEXT("Tracker should be created"), Tracker);
	if (!Tracker)
	{
		return false;
	}

	Tracker->LostThresholdS = 2.f;
	Tracker->LostRetentionS = 3.f;
	Tracker->DecayPerSecond = 0.25f;

	TArray<FSonarDetectionSample> Samples;
	Samples.Add(MakeSample(FVector(3000.f, 0.f, 0.f), 0.45f));
	Tracker->ConsumeSamples(Samples, 1.f, FVector::ZeroVector);
	Tracker->UpdateTracker(2.2f, 3.2f, FVector::ZeroVector);

	const TArray<FSonarTrack>& Tracks = Tracker->GetTracks();
	TestEqual(TEXT("Track should still exist before retention timeout"), Tracks.Num(), 1);
	if (Tracks.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("Track should move to Lost after threshold"), Tracks[0].State, ESonarTrackState::Lost);

	Tracker->UpdateTracker(3.2f, 6.4f, FVector::ZeroVector);
	TestEqual(TEXT("Lost track should be removed after retention"), Tracker->GetTracks().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubSonarTrackerFusionTest,
	"Sub3D.Submarine.SonarTracker.SampleFusion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubSonarTrackerFusionTest::RunTest(const FString& Parameters)
{
	USonarContactTrackerComponent* Tracker = NewObject<USonarContactTrackerComponent>();
	TestNotNull(TEXT("Tracker should be created"), Tracker);
	if (!Tracker)
	{
		return false;
	}

	Tracker->MergeDistanceCm = 1500.f;

	TArray<FSonarDetectionSample> FirstBurst;
	FirstBurst.Add(MakeSample(FVector(6000.f, 100.f, 0.f), 0.35f));
	FirstBurst.Add(MakeSample(FVector(6100.f, 120.f, 0.f), 0.30f));
	Tracker->ConsumeSamples(FirstBurst, 1.f, FVector::ZeroVector);
	TestEqual(TEXT("Nearby samples should fuse into one track"), Tracker->GetTracks().Num(), 1);

	TArray<FSonarDetectionSample> SecondBurst;
	SecondBurst.Add(MakeSample(FVector(12000.f, 0.f, 0.f), 0.35f));
	Tracker->ConsumeSamples(SecondBurst, 2.f, FVector::ZeroVector);
	TestEqual(TEXT("Far sample should create a second track"), Tracker->GetTracks().Num(), 2);
	return true;
}

#endif
