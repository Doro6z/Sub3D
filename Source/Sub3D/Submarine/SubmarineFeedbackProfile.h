#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SubmarineFeedbackProfile.generated.h"

class UCameraShakeBase;
class USoundBase;

UCLASS(BlueprintType)
class SUB3D_API USubmarineFeedbackProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Policy")
	bool bRestrictInteriorFeedbackToEmbarkedCrew = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|HullImpact")
	TSubclassOf<UCameraShakeBase> HullImpactShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|HullImpact")
	TObjectPtr<USoundBase> HullImpactSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|HullImpact", meta = (ClampMin = "0.0"))
	float HullImpactInnerRadiusCm = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|HullImpact", meta = (ClampMin = "0.0"))
	float HullImpactOuterRadiusCm = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|HullImpact", meta = (ClampMin = "1.0"))
	float HullImpactShakeDamageDivisor = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|HullImpact", meta = (ClampMin = "0.0"))
	float HullImpactMinShakeScale = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|HullImpact", meta = (ClampMin = "0.0"))
	float HullImpactMaxShakeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|HullImpact", meta = (ClampMin = "1.0"))
	float HullImpactSoundDamageDivisor = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|HullImpact", meta = (ClampMin = "0.0"))
	float HullImpactMinSoundVolume = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|HullImpact", meta = (ClampMin = "0.0"))
	float HullImpactMaxSoundVolume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Alarm")
	TObjectPtr<USoundBase> AlarmSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Alarm", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AlarmFloodThreshold01 = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Alarm", meta = (ClampMin = "1"))
	int32 AlarmTriggerPeriodSeconds = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Alarm", meta = (ClampMin = "0.0"))
	float AlarmGain = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Alarm")
	FName AlarmTriggerPeriodParameter = TEXT("TriggerPeriod");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Alarm")
	FName AlarmGainParameter = TEXT("GainInput");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Leak")
	TObjectPtr<USoundBase> LeakSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Leak", meta = (ClampMin = "0.1"))
	float LeakRadiusToRateDivisorCm = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Leak", meta = (ClampMin = "0.1"))
	float LeakForceToPressureDivisor = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Leak", meta = (ClampMin = "0.0"))
	float LeakBaseGain = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Leak", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxRuntimeLeakSources = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Leak")
	FName LeakRateParameter = TEXT("LeakRate01");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Leak")
	FName LeakPressureParameter = TEXT("Pressure01");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|Leak")
	FName LeakGainParameter = TEXT("GainInput");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|FloodInterior")
	TObjectPtr<USoundBase> FloodInteriorSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|FloodInterior", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FloodInteriorActivationThreshold01 = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|FloodInterior", meta = (ClampMin = "0.0"))
	float FloodInteriorBaseGain = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|FloodInterior")
	FName FloodInteriorWaterLevelParameter = TEXT("WaterLevel01");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|FloodInterior")
	FName FloodInteriorTurbulenceParameter = TEXT("Turbulence01");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback|FloodInterior")
	FName FloodInteriorGainParameter = TEXT("GainInput");
};
