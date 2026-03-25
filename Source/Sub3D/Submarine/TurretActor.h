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

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Turret")
	FRotator CurrentAim = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Turret")
	FRotator TargetAim = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Turret")
	bool bFireHeld = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Turret")
	bool bOnline = true;

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
