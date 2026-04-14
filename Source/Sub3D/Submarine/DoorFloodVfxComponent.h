#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructuralHullTypes.h"
#include "SubmarineRuntimeTypes.h"
#include "DoorFloodVfxComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USubHullComponent;
class USubmarineCompartmentComponent;
class ASubmarineBase;

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API UDoorFloodVfxComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDoorFloodVfxComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Flood|DoorVFX")
	int32 GetActiveCascadeCount() const { return ActiveCascadeCount; }

	UFUNCTION(BlueprintCallable, Category = "Flood|DoorVFX")
	void RefreshFromCurrentFloodState();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX")
	TObjectPtr<UNiagaraSystem> CascadeEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX", meta = (ClampMin = "0.0"))
	float HeightDeltaThresholdCm = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX", meta = (ClampMin = "1.0"))
	float MaxHeightDeltaCm = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX", meta = (ClampMin = "0"))
	int32 MaxActiveCascades = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|Parameters")
	FName FlowIntensityParam = TEXT("FlowIntensity01");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|DoorVFX|Parameters")
	FName FlowDirectionParam = TEXT("FlowDirection");

protected:
	UFUNCTION()
	void HandleCompartmentFloodUpdated(const TArray<FCompartmentRuntimeState>& CompartmentStates);

private:
	struct FDoorCascadeCandidate
	{
		FName DoorId;
		FTransform WorldTransform;
		float HeightDeltaCm;
		FVector FlowDirection;
	};

	void GatherCascadeCandidates(const TArray<FCompartmentRuntimeState>& CompartmentStates, TArray<FDoorCascadeCandidate>& OutCandidates) const;
	UNiagaraComponent* GetOrCreateCascadeComponent(int32 CascadeIndex);
	void ApplyCascadeParameters(UNiagaraComponent* Component, const FDoorCascadeCandidate& Candidate) const;
	void DeactivateUnusedCascades(int32 FirstUnusedIndex);
	void DestroyPooledCascades();

	float GetCompartmentWaterHeightCm(const TArray<FCompartmentRuntimeState>& States, FName CompartmentId) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<USubHullComponent> SubHull = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USubmarineCompartmentComponent> CompartmentComp = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> CascadePool;

	int32 ActiveCascadeCount = 0;
};
