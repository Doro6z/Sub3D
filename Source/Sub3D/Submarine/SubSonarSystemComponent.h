#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubSonarV2Types.h"
#include "SubSonarSystemComponent.generated.h"

class USonarContactTrackerComponent;
class USonarSystemConfigData;
class USubSonarComponent;

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubSonarSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubSonarSystemComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sonar|Config")
	TObjectPtr<USonarSystemConfigData> SystemConfig = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Runtime")
	bool bEnablePassiveSweep = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Runtime")
	bool bEnableRouteCoarseSeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Runtime")
	bool bEnableDebugLogs = false;

	UPROPERTY(ReplicatedUsing = OnRep_RuntimeState, BlueprintReadOnly, Category = "Sonar|Runtime")
	ESonarMode CurrentMode = ESonarMode::PassiveStandard;

	UPROPERTY(ReplicatedUsing = OnRep_RuntimeState, BlueprintReadOnly, Category = "Sonar|Runtime")
	int32 RangePresetIndex = 1;

	UPROPERTY(ReplicatedUsing = OnRep_RuntimeState, BlueprintReadOnly, Category = "Sonar|Runtime")
	float FocusBearingDeg = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_RuntimeState, BlueprintReadOnly, Category = "Sonar|Runtime")
	FSonarSelfNoiseState SelfNoiseState;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Sonar|Runtime")
	float LastProcessedPingTimestamp = -1000.f;

	UPROPERTY(ReplicatedUsing = OnRep_Tracks, BlueprintReadOnly, Category = "Sonar|Tracks")
	TArray<FSonarTrack> ReplicatedTracks;

	UPROPERTY(ReplicatedUsing = OnRep_TopoWindow, BlueprintReadOnly, Category = "Sonar|Topology")
	TArray<FSonarTopoCell> ReplicatedTopoWindow;

	UFUNCTION(BlueprintCallable, Category = "Sonar|Runtime")
	void SetSonarMode(ESonarMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Sonar|Runtime")
	void SetFocusBearing(float NewBearingDeg);

	UFUNCTION(BlueprintCallable, Category = "Sonar|Runtime")
	void SetRangePresetIndex(int32 NewIndex);

	UFUNCTION(BlueprintCallable, Category = "Sonar|Tracks")
	void MarkPriorityTrack(int32 TrackId, bool bPriority);

	UFUNCTION(BlueprintPure, Category = "Sonar|Runtime")
	float GetCurrentRangeCm() const;

	UFUNCTION(BlueprintPure, Category = "Sonar|Runtime")
	bool IsPingReady() const;

	const TArray<FSonarTrack>& GetTracks() const
	{
		return ReplicatedTracks;
	}

	const TArray<FSonarTopoCell>& GetTopoCells() const
	{
		return ReplicatedTopoWindow;
	}

	UFUNCTION(BlueprintCallable, Category = "Sonar|Topology")
	void ForceRebuildTopologyFromRoute();

	UFUNCTION(BlueprintImplementableEvent, Category = "Sonar")
	void BP_OnSonarSystemUpdated();

protected:
	UFUNCTION()
	void OnRep_RuntimeState();

	UFUNCTION()
	void OnRep_Tracks();

	UFUNCTION()
	void OnRep_TopoWindow();

private:
	struct FRuntimeTopoCell
	{
		float HeightCm = 0.f;
		float Occupancy01 = 0.f;
		float Confidence01 = 0.f;
		float LastUpdateTime = 0.f;
		bool bFromSeed = false;
	};

	void EnsureTrackerComponent();
	void UpdateSelfNoise(float DeltaTime);
	void RunPassiveSweep();
	void RunTerrainSweep();
	void ProcessNewActivePing();
	void AddTopologyObservation(const FVector& WorldLocation, float Occupancy01, float Confidence01, bool bFromSeed);
	void AgeAndPruneTopology(float DeltaTime);
	void RefreshReplicatedTracks();
	void RefreshReplicatedTopoWindow();
	void ApplyAcousticVolumeModifiers(float& OutAmbientNoiseBias, float& OutClutterBias, float& OutPassiveModifier) const;
	void SeedTopologyFromRoute();
	void NotifyRuntimeUpdated();
	static int64 MakeTopoKey(int32 GridX, int32 GridY);

	UPROPERTY(Transient)
	TObjectPtr<USubSonarComponent> CachedSonar = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USonarContactTrackerComponent> ContactTracker = nullptr;

	TMap<int64, FRuntimeTopoCell> RuntimeTopoCells;
	float PassiveSweepAccumulator = 0.f;
	float TrackerUpdateAccumulator = 0.f;
	float NoiseUpdateAccumulator = 0.f;
	bool bRouteSeedApplied = false;
};
