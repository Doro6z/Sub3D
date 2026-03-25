#pragma once

#include "CoreMinimal.h"
#include "SubmarineTypes.generated.h"

UENUM(BlueprintType)
enum class ESubStationType : uint8
{
	None   UMETA(DisplayName = "None"),
	Helm   UMETA(DisplayName = "Helm"),
	Engine UMETA(DisplayName = "Engine"),
	Pump   UMETA(DisplayName = "Pump", Hidden),
	Repair UMETA(DisplayName = "Repair", Hidden),
	Ballast UMETA(DisplayName = "Ballast"),
	Turret  UMETA(DisplayName = "Turret")
};

UENUM(BlueprintType)
enum class EPumpState : uint8
{
	Nominal  UMETA(DisplayName = "Nominal"),
	Degraded UMETA(DisplayName = "Degraded"),
	Dead     UMETA(DisplayName = "Dead")
};

/**
 * Represents a single ballast tank on the submarine.
 * Multiple tanks allow independent control of fill level,
 * enabling pitch trimming (e.g. front heavy = nose down).
 */
USTRUCT(BlueprintType)
struct FBallastTank
{
	GENERATED_BODY()

	// Position in submarine local space (X: forward/backward, Z: up/down)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballast")
	FVector LocalPosition = FVector::ZeroVector;

	// Max water volume in m³
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballast")
	float Volume = 10.f;

	// Current fill level [0.0 = empty, 1.0 = full] — replicated
	UPROPERTY(BlueprintReadOnly, Category = "Ballast")
	float FillLevel = 0.5f;

	// Target fill level requested by player [0.0 - 1.0]
	UPROPERTY(BlueprintReadOnly, Category = "Ballast")
	float TargetFill = 0.5f;

	// Fill/drain rate in fraction per second (e.g. 0.05 = 5%/s)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballast")
	float PumpFlowRate = 0.05f;

	// Pump state — replicated
	UPROPERTY(BlueprintReadOnly, Category = "Ballast")
	EPumpState PumpState = EPumpState::Nominal;
};
