#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructuralHullTypes.h"
#include "SubmarineRuntimeTypes.h"
#include "Types/Sub3DCompiledTypes.h"
#include "SubHullComponent.generated.h"

class USubmarineLayoutAsset;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHullDamageUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBreachesUpdated, const TArray<FBreachClusterState>&, Breaches);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFlowFieldsUpdated, const TArray<FBreachFlowField>&, FlowFields);

class UCompiledSubmarineRuntimeAsset;

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubHullComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubHullComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** The baked hull data representing the physical envelope (Proto 03). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Submarine|Hull")
    FSub3DCompiledHullData HullData;

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	void InitializeFromCompiledHull(const UCompiledSubmarineRuntimeAsset* Asset);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	void InitializeFromLayout(const USubmarineLayoutAsset* InLayout);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	void ApplyHullImpact(const FVector& LocalHitPosition, float Damage, float RadiusCm);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	bool RepairAtLocalPoint(const FVector& LocalRepairPosition, float RepairStrength, float RadiusCm);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	bool RepairAtWorldPoint(const FVector& WorldRepairPosition, float RepairStrength, float RadiusCm);


	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	const TArray<FStructuralSheetDef>& GetStructuralSheets() const { return StructuralSheets; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	const TArray<FStructuralSheetRuntimeState>& GetSheetStates() const { return SheetStates; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	const TArray<FBreachClusterState>& GetBreachClusters() const { return BreachClusters; }

	/**
	 * Remove all breach clusters from the hull and broadcast the update.
	 * The SubFlood bridge subscribed to OnBreachesUpdated will clear its
	 * inflow entries as a side effect. Authority only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull|Repair")
	int32 ClearAllBreaches();

	/**
	 * Remove breach clusters whose world-space position lies within Radius of
	 * WorldLocation, then broadcast the update. Used for the repair gameplay
	 * mechanic: the crew stands near a breach, interacts, and the breach is
	 * cleared. Returns the number of clusters removed. Authority only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull|Repair")
	int32 ClearBreachNearLocation(const FVector& WorldLocation, float Radius);

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	const TArray<FBreachFlowField>& GetFlowFields() const { return FlowFields; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull|Flow")
	bool SampleSuctionAtWorldLocation(const FVector& WorldLocation, FVector& OutWorldDirection, float& OutForceScale, EBreachPassageState& OutPassageState) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull|Flow")
	float GetLargestOpenRadiusCm() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Submarine|Hull")
	TObjectPtr<const USubmarineLayoutAsset> LayoutAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float DamageToThicknessScale = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float LeakThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float OpenThreshold = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float BaseLeakFlowLitersPerSec = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float FlowPressureScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning", meta = (ClampMin = "1.0"))
	float ExteriorFloodAreaDivisorCm2 = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning", meta = (ClampMin = "0.0"))
	float MaxExteriorFloodInLitersPerSec = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float BaseSuctionRadiusCm = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float ActorEjectRadiusCm = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float CreatureEnterRadiusCm = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Debug")
	bool bDrawDebug = false;

	UPROPERTY(BlueprintAssignable, Category = "Submarine|Hull|Events")
	FOnHullDamageUpdated OnHullDamageUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Submarine|Hull|Events")
	FOnBreachesUpdated OnBreachesUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Submarine|Hull|Events")
	FOnFlowFieldsUpdated OnFlowFieldsUpdated;

private:
	bool ProjectImpactToSheet(const FVector& LocalHitPosition, int32& OutSheetIndex, FVector2D& OutUV) const;
	void ApplyImpactToSheet(int32 SheetIndex, const FVector2D& UV, float Damage, float RadiusCm);
	void RebuildBreachClusters();
	void UpdateFlowFields();
	// LEGACY (Phase 7A, 2026-04-10) — Proto fallback. Used only when no
	// LayoutAsset is assigned. Will be removed in Phase 7B once the generator
	// path is the only init route.
	void EnsureFallbackLayout();
	void BroadcastHullDamageUpdated();
	void BroadcastBreachesUpdated();
	void BroadcastFlowFieldsUpdated();

	UFUNCTION()
	void OnRep_SheetStates();

	UFUNCTION()
	void OnRep_BreachClusters();

	UFUNCTION()
	void OnRep_FlowFields();

private:
	UPROPERTY()
	TArray<FStructuralSheetDef> StructuralSheets;

	UPROPERTY(ReplicatedUsing = OnRep_SheetStates)
	TArray<FStructuralSheetRuntimeState> SheetStates;

	UPROPERTY(ReplicatedUsing = OnRep_BreachClusters)
	TArray<FBreachClusterState> BreachClusters;

	UPROPERTY(ReplicatedUsing = OnRep_FlowFields)
	TArray<FBreachFlowField> FlowFields;
};
