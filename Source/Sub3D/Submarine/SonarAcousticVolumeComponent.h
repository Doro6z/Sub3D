#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SonarAcousticVolumeComponent.generated.h"

UCLASS(ClassGroup = (Sonar), meta = (BlueprintSpawnableComponent))
class SUB3D_API USonarAcousticVolumeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USonarAcousticVolumeComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	bool bVolumeEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float AmbientNoiseBias = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float ClutterBias = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float PassiveDetectionModifier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float ActivePingDistortion = 0.f;

	UFUNCTION(BlueprintPure, Category = "Sonar")
	bool IsLocationInside(const FVector& WorldLocation) const;
};
