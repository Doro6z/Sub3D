#pragma once

#include "CoreMinimal.h"
#include "SubNavWidget.h"
#include "SubTurretWidget.generated.h"

/**
 * Specialized navigation widget for the Turret Station.
 * Handles aiming and firing commands.
 */
UCLASS()
class SUB3D_API USubTurretWidget : public USubNavStationWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Submarine|Turret")
	void SetTurretAim(const FRotator& NewAim);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Turret")
	void SetTurretFireHeld(bool bHeld);

	UFUNCTION(BlueprintPure, Category = "Submarine|Turret")
	FRotator GetCurrentTurretAim() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Turret")
	bool IsTurretOnline() const;
};
