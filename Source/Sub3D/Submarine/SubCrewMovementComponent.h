#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SubCrewMovementComponent.generated.h"

class UPrimitiveComponent;
class USubInteriorFrameComponent;
class ASubmarineBase;

UENUM(BlueprintType)
enum class ECrewPostureState : uint8
{
	Prone,
	Crouched,
	Standing
};

/**
 * Crew movement component for interior submarine traversal.
 * Keeps relative state while relying on stock CMC based movement when embarked.
 */
UCLASS()
class SUB3D_API USubCrewMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	USubCrewMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	/** Authoritative crew pose in the submarine's local space. Updated each tick via rebase/extract. */
	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|LocalGrid")
	FTransform GridSpaceTransform = FTransform::Identity;

	/** When true, crew transport is driven by the rebase/extract pipeline; MovementBase carry is bypassed. */
	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|LocalGrid")
	bool bIsGridSpaceAuthority = false;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew")
	FVector RelativeLinearVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew")
	float SnapThresholdCm = 200.f;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Support")
	bool bHasValidEmbarkedFloor = false;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Support")
	bool bHasAcceptedEmbarkedBase = false;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Support")
	bool bNeedsEmbarkedFloorRecovery = false;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Support")
	float SupportQuality01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Support")
	TObjectPtr<UPrimitiveComponent> LastEmbarkedFloorComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector LocalSubLinearVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector LocalSubLinearAcceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector LocalSubAngularVelocityDegrees = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector LocalSubAngularAccelerationDegrees = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	bool bHasNearbyBraceSupport = false;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	float NearbyBraceDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector NearbyBraceWorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector NearbyBraceWorldNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector BraceQueryOrigin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Support", meta = (ClampMin = "0.01"))
	float FloorRecoveryIntervalSeconds = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Embodiment", meta = (ClampMin = "1.0"))
	float BraceProbeDistanceCm = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Embodiment", meta = (ClampMin = "0.0"))
	float BraceProbeHeightOffsetCm = 70.f;

	// ── Posture System ───────────────────────────────────────

	/** 0 = prone, 0.5 = crouch, 1 = standing. Driven by scroll wheel input. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Submarine|Crew|Posture")
	float PostureAlpha = 1.f;

	/** Target posture set by input. PostureAlpha interpolates toward this. */
	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Posture")
	float PostureTarget = 1.f;

	/** Interpolation speed for posture transitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Posture", meta = (ClampMin = "0.5"))
	float PostureInterpSpeed = 5.f;

	/** Capsule half-height when standing (PostureAlpha=1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Posture", meta = (ClampMin = "10.0"))
	float StandingHalfHeight = 88.f;

	/** Capsule half-height when prone (PostureAlpha=0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Posture", meta = (ClampMin = "10.0"))
	float ProneHalfHeight = 30.f;

	/** Camera Z offset when standing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Posture")
	float StandingCameraZ = 70.f;

	/** Camera Z offset when prone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Posture")
	float ProneCameraZ = 25.f;

	/** Set posture target (0-1). Called from input. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Crew|Posture")
	void SetPostureTarget(float Alpha);

	/** Add to posture target (scroll wheel delta). Clamped 0-1. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Crew|Posture")
	void AddPostureDelta(float Delta);

	/** Whether the character is currently sprinting */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Submarine|Crew|Movement")
	bool bIsRunning = false;

	/** Run speed multiplier applied to MaxWalkSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Movement", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float RunSpeedMultiplier = 1.8f;

	/** Request sprint start. Called from input (Shift pressed). */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Crew|Movement")
	void RequestRunStart();

	/** Request sprint stop. Called from input (Shift released). */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Crew|Movement")
	void RequestRunStop();

	UFUNCTION(BlueprintPure, Category = "Submarine|Crew|Posture")
	ECrewPostureState GetPostureState() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Crew|Movement")
	float GetPostureSpeedScale() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Crew|Movement")
	float GetDesiredWalkSpeedMultiplier() const;

	// ── Hand IK Probes ───────────────────────────────────────

	struct FHandIKProbeResult
	{
		bool bHit = false;
		FVector WorldLocation = FVector::ZeroVector;
		FVector WorldNormal = FVector::ZeroVector;
		float Distance = 0.f;
	};

	/** 6 probes: 0=HandL, 1=HandR, 2=HipL, 3=HipR, 4=ShoulderL, 5=ShoulderR */
	FHandIKProbeResult HandProbes[6];

	// ── Foot IK ──────────────────────────────────────────────

	/** Foot IK offset (Z delta from flat ground) for each foot */
	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|FootIK")
	FVector FootIK_R = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|FootIK")
	FVector FootIK_L = FVector::ZeroVector;

	void InitializeForSubmarine();
	void RefreshEmbarkedFlooring();

	bool IsEmbarked() const;

protected:
	virtual void UpdateBasedMovement(float DeltaSeconds) override;
	virtual void UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation) override;

private:
	ASubmarineBase* GetCurrentSubmarine() const;
	USubInteriorFrameComponent* GetInteriorFrame() const;
	bool IsAcceptedEmbarkedBase(const UPrimitiveComponent* CandidateBase) const;
	void UpdateInertialState();
	void UpdateRelativeState(float DeltaTime);
	void UpdateSupportState();
	void AttemptEmbarkedFloorRecovery(float DeltaTime);
	void UpdateBraceState();
	bool ShouldEvaluateHandIK() const;
	bool QueryBraceSupportHit(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	void CheckAndLogBaseChange();
	void DebugDrawState();
	void LogPeriodicState(float DeltaTime);

	void TickPosture(float DeltaTime);
	void SetRunningState(bool bNewRunning);
	void UpdateHandIKProbes();
	void UpdateFootIKTraces();

	TWeakObjectPtr<UPrimitiveComponent> LastKnownBase;
	FVector PreviousRelativeLocation = FVector::ZeroVector;
	bool bHasPreviousRelativeLocation = false;
	float FloorRecoveryTimer = 0.f;
	float DebugLogTimer = 0.f;

	/** Submarine world transform cached at the end of the previous tick. Used to compute the controller yaw delta. */
	FTransform LastSubWorldTransform = FTransform::Identity;
};
