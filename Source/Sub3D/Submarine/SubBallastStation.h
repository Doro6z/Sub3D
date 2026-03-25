#pragma once

#include "CoreMinimal.h"
#include "SubStationBase.h"
#include "SubBallastStation.generated.h"

/**
 * Ballast/pump station.
 * Encapsulates ballast controls for station-specific UI and interactions.
 */
UCLASS(Blueprintable)
class SUB3D_API ASubBallastStation : public ASubStationBase
{
	GENERATED_BODY()

public:
	ASubBallastStation();

	// Authority-only helper. Intended to be called from server-routed controller code.
	UFUNCTION(BlueprintCallable, Category = "Station|Ballast")
	void SetGlobalBallastTarget(float GlobalTarget);

	UFUNCTION(BlueprintCallable, Category = "Station|Ballast")
	void SetBallastTargetByIndex(int32 Index, float Target);

	UFUNCTION(BlueprintCallable, Category = "Station|Ballast")
	void ResyncBallasts();

	UFUNCTION(BlueprintPure, Category = "Station|Ballast")
	int32 GetBallastCount() const;

	UFUNCTION(BlueprintPure, Category = "Station|Ballast")
	float GetBallastFillLevel(int32 Index) const;
};
