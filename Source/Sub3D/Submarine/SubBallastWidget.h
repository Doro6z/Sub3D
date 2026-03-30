#pragma once

#include "CoreMinimal.h"
#include "SubNavWidget.h"
#include "SubmarineRuntimeTypes.h"
#include "SubBallastWidget.generated.h"

/**
 * Specialized navigation widget for the Ballast Station.
 * Provides access to depth and ballast tank controls.
 */
UCLASS()
class SUB3D_API USubBallastWidget : public USubNavStationWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Submarine|Ballast")
	float GetCurrentDepthMeters() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Ballast")
	float GetTargetDepthMeters() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Ballast")
	bool IsAutoDepthActive() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Ballast")
	float GetGlobalBallastLevel() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Ballast")
	bool IsBallastActive() const;
};
