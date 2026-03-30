#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubmarineAlarmBeacon.generated.h"

class UAudioComponent;
class UPointLightComponent;
class URotatingMovementComponent;
class USceneComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS(Blueprintable)
class SUB3D_API ASubmarineAlarmBeacon : public AActor
{
	GENERATED_BODY()

public:
	ASubmarineAlarmBeacon();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BeaconMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPointLightComponent* AlarmLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* AlarmAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	URotatingMovementComponent* RotatingMovement;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm|Visual")
	bool bUseAlarmLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm|Visual", meta = (ClampMin = "0.0"))
	float ActiveLightIntensity = 9000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm|Visual", meta = (ClampMin = "0.0"))
	float IdleLightIntensity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm|Visual")
	FLinearColor ActiveLightColor = FLinearColor(1.f, 0.1f, 0.1f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm|Visual")
	bool bRotateWhenActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm|Visual")
	FRotator ActiveRotationRate = FRotator(0.f, 240.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alarm|Audio")
	TObjectPtr<USoundBase> AlarmSoundOverride = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Alarm")
	void SetAlarmState(
		bool bActive,
		float Severity01,
		USoundBase* FallbackSound,
		int32 TriggerPeriodSeconds,
		float GainInput,
		FName TriggerPeriodParameter,
		FName GainParameter);

	UFUNCTION(BlueprintPure, Category = "Alarm")
	bool IsAlarmActive() const { return bAlarmActive; }

private:
	void ApplyVisualState(bool bActive, float Severity01);

private:
	bool bAlarmActive = false;
};
