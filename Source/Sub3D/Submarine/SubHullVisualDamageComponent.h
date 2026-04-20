#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructuralHullTypes.h"
#include "SubHullVisualDamageComponent.generated.h"

class UMaterialInstanceDynamic;
class UMeshComponent;
class USubHullComponent;

USTRUCT(BlueprintType)
struct FSubHullBreachVisualState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Breach|Visual")
	FName SheetId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Breach|Visual")
	FName CompartmentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Breach|Visual")
	int32 MaterialSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Breach|Visual")
	FVector LocalCenter = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Breach|Visual")
	FVector2D SheetSpaceCenter01 = FVector2D(0.5f, 0.5f);

	UPROPERTY(BlueprintReadOnly, Category = "Breach|Visual")
	float VisibleRadiusCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Breach|Visual")
	float TargetVisibleRadiusCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Breach|Visual")
	float VisibleRadius01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Breach|Visual")
	bool bTouchesExterior = false;
};

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubHullVisualDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubHullVisualDamageComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Breach|Visual")
	void RefreshFromCurrentBreaches();

	UFUNCTION(BlueprintPure, Category = "Breach|Visual")
	const TArray<FSubHullBreachVisualState>& GetActiveBreachVisuals() const { return ActiveBreachVisuals; }

	UFUNCTION(BlueprintPure, Category = "Breach|Visual")
	UMeshComponent* GetBoundVisualHullComponent() const { return VisualHullComponent; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|Visual")
	bool bAutoCreateDynamicMaterials = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|Visual")
	bool bApplyMaterialParameters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|Visual", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxTrackedBreaches = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|Visual|Parameters")
	FName BreachCountParameter = TEXT("BreachCount");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|Visual|Parameters")
	FName BreachCenterParameterPrefix = TEXT("BreachCenter");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breach|Visual|Parameters")
	FName BreachRadiusParameterPrefix = TEXT("BreachRadius");

protected:
	UFUNCTION()
	void HandleBreachesUpdated(const TArray<FBreachClusterState>& Breaches);

private:
	bool ResolveSubHullComponent();
	bool ResolveVisualHullComponent();
	void InitializeDynamicMaterials();
	void ApplyMaterialParameters();
	bool BuildVisualState(const FBreachClusterState& Cluster, FSubHullBreachVisualState& OutState) const;
	const FStructuralSheetDef* FindSheetDef(FName SheetId) const;
	const FStructuralSheetCompiledBinding* FindCompiledBinding(FName SheetId) const;
	FName MakeIndexedParameterName(FName Prefix, int32 Index) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<USubHullComponent> SubHull = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> VisualHullComponent = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;

	UPROPERTY(Transient)
	TArray<FSubHullBreachVisualState> ActiveBreachVisuals;
};
