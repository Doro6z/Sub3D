#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructuralHullTypes.h"
#include "SubmarineFeedbackDirectorComponent.generated.h"

class APlayerController;
class ASubmarineAlarmBeacon;
class ASubmarineBase;
class ASubmarineFloodAudioAnchor;
class UAudioComponent;
class USubFloodComponent;
class USubHullComponent;
class USubmarineFeedbackProfile;

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubmarineFeedbackDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubmarineFeedbackDirectorComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Submarine|Feedback")
	TObjectPtr<USubmarineFeedbackProfile> FeedbackProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Submarine|Feedback|Debug")
	bool bLogFeedbackDispatch = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Submarine|Feedback|Alarm")
	bool bUseFallbackAlarmAudioWhenNoBeacon = true;

	UFUNCTION(BlueprintCallable, Category = "Submarine|Feedback")
	void DispatchHullImpactFeedback(const FVector& WorldLocation, float Damage, float RadiusCm);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Submarine|Feedback|Alarm")
	void RefreshAlarmBeacons();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Submarine|Feedback|Audio")
	void RefreshSpatialAudioAnchors();

	UFUNCTION(BlueprintPure, Category = "Submarine|Feedback")
	USubmarineFeedbackProfile* GetFeedbackProfile() const { return FeedbackProfile; }

	const TArray<TObjectPtr<ASubmarineAlarmBeacon>>& GetAlarmBeacons() const { return AlarmBeacons; }

	static float ComputeDistanceAlpha(float DistanceCm, float InnerRadiusCm, float OuterRadiusCm);

private:
	UFUNCTION()
	void HandleSubFloodUpdated(const TArray<FCompartmentState>& InStates);

	UFUNCTION()
	void HandleFloodInitialized();

	UFUNCTION()
	void HandleBreachesUpdated(const TArray<FBreachClusterState>& Breaches);

	UFUNCTION()
	void HandleFlowFieldsUpdated(const TArray<FBreachFlowField>& FlowFields);

	void ActivateSubFloodPath();
	void EnsureFallbackAlarmAudio();
	void EnsureLeakAudioPoolSize(int32 DesiredCount);
	void UpdateFloodAlarmFromStates(const TArray<FCompartmentState>& States);
	void UpdateAlarmBeacons(bool bAlarmActive, float Severity01);
	void UpdateLeakAudioRuntime();
	void UpdateFloodInteriorAudioAnchorsFromStates(const TArray<FCompartmentState>& States);
	ASubmarineBase* GetOwningSubmarine() const;
	USubHullComponent* GetOwningHull() const;
	bool IsControllerEligible(const APlayerController* PlayerController) const;
	FName FindCompartmentIdForSheet(FName SheetId) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<USubFloodComponent> SubFlood = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FallbackAlarmAudio = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASubmarineAlarmBeacon>> AlarmBeacons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> RuntimeLeakAudioComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASubmarineFloodAudioAnchor>> FloodAudioAnchors;

	UPROPERTY(Transient)
	TArray<FBreachClusterState> LatestBreaches;

	UPROPERTY(Transient)
	TArray<FBreachFlowField> LatestFlowFields;

	bool bFloodAlarmActive = false;
};
