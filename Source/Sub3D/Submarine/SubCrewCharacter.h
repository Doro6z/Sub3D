#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SubmarineRuntimeTypes.h"
#include "SubCrewCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class ASubmarineBase;
class UInteractableComponent;
class USubInteractionComponent;
class USubCrewMovementComponent;

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
	USpringArmComponent* TPSCameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* TPSCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubInteractionComponent* InteractionComponent;

	/** Typed getter for the custom movement component. No cast needed in BP. */
	UFUNCTION(BlueprintPure, Category = "Crew")
	USubCrewMovementComponent* GetCrewMovement() const;

	UFUNCTION(BlueprintCallable, Category = "Crew|Camera")
	void ToggleCameraMode();

	UFUNCTION(BlueprintCallable, Category = "Crew|Camera")
	void SetFirstPersonMode(bool bNewFirstPerson);

	UFUNCTION(BlueprintPure, Category = "Crew|Camera")
	bool IsFirstPersonMode() const { return bWantsFirstPerson; }

	UFUNCTION(BlueprintPure, Category = "Crew|Camera")
	UCameraComponent* GetActiveViewCamera() const;

	UFUNCTION(BlueprintPure, Category = "Crew|Camera")
	float GetCurrentPostureCameraZ() const;

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

	// ── Health ────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Health")
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crew|Health")
	float Health = 100.f;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Crew|Health")
	float GetHealthNormalized() const { return FMath::Clamp(Health / FMath::Max(1.f, MaxHealth), 0.f, 1.f); }

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

	/** Hide head bone for local player in FPS mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Camera")
	bool bHideHeadInFPS = true;

	/** Camera sway from submarine acceleration (cm per cm/s²) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Camera")
	float CameraSwayAccelScale = 0.002f;

	/** Camera sway from submarine angular velocity (cm per deg/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Camera")
	float CameraSwayAngularScale = 0.05f;

	/** Max camera sway offset (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Camera")
	float CameraSwayMaxCm = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Camera", meta = (ClampMin = "0.1"))
	float CameraBlendSpeed = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Camera", meta = (ClampMin = "0.0"))
	float ThirdPersonArmLength = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Camera")
	FVector ThirdPersonStandingSocketOffset = FVector(0.f, 55.f, 8.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Camera")
	FVector ThirdPersonProneSocketOffset = FVector(0.f, 35.f, 12.f);

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Camera")
	float CameraBlendAlpha = 0.f;

	/** Show the anim tuner panel at startup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Debug")
	bool bShowAnimDebugPanel = true;

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

	UFUNCTION(Server, Reliable)
	void ServerSetPostureTarget(float Alpha);

	UFUNCTION(Server, Reliable)
	void ServerSetRunning(bool bNewRunning);

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
	ASubmarineBase* ResolveSubmarineFromMovementBase() const;
	void EnsureEmbarkedSubmarineBinding(const TCHAR* Context);
	void UpdateEnvironmentalEffects(float DeltaSeconds);
	bool ResolveCurrentCompartment(FCompartmentState& OutState, FBox& OutLocalBounds) const;
	void ApplyPressureEffects(float DeltaSeconds, float AmbientPressureKPa);
	void ApplyWaterMovementState(float WaterImmersion01);
	void ResetEnvironmentalState();
	void UpdateCameraMode(float DeltaSeconds);
	void UpdateCameraRig();
	void UpdateLocalHeadVisibility();

	UFUNCTION(Server, Reliable)
	void Server_TakeHelm();

	UFUNCTION(Server, Reliable)
	void Server_ReleaseHelm();

	float DefaultWalkSpeed = 300.f;
	float DefaultSwimSpeed = 240.f;
	float EnvironmentDebugLogTimer = 0.f;
	bool bWantsFirstPerson = true;
	bool bHeadHiddenForLocalView = false;
};
