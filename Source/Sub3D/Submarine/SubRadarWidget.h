#pragma once

#include "CoreMinimal.h"
#include "SubNavWidget.h"
#include "SubmarineRuntimeTypes.h"
#include "SubRadarWidget.generated.h"

/**
 * Specialized navigation widget for the Radar/Sonar Station.
 * Displays external pips and scan progress.
 */
UCLASS()
class SUB3D_API USubRadarWidget : public USubNavStationWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Submarine|Radar")
	TArray<FRadarContact> GetRadarContacts() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Radar")
	float GetPingProgress() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Radar")
	float GetRadarRange() const;
};
