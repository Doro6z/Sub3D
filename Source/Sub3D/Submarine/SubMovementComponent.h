#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineRuntimeTypes.h"
#include "SubmarineTypes.h"
#include "SubMovementComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSubmarineSnapped, float, const FSubmarineNetState&);

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float BaseMass = 200000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float WaterDensity = 1025.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float SubmergedVolume = 205.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	bool bAutoNeutralBuoyancyOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NeutralBuoyancyFill01 = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float NeutralBuoyancyMassBiasKg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FVector DragCoefficients = FVector(0.18f, 1.2f, 1.1f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FVector CrossSections = FVector(2.f, 20.f, 20.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxThrust = 25000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxForwardSpeed = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxReverseSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxVerticalSpeed = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float IdleForwardSpeedDamping = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float LateralSpeedDamping = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float ContactVelocityDamping = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float RudderTurnRate = 15.f;

	// Minimum rudder authority at standstill (0..1). Allows very slow turn in place.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RudderStandstillAuthority = 0.08f;

	// Speed at which rudder reaches full authority (cm/s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float RudderFullAuthoritySpeed = 350.f;

	// Yaw rate damping. Lower = more responsive, higher = more sluggish.
	// Halved from 2.0 for more hydrodynamic inertia feel; rudder also halved
	// so steady-state yaw rate at full input is unchanged (15/1 vs 30/2).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float YawRateDamping = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float DivePlanePitchRate = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float HydroplaneAuthoritySpeed = 600.f;

	// Pitch rate damping. Halved from 2.2 for inertia feel; hydroplane input
	// halved in tandem so steady-state pitch rate at full input is unchanged.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float PitchRateDamping = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float PitchFromHydroplaneAccel = 12.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float VerticalFromPitchFactor = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float MaxDivePlanePitch = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float BallastPitchFactor = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	bool bEnableBallastTrimPitch = true;

	// Halved alongside PitchRateDamping to preserve steady-state trim rate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float BallastTrimPitchRate = 4.f;

	// Amplifier on the ballast fill deviation from neutral. Values > 1 make
	// the player feel ballast changes more aggressively on vertical motion.
	// Neutral buoyancy is unaffected (deviation = 0 * anything = 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Ballast", meta = (ClampMin = "0.0"))
	float BallastEffectScale = 2.f;

	// How much pitch modulates the forward speed cap. 0 = no coupling.
	// Example: 0.2 gives factor 0.9 at pitch +30° (nose up) and 1.1 at -30°.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PitchVmaxInfluence = 0.2f;

	// ── BG Restoring Moment ─────────────────────────────────────────────
	// Vertical distance from center of buoyancy to center of gravity (cm).
	// Positive = B above G = stable. Creates passive pitch return-to-level.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Stability")
	float BG_DistanceCm = 30.f;

	// Strength of the BG restoring torque on pitch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Stability")
	float PitchRestorationDamping = 3.f;

	// ── Engine Spool ────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Thrust")
	float EngineSpoolUpRate = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Thrust")
	float EngineSpoolDownRate = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Flooding")
	float FloodedMassInfluence = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Ballasts")
	TArray<FBallastTank> Ballasts;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Ballasts")
	float GlobalTargetFill = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float CurrentDepth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float FloodedMassKg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float ForwardSpeedCmS = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float EffectivePowerInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float YawRateDegPerSec = 0.f;

	// Server-authoritative sim rate. Higher = smoother visuals (smaller
	// extrapolation gap between sim steps) at the cost of CPU. 60 keeps
	// sub-tick visual jitter under 17 ms at any velocity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Net")
	float FixedSimulationHz = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Net")
	float InterpSnapDistanceCm = 500.f;

	// How long the client may extrapolate past the latest snapshot using the
	// replicated velocity before clamping. Caps unbounded drift if a snapshot
	// is dropped or arrives very late. 200 ms = 6 frames at 30 fps server sim.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Net", meta = (ClampMin = "0.0"))
	float MaxClientExtrapolationSec = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Submarine|Debug")
	bool bDebugLogSubMovement = false;

	UPROPERTY(EditAnywhere, Category = "Submarine|Debug")
	bool bDebugLogCollisionSweeps = false;

	FOnSubmarineSnapped OnSubmarineSnapped;

	// -------------------------------------------------------------------------
	// Input — server-authoritative, replicated to clients for visual feedback.
	// The PlayerController owning the helmsman routes raw input to the server
	// via ServerRPC; the server calls the Set*Input functions below, which
	// write to the replicated fields so every client sees the same rudder /
	// hydroplane / thrust value and can drive the visual mesh rotation off
	// those values with zero additional bandwidth work.
	//
	// For low-frequency input changes (rudder held, dive plane held), Replicated
	// floats are cheap: 3 × 4 bytes per dirty rep, well under the cost of the
	// existing FSubmarineNetState snapshot. If profiling ever requires tighter
	// packing, promote these to fixed-point int8 or fold them into
	// FSubmarineNetState — no public API change required.
	// -------------------------------------------------------------------------

	/** Set thrust input clamped to [-1, 1]. Server-authoritative. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Input", BlueprintAuthorityOnly)
	void SetPowerInput(float Value);

	/** Alias kept for legacy callers; routes through SetPowerInput. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Input", BlueprintAuthorityOnly)
	void SetThrustInput(float Value);

	/** Set rudder input clamped to [-1, 1]. Server-authoritative. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Input", BlueprintAuthorityOnly)
	void SetRudderInput(float Value);

	/** Set dive plane input clamped to [-1, 1]. Server-authoritative. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Input", BlueprintAuthorityOnly)
	void SetDivePlaneInput(float Value);

	/** Current rudder input, -1..1, replicated to all clients. Use this for mesh rotation. */
	UFUNCTION(BlueprintPure, Category = "Submarine|Input")
	float GetRudderInput() const { return RudderInput; }

	/** Current dive plane input, -1..1, replicated to all clients. Use this for mesh rotation. */
	UFUNCTION(BlueprintPure, Category = "Submarine|Input")
	float GetDivePlaneInput() const { return DivePlaneInput; }

	/** Current thrust input, -1..1, replicated to all clients. */
	UFUNCTION(BlueprintPure, Category = "Submarine|Input")
	float GetThrustInput() const { return ThrustInput; }

	void SetBallastTarget(int32 Index, float Target);
	void ResyncAllBallasts();
	void ApplyCommandState(const FSubmarineCommandState& CommandState);

	/** Copy authored performance fields (BaseMass, MaxSpeed, MaxThrust) from the
	  * Definition onto this component. Fields with a value of 0 are skipped.
	  * Recomputes neutral buoyancy if BaseMass changed. */
	void ApplyPerformanceProfileFromDefinition(const class USubmarineDefinition* Definition);

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float ComputeTotalMass() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float ComputeBuoyancyForce() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float ComputeCenterOfMassXOffset() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Physics")
	float GetPressureAtDepth(float DepthMeters) const;

	void HandleReplicatedNetState(const FSubmarineNetState& NewState);
	int32 GetSimFrameCounter() const { return SimFrameCounter; }
	float GetPitchRateDegPerSec() const { return PitchRateDegPerSec; }
	float GetYawRateDegPerSec() const { return YawRateDegPerSec; }

private:
	// Input values replicated to all clients for visual feedback (rudder mesh
	// yaw, hydroplane mesh pitch). Server-authoritative; writes come from
	// SetThrustInput / SetRudderInput / SetDivePlaneInput.
	UPROPERTY(Replicated)
	float ThrustInput = 0.f;

	UPROPERTY(Replicated)
	float RudderInput = 0.f;

	UPROPERTY(Replicated)
	float DivePlaneInput = 0.f;

	float SpooledPower = 0.f;
	float PitchRateDegPerSec = 0.f;
	float SimAccumulator = 0.f;
	int32 SimFrameCounter = 0;

	FSubmarineNetState PrevSnapshot;
	FSubmarineNetState TargetSnapshot;
	float InterpAlpha = 1.f;
	float InterpDuration = 1.f / 20.f;
	float DebugLogTimer = 0.f;
	bool bHasReceivedSnapshot = false;
	float ClientExtrapolationElapsedSec = 0.f;

	FVector AuthoritativeLocation = FVector::ZeroVector;
	FRotator AuthoritativeRotation = FRotator::ZeroRotator;
	bool bHasVisualExtrapolation = false;

	void UpdateBallasts(float DeltaTime);
	void SimulateStep(float DeltaTime);
	void ApplyPhysics(float DeltaTime);
	float ComputeFloodedMassKg() const;
	void InterpolateClient(float DeltaTime);
	void InitializeNeutralBuoyancy();
};
