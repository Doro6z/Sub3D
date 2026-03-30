#include "SonarContactTrackerComponent.h"

USonarContactTrackerComponent::USonarContactTrackerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USonarContactTrackerComponent::ResetTracker()
{
	Tracks.Reset();
	NextTrackId = 1;
}

void USonarContactTrackerComponent::ConsumeSamples(const TArray<FSonarDetectionSample>& Samples, float WorldTimeSeconds, const FVector& SubmarineLocation)
{
	for (const FSonarDetectionSample& Sample : Samples)
	{
		const int32 TrackIndex = FindBestTrackIndex(Sample);
		if (TrackIndex != INDEX_NONE)
		{
			UpdateTrackFromSample(Tracks[TrackIndex], Sample, WorldTimeSeconds, SubmarineLocation);
			continue;
		}

		FSonarTrack& NewTrack = Tracks.AddDefaulted_GetRef();
		NewTrack.TrackId = NextTrackId++;
		NewTrack.ProbableClass = Sample.SuggestedClass;
		NewTrack.EstimatedWorldLocation = Sample.EstimatedWorldLocation;
		NewTrack.EstimatedVelocity = FVector::ZeroVector;
		NewTrack.Confidence = FMath::Clamp(0.15f + Sample.ConfidenceDelta, 0.f, 1.f);
		NewTrack.LastUpdateTime = WorldTimeSeconds;
		NewTrack.LostTime = 0.f;
		NewTrack.BearingDeg = Sample.BearingDeg;
		NewTrack.EstimatedDistanceCm = FVector::Distance(SubmarineLocation, Sample.EstimatedWorldLocation);
		NewTrack.bLikelyHostile = Sample.SuggestedClass == ESonarContactClass::MobileThreat;
		NewTrack.State = ResolveTrackState(NewTrack.Confidence, 0.f, LostThresholdS, ClassifiedThreshold, ConfirmedThreshold);
	}
}

void USonarContactTrackerComponent::UpdateTracker(float DeltaSeconds, float WorldTimeSeconds, const FVector& SubmarineLocation)
{
	const float SafeDeltaSeconds = FMath::Max(DeltaSeconds, 0.f);
	for (int32 Index = Tracks.Num() - 1; Index >= 0; --Index)
	{
		FSonarTrack& Track = Tracks[Index];
		const float SinceLastUpdate = FMath::Max(0.f, WorldTimeSeconds - Track.LastUpdateTime);
		const bool bIsAging = SinceLastUpdate > KINDA_SMALL_NUMBER;
		if (bIsAging)
		{
			Track.Confidence = FMath::Clamp(Track.Confidence - DecayPerSecond * SafeDeltaSeconds, 0.f, 1.f);
		}

		Track.EstimatedDistanceCm = FVector::Distance(SubmarineLocation, FVector(Track.EstimatedWorldLocation));
		const FVector ToTrack = FVector(Track.EstimatedWorldLocation) - SubmarineLocation;
		Track.BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(ToTrack.Y, ToTrack.X));
		Track.State = ResolveTrackState(Track.Confidence, SinceLastUpdate, LostThresholdS, ClassifiedThreshold, ConfirmedThreshold);
		if (Track.State == ESonarTrackState::Lost)
		{
			Track.LostTime += SafeDeltaSeconds;
			if (Track.LostTime >= LostRetentionS)
			{
				Tracks.RemoveAtSwap(Index);
			}
		}
		else
		{
			Track.LostTime = 0.f;
		}
	}
}

void USonarContactTrackerComponent::MarkPriorityTrack(int32 TrackId, bool bPriority)
{
	for (FSonarTrack& Track : Tracks)
	{
		if (Track.TrackId == TrackId)
		{
			Track.bPriority = bPriority;
			break;
		}
	}
}

int32 USonarContactTrackerComponent::FindBestTrackIndex(const FSonarDetectionSample& Sample) const
{
	int32 BestIndex = INDEX_NONE;
	float BestDistSq = TNumericLimits<float>::Max();
	const float MergeDistanceSq = FMath::Square(FMath::Max(100.f, MergeDistanceCm));

	for (int32 Index = 0; Index < Tracks.Num(); ++Index)
	{
		const FSonarTrack& Track = Tracks[Index];
		if (Track.State == ESonarTrackState::Lost && Track.LostTime >= LostRetentionS)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(FVector(Track.EstimatedWorldLocation), Sample.EstimatedWorldLocation);
		if (DistSq <= MergeDistanceSq && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = Index;
		}
	}

	return BestIndex;
}

void USonarContactTrackerComponent::UpdateTrackFromSample(FSonarTrack& Track, const FSonarDetectionSample& Sample, float WorldTimeSeconds, const FVector& SubmarineLocation)
{
	const FVector PreviousLocation = FVector(Track.EstimatedWorldLocation);
	const float DeltaTime = FMath::Max(WorldTimeSeconds - Track.LastUpdateTime, 0.01f);
	const float Blend = Sample.bFromActivePing ? 0.6f : 0.35f;
	const FVector NewLocation = FMath::Lerp(PreviousLocation, Sample.EstimatedWorldLocation, Blend);
	Track.EstimatedVelocity = (NewLocation - PreviousLocation) / DeltaTime;
	Track.EstimatedWorldLocation = NewLocation;
	Track.LastUpdateTime = WorldTimeSeconds;
	Track.Confidence = FMath::Clamp(Track.Confidence + FMath::Max(0.05f, Sample.ConfidenceDelta), 0.f, 1.f);
	Track.BearingDeg = Sample.BearingDeg;
	Track.EstimatedDistanceCm = FVector::Distance(SubmarineLocation, NewLocation);
	Track.ProbableClass = (Sample.SuggestedClass != ESonarContactClass::Unknown) ? Sample.SuggestedClass : Track.ProbableClass;
	Track.bLikelyHostile = Track.bLikelyHostile || (Track.ProbableClass == ESonarContactClass::MobileThreat);
	Track.State = ResolveTrackState(Track.Confidence, 0.f, LostThresholdS, ClassifiedThreshold, ConfirmedThreshold);
	Track.LostTime = 0.f;
}

ESonarTrackState USonarContactTrackerComponent::ResolveTrackState(
	float Confidence,
	float SecondsSinceUpdate,
	float LostThreshold,
	float ClassifiedThreshold,
	float ConfirmedThreshold)
{
	if (SecondsSinceUpdate >= LostThreshold)
	{
		return ESonarTrackState::Lost;
	}

	if (Confidence >= ConfirmedThreshold)
	{
		return ESonarTrackState::Confirmed;
	}

	if (Confidence >= ClassifiedThreshold)
	{
		return ESonarTrackState::Classified;
	}

	if (Confidence >= 0.28f)
	{
		return ESonarTrackState::Tracked;
	}

	return ESonarTrackState::Suspected;
}
