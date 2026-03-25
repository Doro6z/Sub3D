#pragma once

#include "CoreMinimal.h"
#include "SubStationBase.h"
#include "SubTurretStation.generated.h"

class ATurretActor;

UCLASS(Blueprintable)
class SUB3D_API ASubTurretStation : public ASubStationBase
{
	GENERATED_BODY()

public:
	ASubTurretStation();

	UFUNCTION(BlueprintCallable, Category = "Station|Turret")
	void SetTurretAim(const FRotator& Aim);

	UFUNCTION(BlueprintCallable, Category = "Station|Turret")
	void SetTurretFireHeld(bool bHeld);

	UFUNCTION(BlueprintPure, Category = "Station|Turret")
	bool HasTurretAuthority() const;

protected:
	ATurretActor* ResolveTurret() const;
};
