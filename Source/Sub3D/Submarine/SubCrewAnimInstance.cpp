#include "SubCrewAnimInstance.h"

#include "CrewProceduralAnimProfile.h"
#include "SubCrewCharacter.h"
#include "SubCrewMovementComponent.h"
#include "SubmarineBase.h"

#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

namespace
{
constexpr float TwoPi = 2.f * PI;

float WrapPhase01(float Phase01)
{
	return FMath::Frac(Phase01 + 10.f);
}

float SmoothStep01(float Edge0, float Edge1, float Value)
{
	const float Alpha = FMath::Clamp((Value - Edge0) / FMath::Max(KINDA_SMALL_NUMBER, Edge1 - Edge0), 0.f, 1.f);
	return Alpha * Alpha * (3.f - 2.f * Alpha);
}

float WrappedPhaseDistance01(float A, float B)
{
	const float Delta = FMath::Abs(WrapPhase01(A) - WrapPhase01(B));
	return FMath::Min(Delta, 1.f - Delta);
}

float PhasePulse01(float Phase01, float Center01, float HalfWidth01)
{
	const float Distance = WrappedPhaseDistance01(Phase01, Center01);
	return 1.f - SmoothStep01(0.f, FMath::Max(0.001f, HalfWidth01), Distance);
}

float PhaseWindow01(float Phase01, float Start01, float End01, float Blend01)
{
	const float WrappedPhase = WrapPhase01(Phase01);
	if (Start01 <= End01)
	{
		const float In = SmoothStep01(Start01, Start01 + Blend01, WrappedPhase);
		const float Out = 1.f - SmoothStep01(End01 - Blend01, End01, WrappedPhase);
		return FMath::Clamp(In * Out, 0.f, 1.f);
	}

	return FMath::Max(
		PhaseWindow01(WrappedPhase, Start01, 1.f, Blend01),
		PhaseWindow01(WrappedPhase, 0.f, End01, Blend01));
}

FRotator MakeAxisRotatorInternal(float Degrees, int32 Axis, bool bNegate)
{
	const float SignedDegrees = bNegate ? -Degrees : Degrees;
	switch (Axis)
	{
	case 0:
		return FRotator(0.f, 0.f, SignedDegrees); // Local X, Unreal Roll.
	case 1:
		return FRotator(SignedDegrees, 0.f, 0.f); // Local Y, Unreal Pitch.
	case 2:
		return FRotator(0.f, SignedDegrees, 0.f); // Local Z, Unreal Yaw.
	default:
		return FRotator(0.f, 0.f, SignedDegrees);
	}
}

FRotator MakeAxisRotatorInternal(float Degrees, const FCrewProceduralAxisTuning& Axis)
{
	return MakeAxisRotatorInternal(Degrees, Axis.Axis, Axis.bNegate);
}

FCrewProceduralGroundedTuning GetGroundedTuning(const USubCrewAnimInstance& AnimInstance)
{
	return AnimInstance.ProceduralProfile ? AnimInstance.ProceduralProfile->Grounded : AnimInstance.GroundedTuning;
}

FCrewProceduralRigAxisProfile GetRigAxes(const USubCrewAnimInstance& AnimInstance)
{
	if (AnimInstance.ProceduralProfile)
	{
		return AnimInstance.ProceduralProfile->RigAxes;
	}

	return AnimInstance.RigAxesTuning;
}

FCrewProceduralSubMotionTuning GetSubMotionTuning(const USubCrewAnimInstance& AnimInstance)
{
	return AnimInstance.ProceduralProfile ? AnimInstance.ProceduralProfile->SubMotion : AnimInstance.SubMotionTuning;
}

FCrewProceduralSwimTuning GetSwimTuning(const USubCrewAnimInstance& AnimInstance)
{
	return AnimInstance.ProceduralProfile ? AnimInstance.ProceduralProfile->Swim : AnimInstance.SwimTuning;
}

float GetGaitCycleLengthCm(const FCrewProceduralGaitTuning& Gait)
{
	const float CycleRate = FMath::Max(0.1f, Gait.StepRate * 0.5f);
	return FMath::Max(1.f, Gait.SpeedCmS / CycleRate);
}

FCrewProceduralGaitTuning LerpGait(const FCrewProceduralGaitTuning& A, const FCrewProceduralGaitTuning& B, float Alpha)
{
	const float T = FMath::Clamp(Alpha, 0.f, 1.f);
	FCrewProceduralGaitTuning Result;
	Result.SpeedCmS = FMath::Lerp(A.SpeedCmS, B.SpeedCmS, T);
	Result.StepRate = FMath::Lerp(A.StepRate, B.StepRate, T);
	Result.StanceFraction = FMath::Lerp(A.StanceFraction, B.StanceFraction, T);
	Result.ThighForwardDeg = FMath::Lerp(A.ThighForwardDeg, B.ThighForwardDeg, T);
	Result.ThighBackDeg = FMath::Lerp(A.ThighBackDeg, B.ThighBackDeg, T);
	Result.KneeBendDeg = FMath::Lerp(A.KneeBendDeg, B.KneeBendDeg, T);
	Result.FootPitchDeg = FMath::Lerp(A.FootPitchDeg, B.FootPitchDeg, T);
	Result.ToeOffDeg = FMath::Lerp(A.ToeOffDeg, B.ToeOffDeg, T);
	Result.PelvisBobCm = FMath::Lerp(A.PelvisBobCm, B.PelvisBobCm, T);
	Result.PelvisCompressionCm = FMath::Lerp(A.PelvisCompressionCm, B.PelvisCompressionCm, T);
	Result.SpineLeanDeg = FMath::Lerp(A.SpineLeanDeg, B.SpineLeanDeg, T);
	Result.PelvisRollDeg = FMath::Lerp(A.PelvisRollDeg, B.PelvisRollDeg, T);
	Result.PelvisTwistDeg = FMath::Lerp(A.PelvisTwistDeg, B.PelvisTwistDeg, T);
	Result.ArmSwingDeg = FMath::Lerp(A.ArmSwingDeg, B.ArmSwingDeg, T);
	Result.ElbowBendDeg = FMath::Lerp(A.ElbowBendDeg, B.ElbowBendDeg, T);
	return Result;
}

FCrewProceduralGaitTuning ResolveGroundedGait(
	const FCrewProceduralGroundedTuning& Grounded,
	float SpeedCmS,
	float& OutRunBlendAlpha)
{
	const float WalkToJog = FMath::Clamp(
		(SpeedCmS - Grounded.Walk.SpeedCmS) / FMath::Max(1.f, Grounded.Jog.SpeedCmS - Grounded.Walk.SpeedCmS),
		0.f,
		1.f);
	const float JogToRun = FMath::Clamp(
		(SpeedCmS - Grounded.Jog.SpeedCmS) / FMath::Max(1.f, Grounded.Run.SpeedCmS - Grounded.Jog.SpeedCmS),
		0.f,
		1.f);

	if (SpeedCmS > Grounded.Jog.SpeedCmS)
	{
		OutRunBlendAlpha = JogToRun;
		return LerpGait(Grounded.Jog, Grounded.Run, JogToRun);
	}

	OutRunBlendAlpha = 0.f;
	return LerpGait(Grounded.Walk, Grounded.Jog, WalkToJog);
}

struct FCrewLegPhaseWeights
{
	float Contact = 0.f;
	float WeightAccept = 0.f;
	float Compression = 0.f;
	float MidStance = 0.f;
	float HeelRise = 0.f;
	float ToeOff = 0.f;
	float KneeLift = 0.f;
	float Swing = 0.f;
	float Reach = 0.f;
	float Plant = 0.f;
	float Stride = 0.f;
};

FCrewLegPhaseWeights EvaluateLegPhase(float Phase01, float StanceFraction)
{
	const float P = WrapPhase01(Phase01);
	const float StanceEnd = FMath::Clamp(StanceFraction, 0.35f, 0.75f);
	const float SwingStart = StanceEnd;
	FCrewLegPhaseWeights W;

	W.Contact = PhasePulse01(P, 0.f, 0.08f);
	W.WeightAccept = PhasePulse01(P, 0.10f, 0.11f);
	W.Compression = PhasePulse01(P, 0.16f, 0.16f);
	W.MidStance = PhasePulse01(P, 0.34f, 0.18f);
	W.HeelRise = PhasePulse01(P, StanceEnd - 0.12f, 0.10f);
	W.ToeOff = PhasePulse01(P, StanceEnd - 0.02f, 0.10f);
	W.KneeLift = PhasePulse01(P, SwingStart + 0.12f, 0.14f);
	W.Swing = PhaseWindow01(P, SwingStart, 0.92f, 0.08f);
	W.Reach = PhasePulse01(P, 0.90f, 0.12f);
	W.Plant = 1.f - SmoothStep01(StanceEnd - 0.06f, StanceEnd + 0.04f, P);

	if (P < StanceEnd)
	{
		const float StanceT = SmoothStep01(0.f, 1.f, P / FMath::Max(0.01f, StanceEnd));
		W.Stride = FMath::Lerp(1.f, -1.f, StanceT);
	}
	else
	{
		const float SwingT = SmoothStep01(0.f, 1.f, (P - SwingStart) / FMath::Max(0.01f, 1.f - SwingStart));
		W.Stride = FMath::Lerp(-1.f, 1.f, SwingT);
	}

	return W;
}

float WalkBodyRiseAlphaFromPhase01(float Phase01)
{
	const float WrappedPhase = WrapPhase01(Phase01);
	return 0.5f - 0.5f * FMath::Cos(WrappedPhase * TwoPi * 2.f);
}

float GetWorldDeltaSeconds(const UObject* Object)
{
	const UWorld* World = Object ? Object->GetWorld() : nullptr;
	return World ? FMath::Max(World->GetDeltaSeconds(), UE_SMALL_NUMBER) : 0.016f;
}

FCrewProceduralSkeletonMap GetSkeletonMap(const USubCrewAnimInstance& AnimInstance)
{
	return AnimInstance.ProceduralProfile ? AnimInstance.ProceduralProfile->Skeleton : FCrewProceduralSkeletonMap();
}

bool ShouldDrawBoneAxes(const USubCrewAnimInstance& AnimInstance)
{
	return AnimInstance.bDrawBoneAxes
		|| (AnimInstance.ProceduralProfile && AnimInstance.ProceduralProfile->Debug.bDrawBoneAxes);
}

bool ShouldDrawFootProbes(const USubCrewAnimInstance& AnimInstance)
{
	return AnimInstance.bDrawFootProbes
		|| (AnimInstance.ProceduralProfile && AnimInstance.ProceduralProfile->Debug.bDrawFootProbes);
}

float GetDebugAxisLengthCm(const USubCrewAnimInstance& AnimInstance)
{
	if (AnimInstance.ProceduralProfile)
	{
		return FMath::Max(1.f, AnimInstance.ProceduralProfile->Debug.AxisLengthCm);
	}

	return FMath::Max(1.f, AnimInstance.DebugAxisLengthCm);
}

float GetDebugDrawDurationSeconds(const USubCrewAnimInstance& AnimInstance)
{
	return AnimInstance.ProceduralProfile ? AnimInstance.ProceduralProfile->Debug.DrawDurationSeconds : 0.f;
}
}

void USubCrewAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(TryGetPawnOwner()))
	{
		CrewCharacter = Crew;
		CrewMovement = Crew->GetCrewMovement();
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

	const bool bHasRuntimeInput = CrewCharacter.IsValid() && CrewMovement.IsValid();
	bUsingPreviewInputsThisFrame = bUseEditorPreviewInputs || !bHasRuntimeInput;

	if ((!bHasRuntimeInput && !bUsingPreviewInputsThisFrame) || DeltaSeconds <= 0.f)
	{
		return;
	}

	ResetProceduralOutputs();
	if (bUsingPreviewInputsThisFrame)
	{
		ApplyPreviewInputState();
	}
	else
	{
		ReadInputState();
	}

	if (bIsSwimming)
	{
		ComputeSwimCycle(DeltaSeconds);
	}
	else
	{
		SmoothedWaterSprintAlpha = FMath::FInterpTo(SmoothedWaterSprintAlpha, 0.f, DeltaSeconds, 8.f);
		ComputeWalkCycle(DeltaSeconds);
		if (bEnableProceduralBodyMotion)
		{
			ComputeBreathing(DeltaSeconds);
		}
	}

	ComputePosturePose();

	if (bEnableProceduralSubMotion)
	{
		ComputeSubMotion();
	}

	if (bEnableProceduralAim)
	{
		ComputeUpperBodyAim();
	}
	else
	{
		UpperBodyYawOffset = 0.f;
	}

	ComputeHandIK();
	ComputeFootIK();

	ComputeRestPose();

	if (bDrawProceduralDebug || ShouldDrawBoneAxes(*this) || ShouldDrawFootProbes(*this))
	{
		DrawProceduralDebug();
	}
}

void USubCrewAnimInstance::ResetProceduralOutputs()
{
	LocomotionState = FCrewAnimLocomotionState();

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

	FootIK_R_Offset = FVector::ZeroVector;
	FootIK_L_Offset = FVector::ZeroVector;
	DebugFootLWorld = FVector::ZeroVector;
	DebugFootRWorld = FVector::ZeroVector;
	DebugRightFootPhase01 = 0.f;
	DebugLeftFootPhase01 = 0.f;
	DebugRightFootSwingAlpha = 0.f;
	DebugLeftFootSwingAlpha = 0.f;
	DebugRightFootToeOffAlpha = 0.f;
	DebugLeftFootToeOffAlpha = 0.f;
	DebugActiveCycleLengthCm = 0.f;
	DebugActiveStepRate = 0.f;
	DebugRunBlendAlpha = 0.f;
}

FRotator USubCrewAnimInstance::MakeAxisRotator(float Degrees, int32 Axis, bool bNegate)
{
	return MakeAxisRotatorInternal(Degrees, Axis, bNegate);
}

void USubCrewAnimInstance::ReadInputState()
{
	ASubCrewCharacter* Crew = CrewCharacter.Get();
	USubCrewMovementComponent* CMC = CrewMovement.Get();
	if (!Crew || !CMC)
	{
		return;
	}

	MoveIntent = CMC->GetLastMoveIntent();
	LocomotionFrame = CMC->GetLastLocomotionFrame();
	LocomotionState.MoveIntent = MoveIntent;
	LocomotionState.Frame = LocomotionFrame;

	Speed = LocomotionFrame.Speed2D;
	Direction = LocomotionFrame.DirectionDeg;
	bIsMoving = LocomotionFrame.bIsMoving || Speed > 3.f;
	bIsRunning = LocomotionFrame.bIsRunning || CMC->bIsRunning;
	PostureAlpha = FMath::Clamp(LocomotionFrame.PostureAlpha, 0.f, 1.f);
	Stance = LocomotionFrame.Stance;
	Gait = LocomotionFrame.Gait;

	const bool bWaterMovement = LocomotionFrame.bIsSwimming || Crew->IsCrewSwimming();
	bIsSwimming = bWaterMovement && !CMC->IsMovingOnGround();
	if (bIsSwimming)
	{
		Stance = ECrewLocomotionStance::Swimming;
		Gait = ECrewLocomotionGait::Swim;
	}
	bIsWaterSprinting = bIsSwimming && LocomotionFrame.bIsWaterSprinting;

	SubPitchDeg = LocomotionFrame.SubTiltPitchDeg;
	SubRollDeg = LocomotionFrame.SubTiltRollDeg;
	SubAccelForward = LocomotionFrame.LocalSubLinearAcceleration.X;
	SubAccelLateral = LocomotionFrame.LocalSubLinearAcceleration.Y;
	SubAngularVelocityDegrees = LocomotionFrame.LocalSubAngularVelocityDegrees;
	SubAngularAccelerationDegrees = LocomotionFrame.LocalSubAngularAccelerationDegrees;
	SupportQuality = LocomotionFrame.SupportQuality01;
	LocalTurnRateDegPerSec = LocomotionFrame.LocalTurnRateDegPerSec;
}

void USubCrewAnimInstance::ApplyPreviewInputState()
{
	MoveIntent = FCrewMoveIntent();
	LocomotionFrame = FCrewLocomotionFrame();
	LocomotionState.MoveIntent = MoveIntent;
	LocomotionState.Frame = LocomotionFrame;

	Speed = FMath::Max(0.f, PreviewSpeedCmS);
	Direction = FMath::Clamp(PreviewDirectionDeg, -180.f, 180.f);
	bIsMoving = Speed > 3.f;
	bIsSwimming = bPreviewIsSwimming;
	bIsWaterSprinting = bIsSwimming && bPreviewIsWaterSprinting;
	bIsRunning = false;
	PostureAlpha = FMath::Clamp(PreviewPostureAlpha, 0.f, 1.f);
	SupportQuality = FMath::Clamp(PreviewSupportQuality, 0.f, 1.f);
	LocalTurnRateDegPerSec = PreviewLocalTurnRateDegPerSec;
	SubPitchDeg = PreviewSubTiltPitchDeg;
	SubRollDeg = PreviewSubTiltRollDeg;
	SubAccelForward = PreviewSubAccelForwardCmS2;
	SubAccelLateral = PreviewSubAccelLateralCmS2;
	SubAngularVelocityDegrees = FVector(0.f, 0.f, PreviewLocalTurnRateDegPerSec);
	SubAngularAccelerationDegrees = FVector::ZeroVector;

	Stance = bIsSwimming ? ECrewLocomotionStance::Swimming : ECrewLocomotionStance::Standing;
	Gait = bIsSwimming ? ECrewLocomotionGait::Swim : (bIsMoving ? ECrewLocomotionGait::Walk : ECrewLocomotionGait::Idle);

	const float DirectionRadians = FMath::DegreesToRadians(Direction);
	const FVector PreviewLocalVelocity(
		FMath::Cos(DirectionRadians) * Speed,
		FMath::Sin(DirectionRadians) * Speed,
		0.f);

	MoveIntent.MoveAxis = FVector2D(PreviewLocalVelocity.X, PreviewLocalVelocity.Y).GetSafeNormal();
	MoveIntent.LocalMoveDirection = PreviewLocalVelocity.GetSafeNormal();
	MoveIntent.WorldMoveDirection = MoveIntent.LocalMoveDirection;
	MoveIntent.MoveInputStrength = bIsMoving ? 1.f : 0.f;
	MoveIntent.bHasMoveInput = bIsMoving;
	MoveIntent.MoveWorldYawDeg = Direction;
	MoveIntent.DesiredWorldYawDeg = Direction;
	MoveIntent.DesiredGridYawDeg = Direction;

	LocomotionFrame.LocalVelocity = PreviewLocalVelocity;
	LocomotionFrame.WorldVelocity = PreviewLocalVelocity;
	LocomotionFrame.Speed2D = Speed;
	LocomotionFrame.DirectionDeg = Direction;
	LocomotionFrame.PostureAlpha = PostureAlpha;
	LocomotionFrame.SupportQuality01 = SupportQuality;
	LocomotionFrame.LocalTurnRateDegPerSec = LocalTurnRateDegPerSec;
	LocomotionFrame.SubTiltPitchDeg = SubPitchDeg;
	LocomotionFrame.SubTiltRollDeg = SubRollDeg;
	LocomotionFrame.LocalSubLinearAcceleration = FVector(SubAccelForward, SubAccelLateral, 0.f);
	LocomotionFrame.LocalSubAngularVelocityDegrees = SubAngularVelocityDegrees;
	LocomotionFrame.LocalSubAngularAccelerationDegrees = SubAngularAccelerationDegrees;
	LocomotionFrame.Stance = Stance;
	LocomotionFrame.Gait = Gait;
	LocomotionFrame.bIsMoving = bIsMoving;
	LocomotionFrame.bIsSwimming = bIsSwimming;
	LocomotionFrame.bIsWaterSprinting = bIsWaterSprinting;
	LocomotionFrame.bIsRunning = bIsRunning;

	LocomotionState.MoveIntent = MoveIntent;
	LocomotionState.Frame = LocomotionFrame;
}

void USubCrewAnimInstance::ComputeWalkCycle(float DeltaSeconds)
{
	const FCrewProceduralGroundedTuning Grounded = GetGroundedTuning(*this);
	const FCrewProceduralRigAxisProfile Axes = GetRigAxes(*this);
	float RunBlendAlpha = 0.f;
	const FCrewProceduralGaitTuning ActiveGait = ResolveGroundedGait(Grounded, Speed, RunBlendAlpha);
	const float CycleLengthCm = GetGaitCycleLengthCm(ActiveGait);
	const float TargetSpeedAlpha = bIsMoving ? FMath::Clamp(Speed / FMath::Max(1.f, ActiveGait.SpeedCmS), 0.f, 1.25f) : 0.f;
	SmoothedSpeedAlpha = FMath::FInterpTo(SmoothedSpeedAlpha, TargetSpeedAlpha, DeltaSeconds, Grounded.StepSmoothingSpeed);
	const float PoseAlpha = FMath::Clamp(SmoothedSpeedAlpha, 0.f, 1.f);

	if (bUsingPreviewInputsThisFrame && bPreviewLockStridePhase)
	{
		WalkPhase = WrapPhase01(PreviewStridePhase01) * TwoPi;
	}
	else if (bIsMoving)
	{
		const float PhaseDelta = (Speed * DeltaSeconds / CycleLengthCm) * TwoPi;
		WalkPhase = FMath::Fmod(WalkPhase + PhaseDelta, TwoPi);
	}

	LocomotionState.StridePhase01 = WrapPhase01(WalkPhase / TwoPi);
	const float DirectionSign = FMath::Abs(Direction) > 90.f ? -1.f : 1.f;
	const float RPhase01 = LocomotionState.StridePhase01;
	const float LPhase01 = WrapPhase01(LocomotionState.StridePhase01 + 0.5f);

	const FCrewLegPhaseWeights RLeg = EvaluateLegPhase(RPhase01, ActiveGait.StanceFraction);
	const FCrewLegPhaseWeights LLeg = EvaluateLegPhase(LPhase01, ActiveGait.StanceFraction);
	LocomotionState.RightFootPlantAlpha = bIsMoving ? RLeg.Plant : 1.f;
	LocomotionState.LeftFootPlantAlpha = bIsMoving ? LLeg.Plant : 1.f;

	DebugActiveCycleLengthCm = CycleLengthCm;
	DebugActiveStepRate = ActiveGait.StepRate;
	DebugRunBlendAlpha = RunBlendAlpha;
	DebugRightFootPhase01 = RPhase01;
	DebugLeftFootPhase01 = LPhase01;
	DebugRightFootSwingAlpha = RLeg.Swing;
	DebugLeftFootSwingAlpha = LLeg.Swing;
	DebugRightFootToeOffAlpha = RLeg.ToeOff;
	DebugLeftFootToeOffAlpha = LLeg.ToeOff;

	const auto ComputeThighDeg = [&ActiveGait, DirectionSign, PoseAlpha](const FCrewLegPhaseWeights& Leg)
	{
		const float Magnitude = Leg.Stride >= 0.f
			? Leg.Stride * ActiveGait.ThighForwardDeg
			: Leg.Stride * ActiveGait.ThighBackDeg;
		return Magnitude * DirectionSign * PoseAlpha;
	};

	const auto ComputeKneeDeg = [&ActiveGait, PoseAlpha](const FCrewLegPhaseWeights& Leg)
	{
		const float Bend =
			(Leg.WeightAccept * 0.18f)
			+ (Leg.Compression * 0.34f)
			+ (Leg.ToeOff * 0.24f)
			+ (Leg.KneeLift * 1.08f)
			+ (Leg.Swing * 0.48f)
			+ (Leg.Reach * 0.22f);
		return Bend * ActiveGait.KneeBendDeg * PoseAlpha;
	};

	const auto ComputeFootDeg = [&ActiveGait, PoseAlpha](const FCrewLegPhaseWeights& Leg)
	{
		const float FootPitch =
			(-Leg.Contact * 0.25f * ActiveGait.FootPitchDeg)
			+ (Leg.Swing * 0.35f * ActiveGait.FootPitchDeg)
			+ (Leg.Reach * 0.45f * ActiveGait.FootPitchDeg)
			- (Leg.ToeOff * ActiveGait.ToeOffDeg);
		return FootPitch * PoseAlpha;
	};

	const float RThighSwing = ComputeThighDeg(RLeg);
	const float LThighSwing = ComputeThighDeg(LLeg);
	if (bEnableProceduralLegs && bEnableProceduralThighs)
	{
		Proc_ThighR_Rot = MakeAxisRotatorInternal(RThighSwing, Axes.ThighSwing);
		Proc_ThighL_Rot = MakeAxisRotatorInternal(LThighSwing, Axes.ThighSwing);
	}

	const float RKnee = ComputeKneeDeg(RLeg);
	const float LKnee = ComputeKneeDeg(LLeg);
	if (bEnableProceduralLegs && bEnableProceduralCalves)
	{
		Proc_CalfR_Rot = MakeAxisRotatorInternal(-RKnee, Axes.KneeBend);
		Proc_CalfL_Rot = MakeAxisRotatorInternal(-LKnee, Axes.KneeBend);
	}

	const float RFoot = ComputeFootDeg(RLeg);
	const float LFoot = ComputeFootDeg(LLeg);
	if (bEnableProceduralLegs && bEnableProceduralFeet)
	{
		Proc_FootR_Rot = MakeAxisRotatorInternal(RFoot, Axes.FootPitch);
		Proc_FootL_Rot = MakeAxisRotatorInternal(LFoot, Axes.FootPitch);
	}

	const float SupportBias = FMath::Clamp(LLeg.Plant - RLeg.Plant, -1.f, 1.f);
	const float PelvisRide = FMath::Clamp(LLeg.Compression - RLeg.Compression, -1.f, 1.f);
	const float RArmSwing = -RLeg.Stride * ActiveGait.ArmSwingDeg * PoseAlpha;
	const float LArmSwing = -LLeg.Stride * ActiveGait.ArmSwingDeg * PoseAlpha;
	if (bEnableProceduralArms)
	{
		const float RElbow = (
			ActiveGait.ElbowBendDeg
			+ (FMath::Abs(RArmSwing) * 0.18f)
			+ (RLeg.Swing * ActiveGait.ElbowBendDeg * 0.35f)
			+ (RLeg.ToeOff * ActiveGait.ElbowBendDeg * 0.20f)) * PoseAlpha;
		const float LElbow = (
			ActiveGait.ElbowBendDeg
			+ (FMath::Abs(LArmSwing) * 0.18f)
			+ (LLeg.Swing * ActiveGait.ElbowBendDeg * 0.35f)
			+ (LLeg.ToeOff * ActiveGait.ElbowBendDeg * 0.20f)) * PoseAlpha;
		Proc_UpperarmR_Rot += MakeAxisRotatorInternal(RArmSwing, Axes.ShoulderSwing);
		Proc_UpperarmL_Rot += MakeAxisRotatorInternal(LArmSwing, Axes.ShoulderSwing);
		Proc_LowerarmR_Rot += MakeAxisRotatorInternal(-RElbow, Axes.ElbowBend);
		Proc_LowerarmL_Rot += MakeAxisRotatorInternal(LElbow, Axes.ElbowBend);
	}

	if (bEnableProceduralBodyMotion)
	{
		const float BodyRise01 = WalkBodyRiseAlphaFromPhase01(LocomotionState.StridePhase01);
		const float Compression = FMath::Clamp(RLeg.Compression + LLeg.Compression, 0.f, 1.f);
		const float BodyExtensionDeg = (BodyRise01 - 0.5f) * 0.9f * PoseAlpha;
		const float StaticLeanDeg = ActiveGait.SpineLeanDeg * PoseAlpha;
		const float CompressionBendDeg = Compression * 0.55f * PoseAlpha;
		const float PelvisRollDeg = (SupportBias * ActiveGait.PelvisRollDeg) + (PelvisRide * ActiveGait.PelvisRollDeg * 0.60f);
		const float PelvisTwistDeg = ((RLeg.Stride - LLeg.Stride) * ActiveGait.PelvisTwistDeg) * 0.50f;

		Proc_Pelvis_Rot += MakeAxisRotatorInternal(PelvisRollDeg * 0.35f * PoseAlpha, Axes.SpineTwist);
		Proc_Pelvis_Rot += MakeAxisRotatorInternal(PelvisTwistDeg * 0.30f * PoseAlpha, Axes.SpineTwist);
		Proc_Spine01_Rot += MakeAxisRotatorInternal(CompressionBendDeg * 0.25f, Axes.SpineBend);
		Proc_Spine02_Rot += MakeAxisRotatorInternal(CompressionBendDeg * 0.25f, Axes.SpineBend);
		Proc_Spine01_Rot += MakeAxisRotatorInternal(PelvisRollDeg * 0.20f * PoseAlpha, Axes.SpineTwist);
		Proc_Spine02_Rot += MakeAxisRotatorInternal(PelvisTwistDeg * 0.18f * PoseAlpha, Axes.SpineTwist);
		Proc_Spine03_Rot += MakeAxisRotatorInternal((StaticLeanDeg * 0.30f) - (BodyExtensionDeg * 0.30f), Axes.SpineBend);
		Proc_Spine04_Rot += MakeAxisRotatorInternal((StaticLeanDeg * 0.35f) - (BodyExtensionDeg * 0.35f), Axes.SpineBend);
		Proc_Spine05_Rot += MakeAxisRotatorInternal((StaticLeanDeg * 0.35f) - (BodyExtensionDeg * 0.35f), Axes.SpineBend);
		Proc_UpperarmR_Rot += MakeAxisRotatorInternal(-SupportBias * 2.0f * PoseAlpha, Axes.ShoulderSwing);
		Proc_UpperarmL_Rot += MakeAxisRotatorInternal(SupportBias * 2.0f * PoseAlpha, Axes.ShoulderSwing);
	}
}

void USubCrewAnimInstance::ComputeSwimCycle(float DeltaSeconds)
{
	const FCrewProceduralSwimTuning Swim = GetSwimTuning(*this);
	const FCrewProceduralRigAxisProfile Axes = GetRigAxes(*this);
	const float TargetSpeedAlpha = FMath::Clamp(Speed / FMath::Max(1.f, Swim.ReferenceSpeedCmS), 0.f, 1.25f);
	const float TargetSprintAlpha = bIsWaterSprinting ? 1.f : 0.f;
	const float SmoothingSpeed = FMath::Max(0.f, Swim.SmoothingSpeed);
	SmoothedSpeedAlpha = FMath::FInterpTo(SmoothedSpeedAlpha, TargetSpeedAlpha, DeltaSeconds, SmoothingSpeed);
	SmoothedWaterSprintAlpha = FMath::FInterpTo(SmoothedWaterSprintAlpha, TargetSprintAlpha, DeltaSeconds, SmoothingSpeed);

	const float MoveAlpha = FMath::Clamp(SmoothedSpeedAlpha, 0.f, 1.f);
	const float SprintAlpha = FMath::Clamp(SmoothedWaterSprintAlpha, 0.f, 1.f);
	const float StrokeRate = FMath::Lerp(
		FMath::Lerp(Swim.IdleStrokeRate, Swim.MoveStrokeRate, MoveAlpha),
		Swim.SprintStrokeRate,
		SprintAlpha);
	SwimPhase = FMath::Fmod(SwimPhase + DeltaSeconds * StrokeRate * TwoPi, TwoPi);
	LocomotionState.StridePhase01 = WrapPhase01(SwimPhase / TwoPi);
	LocomotionState.RightFootPlantAlpha = 0.f;
	LocomotionState.LeftFootPlantAlpha = 0.f;

	const float Phase01 = LocomotionState.StridePhase01;
	const float PrimaryWave = FMath::Sin(SwimPhase);
	const float SecondaryWave = FMath::Sin(SwimPhase + PI);
	const float BodyWave = FMath::Sin(SwimPhase + HALF_PI);
	const float EffortAlpha = FMath::Clamp(MoveAlpha + SprintAlpha * 0.35f, 0.f, 1.f);
	const float SwimPoseAlpha = FMath::Clamp(0.25f + EffortAlpha * 0.75f, 0.f, 1.f);
	const float LegEffortAlpha = FMath::Clamp(0.20f + EffortAlpha * 0.80f, 0.f, 1.f);
	const float ArmEffortAlpha = FMath::Clamp(0.15f + EffortAlpha * 0.85f, 0.f, 1.f);

	const float ThighAmp = FMath::Lerp(Swim.KickThighDeg, Swim.SprintKickThighDeg, SprintAlpha) * LegEffortAlpha;
	const float KneeAmp = FMath::Lerp(Swim.KickKneeDeg, Swim.SprintKickKneeDeg, SprintAlpha) * LegEffortAlpha;
	const float FootAmp = Swim.FootPitchDeg * LegEffortAlpha;

	if (bEnableProceduralLegs)
	{
		if (bEnableProceduralThighs)
		{
			Proc_ThighR_Rot += MakeAxisRotatorInternal(PrimaryWave * ThighAmp, Axes.ThighSwing);
			Proc_ThighL_Rot += MakeAxisRotatorInternal(SecondaryWave * ThighAmp, Axes.ThighSwing);
		}
		if (bEnableProceduralCalves)
		{
			Proc_CalfR_Rot += MakeAxisRotatorInternal(-FMath::Abs(PrimaryWave) * KneeAmp, Axes.KneeBend);
			Proc_CalfL_Rot += MakeAxisRotatorInternal(-FMath::Abs(SecondaryWave) * KneeAmp, Axes.KneeBend);
		}
		if (bEnableProceduralFeet)
		{
			Proc_FootR_Rot += MakeAxisRotatorInternal(-PrimaryWave * FootAmp, Axes.FootPitch);
			Proc_FootL_Rot += MakeAxisRotatorInternal(-SecondaryWave * FootAmp, Axes.FootPitch);
		}
	}

	if (bEnableProceduralArms)
	{
		const ASubCrewCharacter* Crew = CrewCharacter.Get();
		const float ViewScale = Crew && Crew->IsFirstPersonMode()
			? FMath::Clamp(Swim.FirstPersonArmScale, 0.f, 1.f)
			: 1.f;
		const float ShoulderAmp = FMath::Lerp(Swim.ShoulderStrokeDeg, Swim.SprintShoulderStrokeDeg, SprintAlpha) * ArmEffortAlpha * ViewScale;
		const float ScullAmp = Swim.ShoulderScullDeg * ArmEffortAlpha * ViewScale;
		const float ElbowAmp = FMath::Lerp(Swim.ElbowBendDeg, Swim.SprintElbowBendDeg, SprintAlpha) * ArmEffortAlpha * ViewScale;
		const float RightPull = PhaseWindow01(Phase01, 0.05f, 0.45f, 0.10f);
		const float LeftPull = PhaseWindow01(WrapPhase01(Phase01 + 0.5f), 0.05f, 0.45f, 0.10f);
		const float RightRecover = PhaseWindow01(Phase01, 0.50f, 0.95f, 0.12f);
		const float LeftRecover = PhaseWindow01(WrapPhase01(Phase01 + 0.5f), 0.50f, 0.95f, 0.12f);

		Proc_UpperarmR_Rot += MakeAxisRotatorInternal((-RightPull + RightRecover * 0.45f) * ShoulderAmp, Axes.ShoulderSwing);
		Proc_UpperarmL_Rot += MakeAxisRotatorInternal((-LeftPull + LeftRecover * 0.45f) * ShoulderAmp, Axes.ShoulderSwing);
		Proc_UpperarmR_Rot += MakeAxisRotatorInternal(PrimaryWave * ScullAmp, Axes.ShoulderLower);
		Proc_UpperarmL_Rot += MakeAxisRotatorInternal(SecondaryWave * ScullAmp, Axes.ShoulderLower);
		Proc_LowerarmR_Rot += MakeAxisRotatorInternal(-(RightPull * 0.85f + RightRecover * 0.25f) * ElbowAmp, Axes.ElbowBend);
		Proc_LowerarmL_Rot += MakeAxisRotatorInternal((LeftPull * 0.85f + LeftRecover * 0.25f) * ElbowAmp, Axes.ElbowBend);
	}

	if (bEnableProceduralBodyMotion)
	{
		const float BasePitch = FMath::Lerp(Swim.BodyPitchDeg, Swim.SprintBodyPitchDeg, SprintAlpha) * SwimPoseAlpha;
		const float VerticalPitch = FMath::Clamp(MoveIntent.VerticalAxis, -1.f, 1.f) * Swim.VerticalInputPitchDeg;
		const float LateralRoll = FMath::Clamp(MoveIntent.MoveAxis.Y, -1.f, 1.f) * Swim.LateralInputRollDeg;
		const float WavePitch = BodyWave * Swim.BodyWaveDeg * EffortAlpha;

		Proc_Pelvis_Offset.Z += BodyWave * Swim.PelvisBobCm * EffortAlpha;
		Proc_Pelvis_Rot += MakeAxisRotatorInternal(LateralRoll * 0.30f, Axes.SpineTwist);
		Proc_Spine01_Rot += MakeAxisRotatorInternal((BasePitch + VerticalPitch) * 0.15f, Axes.SpineBend);
		Proc_Spine02_Rot += MakeAxisRotatorInternal((BasePitch + VerticalPitch) * 0.20f, Axes.SpineBend);
		Proc_Spine03_Rot += MakeAxisRotatorInternal((BasePitch + WavePitch) * 0.25f, Axes.SpineBend);
		Proc_Spine04_Rot += MakeAxisRotatorInternal((BasePitch - WavePitch * 0.35f) * 0.22f, Axes.SpineBend);
		Proc_Spine05_Rot += MakeAxisRotatorInternal((BasePitch - WavePitch * 0.25f) * 0.18f, Axes.SpineBend);
		Proc_Spine03_Rot += MakeAxisRotatorInternal(LateralRoll * 0.22f, Axes.SpineTwist);
		Proc_Spine04_Rot += MakeAxisRotatorInternal(LateralRoll * 0.28f, Axes.SpineTwist);
		Proc_Spine05_Rot += MakeAxisRotatorInternal(LateralRoll * 0.20f, Axes.SpineTwist);
	}
}

void USubCrewAnimInstance::ComputePosturePose()
{
	if (bIsSwimming)
	{
		return;
	}

	const float FoldAlpha = 1.f - FMath::Clamp(PostureAlpha, 0.f, 1.f);
	if (FoldAlpha <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FCrewProceduralRigAxisProfile Axes = GetRigAxes(*this);
	const float CrawlAlpha = 1.f - SmoothStep01(0.15f, 0.55f, PostureAlpha);
	const float BodyBend = MaxPostureBendDeg * FoldAlpha;

	if (bEnableProceduralBodyMotion)
	{
		Proc_Spine01_Rot += MakeAxisRotatorInternal(BodyBend * 0.16f, Axes.SpineBend);
		Proc_Spine02_Rot += MakeAxisRotatorInternal(BodyBend * 0.18f, Axes.SpineBend);
		Proc_Spine03_Rot += MakeAxisRotatorInternal(BodyBend * 0.22f, Axes.SpineBend);
		Proc_Spine04_Rot += MakeAxisRotatorInternal(BodyBend * 0.24f, Axes.SpineBend);
		Proc_Spine05_Rot += MakeAxisRotatorInternal(BodyBend * 0.20f, Axes.SpineBend);
		Proc_Neck01_Rot += MakeAxisRotatorInternal(-BodyBend * 0.18f, Axes.SpineBend);
		Proc_Head_Rot += MakeAxisRotatorInternal(-BodyBend * 0.12f, Axes.SpineBend);
	}

	const float ThighFold = FMath::Lerp(CrouchThighFoldDeg, CrawlThighFoldDeg, CrawlAlpha) * FoldAlpha;
	const float CalfFold = FMath::Lerp(CrouchCalfFoldDeg, CrawlCalfFoldDeg, CrawlAlpha) * FoldAlpha;
	const float FootComp = FMath::Lerp(CrouchFootCompDeg, CrawlFootCompDeg, CrawlAlpha) * FoldAlpha;
	if (bEnableProceduralLegs)
	{
		if (bEnableProceduralThighs)
		{
			Proc_ThighR_Rot += MakeAxisRotatorInternal(ThighFold, Axes.ThighSwing);
			Proc_ThighL_Rot += MakeAxisRotatorInternal(ThighFold, Axes.ThighSwing);
		}
		if (bEnableProceduralCalves)
		{
			Proc_CalfR_Rot += MakeAxisRotatorInternal(-CalfFold, Axes.KneeBend);
			Proc_CalfL_Rot += MakeAxisRotatorInternal(-CalfFold, Axes.KneeBend);
		}
		if (bEnableProceduralFeet)
		{
			Proc_FootR_Rot += MakeAxisRotatorInternal(FootComp, Axes.FootPitch);
			Proc_FootL_Rot += MakeAxisRotatorInternal(FootComp, Axes.FootPitch);
		}
	}

	if (bEnableProceduralArms)
	{
		const float ArmForward = FMath::Lerp(CrouchArmForwardDeg, CrawlArmForwardDeg, CrawlAlpha) * FoldAlpha;
		const float ElbowBend = FMath::Lerp(CrouchElbowBendDeg, CrawlElbowBendDeg, CrawlAlpha) * FoldAlpha;
		Proc_UpperarmR_Rot += MakeAxisRotatorInternal(ArmForward, Axes.ShoulderSwing);
		Proc_UpperarmL_Rot += MakeAxisRotatorInternal(ArmForward, Axes.ShoulderSwing);
		Proc_LowerarmR_Rot += MakeAxisRotatorInternal(-ElbowBend, Axes.ElbowBend);
		Proc_LowerarmL_Rot += MakeAxisRotatorInternal(ElbowBend, Axes.ElbowBend);
	}
}

void USubCrewAnimInstance::ComputeRestPose()
{
	if (!bEnableProceduralRestPose || !bEnableProceduralArms || bIsSwimming)
	{
		return;
	}

	const float MoveAttenuation = 1.f - FMath::Clamp(SmoothedSpeedAlpha * 0.75f, 0.f, 0.75f);
	const float LowerSign = bInvertArmRestLowerDirection ? -1.f : 1.f;
	const float RightLowerDeg = -ArmRestLowerDeg * LowerSign;
	const float LeftLowerDeg = ArmRestLowerDeg * LowerSign;
	const float ShoulderRelax = ArmRestShoulderDeg * MoveAttenuation;
	const float ElbowSoftness = ArmRestElbowDeg * MoveAttenuation;
	const FCrewProceduralRigAxisProfile Axes = GetRigAxes(*this);

	if (!FMath::IsNearlyZero(ArmRestLowerDeg, 0.01f))
	{
		const FQuat RightLowerQuat = MakeAxisRotatorInternal(RightLowerDeg, Axes.ShoulderLower).Quaternion();
		const FQuat LeftLowerQuat = MakeAxisRotatorInternal(LeftLowerDeg, Axes.ShoulderLower).Quaternion();
		Proc_UpperarmR_Rot = (RightLowerQuat * Proc_UpperarmR_Rot.Quaternion()).Rotator();
		Proc_UpperarmL_Rot = (LeftLowerQuat * Proc_UpperarmL_Rot.Quaternion()).Rotator();
	}

	if (!FMath::IsNearlyZero(ShoulderRelax, 0.01f))
	{
		Proc_UpperarmR_Rot += MakeAxisRotatorInternal(ShoulderRelax, Axes.ShoulderSwing);
		Proc_UpperarmL_Rot += MakeAxisRotatorInternal(ShoulderRelax, Axes.ShoulderSwing);
	}

	if (!FMath::IsNearlyZero(ElbowSoftness, 0.01f))
	{
		Proc_LowerarmR_Rot += MakeAxisRotatorInternal(-ElbowSoftness, Axes.ElbowBend);
		Proc_LowerarmL_Rot += MakeAxisRotatorInternal(ElbowSoftness, Axes.ElbowBend);
	}
}

void USubCrewAnimInstance::ComputeBreathing(float DeltaSeconds)
{
	const FCrewProceduralRigAxisProfile Axes = GetRigAxes(*this);
	BreathPhase = FMath::Fmod(BreathPhase + DeltaSeconds * BreathingRate * TwoPi, TwoPi);

	const float BreathAlpha = 1.f - FMath::Clamp(Speed / 100.f, 0.f, 1.f);
	const float Breath = FMath::Sin(BreathPhase) * BreathingAmplitudeDeg * BreathAlpha;
	Proc_Spine04_Rot += MakeAxisRotatorInternal(Breath * 0.45f, Axes.SpineBend);
	Proc_Spine05_Rot += MakeAxisRotatorInternal(Breath * 0.55f, Axes.SpineBend);
}

void USubCrewAnimInstance::ComputeSubMotion()
{
	const FCrewProceduralRigAxisProfile Axes = GetRigAxes(*this);
	const FCrewProceduralSubMotionTuning Tuning = GetSubMotionTuning(*this);
	const float MaxLeanDeg = FMath::Max(0.f, Tuning.MaxLeanDeg);
	const float Instability = 1.f - FMath::Clamp(SupportQuality, 0.f, 1.f);

	const float TiltPitch = FMath::Clamp(-SubPitchDeg * Tuning.TiltCounterLeanScale, -MaxLeanDeg, MaxLeanDeg);
	const float TiltRoll = FMath::Clamp(-SubRollDeg * Tuning.TiltCounterLeanScale, -MaxLeanDeg, MaxLeanDeg);
	const float AccelPitch = FMath::Clamp(-SubAccelForward * Tuning.LinearAccelerationLeanScale * Instability, -MaxLeanDeg, MaxLeanDeg);
	const float AccelRoll = FMath::Clamp(SubAccelLateral * Tuning.LinearAccelerationLeanScale * Instability, -MaxLeanDeg, MaxLeanDeg);
	const float AngularAccelTwist = FMath::Clamp(
		SubAngularAccelerationDegrees.Z * Tuning.AngularAccelerationLeanScale * (0.5f + Instability * 0.5f),
		-MaxLeanDeg,
		MaxLeanDeg);
	const float TurnTwist = FMath::Clamp(
		(LocalTurnRateDegPerSec + SubAngularVelocityDegrees.Z) * Tuning.AngularVelocityLeanScale * (0.5f + Instability * 0.5f),
		-MaxLeanDeg,
		MaxLeanDeg);

	const float BendLean = TiltPitch + AccelPitch;
	const float SideLean = TiltRoll + AccelRoll;
	const FCrewProceduralAxisTuning BodyForwardLeanAxis = Axes.ThighSwing;

	Proc_Spine03_Rot += MakeAxisRotatorInternal(BendLean * 0.30f, BodyForwardLeanAxis);
	Proc_Spine04_Rot += MakeAxisRotatorInternal(BendLean * 0.40f, BodyForwardLeanAxis);
	Proc_Spine05_Rot += MakeAxisRotatorInternal(BendLean * 0.30f, BodyForwardLeanAxis);

	Proc_Pelvis_Rot += MakeAxisRotatorInternal(SideLean * 0.35f, Axes.SpineTwist);
	Proc_Spine01_Rot += MakeAxisRotatorInternal(SideLean * 0.20f, Axes.SpineTwist);
	Proc_Spine02_Rot += MakeAxisRotatorInternal(SideLean * 0.20f, Axes.SpineTwist);
	const float InertiaTwist = TurnTwist + AngularAccelTwist;
	const float ShoulderInertiaDeg = FMath::Clamp(-InertiaTwist * Tuning.ShoulderInertiaScale, -MaxLeanDeg, MaxLeanDeg);

	Proc_Spine03_Rot += MakeAxisRotatorInternal(InertiaTwist * 0.22f, Axes.SpineTwist);
	Proc_Spine04_Rot += MakeAxisRotatorInternal(InertiaTwist * 0.28f, Axes.SpineTwist);
	Proc_Spine05_Rot += MakeAxisRotatorInternal(InertiaTwist * 0.20f, Axes.SpineTwist);
	if (bEnableProceduralArms)
	{
		Proc_UpperarmR_Rot += MakeAxisRotatorInternal(ShoulderInertiaDeg * 0.45f, Axes.ShoulderSwing);
		Proc_UpperarmL_Rot += MakeAxisRotatorInternal(-ShoulderInertiaDeg * 0.45f, Axes.ShoulderSwing);
		Proc_UpperarmR_Rot += MakeAxisRotatorInternal(-ShoulderInertiaDeg * 0.20f, Axes.ShoulderLower);
		Proc_UpperarmL_Rot += MakeAxisRotatorInternal(-ShoulderInertiaDeg * 0.20f, Axes.ShoulderLower);
	}
}

void USubCrewAnimInstance::ComputeUpperBodyAim()
{
	ASubCrewCharacter* Crew = CrewCharacter.Get();
	USubCrewMovementComponent* CMC = CrewMovement.Get();
	if (!Crew || !CMC)
	{
		UpperBodyYawOffset = 0.f;
		return;
	}

	const FCrewProceduralRigAxisProfile Axes = GetRigAxes(*this);
	const float DeltaSeconds = GetWorldDeltaSeconds(this);
	const float AimAlpha = FMath::Clamp(PostureAlpha, 0.25f, 1.f);

	if (bIsMoving)
	{
		FVector TraversalVelocityWorld = CMC->Velocity;
		if (Crew->CurrentSubmarine && CMC->EmbarkState == ECrewEmbarkState::Embarked)
		{
			TraversalVelocityWorld = Crew->CurrentSubmarine->GetActorTransform().TransformVectorNoScale(CMC->RelativeLinearVelocity);
		}

		const float CameraYaw = Crew->GetControlRotation().Yaw;
		const float VelocityYaw = TraversalVelocityWorld.ToOrientationRotator().Yaw;
		const float RawOffset = FMath::FindDeltaAngleDegrees(VelocityYaw, CameraYaw);
		const float ForwardScale = FMath::Clamp(1.f - FMath::Abs(Direction) / 70.f, 0.f, 1.f);
		const float TargetOffset = FMath::Clamp(RawOffset, -45.f, 45.f) * ForwardScale * AimAlpha;
		UpperBodyYawOffset = FMath::FInterpTo(UpperBodyYawOffset, TargetOffset, DeltaSeconds, 5.f);
	}
	else
	{
		UpperBodyYawOffset = FMath::FInterpTo(UpperBodyYawOffset, 0.f, DeltaSeconds, 3.f);
	}

	const float LowerYaw = FMath::Clamp(-UpperBodyYawOffset, -MaxLowerBodyYawDeg, MaxLowerBodyYawDeg);
	Proc_Pelvis_Rot += MakeAxisRotatorInternal(LowerYaw * 0.50f, Axes.SpineTwist);
	Proc_Spine01_Rot += MakeAxisRotatorInternal(LowerYaw * 0.30f, Axes.SpineTwist);
	Proc_Spine02_Rot += MakeAxisRotatorInternal(LowerYaw * 0.20f, Axes.SpineTwist);

	Proc_Spine03_Rot += MakeAxisRotatorInternal(UpperBodyYawOffset * 0.20f, Axes.SpineTwist);
	Proc_Spine04_Rot += MakeAxisRotatorInternal(UpperBodyYawOffset * 0.30f, Axes.SpineTwist);
	Proc_Spine05_Rot += MakeAxisRotatorInternal(UpperBodyYawOffset * 0.30f, Axes.SpineTwist);
	Proc_Neck01_Rot += MakeAxisRotatorInternal(UpperBodyYawOffset * 0.20f, Axes.SpineTwist);

	const float HeadPitch = FMath::ClampAngle(Crew->GetControlRotation().Pitch, -35.f, 30.f) * AimAlpha;
	Proc_Head_Rot += MakeAxisRotatorInternal(HeadPitch * 0.60f, Axes.SpineBend);
	Proc_Neck01_Rot += MakeAxisRotatorInternal(HeadPitch * 0.40f, Axes.SpineBend);
}

void USubCrewAnimInstance::ComputeHandIK()
{
	USubCrewMovementComponent* CMC = CrewMovement.Get();
	if (!CMC || bIsSwimming)
	{
		HandIK_L_Weight = FMath::FInterpTo(HandIK_L_Weight, 0.f, GetWorldDeltaSeconds(this), 5.f);
		HandIK_R_Weight = FMath::FInterpTo(HandIK_R_Weight, 0.f, GetWorldDeltaSeconds(this), 5.f);

		if (HandIK_L_Weight <= KINDA_SMALL_NUMBER)
		{
			HandIK_L_Target = FVector::ZeroVector;
			HandIK_L_State = FCrewHandIKState();
		}

		if (HandIK_R_Weight <= KINDA_SMALL_NUMBER)
		{
			HandIK_R_Target = FVector::ZeroVector;
			HandIK_R_State = FCrewHandIKState();
		}

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

	const float DeltaSeconds = GetWorldDeltaSeconds(this);
	const float InstabilityAlpha = FMath::Clamp(
		(1.f - SupportQuality)
		+ FMath::Abs(SubAccelForward) * 0.002f
		+ FMath::Abs(SubAccelLateral) * 0.002f
		+ FMath::Abs(LocalTurnRateDegPerSec) * 0.01f,
		0.f,
		1.f);
	const float TargetBraceAlpha = InstabilityAlpha * FMath::Lerp(1.f, 0.25f, SmoothedSpeedAlpha);

	const int32 LeftProbeIndices[] = { 0, 2, 4 };
	const int32 RightProbeIndices[] = { 1, 3, 5 };
	const USubCrewMovementComponent::FHandIKProbeResult* ProbeL = PickBestProbe(LeftProbeIndices, UE_ARRAY_COUNT(LeftProbeIndices));
	const USubCrewMovementComponent::FHandIKProbeResult* ProbeR = PickBestProbe(RightProbeIndices, UE_ARRAY_COUNT(RightProbeIndices));

	const auto UpdateHand = [DeltaSeconds, TargetBraceAlpha](const USubCrewMovementComponent::FHandIKProbeResult* Probe, float& Weight, FVector& Target, FCrewHandIKState& State)
	{
		const float TargetWeight = Probe ? TargetBraceAlpha : 0.f;
		Weight = FMath::FInterpTo(Weight, TargetWeight, DeltaSeconds, Probe ? 6.f : 4.f);

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

	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (Mesh)
	{
		const FCrewProceduralSkeletonMap Skeleton = GetSkeletonMap(*this);
		DebugFootRWorld = Mesh->GetSocketTransform(Skeleton.FootR, RTS_World).GetLocation();
		DebugFootLWorld = Mesh->GetSocketTransform(Skeleton.FootL, RTS_World).GetLocation();
	}
}

void USubCrewAnimInstance::DrawProceduralDebug() const
{
	const UWorld* World = GetWorld();
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!World || !Mesh)
	{
		return;
	}

	const bool bDrawAxesNow = bDrawProceduralDebug || ShouldDrawBoneAxes(*this);
	const bool bDrawFeetNow = bDrawProceduralDebug || ShouldDrawFootProbes(*this);
	const float Duration = GetDebugDrawDurationSeconds(*this);
	const float AxisLength = GetDebugAxisLengthCm(*this);
	const FCrewProceduralSkeletonMap Skeleton = GetSkeletonMap(*this);

	if (bDrawAxesNow)
	{
		const FName BoneNames[] =
		{
			Skeleton.Pelvis,
			Skeleton.Spine01,
			Skeleton.Spine03,
			Skeleton.Spine05,
			Skeleton.Head,
			Skeleton.ThighL,
			Skeleton.CalfL,
			Skeleton.FootL,
			Skeleton.ThighR,
			Skeleton.CalfR,
			Skeleton.FootR,
			Skeleton.UpperArmL,
			Skeleton.LowerArmL,
			Skeleton.HandL,
			Skeleton.UpperArmR,
			Skeleton.LowerArmR,
			Skeleton.HandR,
		};

		for (const FName BoneName : BoneNames)
		{
			if (BoneName.IsNone() || Mesh->GetBoneIndex(BoneName) == INDEX_NONE)
			{
				continue;
			}

			const FTransform BoneTransform = Mesh->GetSocketTransform(BoneName, RTS_World);
			DrawDebugCoordinateSystem(World, BoneTransform.GetLocation(), BoneTransform.Rotator(), AxisLength, false, Duration, 0, 0.75f);
		}
	}

	if (bDrawFeetNow)
	{
		const FColor RightColor = FootIK_R_State.bIsPlanted ? FColor::Green : FColor::Red;
		const FColor LeftColor = FootIK_L_State.bIsPlanted ? FColor::Green : FColor::Red;

		DrawDebugSphere(World, DebugFootRWorld, 4.f, 8, RightColor, false, Duration, 0, 1.f);
		DrawDebugSphere(World, DebugFootLWorld, 4.f, 8, LeftColor, false, Duration, 0, 1.f);

		if (FootIK_R_State.bHasHit)
		{
			DrawDebugLine(World, DebugFootRWorld, FootIK_R_State.TargetWorldLocation, RightColor, false, Duration, 0, 1.f);
			DrawDebugPoint(World, FootIK_R_State.TargetWorldLocation, 6.f, RightColor, false, Duration, 0);
		}

		if (FootIK_L_State.bHasHit)
		{
			DrawDebugLine(World, DebugFootLWorld, FootIK_L_State.TargetWorldLocation, LeftColor, false, Duration, 0, 1.f);
			DrawDebugPoint(World, FootIK_L_State.TargetWorldLocation, 6.f, LeftColor, false, Duration, 0);
		}
	}
}
