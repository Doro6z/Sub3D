#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineRuntimeTypes.h"
#include "SubmarineRadarComponent.generated.h"

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubmarineRadarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubmarineRadarComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Submarine|Radar")
	void ForcePing();

	UFUNCTION(BlueprintPure, Category = "Submarine|Radar")
	const TArray<FRadarContact>& GetContacts() const { return Contacts; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Radar")
	float GetPingProgress01() const { return FMath::Clamp(TimeSinceLastPing / FMath::Max(0.1f, PingIntervalSeconds), 0.f, 1.f); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Radar")
	float PingRadiusCm = 20000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Radar")
	float PingIntervalSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Radar")
	TEnumAsByte<ECollisionChannel> DetectionChannel = ECC_WorldStatic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Radar")
	bool bDetectWorldStatic = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Radar")
	bool bDetectDynamicActors = true;

private:
	void PerformPing();
	bool ShouldIncludeActor(const AActor* Actor) const;

private:
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Submarine|Radar", meta = (AllowPrivateAccess = "true"))
	TArray<FRadarContact> Contacts;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Submarine|Radar", meta = (AllowPrivateAccess = "true"))
	float TimeSinceLastPing = 0.f;
};
