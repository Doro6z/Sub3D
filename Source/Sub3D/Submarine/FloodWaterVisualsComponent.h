#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructuralHullTypes.h"
#include "FloodWaterVisualsComponent.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class USubHullComponent;

USTRUCT()
struct FFloodWaterPlaneState
{
	GENERATED_BODY()

	UPROPERTY()
	FName CompartmentId = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PlaneComponent = nullptr;

	UPROPERTY()
	FVector LocalCenter = FVector::ZeroVector;

	UPROPERTY()
	FVector2D LocalSizeCm = FVector2D(100.f, 100.f);

	UPROPERTY()
	float LocalMinZ = 0.f;

	UPROPERTY()
	float LocalMaxZ = 0.f;

	UPROPERTY()
	float CurrentLocalZ = 0.f;

	UPROPERTY()
	float TargetLocalZ = 0.f;

	UPROPERTY()
	bool bTargetVisible = false;
};

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API UFloodWaterVisualsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFloodWaterVisualsComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Flood|Visuals")
	int32 GetWaterPlaneCount() const { return WaterPlanes.Num(); }

	UFUNCTION(BlueprintPure, Category = "Flood|Visuals")
	int32 GetVisibleWaterPlaneCount() const;

	UFUNCTION(BlueprintPure, Category = "Flood|Visuals")
	float ComputeSurfaceLocalZ(const FBox& LocalBounds, float WaterLevelNormalized) const;

	UFUNCTION(BlueprintCallable, Category = "Flood|Visuals")
	void RefreshFromCurrentFloodState();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Visuals")
	TObjectPtr<UStaticMesh> WaterPlaneMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Visuals")
	TObjectPtr<UMaterialInterface> WaterMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Visuals", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WaterVisibleThreshold = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood|Visuals", meta = (ClampMin = "0.0"))
	float InterpolationSpeed = 3.f;

protected:
	UFUNCTION()
	void HandleCompartmentFloodUpdated(const TArray<FCompartmentRuntimeState>& InCompartmentStates);

private:
	void InitializeWaterPlanesFromHull();
	bool BuildCompartmentBounds(FName CompartmentId, FBox& OutLocalBounds) const;
	static void AppendSheetBounds(FBox& InOutBounds, const FStructuralSheetDef& Sheet);
	UStaticMeshComponent* CreatePlaneComponent(int32 PlaneIndex, const FFloodWaterPlaneState& PlaneState);
	void UpdatePlaneVisual(FFloodWaterPlaneState& PlaneState) const;
	void UpdateTickEnabled();
	void DestroyWaterPlanes();

private:
	UPROPERTY(Transient)
	TObjectPtr<USubHullComponent> SubHull = nullptr;

	UPROPERTY(Transient)
	TArray<FFloodWaterPlaneState> WaterPlanes;
};
