#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineRuntimeTypes.h"
#include "SubFloodComponent.generated.h"

class USubmarineDefinition;
class USubmarineLayoutAsset;
class USubHullBoundaryComponent;
class UCompartmentVolumeComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFloodStateUpdated, const TArray<FCompartmentState>&, States);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFloodInitialized);

// --- Runtime breach state (not part of the generated definition) ---------

USTRUCT(BlueprintType)
struct FCompartmentBreachState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breach")
	FName CompartmentId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breach")
	bool bBreached = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breach")
	float InflowRateLitersPerSec = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breach")
	FVector BreachLocalCenter = FVector::ZeroVector;
};

// --- Runtime per-compartment flood state ---------------------------------

USTRUCT()
struct FFloodCompartmentState
{
	GENERATED_BODY()

	UPROPERTY()
	FName CompartmentId = NAME_None;

	UPROPERTY()
	float CapacityLiters = 0.f;

	UPROPERTY()
	float MaxWaterHeightCm = 200.f;

	UPROPERTY()
	float CurrentWaterLiters = 0.f;

	UPROPERTY()
	float WaterLevelNormalized = 0.f;

	UPROPERTY()
	float WaterHeightCm = 0.f;

	UPROPERTY()
	float FloodRateIn = 0.f;

	UPROPERTY()
	float FloodRateOut = 0.f;

	// Pump
	UPROPERTY()
	bool bPumpActive = false;

	UPROPERTY()
	float PumpRateLitersPerSec = 0.f;

	// Breach inflow (external)
	UPROPERTY()
	float BreachInflowLitersPerSec = 0.f;
};

// --- Runtime per-edge state ----------------------------------------------

USTRUCT()
struct FFloodEdgeState
{
	GENERATED_BODY()

	UPROPERTY()
	FName ClosureId = NAME_None;

	UPROPERTY()
	FName VolumeA = NAME_None;

	UPROPERTY()
	FName VolumeB = NAME_None;

	UPROPERTY()
	float PassageAreaCm2 = 0.f;

	UPROPERTY()
	bool bExteriorEdge = false;

	// Door state: true = closed = no flow through this edge
	UPROPERTY()
	bool bClosed = false;
};

/**
 * Standalone flood simulation component.
 * Reads topology from USubmarineDefinition. Simulates water volume
 * per compartment based on breach inflows, door states, and pumps.
 * Server-authoritative, replicates compartment states to clients.
 */
UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubFloodComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubFloodComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Initialization --------------------------------------------------

	/** Initialize flood graph from a generated submarine definition. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Flood")
	void InitializeFromDefinition(const USubmarineDefinition* Definition);

	/** Initialize from a legacy layout asset (synthesizes edges from structural sheets). */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Flood")
	void InitializeFromLayout(const USubmarineLayoutAsset* Layout);

	/**
	 * Initialize from in-BP UCompartmentVolumeComponent children (Craniata manual path).
	 * Builds one node per CompartmentId, capacity derived from the box extent,
	 * max water height from Z extent. No edges synthesized — each compartment is
	 * isolated for FP testing (breaches fill only their own compartment).
	 * Doors/edges are deferred post-FP.
	 */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Flood")
	void InitializeFromCompartmentVolumes(const TArray<UCompartmentVolumeComponent*>& Volumes);

	UFUNCTION(BlueprintPure, Category = "Submarine|Flood")
	bool IsInitialized() const { return CompartmentStates.Num() > 0; }

	// --- Door state ------------------------------------------------------

	/** Set the open/closed state of a connection (by ConnectionId). */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Flood")
	void SetDoorState(FName ConnectionId, bool bClosed);

	// --- Breach ----------------------------------------------------------

	/** Create or update a breach inflow on a compartment. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Flood")
	void CreateBreach(FName CompartmentId, float InflowRateLitersPerSec, FVector BreachLocalCenter = FVector::ZeroVector);

	/** Remove a breach from a compartment. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Flood")
	void RemoveBreach(FName CompartmentId);

	// --- Pump ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Submarine|Flood")
	void SetPumpActive(FName CompartmentId, bool bActive, float RateLitersPerSec = 100.f);

	// --- Debug -----------------------------------------------------------

	/** Directly set a compartment flood level (0-1). Bypasses simulation. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Submarine|Flood|Debug")
	void SetCompartmentFloodDirect(FName CompartmentId, float Level01);

	// --- Queries ---------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Submarine|Flood")
	float GetCompartmentFloodLevel01(FName CompartmentId) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Flood")
	float GetCompartmentWaterLiters(FName CompartmentId) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Flood")
	float GetCompartmentWaterHeightCm(FName CompartmentId) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Flood")
	float GetTotalWaterLiters() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Flood")
	float GetTotalWaterMassKg() const;

	/** Export to the shared FCompartmentState format used by existing systems. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Flood")
	void ExportCompartmentStates(TArray<FCompartmentState>& OutStates) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Flood")
	const TArray<FCompartmentBreachState>& GetBreaches() const { return Breaches; }

	// --- Events ----------------------------------------------------------

	/** Broadcast each tick with exported FCompartmentState array. */
	UPROPERTY(BlueprintAssignable, Category = "Submarine|Flood|Events")
	FOnFloodStateUpdated OnFloodStateUpdated;

	/** Fired once after InitializeFromDefinition completes successfully. */
	UPROPERTY(BlueprintAssignable, Category = "Submarine|Flood|Events")
	FOnFloodInitialized OnFloodInitialized;

	// --- Tuning ----------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Flood|Tuning", meta = (ClampMin = "0.0"))
	float MaxExteriorInflowLitersPerSec = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Flood|Tuning", meta = (ClampMin = "0.0"))
	float MaxInternalFlowLitersPerSec = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Flood|Tuning", meta = (ClampMin = "1.0"))
	float InternalConnectionAreaDivisorCm2 = 40000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Flood|Tuning", meta = (ClampMin = "1.0"))
	float MinCompartmentHeightCm = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Flood|Debug")
	bool bLogWaterLevels = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Flood|Debug", meta = (ClampMin = "0.1"))
	float WaterLevelLogIntervalSeconds = 1.f;

private:
	void AdvanceFlooding(float DeltaTime);
	void UpdateDerivedState();
	void MaybeLogWaterLevels(float DeltaTime);

	FFloodCompartmentState* FindState(FName CompartmentId);
	const FFloodCompartmentState* FindState(FName CompartmentId) const;
	FFloodEdgeState* FindEdge(FName ClosureId);
	TArray<FCompartmentState> GetExportedStates() const;

	UFUNCTION()
	void OnRep_CompartmentStates();

	UPROPERTY(ReplicatedUsing = OnRep_CompartmentStates)
	TArray<FFloodCompartmentState> CompartmentStates;

	UPROPERTY(Replicated)
	TArray<FFloodEdgeState> EdgeStates;

	UPROPERTY(Replicated)
	TArray<FCompartmentBreachState> Breaches;

	/**
	 * Server-side tracking of hull boundary components spawned for active breaches.
	 * Each active breach owns one USubHullBoundaryComponent on the sub actor (Kind=Breach)
	 * that drives crew handoff (Embarked <-> Outside via the breach plane).
	 * Weak so destruction of the sub autoclears; lifecycle is CreateBreach/RemoveBreach.
	 */
	TMap<FName, TWeakObjectPtr<USubHullBoundaryComponent>> BreachBoundariesByCompartment;

	float WaterLevelLogAccumulator = 0.f;

	// Tracks whether the client has received its first CompartmentStates replication.
	bool bClientInitialized = false;
};
