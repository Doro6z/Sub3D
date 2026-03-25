#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructuralHullTypes.h"
#include "SubmarineRuntimeTypes.h"
#include "SubHullComponent.generated.h"

class USubmarineLayoutAsset;
class UStaticMeshComponent;

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
	const TArray<FBreachClusterState>& GetBreachClusters() const { return BreachClusters; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	const TArray<FBreachFlowField>& GetFlowFields() const { return FlowFields; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Hull")
	const TArray<FCompartmentRuntimeState>& GetCompartmentStates() const { return CompartmentStates; }

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float BaseSuctionRadiusCm = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float ActorEjectRadiusCm = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Tuning")
	float CreatureEnterRadiusCm = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Hull|Debug")
	bool bDrawDebug = false;

private:
	bool ProjectImpactToSheet(const FVector& LocalHitPosition, int32& OutSheetIndex, FVector2D& OutUV) const;
	void ApplyImpactToSheet(int32 SheetIndex, const FVector2D& UV, float Damage, float RadiusCm);
	void RebuildBreachClusters();
	void UpdateFlowFields();
	void AdvanceFlooding(float DeltaTime);
	void EnsureFallbackLayout();
	FCompartmentRuntimeState* FindCompartmentState(FName CompartmentId);
	const FCompartmentRuntimeState* FindCompartmentState(FName CompartmentId) const;

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
};
