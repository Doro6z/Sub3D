#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CrewLocomotionTypes.h"
#include "SubCrewNetTypes.h"
#include "SubCrewMovementComponent.generated.h"

class UPrimitiveComponent;
class ASubmarineBase;
class ULadderClimbComponent;

UENUM(BlueprintType)
enum class ECrewPostureState : uint8
{
	Prone,
	Crouched,
	Standing
};

/**
 * Crew locomotion axis state. Orthogonal to environment context (see ASubCrewCharacter::CurrentCompartment).
 * Outside       = World-space, CMC native (ocean swim, world walking).
 * Embarked      = Local grid-space rebase active (inside the submarine moving frame).
 * Transitioning = Handoff in progress (reserved for post-FP multi-tick velocity blend; FP does instant flips).
 */
UENUM(BlueprintType)
enum class ECrewEmbarkState : uint8
{
	Outside,
	Embarked,
	Transitioning
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

	/**
	 * Authoritative crew pose in the submarine's local space. Updated each tick via rebase/extract.
	 * Replicated with COND_SkipOwner: owning client computes locally (via its own rebase), non-owning
	 * clients receive the server-computed value and rebase the peer crew against their local sub pose.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GridSpaceTransform, Category = "Submarine|Crew|LocalGrid")
	FTransform GridSpaceTransform = FTransform::Identity;

	/** Current yaw owned by the local grid frame while embarked. Baked into GridSpaceTransform for replication. */
	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|LocalGrid")
	float GridFacingYawDeg = 0.f;

	/** Target local-grid yaw used by the turn-rate clamp. */
	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|LocalGrid")
	float DesiredGridFacingYawDeg = 0.f;

	/** Current local-grid yaw rate, used by animation/debug. */
	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|LocalGrid")
	float GridFacingYawRateDegPerSec = 0.f;

	/** Maximum local-grid turn rate for embarked crew yaw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|LocalGrid", meta = (ClampMin = "1.0"))
	float GridFacingTurnRateDegPerSec = 720.f;

	/**
	 * Crew locomotion axis. Replicated with COND_SkipOwner — owner predicts state transitions
	 * locally (via hull boundary crossing detection); non-owning clients get server's value.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Submarine|Crew|LocalGrid")
	ECrewEmbarkState EmbarkState = ECrewEmbarkState::Outside;

	/** True when the rebase owns the crew pose (Embarked or Transitioning). Read-only accessor for all rebase call-sites. */
	UFUNCTION(BlueprintPure, Category = "Submarine|Crew|LocalGrid")
	FORCEINLINE bool IsGridAuthoritative() const
	{
		return EmbarkState == ECrewEmbarkState::Embarked || EmbarkState == ECrewEmbarkState::Transitioning;
	}

	/** Sets the locomotion state. Single write-site so transitions can be centrally logged later. */
	void SetEmbarkState(ECrewEmbarkState NewState);

	/**
	 * Called by ASubCrewCharacter::HandleHullCrossing to mark that a crossing event fired
	 * during the current move. The FSavedMove_SubCrew reads this via ConsumePendingHandoff
	 * when the saved move is captured, packing the event into the network payload.
	 */
	void SetPendingHandoff(ECrewHandoffKind Kind);

	/** Returns and clears the pending handoff kind. Called by FSavedMove_SubCrew::SetMoveFor. */
	ECrewHandoffKind ConsumePendingHandoff();

	// ── Ladder climb (post-helm system) ────────────────────────
	/** Active ladder while climbing. Null when not climbing. Replicated for peer rendering. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Submarine|Crew|Ladder")
	TObjectPtr<ULadderClimbComponent> CurrentLadder = nullptr;

	/** Climb traversal progress along the ladder, 0 = start, 1 = end. Replicated COND_SkipOwner. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LadderClimbProgress, Category = "Submarine|Crew|Ladder")
	float LadderClimbProgress01 = 0.f;

	UFUNCTION(BlueprintPure, Category = "Submarine|Crew|Ladder")
	bool IsClimbingLadder() const { return CurrentLadder != nullptr; }

	/** Begin a climb on the given ladder. Server-only entry point; triggers replication so peers render. */
	void BeginLadderClimb(ULadderClimbComponent* Ladder, float StartProgress01);

	/** End the active climb. Optional StepOff parameter chooses which end to step off (0 = bottom, 1 = top). */
	void EndLadderClimb(float StepOffProgress01);

	/** Submarine world transform cached at the end of the previous tick. Used to compute the controller yaw delta and seeded on authority transitions. */
	FTransform LastSubWorldTransform = FTransform::Identity;

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

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Movement")
	FCrewMoveIntent LastMoveIntent;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Movement")
	FCrewLocomotionFrame LastLocomotionFrame;

	/** Run speed multiplier applied to MaxWalkSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Movement", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float RunSpeedMultiplier = 1.8f;

	/**
	 * Sole locomotion input entry point for crew walking/swimming.
	 * MoveAxis.X = forward/back, MoveAxis.Y = right/left.
	 */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Crew|Movement")
	void ApplyCrewPlanarMoveInput(FVector2D MoveAxis);

	UFUNCTION(BlueprintPure, Category = "Submarine|Crew|Movement")
	FCrewMoveIntent GetLastMoveIntent() const { return LastMoveIntent; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Crew|Movement")
	FCrewLocomotionFrame GetLastLocomotionFrame() const { return LastLocomotionFrame; }

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

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|FootIK")
	FCrewFootIKState FootIK_R_State;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|FootIK")
	FCrewFootIKState FootIK_L_State;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|FootIK")
	FName RightFootIKSocketName = TEXT("foot_r");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|FootIK")
	FName LeftFootIKSocketName = TEXT("foot_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|FootIK", meta = (ClampMin = "0.0"))
	float FootIKTraceUpCm = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|FootIK", meta = (ClampMin = "1.0"))
	float FootIKTraceDownCm = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|FootIK", meta = (ClampMin = "0.0"))
	float FootIKMaxOffsetCm = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|FootIK", meta = (ClampMin = "0.1"))
	float FootIKInterpSpeed = 14.f;

	void InitializeForSubmarine();
	void RefreshEmbarkedFlooring();

	/** True while the crew still has a submarine/interior-frame context, including EVA Outside state. */
	bool HasSubmarineBinding() const;

protected:
	virtual void UpdateBasedMovement(float DeltaSeconds) override;
	virtual void UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation) override;

	/**
	 * Override to suppress CMC's mesh-translation-offset creation while grid-authoritative.
	 * SmoothCorrection() is called when the replicated pose differs from the current actor
	 * pose; it captures the delta into MeshTranslationOffset for visual smoothing. In grid
	 * mode the rebase intentionally places the actor at SubTransform × GridSpaceTransform
	 * (≠ ReplicatedMovement.Location, which is the SERVER's world-space pose), so every
	 * replication arrival would seed a non-zero offset. Without SmoothCorrection running,
	 * Super::SmoothClientPosition stays harmless (nothing to decay) — so we don't override
	 * SmoothClientPosition itself; otherwise any leftover offset never goes back to zero
	 * after exiting grid mode and the peer's mesh appears glued in place.
	 */
	virtual void SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation) override;

	/** Returns our custom FNetworkPredictionData_Client_SubCrew for client-side move saving. */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	/**
	 * Override to apply client-reported grid-space state after CMC's native MoveAutonomous
	 * processing. Reads the current FCharacterNetworkMoveData_SubCrew and syncs
	 * GridSpaceTransform + EmbarkState on the server. For FP co-op the server trusts the
	 * client's reported grid-space pose; production validation would bound the delta.
	 */
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;

	/**
	 * Override world-space error check. When Embarked the server's capsule world pose differs
	 * from the client's because both rebase against a sub pose that's interp-offset differently
	 * (server sim vs client interp). Default CMC validation would trigger constant corrections
	 * and rubber-band the owner. Kept as a safety net alongside the FSavedMove local-space path.
	 */
	virtual bool ServerCheckClientError(
		float ClientTimeStamp,
		float DeltaTime,
		const FVector& Accel,
		const FVector& ClientWorldLocation,
		const FVector& RelativeClientLocation,
		UPrimitiveComponent* ClientMovementBase,
		FName ClientBaseBoneName,
		uint8 ClientMovementMode) override;

private:
	UFUNCTION()
	void OnRep_GridSpaceTransform();

	UFUNCTION()
	void OnRep_LadderClimbProgress();

	void TickLadderClimb(float DeltaTime);

	ASubmarineBase* GetCurrentSubmarine() const;
	bool IsAcceptedEmbarkedBase(const UPrimitiveComponent* CandidateBase) const;
	void UpdateGridFacingYaw(float DeltaTime, const FTransform& SubTransform);
	void ResetGridFacingYaw();
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

	// Master "Motion chain tick" aggregate emitter. One log line per render frame per
	// machine, gated on bLogPresentationChain + locally-controlled. Correlates the sub
	// pose/velocity/inputs with the crew rebase pose/embark state at the same render
	// time, so multi-stream investigation reduces to a single grep.
	void LogMotionChainTick(float DeltaTime) const;

	// Per-tick TRACE snapshot of the entire motion chain. Captured at 4 points across
	// the tick so handoff deltas (rebase teleport, CMC simulate, grid extract) become
	// inspectable line-by-line. Gated on bLogMotionChainTrace.
	struct FMotionChainSnapshot
	{
		FVector SubLoc = FVector::ZeroVector;
		FRotator SubRot = FRotator::ZeroRotator;
		FVector SubVel = FVector::ZeroVector;
		FVector CrewWorld = FVector::ZeroVector;
		FRotator CrewWorldRot = FRotator::ZeroRotator;
		FVector CMCVelocity = FVector::ZeroVector;
		FVector GridLocal = FVector::ZeroVector;
		float GridYaw = 0.f;
		uint8 MovementMode = 0;
		bool bFalling = false;
		FName BaseName = NAME_None;
		FName BaseOwnerName = NAME_None;
		FVector FloorImpact = FVector::ZeroVector;
		float FloorDist = 0.f;
		bool bWalkable = false;
	};
	mutable FMotionChainSnapshot TracePrevPostExtract;
	mutable bool bHasTracePrev = false;

	void CaptureTraceSnapshot(FMotionChainSnapshot& Out) const;
	void EmitMotionChainTrace(const FMotionChainSnapshot& PreReb,
		const FMotionChainSnapshot& PostReb,
		const FMotionChainSnapshot& PostCMC,
		const FMotionChainSnapshot& PostExtract,
		float DeltaTime) const;

	void TickPosture(float DeltaTime);
	void SetRunningState(bool bNewRunning);
	FCrewMoveIntent BuildMoveIntent(FVector2D MoveAxis) const;
	void UpdateLocomotionFrame();
	void UpdateHandIKProbes();
	void UpdateFootIKTraces();

	TWeakObjectPtr<UPrimitiveComponent> LastKnownBase;
	FTransform LastReceivedReplicatedGridSpaceTransform = FTransform::Identity;
	double LastReceivedGridSpaceTransformRealTime = 0.0;
	bool bHasReceivedReplicatedGridSpaceTransform = false;
	FVector PreviousRelativeLocation = FVector::ZeroVector;
	bool bHasPreviousRelativeLocation = false;
	float FloorRecoveryTimer = 0.f;
	float DebugLogTimer = 0.f;

	/** Tracks submarine binding across ticks so the lazy latch only fires on the rising edge (false->true). */
	bool bHadSubmarineBindingLastTick = false;

	/** True after GridFacingYawDeg has been seeded from GridSpaceTransform or replication. */
	bool bHasGridFacingYaw = false;

	/** Set by ApplyCrewPlanarMoveInput so LastMoveIntent can be cleared when input stops. */
	bool bReceivedMoveInputThisFrame = false;

	/**
	 * Handoff event pending capture into the next saved move. Set by
	 * ASubCrewCharacter::HandleHullCrossing, consumed by FSavedMove_SubCrew::SetMoveFor.
	 * Serialized over the wire so the server can mirror the state flip.
	 */
	ECrewHandoffKind PendingHandoff = static_cast<ECrewHandoffKind>(0);

	/** Server-side container feeding our custom FCharacterNetworkMoveData_SubCrew to CMC's move pipeline. */
	TUniquePtr<FCharacterNetworkMoveDataContainer_SubCrew> SubCrewMoveDataContainer;
};
