#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineRuntimeTypes.h"
#include "SubmarineSystemsComponent.generated.h"

class ASubmarineBase;
class ATurretActor;

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubmarineSystemsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubmarineSystemsComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Submarine|Systems")
	const FSubmarineCommandState& GetCommandState() const { return CommandState; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Systems")
	float GetEngineHealth01() const { return EngineHealth01; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Systems")
	float GetElectricalHealth01() const { return ElectricalHealth01; }

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetHelmThrottleCommand(float Value);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetHelmYawCommand(float Value);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetHelmTrimCommand(float Value);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetAutoDepthEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetTargetDepthMeters(float DepthMeters);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetGlobalBallastTarget(float Target);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetBallastTargetByIndex(int32 Index, float Target);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetBallastsActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void ResyncAllBallasts();

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetPumpActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetPumpPower01(float Power01);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetEngineBoost(float Value);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetTurretAim(const FRotator& Aim);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetTurretFireHeld(bool bHeld);

protected:
	UFUNCTION()
	void OnRep_CommandState();

private:
	ASubmarineBase* ResolveOwnerSubmarine() const;
	void PushPumpStateToHull() const;
	void UpdateAutoDepth(float DeltaTime);

private:
	UPROPERTY(ReplicatedUsing = OnRep_CommandState, BlueprintReadOnly, Category = "Submarine|Systems", meta = (AllowPrivateAccess = "true"))
	FSubmarineCommandState CommandState;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Submarine|Systems", meta = (AllowPrivateAccess = "true"))
	float EngineHealth01 = 1.f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Submarine|Systems", meta = (AllowPrivateAccess = "true"))
	float ElectricalHealth01 = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Pump", meta = (AllowPrivateAccess = "true"))
	FName DefaultPumpCompartmentId = FName(TEXT("HullMain"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Pump", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float BasePumpRateLitersPerSec = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|AutoDepth", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float AutoDepthBallastGain = 0.04f;
};
