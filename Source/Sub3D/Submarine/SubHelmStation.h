#pragma once

#include "CoreMinimal.h"
#include "SubStationBase.h"
#include "SubHelmStation.generated.h"

/**
 * Dedicated helm station class.
 * Use as C++ parent for BP_HelmStation.
 */
UCLASS(Blueprintable)
class SUB3D_API ASubHelmStation : public ASubStationBase
{
	GENERATED_BODY()

public:
	ASubHelmStation();

	virtual bool CanEnterStation_Implementation(AController* Controller) const override;
};
