#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TurretActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class SUB3D_API ATurretActor : public AActor
{
	GENERATED_BODY()

public:
	ATurretActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* YawPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* PitchPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TurretMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	float AimInterpSpeed = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	float FireCooldown = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	float FireRange = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
	float DamagePerShot = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Ammo", meta = (ClampMin = "1"))
	int32 MaxAmmo = 50;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Turret")
	FRotator CurrentAim = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Turret")
	FRotator TargetAim = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Turret")
	bool bFireHeld = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Turret")
	bool bOnline = true;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Turret|Ammo")
	int32 CurrentAmmo = 50;

	// Server world time of the last successful shot. Clients observe this to
	// trigger one-shot fire feedback (HUD flash, audio, camera kick).
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LastFireTime, Category = "Turret")
	float LastFireServerTime = 0.f;

	UFUNCTION()
	void OnRep_LastFireTime();

	// Blueprint hook so cosmetic clients can run FX on the replicated fire event.
	UFUNCTION(BlueprintImplementableEvent, Category = "Turret")
	void BP_OnFiredReplicated();

	UFUNCTION(BlueprintCallable, Category = "Turret")
	void SetAimCommand(const FRotator& InAim);

	UFUNCTION(BlueprintCallable, Category = "Turret")
	void SetFireHeld(bool bHeld);

	UFUNCTION(BlueprintCallable, Category = "Turret")
	void SetOnline(bool bInOnline);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Turret")
	void BP_OnFired();

private:
	void ApplyAimVisuals();
	void TryFire();

private:
	float CooldownRemaining = 0.f;
};
