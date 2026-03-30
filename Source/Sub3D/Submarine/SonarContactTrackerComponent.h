#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubSonarV2Types.h"
#include "SonarContactTrackerComponent.generated.h"

UCLASS(ClassGroup = (Sonar), meta = (BlueprintSpawnableComponent))
class SUB3D_API USonarContactTrackerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USonarContactTrackerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Tracker", meta = (ClampMin = "100.0"))
	float MergeDistanceCm = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Tracker", meta = (ClampMin = "0.0"))
	float DecayPerSecond = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Tracker", meta = (ClampMin = "0.0"))
	float LostThresholdS = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Tracker", meta = (ClampMin = "0.0"))
	float LostRetentionS = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Tracker", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClassifiedThreshold = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Tracker", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ConfirmedThreshold = 0.8f;

	UFUNCTION(BlueprintCallable, Category = "Sonar|Tracker")
	void ResetTracker();

	UFUNCTION(BlueprintCallable, Category = "Sonar|Tracker")
	void ConsumeSamples(const TArray<FSonarDetectionSample>& Samples, float WorldTimeSeconds, const FVector& SubmarineLocation);

	UFUNCTION(BlueprintCallable, Category = "Sonar|Tracker")
	void UpdateTracker(float DeltaSeconds, float WorldTimeSeconds, const FVector& SubmarineLocation);

	UFUNCTION(BlueprintCallable, Category = "Sonar|Tracker")
	void MarkPriorityTrack(int32 TrackId, bool bPriority);

	const TArray<FSonarTrack>& GetTracks() const
	{
		return Tracks;
	}

private:
	int32 FindBestTrackIndex(const FSonarDetectionSample& Sample) const;
	void UpdateTrackFromSample(FSonarTrack& Track, const FSonarDetectionSample& Sample, float WorldTimeSeconds, const FVector& SubmarineLocation);
	static ESonarTrackState ResolveTrackState(float Confidence, float SecondsSinceUpdate, float LostThreshold, float ClassifiedThreshold, float ConfirmedThreshold);

	UPROPERTY()
	TArray<FSonarTrack> Tracks;

	int32 NextTrackId = 1;
};
