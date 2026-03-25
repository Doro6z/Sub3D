#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SubmarineTypes.h"
#include "SubStationInterface.generated.h"

class AController;
class ASubmarineBase;

UINTERFACE(MinimalAPI)
class USubStationInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for interactive submarine stations (helm, pump, etc.)
 */
class SUB3D_API ISubStationInterface
{
	GENERATED_BODY()

public:
	// Whether this station can currently be entered by this controller
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Station")
	bool CanEnterStation(AController* Controller) const;

	// Request station occupation by a controller
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Station")
	void RequestEnterStation(AController* Controller);

	// Request station exit by a controller
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Station")
	void RequestExitStation(AController* Controller);

	// Get the station type for input/UI routing
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Station")
	ESubStationType GetStationType() const;

	// Get the current occupant, if any
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Station")
	AController* GetCurrentOccupant() const;

	// Get the submarine that owns this station
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Station")
	ASubmarineBase* GetOwningSubmarine() const;
};
