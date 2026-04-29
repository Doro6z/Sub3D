#pragma once

#include "CoreMinimal.h"
#include "CrewLocomotionTypes.generated.h"

UENUM(BlueprintType)
enum class ECrewLocomotionStance : uint8
{
	Prone,
	Crouched,
	Standing,
	Swimming
};

UENUM(BlueprintType)
enum class ECrewLocomotionGait : uint8
{
	Idle,
	Walk,
	Sprint,
	CrouchWalk,
	ProneCrawl,
	Swim
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewMoveIntent
{
	GENERATED_BODY()

	/** X = forward/back, Y = right/left. This is input intent, not a camera vector. */
	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	FVector2D MoveAxis = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	FVector WorldMoveDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	FVector LocalMoveDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float MoveInputStrength = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float ControlYawDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float MoveWorldYawDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float DesiredWorldYawDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float DesiredGridYawDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	bool bHasMoveInput = false;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewLocomotionFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	FVector WorldVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	FVector LocalVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float Speed2D = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float DirectionDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float BodyWorldYawDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float BodyLocalYawDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float LocalTurnRateDegPerSec = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float SupportQuality01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	float PostureAlpha = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	ECrewLocomotionStance Stance = ECrewLocomotionStance::Standing;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	ECrewLocomotionGait Gait = ECrewLocomotionGait::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	bool bIsGridAuthoritative = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	bool bIsSwimming = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Locomotion")
	bool bIsRunning = false;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewFootIKState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	bool bHasHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	bool bIsPlanted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FVector TargetWorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FVector TargetWorldNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	float Weight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	float PlantAlpha = 0.f;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewHandIKState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	bool bHasHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FVector TargetWorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	FVector TargetWorldNormal = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|IK")
	float Weight = 0.f;
};

USTRUCT(BlueprintType)
struct SUB3D_API FCrewAnimLocomotionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Anim")
	FCrewMoveIntent MoveIntent;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Anim")
	FCrewLocomotionFrame Frame;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Anim")
	float StridePhase01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Anim")
	float RightFootPlantAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crew|Anim")
	float LeftFootPlantAlpha = 0.f;
};
