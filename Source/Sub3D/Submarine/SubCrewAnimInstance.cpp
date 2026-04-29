#include "SubCrewAnimInstance.h"
#include "SubCrewCharacter.h"
#include "SubCrewMovementComponent.h"
#include "SubmarineBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace
{
float WrapPhase01(float Phase01)
{
	return FMath::Frac(Phase01 + 10.f);
}

float SmoothStep01(float Edge0, float Edge1, float Value)
{
	const float Alpha = FMath::Clamp((Value - Edge0) / FMath::Max(KINDA_SMALL_NUMBER, Edge1 - Edge0), 0.f, 1.f);
	return Alpha * Alpha * (3.f - 2.f * Alpha);
}

float FootPlantAlphaFromPhase01(float Phase01)
{
	const float WrappedPhase = WrapPhase01(Phase01);
	const float PlantIn = SmoothStep01(0.02f, 0.10f, WrappedPhase);
	const float PlantOut = 1.f - SmoothStep01(0.46f, 0.56f, WrappedPhase);
	return FMath::Clamp(PlantIn * PlantOut, 0.f, 1.f);
}
}

void USubCrewAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* Owner = TryGetPawnOwner();
	if (ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(Owner))
	{
		CrewCharacter = Crew;
		CrewMovement = Cast<USubCrewMovementComponent>(Crew->GetCharacterMovement());
		LastPosition = Crew->GetActorLocation();
	}
}

void USubCrewAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!CrewCharacter.IsValid())
	{
		CrewCharacter = Cast<ASubCrewCharacter>(TryGetPawnOwner());
	}

	if (!CrewMovement.IsValid() && CrewCharacter.IsValid())
	{
		CrewMovement = CrewCharacter->GetCrewMovement();
	}

	if (!CrewCharacter.IsValid() || !CrewMovement.IsValid() || DeltaSeconds <= 0.f)
	{
		return;
	}

	LocomotionState = FCrewAnimLocomotionState();

	// Reset all bone transforms
	Proc_Pelvis_Rot = FRotator::ZeroRotator;
	Proc_Pelvis_Offset = FVector::ZeroVector;
	Proc_Spine01_Rot = FRotator::ZeroRotator;
	Proc_Spine02_Rot = FRotator::ZeroRotator;
	Proc_Spine03_Rot = FRotator::ZeroRotator;
	Proc_Spine04_Rot = FRotator::ZeroRotator;
	Proc_Spine05_Rot = FRotator::ZeroRotator;
	Proc_Neck01_Rot = FRotator::ZeroRotator;
	Proc_Head_Rot = FRotator::ZeroRotator;
	Proc_ThighR_Rot = FRotator::ZeroRotator;
	Proc_ThighL_Rot = FRotator::ZeroRotator;
	Proc_CalfR_Rot = FRotator::ZeroRotator;
	Proc_CalfL_Rot = FRotator::ZeroRotator;
	Proc_FootR_Rot = FRotator::ZeroRotator;
	Proc_FootL_Rot = FRotator::ZeroRotator;
	Proc_UpperarmR_Rot = FRotator::ZeroRotator;
	Proc_UpperarmL_Rot = FRotator::ZeroRotator;
	Proc_LowerarmR_Rot = FRotator::ZeroRotator;
	Proc_LowerarmL_Rot = FRotator::ZeroRotator;

	ReadInputState();

	if (bIsSwimming)
	{
		ComputeSwimCycle(DeltaSeconds);
	}
	else
	{
		ComputeWalkCycle(DeltaSeconds);
		ComputeBreathing(DeltaSeconds);
	}

	ComputePosture();
	ComputeSubMotion();
	ComputeUpperBodyAim();
	ComputeHandIK();
	ComputeFootIK();

	// Arm rest pose — direct FRotator per arm, no axis guessing
	if (!bIsSwimming)
	{
		Proc_UpperarmR_Rot += ArmRestR;
		Proc_UpperarmL_Rot += ArmRestL;
		Proc_LowerarmR_Rot += ForearmRestR;
		Proc_LowerarmL_Rot += ForearmRestL;
	}
}


// ═══════════════════════════════════════════════════════════
// AXIS HELPER
// ═══════════════════════════════════════════════════════════

FRotator USubCrewAnimInstance::MakeAxisRotator(float Degrees, int32 Axis, bool bNegate)
{
	const float D = bNegate ? -Degrees : Degrees;
	switch (Axis)
	{
	case 0: return FRotator(D, 0.f, 0.f);  // Pitch
	case 1: return FRotator(0.f, D, 0.f);  // Yaw
	case 2: return FRotator(0.f, 0.f, D);  // Roll
	default: return FRotator(D, 0.f, 0.f);
	}
}


// ═══════════════════════════════════════════════════════════
// READ INPUT STATE
// ═══════════════════════════════════════════════════════════

void USubCrewAnimInstance::ReadInputState()
{
	ASubCrewCharacter* Crew = CrewCharacter.Get();
	USubCrewMovementComponent* CMC = CrewMovement.Get();

	MoveIntent = CMC->GetLastMoveIntent();
	LocomotionFrame = CMC->GetLastLocomotionFrame();
	LocomotionState.MoveIntent = MoveIntent;
	LocomotionState.Frame = LocomotionFrame;

	Speed = LocomotionFrame.Speed2D;
	Direction = LocomotionFrame.DirectionDeg;
	bIsMoving = LocomotionFrame.bIsMoving;
	bIsSwimming = LocomotionFrame.bIsSwimming || Crew->bIsSwimmingByFlood;
	bIsRunning = LocomotionFrame.bIsRunning;
	PostureAlpha = LocomotionFrame.PostureAlpha;
	Stance = LocomotionFrame.Stance;
	Gait = LocomotionFrame.Gait;

	SubPitchDeg = CMC->LocalSubAngularVelocityDegrees.Y;
	SubRollDeg = CMC->LocalSubAngularVelocityDegrees.X;
	SubAccelForward = CMC->LocalSubLinearAcceleration.X;
	SubAccelLateral = CMC->LocalSubLinearAcceleration.Y;
	SupportQuality = LocomotionFrame.SupportQuality01;
	LocalTurnRateDegPerSec = LocomotionFrame.LocalTurnRateDegPerSec;
}


// ═══════════════════════════════════════════════════════════
// WALK/RUN CYCLE — Richard Williams 4-phase approach
// Contact → Down → Pass → Up per leg, per stride
// ═══════════════════════════════════════════════════════════

void USubCrewAnimInstance::ComputeWalkCycle(float DeltaSeconds)
{
	USubCrewMovementComponent* CMC = CrewMovement.Get();
	const float CrawlAlpha = FMath::Clamp(FMath::GetRangePct(0.3f, 0.f, PostureAlpha), 0.f, 1.f);
	if (CrawlAlpha > 0.f)
	{
		ComputeCrawlCycle(DeltaSeconds);
		return;
	}

	const float CrouchAlpha = FMath::Clamp(FMath::GetRangePct(0.8f, 0.3f, PostureAlpha), 0.f, 1.f);

	// Determine run state
	bIsRunning = CMC->bIsRunning && Speed > 100.f;

	// Blend between walk and run parameters
	const float RunBlend = bIsRunning ? FMath::Clamp((Speed - 300.f) / 200.f, 0.f, 1.f) : 0.f;
	const float CycleRate = FMath::Lerp(WalkCycleRate, RunCycleRate, RunBlend);
	const float MaxSpd = FMath::Lerp(300.f, RunMaxSpeed, RunBlend);
	const float LegSwingMax = FMath::Lerp(WalkLegSwingDeg, RunLegSwingDeg, RunBlend);
	const float ArmSwingMax = FMath::Lerp(WalkArmSwingDeg, RunArmSwingDeg, RunBlend);
	const float BobMax = FMath::Lerp(WalkPelvisBobCm, RunPelvisBobCm, RunBlend);
	const float LeanMax = FMath::Lerp(WalkSpineLeanDeg, RunSpineLeanDeg, RunBlend);

	// Advance phase based on distance
	if (bIsMoving)
	{
		DistanceTraveled += Speed * DeltaSeconds;
	}
	WalkPhase = FMath::Fmod(DistanceTraveled * CycleRate, 2.f * PI);
	LocomotionState.StridePhase01 = WrapPhase01(WalkPhase / (2.f * PI));
	if (bIsMoving)
	{
		LocomotionState.RightFootPlantAlpha = FootPlantAlphaFromPhase01(LocomotionState.StridePhase01);
		LocomotionState.LeftFootPlantAlpha = FootPlantAlphaFromPhase01(LocomotionState.StridePhase01 + 0.5f);
	}
	else
	{
		LocomotionState.RightFootPlantAlpha = 1.f;
		LocomotionState.LeftFootPlantAlpha = 1.f;
	}

	// Speed alpha: 0 at idle, 1 at max
	const float SpeedAlpha = FMath::Clamp(Speed / MaxSpd, 0.f, 1.f);
	const float UprightAlpha = FMath::Clamp(FMath::GetRangePct(0.25f, 1.f, PostureAlpha), 0.f, 1.f);
	const float LegAmp = LegSwingMax * SpeedAlpha * UprightAlpha * FMath::Lerp(0.55f, 1.f, 1.f - CrouchAlpha);
	const float ArmAmp = ArmSwingMax * SpeedAlpha * UprightAlpha * FMath::Lerp(0.3f, 1.f, 1.f - CrouchAlpha);
	const float BobAmp = BobMax * SpeedAlpha * UprightAlpha * FMath::Lerp(0.5f, 1.f, 1.f - CrouchAlpha);

	// ── THIGHS: asymmetric swing (more forward, less back) ──
	// Forward swing is larger than back swing → more natural look
	const float RPhase = WalkPhase;
	const float LPhase = WalkPhase + PI;
	const float RawR = FMath::Sin(RPhase);
	const float RawL = FMath::Sin(LPhase);

	// Asymmetric: forward (positive) gets 60% amplitude, back (negative) gets 40%
	const float RThighSwing = (RawR > 0.f ? RawR * 0.6f : RawR * 0.4f) * LegAmp * 2.f;
	const float LThighSwing = (RawL > 0.f ? RawL * 0.6f : RawL * 0.4f) * LegAmp * 2.f;

	Proc_ThighR_Rot = MakeAxisRotator(RThighSwing, LegSwingAxis, bNegateLegSwing);
	Proc_ThighL_Rot = MakeAxisRotator(LThighSwing, LegSwingAxis, bNegateLegSwing);

	// ── CALVES: knee bend peaks at pass position ──
	// More knee lift during run (RunBlend increases multiplier)
	const float KneeMultiplier = FMath::Lerp(WalkCalfBendMultiplier, WalkCalfBendMultiplier * 2.f, RunBlend);
	const float RKnee = CrouchAlpha * 18.f + FMath::Max(0.f, FMath::Sin(RPhase + 0.3f)) * KneeMultiplier * LegAmp;
	const float LKnee = CrouchAlpha * 18.f + FMath::Max(0.f, FMath::Sin(LPhase + 0.3f)) * KneeMultiplier * LegAmp;
	Proc_CalfR_Rot = MakeAxisRotator(-RKnee, LegSwingAxis, bNegateLegSwing);
	Proc_CalfL_Rot = MakeAxisRotator(-LKnee, LegSwingAxis, bNegateLegSwing);

	// ── FEET: toe-off when pushing, dorsiflexion when lifting ──
	const float RFoot = -RThighSwing * 0.2f + RKnee * 0.3f;
	const float LFoot = -LThighSwing * 0.2f + LKnee * 0.3f;
	Proc_FootR_Rot = MakeAxisRotator(RFoot, LegSwingAxis, bNegateLegSwing);
	Proc_FootL_Rot = MakeAxisRotator(LFoot, LegSwingAxis, bNegateLegSwing);

	// ── ARMS: counter-swing, opposite to legs ──
	const float RArmSwing = FMath::Sin(RPhase + PI) * ArmAmp;
	const float LArmSwing = FMath::Sin(LPhase + PI) * ArmAmp;
	Proc_UpperarmR_Rot = MakeAxisRotator(RArmSwing, ArmSwingAxis, bNegateArmSwing);
	Proc_UpperarmL_Rot = MakeAxisRotator(-LArmSwing, ArmSwingAxis, bNegateArmSwing);  // L negated for mirror

	// Forearm bend: elbow flex on ElbowBendAxis
	const float RElbow = CrouchAlpha * 10.f + FMath::Max(0.f, -RArmSwing) * 0.8f + FMath::Abs(RArmSwing) * 0.2f;
	const float LElbow = CrouchAlpha * 10.f + FMath::Max(0.f, -LArmSwing) * 0.8f + FMath::Abs(LArmSwing) * 0.2f;
	Proc_LowerarmR_Rot = MakeAxisRotator(-RElbow, ElbowBendAxis, false);
	Proc_LowerarmL_Rot = MakeAxisRotator(LElbow, ElbowBendAxis, false);  // L positive for mirror

	// ── PELVIS: bob (double freq) + twist + tilt ──
	// Down position at contact (phase 0, PI), up at pass (PI/2, 3PI/2)
	Proc_Pelvis_Offset.Z = -FMath::Abs(FMath::Sin(WalkPhase)) * BobAmp + BobAmp * 0.5f;
	// Hip twist: hips rotate with the stride
	Proc_Pelvis_Rot += MakeAxisRotator(FMath::Sin(WalkPhase) * SpeedAlpha * 4.f, SpineTwistAxis, false);
	// Hip lateral tilt (shift weight side to side)
	const float HipTilt = FMath::Sin(WalkPhase) * SpeedAlpha * 2.f;
	Proc_Pelvis_Rot.Roll += HipTilt;

	// ── SPINE: forward lean proportional to speed ──
	const float Lean = LeanMax * SpeedAlpha;
	Proc_Spine03_Rot += MakeAxisRotator(Lean * 0.4f, SpineBendAxis, bNegateSpineBend);
	Proc_Spine04_Rot += MakeAxisRotator(Lean * 0.35f, SpineBendAxis, bNegateSpineBend);
	Proc_Spine05_Rot += MakeAxisRotator(Lean * 0.25f, SpineBendAxis, bNegateSpineBend);
}

void USubCrewAnimInstance::ComputeCrawlCycle(float DeltaSeconds)
{
	if (bIsMoving)
	{
		DistanceTraveled += Speed * DeltaSeconds * 0.45f;
	}
	WalkPhase = FMath::Fmod(DistanceTraveled * CrawlCycleRate, 2.f * PI);
	LocomotionState.StridePhase01 = WrapPhase01(WalkPhase / (2.f * PI));
	if (bIsMoving)
	{
		LocomotionState.RightFootPlantAlpha = FootPlantAlphaFromPhase01(LocomotionState.StridePhase01);
		LocomotionState.LeftFootPlantAlpha = FootPlantAlphaFromPhase01(LocomotionState.StridePhase01 + 0.5f);
	}
	else
	{
		LocomotionState.RightFootPlantAlpha = 1.f;
		LocomotionState.LeftFootPlantAlpha = 1.f;
	}

	const float SpeedAlpha = FMath::Clamp(Speed / 140.f, 0.f, 1.f);
	const float RPhase = WalkPhase;
	const float LPhase = WalkPhase + PI;
	const float RLegSwing = FMath::Sin(RPhase) * CrawlLegSwingDeg * SpeedAlpha;
	const float LLegSwing = FMath::Sin(LPhase) * CrawlLegSwingDeg * SpeedAlpha;
	const float RArmSwing = FMath::Sin(RPhase + PI) * CrawlArmSwingDeg * SpeedAlpha;
	const float LArmSwing = FMath::Sin(LPhase + PI) * CrawlArmSwingDeg * SpeedAlpha;

	Proc_ThighR_Rot = MakeAxisRotator(CrawlThighFoldDeg + RLegSwing, LegSwingAxis, bNegateLegSwing);
	Proc_ThighL_Rot = MakeAxisRotator(CrawlThighFoldDeg + LLegSwing, LegSwingAxis, bNegateLegSwing);
	Proc_CalfR_Rot = MakeAxisRotator(-(CrawlCalfFoldDeg - FMath::Max(0.f, RLegSwing) * 0.35f), LegSwingAxis, bNegateLegSwing);
	Proc_CalfL_Rot = MakeAxisRotator(-(CrawlCalfFoldDeg - FMath::Max(0.f, LLegSwing) * 0.35f), LegSwingAxis, bNegateLegSwing);
	Proc_FootR_Rot = MakeAxisRotator(CrawlFootCompDeg + FMath::Max(0.f, -RLegSwing) * 0.2f, LegSwingAxis, bNegateLegSwing);
	Proc_FootL_Rot = MakeAxisRotator(CrawlFootCompDeg + FMath::Max(0.f, -LLegSwing) * 0.2f, LegSwingAxis, bNegateLegSwing);

	Proc_UpperarmR_Rot = MakeAxisRotator(CrawlArmForwardDeg + RArmSwing, ArmSwingAxis, bNegateArmSwing);
	Proc_UpperarmL_Rot = MakeAxisRotator(-(CrawlArmForwardDeg + LArmSwing), ArmSwingAxis, bNegateArmSwing);

	const float RElbow = CrawlElbowBendDeg + FMath::Max(0.f, -RArmSwing) * 0.3f;
	const float LElbow = CrawlElbowBendDeg + FMath::Max(0.f, -LArmSwing) * 0.3f;
	Proc_LowerarmR_Rot = MakeAxisRotator(-RElbow, ElbowBendAxis, false);
	Proc_LowerarmL_Rot = MakeAxisRotator(LElbow, ElbowBendAxis, false);
	Proc_Pelvis_Offset.Z = -FMath::Abs(FMath::Sin(WalkPhase)) * CrawlPelvisBobCm;
}


// ═══════════════════════════════════════════════════════════
// BREATHING
// ═══════════════════════════════════════════════════════════

void USubCrewAnimInstance::ComputeBreathing(float DeltaSeconds)
{
	BreathPhase += DeltaSeconds * BreathingRate * 2.f * PI;
	if (BreathPhase > 2.f * PI) BreathPhase -= 2.f * PI;

	const float BreathAlpha = 1.f - FMath::Clamp(Speed / 100.f, 0.f, 1.f);
	const float BreathVal = FMath::Sin(BreathPhase) * BreathingAmplitudeDeg * BreathAlpha;

	Proc_Spine04_Rot += MakeAxisRotator(BreathVal * 0.4f, SpineBendAxis, bNegateSpineBend);
	Proc_Spine05_Rot += MakeAxisRotator(BreathVal * 0.6f, SpineBendAxis, bNegateSpineBend);
}


// ═══════════════════════════════════════════════════════════
// POSTURE
// ═══════════════════════════════════════════════════════════

void USubCrewAnimInstance::ComputePosture()
{
	const float CrouchAlpha = FMath::Clamp(FMath::GetRangePct(0.8f, 0.3f, PostureAlpha), 0.f, 1.f);
	const float CrawlAlpha = FMath::Clamp(FMath::GetRangePct(0.3f, 0.f, PostureAlpha), 0.f, 1.f);
	const float TotalBend = CrouchAlpha * (MaxPostureBendDeg * 0.35f) + CrawlAlpha * MaxPostureBendDeg;

	Proc_Spine01_Rot += MakeAxisRotator(TotalBend * 0.30f, SpineBendAxis, bNegateSpineBend);
	Proc_Spine02_Rot += MakeAxisRotator(TotalBend * 0.25f, SpineBendAxis, bNegateSpineBend);
	Proc_Spine03_Rot += MakeAxisRotator(TotalBend * 0.20f, SpineBendAxis, bNegateSpineBend);
	Proc_Spine04_Rot += MakeAxisRotator(TotalBend * 0.15f, SpineBendAxis, bNegateSpineBend);
	Proc_Spine05_Rot += MakeAxisRotator(TotalBend * 0.10f, SpineBendAxis, bNegateSpineBend);

	const float NeckComp = -TotalBend * 0.3f;
	Proc_Neck01_Rot += MakeAxisRotator(NeckComp, SpineBendAxis, bNegateSpineBend);

	Proc_Pelvis_Offset.Z -= CrouchAlpha * CrouchPelvisDropCm + CrawlAlpha * (CrouchPelvisDropCm + 6.f);

	const float HipFoldDeg = CrouchAlpha * CrouchThighFoldDeg + CrawlAlpha * CrawlThighFoldDeg;
	const float KneeFoldDeg = CrouchAlpha * CrouchCalfFoldDeg + CrawlAlpha * CrawlCalfFoldDeg;
	const float FootCompDeg = CrouchAlpha * CrouchFootCompDeg + CrawlAlpha * CrawlFootCompDeg;
	Proc_ThighR_Rot += MakeAxisRotator(HipFoldDeg, LegSwingAxis, bNegateLegSwing);
	Proc_ThighL_Rot += MakeAxisRotator(HipFoldDeg, LegSwingAxis, bNegateLegSwing);
	Proc_CalfR_Rot += MakeAxisRotator(-KneeFoldDeg, LegSwingAxis, bNegateLegSwing);
	Proc_CalfL_Rot += MakeAxisRotator(-KneeFoldDeg, LegSwingAxis, bNegateLegSwing);
	Proc_FootR_Rot += MakeAxisRotator(FootCompDeg, LegSwingAxis, bNegateLegSwing);
	Proc_FootL_Rot += MakeAxisRotator(FootCompDeg, LegSwingAxis, bNegateLegSwing);

	const float ArmForwardDeg = CrouchAlpha * CrouchArmForwardDeg + CrawlAlpha * CrawlArmForwardDeg;
	const float ElbowBendDeg = CrouchAlpha * CrouchElbowBendDeg + CrawlAlpha * CrawlElbowBendDeg;
	Proc_UpperarmR_Rot += MakeAxisRotator(ArmForwardDeg, ArmSwingAxis, bNegateArmSwing);
	Proc_UpperarmL_Rot += MakeAxisRotator(-ArmForwardDeg, ArmSwingAxis, bNegateArmSwing);
	Proc_LowerarmR_Rot += MakeAxisRotator(-ElbowBendDeg, ElbowBendAxis, false);
	Proc_LowerarmL_Rot += MakeAxisRotator(ElbowBendDeg, ElbowBendAxis, false);
}


// ═══════════════════════════════════════════════════════════
// SUB MOTION
// ═══════════════════════════════════════════════════════════

void USubCrewAnimInstance::ComputeSubMotion()
{
	const float LeanPitch = SubPitchDeg * SubLeanMultiplier;
	const float LeanRoll = SubRollDeg * SubLeanMultiplier;

	Proc_Spine03_Rot += MakeAxisRotator(LeanPitch * 0.3f, SpineBendAxis, false);
	Proc_Spine04_Rot += MakeAxisRotator(LeanPitch * 0.4f, SpineBendAxis, false);
	Proc_Spine05_Rot += MakeAxisRotator(LeanPitch * 0.3f, SpineBendAxis, false);

	// Side lean — use twist axis (same axis as upper body turn)
	Proc_Spine03_Rot += MakeAxisRotator(LeanRoll * 0.3f, SpineTwistAxis, false);
	Proc_Spine04_Rot += MakeAxisRotator(LeanRoll * 0.4f, SpineTwistAxis, false);
	Proc_Spine05_Rot += MakeAxisRotator(LeanRoll * 0.3f, SpineTwistAxis, false);

	const float StumblePitch = SubAccelForward * SubStumbleMultiplier;
	const float StumbleRoll = SubAccelLateral * SubStumbleMultiplier;
	const float Instability = 1.f - SupportQuality;
	const float TurnTwist = FMath::Clamp(LocalTurnRateDegPerSec * 0.025f, -10.f, 10.f);

	Proc_Pelvis_Rot += MakeAxisRotator(StumblePitch * Instability * 2.f, SpineBendAxis, false);
	Proc_Pelvis_Rot += MakeAxisRotator(StumbleRoll * Instability * 2.f, SpineTwistAxis, false);
	Proc_Pelvis_Rot += MakeAxisRotator(TurnTwist * 0.4f, SpineTwistAxis, false);
	Proc_Spine01_Rot += MakeAxisRotator(TurnTwist * 0.3f, SpineTwistAxis, false);
	Proc_Spine02_Rot += MakeAxisRotator(TurnTwist * 0.2f, SpineTwistAxis, false);
}


// ═══════════════════════════════════════════════════════════
// UPPER BODY AIM
// ═══════════════════════════════════════════════════════════

void USubCrewAnimInstance::ComputeUpperBodyAim()
{
	ASubCrewCharacter* Crew = CrewCharacter.Get();
	const float AimAlpha = FMath::Clamp(FMath::GetRangePct(0.25f, 1.f, PostureAlpha), 0.25f, 1.f);

	if (bIsMoving)
	{
		FVector TraversalVelocityWorld = CrewMovement.Get()->Velocity;
		if (Crew->CurrentSubmarine && CrewMovement.Get()->EmbarkState == ECrewEmbarkState::Embarked)
		{
			TraversalVelocityWorld = Crew->CurrentSubmarine->GetActorTransform().TransformVectorNoScale(CrewMovement.Get()->RelativeLinearVelocity);
		}

		const float CameraYaw = Crew->GetControlRotation().Yaw;
		const float VelocityYaw = TraversalVelocityWorld.ToOrientationRotator().Yaw;
		const float RawOffset = FMath::FindDeltaAngleDegrees(VelocityYaw, CameraYaw);

		// Only apply twist when moving ROUGHLY FORWARD (not strafing)
		// |Direction| near 0 = forward, near 90 = strafe
		// Scale offset to zero during pure strafe
		const float ForwardScale = FMath::Clamp(1.f - FMath::Abs(Direction) / 60.f, 0.f, 1.f);
		const float TargetOffset = FMath::ClampAngle(RawOffset, -45.f, 45.f) * ForwardScale * AimAlpha;

		UpperBodyYawOffset = FMath::FInterpTo(UpperBodyYawOffset, TargetOffset, GetDeltaSeconds(), 5.f);
	}
	else
	{
		UpperBodyYawOffset = FMath::FInterpTo(UpperBodyYawOffset, 0.f, GetDeltaSeconds(), 3.f);
	}

	// Lower body: rotate TOWARD velocity (opposite of upper body offset)
	const float LowerYaw = FMath::Clamp(-UpperBodyYawOffset, -MaxLowerBodyYawDeg, MaxLowerBodyYawDeg);
	Proc_Pelvis_Rot += MakeAxisRotator(LowerYaw * 0.5f, SpineTwistAxis, false);
	Proc_Spine01_Rot += MakeAxisRotator(LowerYaw * 0.3f, SpineTwistAxis, false);
	Proc_Spine02_Rot += MakeAxisRotator(LowerYaw * 0.2f, SpineTwistAxis, false);

	// Upper body: twist toward camera
	Proc_Spine03_Rot += MakeAxisRotator(UpperBodyYawOffset * 0.2f, SpineTwistAxis, false);
	Proc_Spine04_Rot += MakeAxisRotator(UpperBodyYawOffset * 0.3f, SpineTwistAxis, false);
	Proc_Spine05_Rot += MakeAxisRotator(UpperBodyYawOffset * 0.3f, SpineTwistAxis, false);
	Proc_Neck01_Rot += MakeAxisRotator(UpperBodyYawOffset * 0.2f, SpineTwistAxis, false);

	// Head pitch (look up/down)
	const float CameraPitch = Crew->GetControlRotation().Pitch;
	const float HeadPitch = FMath::ClampAngle(CameraPitch, -40.f, 30.f) * AimAlpha;
	Proc_Head_Rot += MakeAxisRotator(HeadPitch * 0.6f, SpineBendAxis, false);
	Proc_Neck01_Rot += MakeAxisRotator(HeadPitch * 0.4f, SpineBendAxis, false);
}


// ═══════════════════════════════════════════════════════════
// SWIM CYCLE — breaststroke-like procedural
// ═══════════════════════════════════════════════════════════

void USubCrewAnimInstance::ComputeSwimCycle(float DeltaSeconds)
{
	SwimPhase += DeltaSeconds * SwimStrokeRate * 2.f * PI;
	if (SwimPhase > 2.f * PI) SwimPhase -= 2.f * PI;
	LocomotionState.StridePhase01 = WrapPhase01(SwimPhase / (2.f * PI));
	LocomotionState.RightFootPlantAlpha = 0.f;
	LocomotionState.LeftFootPlantAlpha = 0.f;

	const float SpeedAlpha = FMath::Clamp(Speed / 200.f, 0.f, 1.f);

	// Body horizontal — lean forward 60°
	const float SwimLean = 60.f;
	Proc_Spine01_Rot += MakeAxisRotator(SwimLean * 0.3f, SpineBendAxis, bNegateSpineBend);
	Proc_Spine02_Rot += MakeAxisRotator(SwimLean * 0.25f, SpineBendAxis, bNegateSpineBend);
	Proc_Spine03_Rot += MakeAxisRotator(SwimLean * 0.2f, SpineBendAxis, bNegateSpineBend);
	Proc_Spine04_Rot += MakeAxisRotator(SwimLean * 0.15f, SpineBendAxis, bNegateSpineBend);
	Proc_Spine05_Rot += MakeAxisRotator(SwimLean * 0.1f, SpineBendAxis, bNegateSpineBend);

	// Head counter-rotation to look forward
	Proc_Neck01_Rot += MakeAxisRotator(-SwimLean * 0.35f, SpineBendAxis, bNegateSpineBend);
	Proc_Head_Rot += MakeAxisRotator(-SwimLean * 0.25f, SpineBendAxis, bNegateSpineBend);

	// Arms — breaststroke: sweep out then pull in
	const float ArmSweep = FMath::Sin(SwimPhase) * SwimArmSwingDeg * SpeedAlpha;
	Proc_UpperarmR_Rot = MakeAxisRotator(ArmSweep, ArmSwingAxis, bNegateArmSwing);
	Proc_UpperarmL_Rot = MakeAxisRotator(-ArmSweep, ArmSwingAxis, bNegateArmSwing);

	// Forearms follow with delay
	const float ForearmSweep = FMath::Sin(SwimPhase - 0.5f) * SwimArmSwingDeg * 0.6f * SpeedAlpha;
	Proc_LowerarmR_Rot = MakeAxisRotator(-FMath::Abs(ForearmSweep), ElbowBendAxis, false);
	Proc_LowerarmL_Rot = MakeAxisRotator(FMath::Abs(ForearmSweep), ElbowBendAxis, false);

	// Legs — flutter kick (opposite phase, faster than arms)
	const float KickR = FMath::Sin(SwimPhase * 2.f) * SwimKickDeg * SpeedAlpha;
	const float KickL = FMath::Sin(SwimPhase * 2.f + PI) * SwimKickDeg * SpeedAlpha;
	Proc_ThighR_Rot = MakeAxisRotator(KickR, LegSwingAxis, bNegateLegSwing);
	Proc_ThighL_Rot = MakeAxisRotator(KickL, LegSwingAxis, bNegateLegSwing);

	// Calves follow thighs with slight delay
	const float CalfR = FMath::Sin(SwimPhase * 2.f - 0.4f) * SwimKickDeg * 0.5f * SpeedAlpha;
	const float CalfL = FMath::Sin(SwimPhase * 2.f + PI - 0.4f) * SwimKickDeg * 0.5f * SpeedAlpha;
	Proc_CalfR_Rot = MakeAxisRotator(-FMath::Abs(CalfR), LegSwingAxis, bNegateLegSwing);
	Proc_CalfL_Rot = MakeAxisRotator(-FMath::Abs(CalfL), LegSwingAxis, bNegateLegSwing);

	// Body undulation
	Proc_Pelvis_Offset.Z = FMath::Sin(SwimPhase) * 1.5f * SpeedAlpha;
}


// ═══════════════════════════════════════════════════════════
// HAND IK — read probes from movement component
// ═══════════════════════════════════════════════════════════

void USubCrewAnimInstance::ComputeHandIK()
{
	USubCrewMovementComponent* CMC = CrewMovement.Get();
	if (!CMC)
	{
		HandIK_L_Weight = 0.f;
		HandIK_R_Weight = 0.f;
		HandIK_L_Target = FVector::ZeroVector;
		HandIK_R_Target = FVector::ZeroVector;
		HandIK_L_State = FCrewHandIKState();
		HandIK_R_State = FCrewHandIKState();
		return;
	}

	// Disable IK only when swimming
	if (bIsSwimming)
	{
		HandIK_L_Weight = FMath::FInterpTo(HandIK_L_Weight, 0.f, GetDeltaSeconds(), 5.f);
		HandIK_R_Weight = FMath::FInterpTo(HandIK_R_Weight, 0.f, GetDeltaSeconds(), 5.f);
		if (HandIK_L_Weight <= KINDA_SMALL_NUMBER)
		{
			HandIK_L_Target = FVector::ZeroVector;
		}
		if (HandIK_R_Weight <= KINDA_SMALL_NUMBER)
		{
			HandIK_R_Target = FVector::ZeroVector;
		}
		HandIK_L_State = FCrewHandIKState();
		HandIK_R_State = FCrewHandIKState();
		return;
	}

	const auto PickBestProbe = [CMC](const int32* ProbeIndices, int32 ProbeCount) -> const USubCrewMovementComponent::FHandIKProbeResult*
	{
		const USubCrewMovementComponent::FHandIKProbeResult* BestProbe = nullptr;
		for (int32 Index = 0; Index < ProbeCount; ++Index)
		{
			const USubCrewMovementComponent::FHandIKProbeResult& Probe = CMC->HandProbes[ProbeIndices[Index]];
			if (!Probe.bHit)
			{
				continue;
			}

			if (!BestProbe || Probe.Distance < BestProbe->Distance)
			{
				BestProbe = &Probe;
			}
		}
		return BestProbe;
	};

	const float ProneHandAlpha = FMath::Clamp(FMath::GetRangePct(0.35f, 0.f, PostureAlpha), 0.f, 1.f);
	const float CrouchBraceAlpha = FMath::Clamp(FMath::GetRangePct(0.85f, 0.35f, PostureAlpha), 0.f, 1.f) * 0.65f;
	const float InstabilityAlpha = FMath::Clamp(
		(1.f - SupportQuality)
		+ FMath::Abs(SubAccelForward) * 0.002f
		+ FMath::Abs(SubAccelLateral) * 0.002f
		+ FMath::Abs(LocalTurnRateDegPerSec) * 0.01f,
		0.f,
		1.f);
	const float SpeedGate = FMath::Lerp(1.f, 0.25f, FMath::Clamp(Speed / 260.f, 0.f, 1.f));
	const float BraceSourceAlpha = FMath::Max(ProneHandAlpha, FMath::Max(CrouchBraceAlpha, InstabilityAlpha));
	const float BraceAlpha = FMath::Clamp(BraceSourceAlpha * SpeedGate, 0.f, 1.f);

	const int32 LeftProbeIndices[] = { 0, 2, 4 };
	const int32 RightProbeIndices[] = { 1, 3, 5 };
	const USubCrewMovementComponent::FHandIKProbeResult* ProbeL = PickBestProbe(LeftProbeIndices, UE_ARRAY_COUNT(LeftProbeIndices));
	const USubCrewMovementComponent::FHandIKProbeResult* ProbeR = PickBestProbe(RightProbeIndices, UE_ARRAY_COUNT(RightProbeIndices));

	const auto UpdateHand = [this, BraceAlpha](const USubCrewMovementComponent::FHandIKProbeResult* Probe, float& Weight, FVector& Target, FCrewHandIKState& State)
	{
		const float TargetWeight = Probe ? BraceAlpha : 0.f;
		Weight = FMath::FInterpTo(Weight, TargetWeight, GetDeltaSeconds(), Probe ? 6.f : 4.f);

		if (Probe)
		{
			Target = Probe->WorldLocation;
			State.bHasHit = true;
			State.TargetWorldLocation = Probe->WorldLocation;
			State.TargetWorldNormal = Probe->WorldNormal;
		}
		else if (Weight <= KINDA_SMALL_NUMBER)
		{
			Target = FVector::ZeroVector;
			State = FCrewHandIKState();
		}

		State.Weight = Weight;
	};

	UpdateHand(ProbeL, HandIK_L_Weight, HandIK_L_Target, HandIK_L_State);
	UpdateHand(ProbeR, HandIK_R_Weight, HandIK_R_Target, HandIK_R_State);
}


// ═══════════════════════════════════════════════════════════
// FOOT IK — read traces from movement component
// ═══════════════════════════════════════════════════════════

void USubCrewAnimInstance::ComputeFootIK()
{
	USubCrewMovementComponent* CMC = CrewMovement.Get();
	if (!CMC || bIsSwimming)
	{
		FootIK_R_Offset = FVector::ZeroVector;
		FootIK_L_Offset = FVector::ZeroVector;
		FootIK_R_State = FCrewFootIKState();
		FootIK_L_State = FCrewFootIKState();
		return;
	}

	FootIK_R_State = CMC->FootIK_R_State;
	FootIK_L_State = CMC->FootIK_L_State;

	const float RightPlantAlpha = bIsMoving ? LocomotionState.RightFootPlantAlpha : 1.f;
	const float LeftPlantAlpha = bIsMoving ? LocomotionState.LeftFootPlantAlpha : 1.f;
	FootIK_R_State.PlantAlpha = RightPlantAlpha;
	FootIK_L_State.PlantAlpha = LeftPlantAlpha;
	FootIK_R_State.bIsPlanted = FootIK_R_State.bHasHit && RightPlantAlpha > 0.5f;
	FootIK_L_State.bIsPlanted = FootIK_L_State.bHasHit && LeftPlantAlpha > 0.5f;

	FootIK_R_Offset = FootIK_R_State.Offset * FootIK_R_State.Weight * RightPlantAlpha;
	FootIK_L_Offset = FootIK_L_State.Offset * FootIK_L_State.Weight * LeftPlantAlpha;
}
