#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "StructuralHullTypes.h"
#include "SubRunPhase.h"
#include "SubGameMode.generated.h"

class AActor;
class ASubGameState;
class ASubmarineBase;
class ASubCrewCharacter;
class ATraversalRouteActor;
class USubHullComponent;

UENUM(BlueprintType)
enum class ESubBootstrapPhase : uint8
{
	None       UMETA(DisplayName = "None"),
	WorldReady UMETA(DisplayName = "World Ready"),
	SubResolved UMETA(DisplayName = "Sub Resolved"),
	SubValidated UMETA(DisplayName = "Sub Validated"),
	CrewSpawned UMETA(DisplayName = "Crew Spawned"),
	CrewEmbarked UMETA(DisplayName = "Crew Embarked"),
	Ready      UMETA(DisplayName = "Ready"),
	Failed     UMETA(DisplayName = "Failed")
};

/**
 * GameMode for the submarine prototype.
 * Owns the authoritative first-playable run phase state machine.
 */
UCLASS()
class SUB3D_API ASubGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ASubGameMode();

	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	UPROPERTY(BlueprintReadWrite, Category = "Submarine")
	ASubmarineBase* ActiveSubmarine = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Run")
	ATraversalRouteActor* ActiveRoute = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Run")
	AActor* StartDock = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Run")
	AActor* EndDock = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run")
	ESubRunPhase RunPhase = ESubRunPhase::Boot;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run|Bootstrap")
	ESubBootstrapPhase BootstrapPhase = ESubBootstrapPhase::None;

	/**
	 * If true, bootstrap requires an ATraversalRouteActor in the world to advance past WorldReady.
	 * Set false on prototype/test maps that have no route. Default false (so test maps don't stall).
	 * Production GameMode subclasses (or DefaultEngine.ini override) should set true.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run|Bootstrap")
	bool bRequireActiveRoute = false;

	/**
	 * Watchdog: if TryAdvanceBootstrap fails to advance the phase for this many seconds,
	 * an Error log is emitted (and re-emitted at the same cadence) describing the stall.
	 * The bootstrap retry timer ticks every 0.5s, so a stall is detected within one timeout window.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run|Bootstrap", meta = (ClampMin = "1.0"))
	float BootstrapPhaseTimeoutSeconds = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Breach", meta = (ClampMin = "1.0"))
	float ScriptedBreachDamageAmount = 150.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run|Breach")
	bool bRunBreachTriggered = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run|Breach")
	bool bBreachObjectiveActive = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run|Breach")
	FName ActiveBreachSheetId = NAME_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run|Breach")
	FName ActiveBreachedCompartmentId = NAME_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run")
	bool bSubmarineInApproachZone = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run|Campaign")
	FName CampaignSegmentID = NAME_None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run|Campaign")
	FTransform RouteStartTransform = FTransform::Identity;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run|Campaign")
	FTransform RouteEndTransform = FTransform::Identity;

	UFUNCTION(BlueprintCallable, Category = "Run")
	void AdvanceRunPhase(ESubRunPhase NewPhase);

	UFUNCTION(BlueprintCallable, Category = "Run")
	void BeginDeparture();

	UFUNCTION(BlueprintCallable, Category = "Run|Breach")
	bool TriggerBreachEvent();

	UFUNCTION(BlueprintCallable, Category = "Run|Breach")
	void NotifyBreachStabilized(FName BreachId);

	UFUNCTION(BlueprintCallable, Category = "Run")
	void TriggerRunFailure();

	UFUNCTION(BlueprintCallable, Category = "Run")
	void SetSubmarineInApproachZone(bool bInApproachZone);

	UFUNCTION(BlueprintCallable, Category = "Run")
	void NotifySubmarineClearedDepartureGate();

	UFUNCTION(BlueprintCallable, Category = "Run")
	void NotifyApproachZoneStateChanged(bool bInApproachZone);

	UFUNCTION(BlueprintPure, Category = "Run")
	ESubRunPhase GetRunPhase() const
	{
		return RunPhase;
	}

	UFUNCTION(BlueprintPure, Category = "Run")
	bool CanAdvanceRunPhase(ESubRunPhase NewPhase) const;

	UFUNCTION(BlueprintPure, Category = "Run|Bootstrap")
	ESubBootstrapPhase GetBootstrapPhase() const { return BootstrapPhase; }

	UFUNCTION(BlueprintCallable, Category = "Run|Bootstrap")
	void TryAdvanceBootstrap();

protected:
	ASubmarineBase* ResolveActiveSubmarine();
	ATraversalRouteActor* ResolveActiveRoute();
	ASubGameState* ResolveSubGameState() const;
	void RefreshRunBootstrapReferences();
	void RefreshBreachObservationBinding();
	void RefreshCampaignSeamData();
	FTransform ResolveCrewSpawnTransform(int32 SlotIndex) const;
	int32 AssignNextSpawnSlot();
	void InitializePlayerCrewState(APlayerController* NewPlayer);

	void SetBootstrapPhase(ESubBootstrapPhase NewPhase);
	bool ResolveWorldBootstrap();
	bool ResolveSubmarineBootstrap();
	bool ValidateSubmarineBootstrap();
	bool SpawnAndEmbarkPendingControllers();
	bool ValidateCrewBootstrap(ASubCrewCharacter* Crew) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APlayerController>> PendingBootstrapControllers;

	UFUNCTION()
	void HandleBreachesUpdated(const TArray<FBreachClusterState>& Breaches);

private:
	bool IsValidRunPhaseTransition(ESubRunPhase FromPhase, ESubRunPhase ToPhase) const;
	bool ResolveScriptedBreachTarget(FName& OutSheetId, FName& OutCompartmentId) const;
	bool IsBreachClusterPresent(FName BreachSheetId, const TArray<FBreachClusterState>& Breaches) const;
	void SyncRunStateToGameState() const;

	void StartBootstrapRetryTimer();
	void StopBootstrapRetryTimer();
	void LogStallIfStuck(const TCHAR* Reason);

	UPROPERTY(Transient)
	TObjectPtr<USubHullComponent> ObservedSubHull = nullptr;

	double PhaseEnteredAtSeconds = 0.;
	double LastStallLogSeconds = 0.;
	FTimerHandle BootstrapRetryTimerHandle;
};
