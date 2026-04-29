#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubSonarTypes.h"
#include "SubSonarComponent.generated.h"

/**
 * Active sonar component for ASubmarineBase.
 *
 * Owned by the submarine. Server authoritative.
 * On ping: fires a batch of raycasts over a forward cone, collects hit points,
 * stores them in SonarPoints (replicated), and culls expired points each tick.
 *
 * Does NOT use USonarFieldComponent. Queries world geometry directly via line traces.
 *
 * Display: USubSonarDisplayWidget reads SonarPoints and computes reveal timing from
 * DistanceCm / PropagationSpeedCmS. Rendering is done in Blueprint.
 */
UCLASS(ClassGroup=(Submarine), meta=(BlueprintSpawnableComponent))
class SUB3D_API USubSonarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubSonarComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── Ping parameters ───────────────────────────────────────────────────────

	// Horizontal ray count (azimuth divisions around the forward cone)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	int32 PingRayCountHorizontal = 24;

	// Vertical ray count (elevation rings within the cone)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	int32 PingRayCountVertical = 12;

	// Maximum range of each ray (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "100.0"))
	float PingMaxRangeCm = 15000.f;

	// Simulated wave propagation speed (cm/s) — controls reveal delay on display only
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "100.0"))
	float PropagationSpeedCmS = 3000.f;

	// How long a revealed point stays at full intensity (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "0.1"))
	float PointPeakDurationS = 2.f;

	// How long the fade takes after peak (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "0.1"))
	float PointFadeDurationS = 7.f;

	// Minimum time between pings (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "0.1"))
	float PingCooldownS = 1.5f;

	// Half-angle of the forward cone (degrees). 85 ≈ full hemisphere.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "10.0", ClampMax = "90.0"))
	float PingHalfAngleDeg = 85.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	bool bOmnidirectionalPing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "5.0", ClampMax = "89.0"))
	float OmniPingVerticalHalfAngleDeg = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	TEnumAsByte<ECollisionChannel> PingTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "0.0"))
	float MinAcceptedHitDistanceCm = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	bool bIgnoreAttachedActors = true;

	// If true, keep previous points and refresh overlaps instead of replacing all points every ping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Persistence")
	bool bAccumulatePointsAcrossPings = true;

	// Radius used to consider two points as overlapping geometry (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Persistence", meta = (ClampMin = "1.0"))
	float PointRefreshRadiusCm = 140.f;

	// Safety cap for replicated point count. Lowered for FP — large arrays caused
	// freeze on first ping in Play-as-Client (full TArray re-rep on each change).
	// Post-FP: switch to FFastArraySerializer for delta replication and raise this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Persistence", meta = (ClampMin = "64"))
	int32 MaxRetainedPoints = 512;

	// Server-side cull rate. Was running every server tick at 60Hz, dirtying the
	// replicated SonarPoints array continuously. Throttled to avoid bandwidth waste.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Persistence", meta = (ClampMin = "0.05"))
	float CullIntervalS = 0.5f;

	// Hold mode: while active, pings are attempted on interval (still constrained by PingCooldownS).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Continuous", meta = (ClampMin = "0.05"))
	float ContinuousPingIntervalS = 0.12f;

	// ── Runtime state (replicated) ─────────────────────────────────────────────

	UPROPERTY(ReplicatedUsing = OnRep_SonarPoints, BlueprintReadOnly, Category = "Sonar")
	TArray<FSonarHitPoint> SonarPoints;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Sonar")
	TArray<float> RecentPingTimestamps;

	// Authoritative timestamp of the last accepted ping, replicated as a single
	// float. Lets clients detect a new ping in O(1) without scanning SonarPoints
	// every NativeTick. Mirrors LastPingTime on server.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Sonar")
	float LastReplicatedPingTime = -1000.f;

	// ── API ───────────────────────────────────────────────────────────────────

	/**
	 * Attempt to fire a ping. Returns true if the ping was accepted (cooldown clear).
	 * Server only — call via ServerRouteSonarPing() on ASubPlayerController.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sonar")
	bool TryFirePing();

	// Starts hold ping behavior (used by pressed input).
	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void StartContinuousPing();

	// Stops hold ping behavior (used by released input).
	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void StopContinuousPing();

	UFUNCTION(BlueprintPure, Category = "Sonar")
	bool IsContinuousPingActive() const { return bContinuousPingActive; }

	/** Server world time of the last accepted ping. Exposed for display widgets. */
	UFUNCTION(BlueprintPure, Category = "Sonar")
	float GetLastPingTime() const { return LastPingTime; }

	UFUNCTION(BlueprintPure, Category = "Sonar")
	const TArray<float>& GetRecentPingTimestamps() const { return RecentPingTimestamps; }

	/** Replicated authoritative timestamp of the last accepted ping. Display
	 *  widgets should read this instead of scanning SonarPoints — O(1) check
	 *  for "is there a new ping". */
	UFUNCTION(BlueprintPure, Category = "Sonar")
	float GetLastReplicatedPingTime() const { return LastReplicatedPingTime; }

	/** Local-only timestamp of the most recent locally-requested cosmetic ping.
	 *  Updated by TriggerLocalCosmeticPing(); not replicated. Display widgets
	 *  use this to start the scan-sweep visual immediately on input press,
	 *  without waiting for the RPC roundtrip + replication in Play-as-Client. */
	UFUNCTION(BlueprintPure, Category = "Sonar")
	float GetLocalCosmeticPingTime() const { return LocalCosmeticPingTime; }

	/** Call from the client input handler BEFORE the server RPC. Updates the
	 *  local cosmetic timestamp and fires OnLocalCosmeticPingRequested for BP
	 *  feedback (sound, UI flash). Does NOT trigger the actual ping — the
	 *  server still owns authoritative TryFirePing. */
	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void TriggerLocalCosmeticPing();

	/** BP hook fired locally when TriggerLocalCosmeticPing is called. Use to
	 *  play immediate cosmetic feedback (scan-circle anim, sound) so input
	 *  feels instantaneous despite the server RPC roundtrip in Play-as-Client. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sonar")
	void OnLocalCosmeticPingRequested();

	/**
	 * Blueprint event fired on clients when SonarPoints is updated via replication.
	 * Override in BP to trigger a paint invalidation on the sonar display widget.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sonar")
	void OnPingSonarPointsUpdated();

	// Hook for ping SFX/VFX routing from BP or feedback profile bridge.
	UFUNCTION(BlueprintImplementableEvent, Category = "Sonar")
	void OnPingFired(float PingTime, int32 PointCount);

	UFUNCTION()
	void OnRep_SonarPoints();

private:
	void ExecutePingRaycasts();
	void TickCullExpiredPoints(float DeltaTime);
	void TickContinuousPing(float DeltaTime);
	void MergePingPoints(TArray<FSonarHitPoint>&& NewPoints, float PingTime);

	// Server-only: world time of last accepted ping
	float LastPingTime = -1000.f;
	bool bContinuousPingActive = false;
	float ContinuousPingAccumulator = 0.f;
	bool bLoggedReplicationPointCap = false;

	// Client-only: world time of the most recent local cosmetic ping request.
	// Set by TriggerLocalCosmeticPing on input press, before the server RPC.
	float LocalCosmeticPingTime = -1000.f;

	// Server-only: accumulator for throttled cull execution (see CullIntervalS).
	float CullAccumulator = 0.f;
};
