#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubSonarV2Types.h"
#include "SonarNoiseEmitterComponent.generated.h"

UCLASS(ClassGroup = (Sonar), meta = (BlueprintSpawnableComponent))
class SUB3D_API USonarNoiseEmitterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USonarNoiseEmitterComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	bool bEmitterEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	ESonarContactClass ContactClass = ESonarContactClass::MobileUnknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "0.0"))
	float BaseNoiseStrength = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "0.0"))
	float ActiveReflectivity = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	bool bLikelyHostile = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "0.0"))
	float VelocityNoiseScale = 0.0025f;

	UFUNCTION(BlueprintPure, Category = "Sonar")
	bool IsSonarRelevant() const { return bEmitterEnabled; }

	UFUNCTION(BlueprintPure, Category = "Sonar")
	float GetCurrentNoiseStrength() const;

	UFUNCTION(BlueprintPure, Category = "Sonar")
	float GetActiveReflectivity() const { return ActiveReflectivity; }

	UFUNCTION(BlueprintPure, Category = "Sonar")
	FVector GetEmissionLocation() const;
};
