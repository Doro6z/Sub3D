#pragma once

#include "CoreMinimal.h"
#include "SubNavWidget.h"
#include "SubEngineWidget.generated.h"

/**
 * Specialized navigation widget for the Engine Station.
 * Controls boost, pumps, and displays engine/electrical health.
 */
UCLASS()
class SUB3D_API USubEngineWidget : public USubNavStationWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Submarine|Engine")
	void SetEngineBoost(float Value);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Engine")
	void SetPumpActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Engine")
	void SetPumpPower(float Value);

	UFUNCTION(BlueprintPure, Category = "Submarine|Engine")
	float GetEngineHealth() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Engine")
	float GetElectricalHealth() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Engine")
	bool IsPumpActive() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Engine")
	float GetPumpPower() const;
};
