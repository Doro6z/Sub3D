#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructuralHullTypes.h"
#include "BreachVfxManagerComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USubHullComponent;
struct FBreachFlowField;

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API UBreachVfxManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBreachVfxManagerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Breach|VFX")
	int32 GetActiveEffectCount() const { return ActiveEffectCount; }

	UFUNCTION(BlueprintPure, Category = "Breach|VFX")
	int32 ComputeDesiredActiveEffectCount(const TArray<FBreachClusterState>& Breaches) const;

	UFUNCTION(BlueprintCallable, Category = "Breach|VFX")
	void RefreshFromCurrentBreaches();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|VFX")
	TObjectPtr<UNiagaraSystem> BreachEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|VFX")
	TObjectPtr<UNiagaraSystem> LeakEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|VFX", meta = (ClampMin = "0.0"))
	float LeakRadiusThresholdCm = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|VFX", meta = (ClampMin = "0"))
	int32 MaxActiveEffects = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|VFX|Parameters", meta = (ClampMin = "0.1"))
	float LeakRateRadiusDivisorCm = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|VFX|Parameters", meta = (ClampMin = "0.1"))
	float LeakPressureForceDivisor = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|VFX|Parameters")
	FName LeakRateParameter = TEXT("LeakRate01");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|VFX|Parameters")
	FName LeakPressureParameter = TEXT("Pressure01");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|VFX|Parameters")
	FName LeakIntensityParameter = TEXT("Intensity01");

protected:
	UFUNCTION()
	void HandleBreachesUpdated(const TArray<FBreachClusterState>& Breaches);

	UFUNCTION()
	void HandleFlowFieldsUpdated(const TArray<FBreachFlowField>& FlowFields);

private:
	static void SortBreachesByPriority(TArray<FBreachClusterState>& Breaches);
	UNiagaraSystem* ResolveEffectForCluster(const FBreachClusterState& Cluster) const;
	UNiagaraComponent* GetOrCreateEffectComponent(int32 EffectIndex);
	const FBreachFlowField* FindFlowFieldForSheet(FName SheetId) const;
	void ApplyEffectParameters(UNiagaraComponent* EffectComponent, const FBreachClusterState& Cluster) const;
	void DeactivateUnusedEffects(int32 FirstUnusedIndex);
	void DestroyPooledEffects();

private:
	UPROPERTY(Transient)
	TObjectPtr<USubHullComponent> SubHull = nullptr;

	UPROPERTY(Transient)
	TArray<FBreachClusterState> LatestBreaches;

	UPROPERTY(Transient)
	TArray<FBreachFlowField> LatestFlowFields;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> EffectPool;

	UPROPERTY(Transient)
	int32 ActiveEffectCount = 0;
};
