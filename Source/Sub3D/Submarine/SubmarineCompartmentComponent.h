#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineRuntimeTypes.h"
#include "SubmarineCompartmentComponent.generated.h"

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubmarineCompartmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubmarineCompartmentComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Submarine|Compartments")
	void RegisterDoor(const FDoorState& DoorState);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Compartments")
	void SetDoorClosed(FName DoorId, bool bClosed);

	UFUNCTION(BlueprintPure, Category = "Submarine|Compartments")
	bool IsDoorClosed(FName DoorId) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Compartments")
	bool TryGetDoorState(FName DoorId, FDoorState& OutDoorState) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Compartments")
	float GetCompartmentFlood01(FName CompartmentId) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Compartments")
	float GetTotalWaterMassLiters() const { return TotalWaterMassLiters; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Compartments")
	float GetTotalWaterMassKg() const { return TotalWaterMassLiters; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Compartments")
	const TArray<FCompartmentState>& GetCompartments() const { return Compartments; }

	UFUNCTION(BlueprintPure, Category = "Submarine|Compartments")
	const TArray<FDoorState>& GetDoors() const { return Doors; }

private:
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Submarine|Compartments", meta = (AllowPrivateAccess = "true"))
	TArray<FCompartmentState> Compartments;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Submarine|Compartments", meta = (AllowPrivateAccess = "true"))
	TArray<FDoorState> Doors;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Submarine|Compartments", meta = (AllowPrivateAccess = "true"))
	float TotalWaterMassLiters = 0.f;
};
