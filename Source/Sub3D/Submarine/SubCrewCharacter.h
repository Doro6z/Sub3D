#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SubmarineRuntimeTypes.h"
#include "SubCrewCharacter.generated.h"

class UCameraComponent;
class ASubmarineBase;
class UInteractableComponent;
class USubInteractionComponent;

/**
 * Crew member character.
 * Input is handled entirely in Blueprint (Event Graph).
 * C++ provides: boarding, helm assignment, Server RPCs to drive sub physics.
 */
UCLASS()
class SUB3D_API ASubCrewCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASubCrewCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── Components ────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* FPSCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubInteractionComponent* InteractionComponent;

	// ── Submarine attachment ──────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentSubmarine, Category = "Crew")
	ASubmarineBase* CurrentSubmarine;

	UFUNCTION()
	void OnRep_CurrentSubmarine();

	// Set the submarine reference without any physical attachment
	UFUNCTION(BlueprintCallable, Category = "Crew")
	void SetCurrentSubmarine(ASubmarineBase* Sub);

	// Teleport into the submarine and ensure walking mode is active
	UFUNCTION(BlueprintCallable, Category = "Crew")
	void EnterOnFootInSubmarine(ASubmarineBase* Sub, const FTransform& SpawnXform);

	// Legacy attachment boarding
	UFUNCTION(BlueprintCallable, Category = "Crew")
	void BoardSubmarine(ASubmarineBase* Submarine);

	UFUNCTION(BlueprintCallable, Category = "Crew")
	void DisembarkSubmarine();

	// ── Helm ──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crew")
	bool bIsAtHelm = false;

	UFUNCTION(BlueprintCallable, Category = "Crew")
	void TakeHelm();

	UFUNCTION(BlueprintCallable, Category = "Crew")
	void ReleaseHelm();

	// Called server-side directly (e.g. from GameMode on first board)
	void ForceHelm();

	// ── Interact ──────────────────────────────────────────────────────────

	// Line-trace from FPS camera, triggers UInteractableComponent if hit
	UFUNCTION(BlueprintCallable, Category = "Crew")
	void Interact();

	// Max interact distance in cm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew")
	float InteractDistance = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Pressure", meta = (ClampMin = "0.0"))
	float BaseSafeAmbientPressureKPa = 121.59f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Pressure", meta = (ClampMin = "0.0"))
	float PressureProtectionKPa = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Pressure", meta = (ClampMin = "0.0"))
	float PressureGraceSeconds = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Pressure", meta = (ClampMin = "0.0"))
	float PressureRecoveryRate = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Pressure", meta = (ClampMin = "0.0"))
	float PressureDamagePerSecond = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Pressure", meta = (ClampMin = "0.0"))
	float PressureDamageSeverityScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShallowWadeThreshold01 = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeepWadeThreshold01 = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwimThreshold01 = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShallowWadeSpeedMultiplier = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeepWadeSpeedMultiplier = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NearSwimSpeedMultiplier = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Water", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float SwimSpeedMultiplier = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Water", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float WaterMovementProtectionMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Debug")
	bool bDebugLogEnvironmentState = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Debug", meta = (ClampMin = "0.1"))
	float EnvironmentDebugLogIntervalSeconds = 1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew|Environment")
	FName CurrentCompartmentId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew|Environment")
	float CurrentAmbientPressureKPa = 101.325f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew|Environment")
	float CurrentWaterHeightCm = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew|Environment")
	float CurrentWaterImmersion01 = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew|Environment")
	float PressureExposureSeconds = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew|Environment")
	bool bPressureDangerous = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew|Environment")
	bool bIsSwimmingByFlood = false;

	UFUNCTION(BlueprintCallable, Category = "Crew|Environment")
	void SetPressureProtectionKPa(float NewPressureProtectionKPa);

	UFUNCTION(BlueprintCallable, Category = "Crew|Environment")
	void SetWaterMovementProtectionMultiplier(float NewWaterMovementProtectionMultiplier);

	// ── Sub input RPCs — call these from Blueprint Event Graph ────────────

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Crew|Helm")
	void Server_SetThrust(float Value);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Crew|Helm")
	void Server_SetRudder(float Value);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Crew|Helm")
	void Server_SetDivePlane(float Value);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Crew|Helm")
	void Server_SetBallastTarget(int32 Index, float Target);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Crew|Helm")
	void Server_ResyncBallasts(float GlobalTarget);

private:
	void UpdateEnvironmentalEffects(float DeltaSeconds);
	bool ResolveCurrentCompartment(FCompartmentState& OutState, FBox& OutLocalBounds) const;
	void ApplyPressureEffects(float DeltaSeconds, float AmbientPressureKPa);
	void ApplyWaterMovementState(float WaterImmersion01);
	void ResetEnvironmentalState();

	UFUNCTION(Server, Reliable)
	void Server_TakeHelm();

	UFUNCTION(Server, Reliable)
	void Server_ReleaseHelm();

	float DefaultWalkSpeed = 300.f;
	float DefaultSwimSpeed = 240.f;
	float EnvironmentDebugLogTimer = 0.f;
};
