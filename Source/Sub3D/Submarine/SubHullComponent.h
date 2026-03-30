#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructuralHullTypes.h"
#include "SubmarineRuntimeTypes.h"
#include "SubHullComponent.generated.h"

class USubmarineLayoutAsset;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHullDamageUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBreachesUpdated, const TArray<FBreachClusterState>&, Breaches);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFlowFieldsUpdated, const TArray<FBreachFlowField>&, FlowFields);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompartmentFloodUpdated, const TArray<FCompartmentRuntimeState>&, CompartmentStates);

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubHullComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubHullComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	void InitializeFromLayout(const USubmarineLayoutAsset* InLayout);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	void ApplyHullImpact(const FVector& LocalHitPosition, float Damage, float RadiusCm);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	bool RepairAtLocalPoint(const FVector& LocalRepairPosition, float RepairStrength, float RadiusCm);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	bool RepairAtWorldPoint(const FVector& WorldRepairPosition, float RepairStrength, float RadiusCm);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	void SetCompartmentPumpState(FName CompartmentId, bool bActive, float PumpRateOutLitersPerSec = 100.f);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	void SetAllPumpsActive(bool bActive, float PumpRateOutLitersPerSec = 100.f, FName PreferredCompartmentId = NAME_None);

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	float GetTotalWaterLiters() const;

	UFUNCTION(BlueprintCallable, Category = "Submarine|Hull")
	void ExportCompartmentStates(TArray<FCompartmentState>& OutStates) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	const TArray<FStructuralSheetDef>& GetStructuralSheets() const { return StructuralSheets; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	const TArray<FStructuralSheetRuntimeState>& GetSheetStates() const { return SheetStates; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	const TArray<FBreachClusterState>& GetBreachClusters() const { return BreachClusters; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	const TArray<FBreachFlowField>& GetFlowFields() const { return FlowFields; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	const TArray<FCompartmentRuntimeState>& GetCompartmentStates() const { return CompartmentStates; }

	bool GetCompartmentLocalBounds(FName CompartmentId, FBox& OutBounds) const;
	bool SampleCompartmentStateAtLocalLocation(const FVector& LocalLocation, FCompartmentState& OutState, FBox* OutLocalBounds = nullptr) const;
	bool SampleCompartmentStateAtWorldLocation(const FVector& WorldLocation, FCompartmentState& OutState, FBox* OutLocalBounds = nullptr) const;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning", meta = (ClampMin = "1.0"))
	float InternalConnectionAreaDivisorCm2 = 40000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning", meta = (ClampMin = "0.0"))
	float InternalConnectionHeightToFlowScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning", meta = (ClampMin = "0.0"))
	float InternalConnectionPressureToFlowScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning", meta = (ClampMin = "0.0"))
	float MaxInternalConnectionFlowLitersPerSec = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning", meta = (ClampMin = "0.0"))
	float PumpPressurePenaltyStartKPa = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning", meta = (ClampMin = "0.0"))
	float PumpPressurePenaltyEndKPa = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinPumpEfficiency01 = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Pressure")
	float NominalInternalPressureAtm = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Pressure", meta = (ClampMin = "0.0"))
	float InternalPressureRelaxationRate = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Pressure", meta = (ClampMin = "1.0"))
	float PressureEqualizationOpenAreaCm2 = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Pressure", meta = (ClampMin = "1.0"))
	float MinCompartmentHeightCm = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Pressure", meta = (ClampMin = "0.0"))
	float PressureCriticalDeltaKPa = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float BaseSuctionRadiusCm = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float ActorEjectRadiusCm = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float CreatureEnterRadiusCm = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Debug")
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Debug")
	bool bLogWaterLevels = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Debug", meta = (ClampMin = "0.1"))
	float WaterLevelLogIntervalSeconds = 1.f;

	UPROPERTY(BlueprintAssignable, Category = "Submarine|Hull|Events")
	FOnHullDamageUpdated OnHullDamageUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Submarine|Hull|Events")
	FOnBreachesUpdated OnBreachesUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Submarine|Hull|Events")
	FOnFlowFieldsUpdated OnFlowFieldsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Submarine|Hull|Events")
	FOnCompartmentFloodUpdated OnCompartmentFloodUpdated;

private:
	bool ProjectImpactToSheet(const FVector& LocalHitPosition, int32& OutSheetIndex, FVector2D& OutUV) const;
	void ApplyImpactToSheet(int32 SheetIndex, const FVector2D& UV, float Damage, float RadiusCm);
	void RebuildBreachClusters();
	void UpdateFlowFields();
	void AdvanceFlooding(float DeltaTime);
	void UpdateCompartmentDerivedState(float DeltaTime);
	void MaybeLogWaterLevels(float DeltaTime);
	void EnsureFallbackLayout();
	FCompartmentRuntimeState* FindCompartmentState(FName CompartmentId);
	const FCompartmentRuntimeState* FindCompartmentState(FName CompartmentId) const;
	float ComputeCompartmentMaxWaterHeightCm(FName CompartmentId) const;
	void BroadcastHullDamageUpdated();
	void BroadcastBreachesUpdated();
	void BroadcastFlowFieldsUpdated();
	void BroadcastCompartmentFloodUpdated();

	UFUNCTION()
	void OnRep_SheetStates();

	UFUNCTION()
	void OnRep_BreachClusters();

	UFUNCTION()
	void OnRep_FlowFields();

	UFUNCTION()
	void OnRep_CompartmentStates();

private:
	UPROPERTY()
	TArray<FStructuralSheetDef> StructuralSheets;

	UPROPERTY(ReplicatedUsing = OnRep_SheetStates)
	TArray<FStructuralSheetRuntimeState> SheetStates;

	UPROPERTY(ReplicatedUsing = OnRep_BreachClusters)
	TArray<FBreachClusterState> BreachClusters;

	UPROPERTY(ReplicatedUsing = OnRep_FlowFields)
	TArray<FBreachFlowField> FlowFields;

	UPROPERTY(ReplicatedUsing = OnRep_CompartmentStates)
	TArray<FCompartmentRuntimeState> CompartmentStates;

	float WaterLevelLogAccumulatorSeconds = 0.f;
};
