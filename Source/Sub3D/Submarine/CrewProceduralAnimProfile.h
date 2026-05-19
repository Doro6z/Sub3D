#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CrewProceduralAnimProfile.generated.h"

USTRUCT(BlueprintType)
struct SUB3D_API FCrewProceduralAxisTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axis", meta = (ClampMin = "0", ClampMax = "2"))
	int32 Axis = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axis")
	bool bNegate = false;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewProceduralRigAxisProfile
{
	GENERATED_BODY()

	FCrewProceduralRigAxisProfile()
	{
		ShoulderSwing.Axis = 0;
		ShoulderLower.Axis = 2;
		ElbowBend.Axis = 2;
		SpineTwist.Axis = 2;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes")
	FCrewProceduralAxisTuning ThighSwing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes")
	FCrewProceduralAxisTuning KneeBend;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes")
	FCrewProceduralAxisTuning FootPitch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes")
	FCrewProceduralAxisTuning ShoulderSwing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes")
	FCrewProceduralAxisTuning ShoulderLower;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes")
	FCrewProceduralAxisTuning ElbowBend;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes")
	FCrewProceduralAxisTuning SpineBend;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axes")
	FCrewProceduralAxisTuning SpineTwist;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewProceduralGaitTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "1.0"))
	float SpeedCmS = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.1"))
	float StepRate = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.01", ClampMax = "0.95"))
	float StanceFraction = 0.58f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float ThighForwardDeg = 24.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float ThighBackDeg = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float KneeBendDeg = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float FootPitchDeg = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float ToeOffDeg = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float PelvisBobCm = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float PelvisCompressionCm = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float SpineLeanDeg = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float PelvisRollDeg = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float PelvisTwistDeg = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float ArmSwingDeg = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0"))
	float ElbowBendDeg = 10.f;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewProceduralGroundedTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grounded")
	FCrewProceduralGaitTuning Walk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grounded")
	FCrewProceduralGaitTuning Jog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grounded")
	FCrewProceduralGaitTuning Run;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grounded", meta = (ClampMin = "0.0"))
	float StepSmoothingSpeed = 12.f;

	FCrewProceduralGroundedTuning()
	{
		Walk.SpeedCmS = 220.f;
		Walk.StepRate = 2.f;
		Walk.StanceFraction = 0.60f;
		Walk.ThighForwardDeg = 24.f;
		Walk.ThighBackDeg = 15.f;
		Walk.KneeBendDeg = 28.f;
		Walk.FootPitchDeg = 10.f;
		Walk.ToeOffDeg = 8.f;
		Walk.PelvisBobCm = 1.4f;
		Walk.PelvisCompressionCm = 0.8f;
		Walk.SpineLeanDeg = 1.f;
		Walk.PelvisRollDeg = 3.f;
		Walk.PelvisTwistDeg = 2.f;
		Walk.ArmSwingDeg = 14.f;
		Walk.ElbowBendDeg = 10.f;

		Jog.SpeedCmS = 360.f;
		Jog.StepRate = 2.6f;
		Jog.StanceFraction = 0.54f;
		Jog.ThighForwardDeg = 34.f;
		Jog.ThighBackDeg = 22.f;
		Jog.KneeBendDeg = 42.f;
		Jog.FootPitchDeg = 14.f;
		Jog.ToeOffDeg = 12.f;
		Jog.PelvisBobCm = 2.4f;
		Jog.PelvisCompressionCm = 1.2f;
		Jog.SpineLeanDeg = 3.5f;
		Jog.PelvisRollDeg = 4.5f;
		Jog.PelvisTwistDeg = 3.f;
		Jog.ArmSwingDeg = 24.f;
		Jog.ElbowBendDeg = 14.f;

		Run.SpeedCmS = 616.f;
		Run.StepRate = 3.2f;
		Run.StanceFraction = 0.46f;
		Run.ThighForwardDeg = 48.f;
		Run.ThighBackDeg = 30.f;
		Run.KneeBendDeg = 62.f;
		Run.FootPitchDeg = 18.f;
		Run.ToeOffDeg = 18.f;
		Run.PelvisBobCm = 4.f;
		Run.PelvisCompressionCm = 1.6f;
		Run.SpineLeanDeg = 7.f;
		Run.PelvisRollDeg = 6.f;
		Run.PelvisTwistDeg = 4.f;
		Run.ArmSwingDeg = 38.f;
		Run.ElbowBendDeg = 20.f;
	}
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewProceduralSwimTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim", meta = (ClampMin = "1.0"))
	float ReferenceSpeedCmS = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim", meta = (ClampMin = "0.0"))
	float IdleStrokeRate = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim", meta = (ClampMin = "0.0"))
	float MoveStrokeRate = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim", meta = (ClampMin = "0.0"))
	float SprintStrokeRate = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim", meta = (ClampMin = "0.0"))
	float SmoothingSpeed = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Body")
	float BodyPitchDeg = 13.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Body")
	float SprintBodyPitchDeg = 19.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Body")
	float VerticalInputPitchDeg = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Body")
	float LateralInputRollDeg = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Body")
	float BodyWaveDeg = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Body", meta = (ClampMin = "0.0"))
	float PelvisBobCm = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Legs", meta = (ClampMin = "0.0"))
	float KickThighDeg = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Legs", meta = (ClampMin = "0.0"))
	float SprintKickThighDeg = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Legs", meta = (ClampMin = "0.0"))
	float KickKneeDeg = 24.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Legs", meta = (ClampMin = "0.0"))
	float SprintKickKneeDeg = 36.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Legs", meta = (ClampMin = "0.0"))
	float FootPitchDeg = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Arms", meta = (ClampMin = "0.0"))
	float ShoulderStrokeDeg = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Arms", meta = (ClampMin = "0.0"))
	float SprintShoulderStrokeDeg = 36.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Arms", meta = (ClampMin = "0.0"))
	float ShoulderScullDeg = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Arms", meta = (ClampMin = "0.0"))
	float ElbowBendDeg = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Arms", meta = (ClampMin = "0.0"))
	float SprintElbowBendDeg = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim|Arms", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FirstPersonArmScale = 0.55f;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewProceduralSubMotionTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubMotion", meta = (ClampMin = "0.0"))
	float TiltCounterLeanScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubMotion", meta = (ClampMin = "0.0"))
	float LinearAccelerationLeanScale = 0.004f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubMotion", meta = (ClampMin = "0.0"))
	float AngularVelocityLeanScale = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubMotion", meta = (ClampMin = "0.0"))
	float AngularAccelerationLeanScale = 0.002f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubMotion", meta = (ClampMin = "0.0"))
	float ShoulderInertiaScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SubMotion", meta = (ClampMin = "0.0"))
	float MaxLeanDeg = 10.f;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewProceduralSkeletonMap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName Root = TEXT("Root");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName Pelvis = TEXT("pelvis");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName Spine01 = TEXT("spine_01");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName Spine02 = TEXT("spine_02");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName Spine03 = TEXT("spine_03");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName Spine04 = TEXT("spine_04");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName Spine05 = TEXT("spine_05");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName Neck01 = TEXT("neck_01");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName Head = TEXT("head");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName ThighL = TEXT("thigh_l");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName CalfL = TEXT("calf_l");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName FootL = TEXT("foot_l");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName ThighR = TEXT("thigh_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName CalfR = TEXT("calf_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName FootR = TEXT("foot_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName UpperArmL = TEXT("upperarm_l");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName LowerArmL = TEXT("lowerarm_l");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName HandL = TEXT("hand_l");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName UpperArmR = TEXT("upperarm_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName LowerArmR = TEXT("lowerarm_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skeleton")
	FName HandR = TEXT("hand_r");
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewProceduralWalkTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "1.0"))
	float StrideLengthCm = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "1.0"))
	float MaxWalkSpeedCmS = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0.0"))
	float LegSwingDeg = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0.0"))
	float KneeBendDeg = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0.0"))
	float FootCounterDeg = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0.0"))
	float ArmSwingDeg = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0.0"))
	float ElbowBendDeg = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0.0"))
	float PelvisBobCm = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0.0"))
	float PelvisSwayDeg = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0.0"))
	float SpineLeanDeg = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0.0"))
	float StepSmoothingSpeed = 12.f;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewProceduralDebugTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawBoneAxes = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawFootProbes = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "1.0"))
	float AxisLengthCm = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "0.0"))
	float DrawDurationSeconds = 0.f;
};

UCLASS(BlueprintType)
class SUB3D_API UCrewProceduralAnimProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crew|Procedural")
	FCrewProceduralSkeletonMap Skeleton;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crew|Procedural")
	FCrewProceduralRigAxisProfile RigAxes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crew|Procedural")
	FCrewProceduralGroundedTuning Grounded;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crew|Procedural")
	FCrewProceduralSwimTuning Swim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crew|Procedural")
	FCrewProceduralSubMotionTuning SubMotion;

	/** Legacy walk tuning kept only so existing assets load. New code uses Grounded.Walk/Jog/Run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crew|Procedural")
	FCrewProceduralWalkTuning Walk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crew|Procedural")
	FCrewProceduralDebugTuning Debug;
};
