#pragma once

#include "CoreMinimal.h"
#include "SubmarineRuntimeTypes.generated.h"

USTRUCT(BlueprintType)
struct FSubmarineCommandState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	float HelmThrottleCmd = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	float HelmYawCmd = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	float HelmTrimCmd = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	bool bAutoDepthEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	float TargetDepthMeters = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	float GlobalBallastTarget01 = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	float MainTrimBiasCmd = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	bool bPumpActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	float PumpPower01 = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	float EngineBoostCmd = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	FRotator TurretAimCmd = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	bool bTurretFireHeld = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	bool bBallastsActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command")
	int32 LastProcessedFrame = 0;
};

USTRUCT(BlueprintType)
struct FSubmarineNetState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	FVector_NetQuantize100 WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	FRotator QuantizedRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	FVector_NetQuantize10 LinearVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	FVector_NetQuantize10 AngularVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	float ForwardSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	float VerticalSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	float DepthMeters = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	float FloodedMassKg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	float BallastGlobal01 = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	float MainTrim01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	bool bPumpActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Net")
	int32 SimFrame = 0;
};

USTRUCT(BlueprintType)
struct FCompartmentState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FName CompartmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	float FloodLevel01 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	float WaterMassLiters = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	float WaterHeightCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	float InternalPressureKPa = 101.325f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	float ExternalPressureKPa = 101.325f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	float PressureDeltaKPa = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	bool bCritical = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	bool bElectricalsWet = false;
};

USTRUCT(BlueprintType)
struct FDoorState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName DoorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName CompartmentA = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName CompartmentB = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bClosed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bLocked = false;
};

USTRUCT(BlueprintType)
struct FRadarContact
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar")
	FVector LocalPosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar")
	float Strength01 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar")
	uint8 Category = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radar")
	bool bHostile = false;
};
