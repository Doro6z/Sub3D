#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineRuntimeTypes.h"
#include "SubmarineSystemsComponent.generated.h"

class ASubmarineBase;
class ATurretActor;

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubmarineSystemsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubmarineSystemsComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Submarine|Systems")
	const FSubmarineCommandState& GetCommandState() const { return CommandState; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Systems")
	float GetEngineHealth01() const { return EngineHealth01; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Systems")
	float GetElectricalHealth01() const { return ElectricalHealth01; }

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetHelmThrottleCommand(float Value);

	// Ramp the throttle toward +1 (Intent=+1) or -1 (Intent=-1) at
	// ThrottleRampRate per second. Intent=0 halts the ramp and leaves the
	// current HelmThrottleCmd in place. Called from key-held input so holding
	// Z / S increments the thrust smoothly instead of snapping to ±100%.
	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetThrottleRampIntent(float Intent);

	// Ramp the rudder command toward +1 / -1 at RudderRampRate per second.
	// Same contract as SetThrottleRampIntent. The existing return-to-zero
	// behavior (when bRudderHoldEnabled=false) is suppressed while the ramp
	// is active, so the ramp wins over the auto-recenter.
	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetRudderRampIntent(float Intent);

	// Ramp the dive plane command toward +1 / -1 at DivePlaneRampRate per
	// second. Same contract as the throttle / rudder ramps; the existing
	// auto-return when bPlaneHoldEnabled=false is paused while ramping.
	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetDivePlaneRampIntent(float Intent);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetHelmYawCommand(float Value);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetHelmTrimCommand(float Value);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetRudderHoldEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetPlaneHoldEnabled(bool bEnabled);

	// ── Stabilization / Dampeners ───────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems|Stabilization")
	void SetStabilizationMasterEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems|Stabilization")
	void SetAutoSpeedEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems|Stabilization")
	void SetAutoDepthEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems|Stabilization")
	void SetAutoPitchEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems|Stabilization")
	void SetTargetSpeedCmS(float SpeedCmS);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems|Stabilization")
	void SetTargetDepthMeters(float DepthMeters);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems|Stabilization")
	void SetTargetPitchDeg(float PitchDeg);

	// Call from input code when the player manually touches a control axis.
	// Suspends the corresponding dampener for SuspendDurationSeconds.
	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems|Stabilization")
	void NotifyManualInput(FName Axis);

	UFUNCTION(BlueprintPure, Category = "Submarine|Systems|Stabilization")
	bool IsStabilizationMasterEnabled() const { return CommandState.bStabilizationMasterEnabled; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Systems|Stabilization")
	bool IsAutoSpeedActive() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Systems|Stabilization")
	bool IsAutoDepthActive() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Systems|Stabilization")
	bool IsAutoPitchActive() const;

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetGlobalBallastTarget(float Target);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetBallastTargetByIndex(int32 Index, float Target);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetBallastsActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void ResyncAllBallasts();

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetPumpActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetPumpPower01(float Power01);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetEngineBoost(float Value);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetTurretAim(const FRotator& Aim);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Systems")
	void SetTurretFireHeld(bool bHeld);

protected:
	UFUNCTION()
	void OnRep_CommandState();

private:
	ASubmarineBase* ResolveOwnerSubmarine() const;
	void PushPumpStateToHull() const;
	void UpdateStabilization(float DeltaTime);

private:
	UPROPERTY(ReplicatedUsing = OnRep_CommandState, BlueprintReadOnly, Category = "Submarine|Systems", meta = (AllowPrivateAccess = "true"))
	FSubmarineCommandState CommandState;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Submarine|Systems", meta = (AllowPrivateAccess = "true"))
	float EngineHealth01 = 1.f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Submarine|Systems", meta = (AllowPrivateAccess = "true"))
	float ElectricalHealth01 = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Pump", meta = (AllowPrivateAccess = "true"))
	FName DefaultPumpCompartmentId = FName(TEXT("HullMain"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Pump", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float BasePumpRateLitersPerSec = 120.f;

	// ── Stabilization tuning ────────────────────────────────────────────
	// How long a dampener stays suspended after manual input (seconds).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Stabilization", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float SuspendDurationSeconds = 3.f;

	// Proportional gain for auto-speed (how aggressively it corrects throttle).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Stabilization", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float AutoSpeedGain = 0.003f;

	// Legacy depth-error gain. Unused since auto-depth switched to targeting
	// zero vertical velocity; kept only to avoid breaking existing assets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Stabilization", meta = (ClampMin = "0.0", AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Auto-depth targets zero vertical velocity via AutoDepthVelocityGain."))
	float AutoDepthBallastGain = 0.04f;

	// Ballast offset per cm/s of vertical velocity. Positive = rising → add
	// ballast (sink). Default 0.004 gives a ~0.2 ballast swing at 0.5 m/s.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Stabilization", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float AutoDepthVelocityGain = 0.004f;

	// How fast the commanded ballast target interpolates toward the new
	// value each tick (FInterpTo rate). Higher = more aggressive, more
	// prone to oscillation around zero vertical velocity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Stabilization", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float AutoDepthResponseRate = 2.0f;

	// Proportional gain for auto-pitch (hydroplane correction per degree of error).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Stabilization", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float AutoPitchPlaneGain = 0.06f;

	// Throttle ramp rate (units/second) applied while ThrottleRampIntent is
	// non-zero. Default 0.35 → ~2.9s for a full 0→100% sweep. Holding Z / S
	// at Intent=±1 walks HelmThrottleCmd up / down at this rate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Helm", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float ThrottleRampRate = 0.35f;

	// Rudder ramp rate. Default 0.6 → ~1.7s for full lock 0→±1. Faster than
	// throttle because steering needs more responsiveness in tight tunnels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Helm", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float RudderRampRate = 0.6f;

	// Dive plane ramp rate. Default 0.6 → ~1.7s for full deflection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Helm", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float DivePlaneRampRate = 0.6f;

	// Return speed for rudder command when hold is disabled (command units per second).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Helm", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float RudderReturnRate = 0.35f;

	// Return speed for hydroplane command when hold is disabled (command units per second).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Systems|Helm", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float PlaneReturnRate = 0.30f;

	// ── Suspend timers (not replicated — server-only) ───────────────────
	float SpeedSuspendTimer = 0.f;
	float DepthSuspendTimer = 0.f;
	float PitchSuspendTimer = 0.f;

	// Current throttle ramp direction, -1..+1. Server-only; read each tick
	// to advance HelmThrottleCmd by ThrottleRampRate * Intent * DeltaTime.
	float ThrottleRampIntent = 0.f;
	float RudderRampIntent = 0.f;
	float DivePlaneRampIntent = 0.f;
};
