#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CrewLocomotionTypes.h"
#include "SubCrewAnimInstance.generated.h"

class ASubCrewCharacter;
class USubCrewMovementComponent;

/**
 * Full procedural AnimInstance for crew characters.
 * Computes per-bone rotations each tick based on:
 *   - Walk cycle (sinusoidal leg/arm swing)
 *   - Breathing (spine oscillation)
 *   - Posture (spine bend chain)
 *   - Submarine motion (lean, stumble)
 *   - Upper/lower body split (aim offset)
 *
 * AnimGraph: Reference Pose → Modify Bone per bone → Output Pose
 */
UCLASS()
class SUB3D_API USubCrewAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ══════════════════════════════════════════════════════════
	// INPUT STATE (read from character/movement component)
	// ══════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	float Direction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	bool bIsSwimming = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	bool bIsRunning = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	float PostureAlpha = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	float UpperBodyYawOffset = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	float SubPitchDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	float SubRollDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	float SubAccelForward = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	float SubAccelLateral = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	float SupportQuality = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	float LocalTurnRateDegPerSec = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	FCrewAnimLocomotionState LocomotionState;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	FCrewMoveIntent MoveIntent;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	FCrewLocomotionFrame LocomotionFrame;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	ECrewLocomotionStance Stance = ECrewLocomotionStance::Standing;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	ECrewLocomotionGait Gait = ECrewLocomotionGait::Idle;

	// ══════════════════════════════════════════════════════════
	// PROCEDURAL BONE ROTATIONS (output, apply via Modify Bone)
	// All additive to reference pose.
	// ══════════════════════════════════════════════════════════

	// ── Pelvis ──
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_Pelvis_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FVector Proc_Pelvis_Offset = FVector::ZeroVector;

	// ── Spine chain (posture + breathing + sub lean) ──
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_Spine01_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_Spine02_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_Spine03_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_Spine04_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_Spine05_Rot = FRotator::ZeroRotator;

	// ── Neck + Head (stabilization + look) ──
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_Neck01_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_Head_Rot = FRotator::ZeroRotator;

	// ── Legs (walk cycle) ──
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_ThighR_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_ThighL_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_CalfR_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_CalfL_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_FootR_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_FootL_Rot = FRotator::ZeroRotator;

	// ── Arms (counter-swing + upper body aim) ──
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_UpperarmR_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_UpperarmL_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_LowerarmR_Rot = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Bones")
	FRotator Proc_LowerarmL_Rot = FRotator::ZeroRotator;

	// ══════════════════════════════════════════════════════════
	// TUNING PARAMETERS
	// ══════════════════════════════════════════════════════════

	/** Walk cycle: leg swing amplitude in degrees at max walk speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Walk")
	float WalkLegSwingDeg = 30.f;

	/** Walk cycle: arm counter-swing amplitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Walk")
	float WalkArmSwingDeg = 20.f;

	/** Walk cycle: pelvis vertical bob amplitude in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Walk")
	float WalkPelvisBobCm = 2.f;

	/** Walk cycle: frequency scale (steps per cm traveled) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Walk")
	float WalkCycleRate = 0.04f;

	/** Calf bend multiplier during walk swing (higher = more knee bend) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Walk")
	float WalkCalfBendMultiplier = 1.2f;

	/** Spine forward lean when walking (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Walk")
	float WalkSpineLeanDeg = 3.f;

	// ── RUN ──

	/** Run speed threshold (above this = running) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Run")
	float RunSpeedThreshold = 400.f;

	/** Run max speed for blend calculations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Run")
	float RunMaxSpeed = 600.f;

	/** Run leg swing amplitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Run")
	float RunLegSwingDeg = 50.f;

	/** Run arm swing amplitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Run")
	float RunArmSwingDeg = 35.f;

	/** Run pelvis bob */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Run")
	float RunPelvisBobCm = 4.f;

	/** Run cycle rate (faster than walk) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Run")
	float RunCycleRate = 0.06f;

	/** Run forward lean */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Run")
	float RunSpineLeanDeg = 8.f;

	/** Idle breathing amplitude in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Idle")
	float BreathingAmplitudeDeg = 1.5f;

	/** Idle breathing speed (cycles per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Idle")
	float BreathingRate = 0.4f;

	/** Max forward spine bend when fully prone (degrees, distributed across 5 spine bones) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Posture")
	float MaxPostureBendDeg = 80.f;

	/** Sub motion: lean multiplier (degrees per degree of sub angular velocity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|SubMotion")
	float SubLeanMultiplier = 0.3f;

	/** Sub motion: stumble multiplier (degrees per cm/s² acceleration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|SubMotion")
	float SubStumbleMultiplier = 0.01f;

	// ── AXIS REMAPPING (fix Blender→UE bone axis mismatch) ──
	// For each bone group, which FRotator axis maps to which motion:
	//   Swing = leg/arm forward-back motion
	//   Twist = rotation around bone's long axis
	// Values: 0=Pitch, 1=Yaw, 2=Roll

	/** Leg swing axis: 0=Pitch, 1=Yaw, 2=Roll. Blender vertical bones: Yaw=forward/back */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Axes", meta = (ClampMin = "0", ClampMax = "2"))
	int32 LegSwingAxis = 1;

	/** Arm swing axis for walk counter-swing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Axes", meta = (ClampMin = "0", ClampMax = "2"))
	int32 ArmSwingAxis = 1;

	/** Elbow bend axis (for forearm flex during walk) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Axes", meta = (ClampMin = "0", ClampMax = "2"))
	int32 ElbowBendAxis = 2;

	/** Spine bend axis (forward/back for posture/breathing). Blender: Yaw=forward/back */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Axes", meta = (ClampMin = "0", ClampMax = "2"))
	int32 SpineBendAxis = 1;

	/** Spine twist axis (left/right turn for upper body aim). Try Roll for Blender skeletons */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Axes", meta = (ClampMin = "0", ClampMax = "2"))
	int32 SpineTwistAxis = 2;

	/** Negate leg swing direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Axes")
	bool bNegateLegSwing = false;

	/** Negate arm swing direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Axes")
	bool bNegateArmSwing = false;

	/** Negate spine bend direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Axes")
	bool bNegateSpineBend = false;

	// ── HAND IK TARGETS ──

	/** IK target for left hand (world space). Zero if not active. */
	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FVector HandIK_L_Target = FVector::ZeroVector;

	/** IK target for right hand (world space). */
	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FVector HandIK_R_Target = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	float HandIK_L_Weight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	float HandIK_R_Weight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FCrewHandIKState HandIK_L_State;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FCrewHandIKState HandIK_R_State;

	// ── FOOT IK ──

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FVector FootIK_R_Offset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FVector FootIK_L_Offset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FCrewFootIKState FootIK_R_State;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FCrewFootIKState FootIK_L_State;

	// ── SWIM PARAMS ──

	/** Swim stroke rate (cycles per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Swim")
	float SwimStrokeRate = 0.8f;

	/** Swim arm swing amplitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Swim")
	float SwimArmSwingDeg = 45.f;

	/** Swim kick amplitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Swim")
	float SwimKickDeg = 30.f;

	/** Right upper arm rest pose (direct FRotator, tunable per P/Y/R component) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	FRotator ArmRestR = FRotator(0.f, 0.f, -85.f);

	/** Left upper arm rest pose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	FRotator ArmRestL = FRotator(0.f, 0.f, 85.f);

	/** Right forearm rest pose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	FRotator ForearmRestR = FRotator(0.f, 0.f, -10.f);

	/** Left forearm rest pose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	FRotator ForearmRestL = FRotator(0.f, 0.f, 10.f);

	/** Max lower body yaw toward velocity direction (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Body")
	float MaxLowerBodyYawDeg = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crouch")
	float CrouchPelvisDropCm = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crouch")
	float CrouchThighFoldDeg = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crouch")
	float CrouchCalfFoldDeg = 42.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crouch")
	float CrouchFootCompDeg = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crouch")
	float CrouchArmForwardDeg = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crouch")
	float CrouchElbowBendDeg = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crawl")
	float CrawlCycleRate = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crawl")
	float CrawlLegSwingDeg = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crawl")
	float CrawlArmSwingDeg = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crawl")
	float CrawlPelvisBobCm = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crawl")
	float CrawlThighFoldDeg = 58.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crawl")
	float CrawlCalfFoldDeg = 84.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crawl")
	float CrawlFootCompDeg = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crawl")
	float CrawlArmForwardDeg = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Crawl")
	float CrawlElbowBendDeg = 52.f;

private:
	void ReadInputState();
	void ComputeWalkCycle(float DeltaSeconds);
	void ComputeCrawlCycle(float DeltaSeconds);
	void ComputeSwimCycle(float DeltaSeconds);
	void ComputeBreathing(float DeltaSeconds);
	void ComputePosture();
	void ComputeSubMotion();
	void ComputeUpperBodyAim();
	void ComputeHandIK();
	void ComputeFootIK();
	static FRotator MakeAxisRotator(float Degrees, int32 Axis, bool bNegate);
	float SwimPhase = 0.f;

	UPROPERTY()
	TWeakObjectPtr<ASubCrewCharacter> CrewCharacter;

	UPROPERTY()
	TWeakObjectPtr<USubCrewMovementComponent> CrewMovement;

public:
	float WalkPhase = 0.f;
private:
	float BreathPhase = 0.f;
	float DistanceTraveled = 0.f;
	FVector LastPosition = FVector::ZeroVector;
};
