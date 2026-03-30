#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubmarineFloodAudioAnchor.generated.h"

class UAudioComponent;
class USceneComponent;
class USoundBase;

UCLASS(Blueprintable)
class SUB3D_API ASubmarineFloodAudioAnchor : public AActor
{
	GENERATED_BODY()

public:
	ASubmarineFloodAudioAnchor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* FloodAudio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flood")
	FName TargetCompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flood")
	TObjectPtr<USoundBase> FloodSoundOverride = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Flood")
	void SetFloodState(
		bool bActive,
		float WaterLevel01,
		float Turbulence01,
		float GainInput,
		USoundBase* FallbackSound,
		FName WaterLevelParameter,
		FName TurbulenceParameter,
		FName GainParameter);
};
