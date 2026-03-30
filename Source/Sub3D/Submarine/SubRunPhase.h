#pragma once

#include "CoreMinimal.h"
#include "SubRunPhase.generated.h"

UENUM(BlueprintType)
enum class ESubRunPhase : uint8
{
	Boot UMETA(DisplayName = "Boot"),
	Boarding UMETA(DisplayName = "Boarding"),
	Departure UMETA(DisplayName = "Departure"),
	Traverse UMETA(DisplayName = "Traverse"),
	BreachCrisis UMETA(DisplayName = "Breach Crisis"),
	Approach UMETA(DisplayName = "Approach"),
	Docking UMETA(DisplayName = "Docking"),
	Success UMETA(DisplayName = "Success"),
	Failure UMETA(DisplayName = "Failure")
};
