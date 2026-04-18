#pragma once

#include "CoreMinimal.h"
#include "SubmarineRuntimeTypes.generated.h"

USTRUCT(BlueprintType)
struct FSubmarineCommandState
{
	GENERATED_BODY()

	// ── Helm inputs ─────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Helm")
	float HelmThrottleCmd = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Helm")
	float HelmYawCmd = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Helm")
	float HelmTrimCmd = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Helm")
	bool bRudderHoldEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Helm")
	bool bPlaneHoldEnabled = false;

	// ── Stabilization / Dampeners ───────────────────────────────────────
	// Master toggle: acts as a kill switch — when off, all auto-systems are
	// disabled regardless of their individual flag. Default true so each
	// per-axis toggle (AutoSpeed/AutoDepth/AutoPitch) works on its own
	// without the player having to enable the master first.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Stabilization")
	bool bStabilizationMasterEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Stabilization")
	bool bAutoSpeedEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Stabilization")
	bool bAutoDepthEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Stabilization")
	bool bAutoPitchEnabled = false;

	// Target values for each auto-system.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Stabilization")
	float TargetSpeedCmS = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Stabilization")
	float TargetDepthMeters = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Stabilization")
	float TargetPitchDeg = 0.f;

	// ── Ballasts ────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Ballast")
	float GlobalBallastTarget01 = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Ballast")
	bool bBallastsActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Helm")
	float MainTrimBiasCmd = 0.f;

	// ── Pumps ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Pump")
	bool bPumpActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Pump")
	float PumpPower01 = 1.f;

	// ── Engine ──────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Engine")
	float EngineBoostCmd = 0.f;

	// ── Turret ──────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Turret")
	FRotator TurretAimCmd = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command|Turret")
	bool bTurretFireHeld = false;

	// ── Sync ────────────────────────────────────────────────────────────
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
	float FloodRateIn = 0.f;

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
