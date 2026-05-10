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
class UCompartmentVolumeComponent;
class USubHullBoundaryComponent;
class UCrewUnderwaterPPComponent;
class UCrewAnimDebugComponent;
class UMaterialInterface;

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

	/**
	 * Drives the underwater post-process effect. Passive C++: compares camera Z to
	 * current compartment's water surface Z, blends PP weight, fires BP events for
	 * designer hooks (droplets, splash). Material-driven visual intelligence.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCrewUnderwaterPPComponent* UnderwaterPP;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCrewAnimDebugComponent* CrewAnimDebugComponent;

	/** Default underwater post-process material applied to UnderwaterPP at BeginPlay. Art designer sets this on the BP class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crew|Underwater")
	TObjectPtr<UMaterialInterface> DefaultUnderwaterPPMaterial = nullptr;

	/** Typed getter for the custom movement component. No cast needed in BP. */
	UFUNCTION(BlueprintPure, Category = "Crew")
	USubCrewMovementComponent* GetCrewMovement() const;

	/** Blueprint input wrapper. MoveAxis.X = forward/back, MoveAxis.Y = right/left. */
	UFUNCTION(BlueprintCallable, Category = "Crew|Movement")
	void ApplyCrewPlanarMoveInput(FVector2D MoveAxis);

	/** Blueprint input wrapper for swim vertical movement. Axis +1 = up, -1 = down. */
	UFUNCTION(BlueprintCallable, Category = "Crew|Movement")
	void ApplyCrewVerticalMoveInput(float Axis);

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

	// Set the submarine reference without any physical attachment or locomotion transition
	UFUNCTION(BlueprintCallable, Category = "Crew")
	void SetCurrentSubmarine(ASubmarineBase* Sub);

	// Teleport into the submarine and ensure walking mode is active
	UFUNCTION(BlueprintCallable, Category = "Crew")
	void EnterOnFootInSubmarine(ASubmarineBase* Sub, const FTransform& SpawnXform);

	// Legacy compatibility wrapper. Use EnterOnFootInSubmarine for spawn/bootstrap.
	UFUNCTION(BlueprintCallable, Category = "Crew", meta = (DeprecatedFunction, DeprecationMessage = "Use EnterOnFootInSubmarine for spawn/bootstrap."))
	void BoardSubmarine(ASubmarineBase* Submarine);

	// Legacy hard-detach path. EVA uses HandleHullCrossing via hull boundaries.
	UFUNCTION(BlueprintCallable, Category = "Crew", meta = (DeprecatedFunction, DeprecationMessage = "Use hull boundary crossing for EVA. This function is a legacy hard-detach path."))
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

	/** Reserved for the future immersion-based swim transition. Not used by the current water movement path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwimThreshold01 = 0.7f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Swim", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float ExteriorSwimSpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Swim", meta = (ClampMin = "0.0"))
	float SwimBrakingDeceleration = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Environment|Swim")
	float SwimGravityScale = 0.f;

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
	bool bShowAnimDebugPanel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Debug", meta = (ClampMin = "0.1"))
	float EnvironmentDebugLogIntervalSeconds = 1.f;

	/**
	 * Authoritative compartment ID (server-side overlap detection).
	 * Replicated with COND_SkipOwner: the owning client's own overlap handlers populate this,
	 * non-owning clients receive it and resolve CurrentCompartment pointer in OnRep.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentCompartmentId, Category = "Crew|Environment")
	FName CurrentCompartmentId = NAME_None;

	UFUNCTION()
	void OnRep_CurrentCompartmentId();

	/**
	 * Environment axis pointer: spatial zone currently occupied by the crew capsule.
	 * nullptr = ocean (outside hull). Updated by overlap events on ECC_CompartmentProbe.
	 * Orthogonal to locomotion state (ECrewEmbarkState).
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Crew|Environment")
	TWeakObjectPtr<UCompartmentVolumeComponent> CurrentCompartment;

	/** True if the capsule center is below the water level of the current compartment (or always true in ocean). */
	UFUNCTION(BlueprintPure, Category = "Crew|Environment")
	bool IsInWater() const;

	/** True if the current compartment still has oxygen. False in ocean unless the crew has an external supply. */
	UFUNCTION(BlueprintPure, Category = "Crew|Environment")
	bool HasOxygen() const;

	UFUNCTION()
	void OnCompartmentOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnCompartmentOverlapEnd(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	/**
	 * Hull-plane crossing handler. Called by USubHullBoundaryComponent when the crew capsule
	 * crosses the hull plane. Applies velocity blending and flips ECrewEmbarkState.
	 *
	 * bOutgoing = true  : Embarked -> Outside. Inject sub velocity so the crew keeps world momentum.
	 * bOutgoing = false : Outside  -> Embarked. Subtract sub velocity and seed GridSpaceTransform
	 *                     from the current world pose.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crew|EVA")
	void HandleHullCrossing(USubHullBoundaryComponent* Boundary, bool bOutgoing);

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

	UFUNCTION(BlueprintPure, Category = "Crew|Environment")
	bool IsCrewSwimming() const;

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
	void UpdateEnvironmentalEffects(float DeltaSeconds);
	bool ResolveCurrentCompartment(FCompartmentState& OutState, FBox& OutLocalBounds) const;
	void ApplyPressureEffects(float DeltaSeconds, float AmbientPressureKPa);
	void ApplyWaterMovementState(float WaterImmersion01);
	void ApplySwimmingMovementState(float SpeedMultiplier);
	void ResetEnvironmentalState();
	void UpdateCameraMode(float DeltaSeconds);
	void UpdateCameraRig();
	void UpdateLocalHeadVisibility();

	/** Picks the best active overlap (nearest center) and assigns it to CurrentCompartment. */
	void RecomputeCurrentCompartment();

	/** Transient set of compartments currently overlapping the capsule. Runtime-only, no UPROPERTY. */
	TSet<TWeakObjectPtr<UCompartmentVolumeComponent>> ActiveCompartmentOverlaps;

	UFUNCTION(Server, Reliable)
	void Server_TakeHelm();

	UFUNCTION(Server, Reliable)
	void Server_ReleaseHelm();

	float DefaultWalkSpeed = 300.f;
	float DefaultSwimSpeed = 240.f;
	float EnvironmentDebugLogTimer = 0.f;
	bool bWantsFirstPerson = true;
	bool bHeadHiddenForLocalView = false;

	/**
	 * Guard flag: SetCurrentSubmarine(nullptr) is only legitimate from inside DisembarkSubmarine.
	 * Any other caller hitting the null path indicates an upstream bug (replication race,
	 * accidental BP wire, sub destruction without disembark). DisembarkSubmarine sets this true
	 * via TGuardValue around its SetCurrentSubmarine(nullptr) call; the ensure in SetCurrentSubmarine
	 * fires when this is false.
	 */
	bool bAllowSubmarineUnbind = false;
};
