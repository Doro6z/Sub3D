#pragma once

#include "CoreMinimal.h"
#include "SubStationBase.h"
#include "SubEngineStation.generated.h"

/**
 * Engine room station. Provides explicit API for propulsion inputs.
 */
UCLASS(Blueprintable)
class SUB3D_API ASubEngineStation : public ASubStationBase
{
	GENERATED_BODY()

public:
	ASubEngineStation();

	// Authority-only helper. Intended to be called from server-routed controller code.
	UFUNCTION(BlueprintCallable, Category = "Station|Engine")
	void SetEngineInputs(float ThrustInput, float RudderInput, float DiveInput);

	UFUNCTION(BlueprintCallable, Category = "Station|Engine")
	void SetPumpActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Station|Engine")
	bool HasEngineAuthority() const;
};
