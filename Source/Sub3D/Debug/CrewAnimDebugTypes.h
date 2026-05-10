#pragma once

#include "CoreMinimal.h"
#include "Submarine/CrewLocomotionTypes.h"
#include "CrewAnimDebugTypes.generated.h"

UENUM(BlueprintType)
enum class ECrewDebugTraversalDomain : uint8
{
	Unknown,
	Ground,
	InteriorWade,
	InteriorSwim,
	ExteriorSwim,
	Ladder,
	Station
};

UENUM(BlueprintType)
enum class ECrewDebugReferenceFrameState : uint8
{
	Unknown,
	SubmarineLocal,
	WorldSpace,
	Transition
};

UENUM(BlueprintType)
enum class ECrewDebugWaterContactState : uint8
{
	Dry,
	ShallowWade,
	DeepWade,
	Swimming,
	Ocean
};

UENUM(BlueprintType)
enum class ECrewAnimDebugWarning : uint8
{
	OutsideNotSwimming,
	SwimmingButEmbarkedDry,
	AnimSwimMismatch,
	InvalidCompartmentOutside,
	NoAnimInstance,
	HandIKActiveDuringSwim,
	FootIKActiveDuringSwim,
	RestPoseDominatesArm,
	GridVelocitySpike
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewDebugImmersionSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FName CompartmentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	ECrewDebugWaterContactState WaterContactState = ECrewDebugWaterContactState::Dry;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	float WaterHeightCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	float Immersion01 = 0.f;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewDebugBonePose
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FName BoneName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FRotator RequestedRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	bool bHasAppliedTransform = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FTransform AppliedTransform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewAnimDebugSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FString CrewName;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	float WorldTimeSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	int64 FrameNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	ECrewDebugTraversalDomain TraversalDomain = ECrewDebugTraversalDomain::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	ECrewDebugReferenceFrameState ReferenceFrame = ECrewDebugReferenceFrameState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FCrewDebugImmersionSample Immersion;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FString RawEmbarkState;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FString RawMovementMode;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	bool bCharacterSwimming = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	bool bAnimSwimming = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	bool bAnimInstanceValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FCrewMoveIntent MoveIntent;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FCrewLocomotionFrame LocomotionFrame;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FVector WorldVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FVector LocalVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	float GridVelocityCmPerSec = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	float HandIKLeftWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	float HandIKRightWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	float FootIKLeftWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	float FootIKRightWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	float StridePhase01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FRotator ArmRestR = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	FRotator ArmRestL = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	TArray<FCrewDebugBonePose> RequestedAndAppliedBones;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|AnimDebug")
	TArray<ECrewAnimDebugWarning> Warnings;
};
