#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CrewLocomotionTypes.h"
#include "CrewProceduralAnimProfile.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural")
	TObjectPtr<UCrewProceduralAnimProfile> ProceduralProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rig")
	FCrewProceduralRigAxisProfile RigAxesTuning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Grounded")
	FCrewProceduralGroundedTuning GroundedTuning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|SubMotion")
	FCrewProceduralSubMotionTuning SubMotionTuning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Swim")
	FCrewProceduralSwimTuning SwimTuning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Debug")
	bool bDrawProceduralDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Debug")
	bool bDrawBoneAxes = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Debug")
	bool bDrawFootProbes = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Debug", meta = (ClampMin = "1.0"))
	float DebugAxisLengthCm = 12.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Procedural|Debug")
	FVector DebugFootLWorld = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Procedural|Debug")
	FVector DebugFootRWorld = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Procedural|Debug")
	float DebugRightFootPhase01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Procedural|Debug")
	float DebugLeftFootPhase01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Procedural|Debug")
	float DebugRightFootSwingAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Procedural|Debug")
	float DebugLeftFootSwingAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Procedural|Debug")
	float DebugRightFootToeOffAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Procedural|Debug")
	float DebugLeftFootToeOffAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Procedural|Debug")
	float DebugActiveCycleLengthCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Procedural|Debug")
	float DebugActiveStepRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Procedural|Debug")
	float DebugRunBlendAlpha = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Calibration")
	bool bEnableProceduralLegs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Calibration")
	bool bEnableProceduralThighs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Calibration")
	bool bEnableProceduralCalves = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Calibration")
	bool bEnableProceduralFeet = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Calibration")
	bool bEnableProceduralArms = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Calibration")
	bool bEnableProceduralBodyMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Calibration")
	bool bEnableProceduralRestPose = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Calibration")
	bool bEnableProceduralAim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Calibration")
	bool bEnableProceduralSubMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview")
	bool bUseEditorPreviewInputs = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview", meta = (ClampMin = "0.0"))
	float PreviewSpeedCmS = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float PreviewDirectionDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewPostureAlpha = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview")
	bool bPreviewIsSwimming = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview")
	bool bPreviewIsWaterSprinting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview")
	bool bPreviewLockStridePhase = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewStridePhase01 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewSupportQuality = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview")
	float PreviewLocalTurnRateDegPerSec = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview")
	float PreviewSubTiltPitchDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview")
	float PreviewSubTiltRollDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview")
	float PreviewSubAccelForwardCmS2 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Procedural|Preview")
	float PreviewSubAccelLateralCmS2 = 0.f;

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
	bool bIsWaterSprinting = false;

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
	FVector SubAngularVelocityDegrees = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Input")
	FVector SubAngularAccelerationDegrees = FVector::ZeroVector;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Walk")
	float WalkLegSwingDeg = 22.f;

	/** Walk cycle: arm counter-swing amplitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Walk")
	float WalkArmSwingDeg = 8.f;

	/** Walk cycle: pelvis vertical bob amplitude in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Walk")
	float WalkPelvisBobCm = 1.2f;

	/** Walk cycle: frequency scale (steps per cm traveled) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Walk")
	float WalkCycleRate = 0.02f;

	/** Calf bend multiplier during walk swing (higher = more knee bend) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Walk")
	float WalkCalfBendMultiplier = 1.f;

	/** Spine forward lean when walking (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Walk")
	float WalkSpineLeanDeg = 0.6f;

	// ── RUN ──

	/** Run speed threshold (above this = running) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Run")
	float RunSpeedThreshold = 400.f;

	/** Run max speed for blend calculations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Run")
	float RunMaxSpeed = 600.f;

	/** Run leg swing amplitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Run")
	float RunLegSwingDeg = 50.f;

	/** Run arm swing amplitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Run")
	float RunArmSwingDeg = 35.f;

	/** Run pelvis bob */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Run")
	float RunPelvisBobCm = 4.f;

	/** Run cycle rate (faster than walk) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Run")
	float RunCycleRate = 0.06f;

	/** Run forward lean */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Run")
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|SubMotion")
	float SubLeanMultiplier = 0.3f;

	/** Sub motion: stumble multiplier (degrees per cm/s² acceleration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|SubMotion")
	float SubStumbleMultiplier = 0.01f;

	// ── AXIS REMAPPING (fix Blender→UE bone axis mismatch) ──
	// For each bone group, which FRotator axis maps to which motion:
	//   Swing = leg/arm forward-back motion
	//   Twist = rotation around bone's long axis
	// Values: 0=Local X / Roll, 1=Local Y / Pitch, 2=Local Z / Yaw

	/** Leg swing axis: 0=Local X/Roll, 1=Local Y/Pitch, 2=Local Z/Yaw. Start with Local X for this skeleton. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Axes", meta = (ClampMin = "0", ClampMax = "2"))
	int32 LegSwingAxis = 0;

	/** Arm swing axis for walk counter-swing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Axes", meta = (ClampMin = "0", ClampMax = "2"))
	int32 ArmSwingAxis = 0;

	/** Axis used to lower both arms from the imported T-pose toward a relaxed standing pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Axes", meta = (ClampMin = "0", ClampMax = "2"))
	int32 ArmLowerAxis = 2;

	/** Elbow bend axis (for forearm flex during walk) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Axes", meta = (ClampMin = "0", ClampMax = "2"))
	int32 ElbowBendAxis = 2;

	/** Spine bend axis (forward/back for posture/breathing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Axes", meta = (ClampMin = "0", ClampMax = "2"))
	int32 SpineBendAxis = 0;

	/** Spine twist axis: 0=Local X/Roll, 1=Local Y/Pitch, 2=Local Z/Yaw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Axes", meta = (ClampMin = "0", ClampMax = "2"))
	int32 SpineTwistAxis = 2;

	/** Negate leg swing direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Axes")
	bool bNegateLegSwing = false;

	/** Negate arm swing direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Axes")
	bool bNegateArmSwing = false;

	/** Negate spine bend direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Legacy|Axes")
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

	/** Legacy direct upper-arm rest pose. Kept for asset compatibility; the current procedural path does not apply it directly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	FRotator ArmRestR = FRotator::ZeroRotator;

	/** Legacy direct upper-arm rest pose. Kept for asset compatibility; the current procedural path does not apply it directly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	FRotator ArmRestL = FRotator::ZeroRotator;

	/** Legacy direct forearm rest pose. Kept for asset compatibility; the current procedural path does not apply it directly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	FRotator ForearmRestR = FRotator::ZeroRotator;

	/** Legacy direct forearm rest pose. Kept for asset compatibility; the current procedural path does not apply it directly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	FRotator ForearmRestL = FRotator::ZeroRotator;

	/** Additive shoulder relaxation for the current rig. Default 0 while arm lowering axis is still being calibrated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	float ArmRestShoulderDeg = 0.f;

	/** Small elbow softness applied through ElbowBendAxis. Kept low to avoid fighting the reference T-pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	float ArmRestElbowDeg = 4.f;

	/** Upper-arm lowering applied before the walk swing so the arms hang near the body instead of staying in T-pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	float ArmRestLowerDeg = 78.f;

	/** Flip both arm-lower signs if the imported skeleton mirrors the shoulder axis the other way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Tuning|Rest")
	bool bInvertArmRestLowerDirection = false;

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
	void ApplyPreviewInputState();
	void ResetProceduralOutputs();
	void ComputeWalkCycle(float DeltaSeconds);
	void ComputeSwimCycle(float DeltaSeconds);
	void ComputePosturePose();
	void ComputeRestPose();
	void ComputeBreathing(float DeltaSeconds);
	void ComputeSubMotion();
	void ComputeUpperBodyAim();
	void ComputeHandIK();
	void ComputeFootIK();
	void DrawProceduralDebug() const;
	static FRotator MakeAxisRotator(float Degrees, int32 Axis, bool bNegate);

	UPROPERTY()
	TWeakObjectPtr<ASubCrewCharacter> CrewCharacter;

	UPROPERTY()
	TWeakObjectPtr<USubCrewMovementComponent> CrewMovement;

public:
	float WalkPhase = 0.f;
private:
	float BreathPhase = 0.f;
	float SwimPhase = 0.f;
	float DistanceTraveled = 0.f;
	float SmoothedSpeedAlpha = 0.f;
	float SmoothedWaterSprintAlpha = 0.f;
	bool bUsingPreviewInputsThisFrame = false;
	FVector LastPosition = FVector::ZeroVector;
};
