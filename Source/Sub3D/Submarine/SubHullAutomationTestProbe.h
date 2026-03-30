#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StructuralHullTypes.h"
#include "SubHullAutomationTestProbe.generated.h"

UCLASS()
class SUB3D_API USubHullAutomationTestProbe : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 HullDamageUpdatedCount = 0;

	UPROPERTY()
	int32 BreachesUpdatedCount = 0;

	UPROPERTY()
	int32 FlowFieldsUpdatedCount = 0;

	UPROPERTY()
	int32 CompartmentFloodUpdatedCount = 0;

	UPROPERTY()
	int32 LastBreachesCount = 0;

	UPROPERTY()
	int32 LastFlowFieldsCount = 0;

	UPROPERTY()
	int32 LastCompartmentStatesCount = 0;

	UFUNCTION()
	void HandleHullDamageUpdated()
	{
		++HullDamageUpdatedCount;
	}

	UFUNCTION()
	void HandleBreachesUpdated(const TArray<FBreachClusterState>& Breaches)
	{
		++BreachesUpdatedCount;
		LastBreachesCount = Breaches.Num();
	}

	UFUNCTION()
	void HandleFlowFieldsUpdated(const TArray<FBreachFlowField>& InFlowFields)
	{
		++FlowFieldsUpdatedCount;
		LastFlowFieldsCount = InFlowFields.Num();
	}

	UFUNCTION()
	void HandleCompartmentFloodUpdated(const TArray<FCompartmentRuntimeState>& InCompartmentStates)
	{
		++CompartmentFloodUpdatedCount;
		LastCompartmentStatesCount = InCompartmentStates.Num();
	}

	void ResetCounters()
	{
		HullDamageUpdatedCount = 0;
		BreachesUpdatedCount = 0;
		FlowFieldsUpdatedCount = 0;
		CompartmentFloodUpdatedCount = 0;
		LastBreachesCount = 0;
		LastFlowFieldsCount = 0;
		LastCompartmentStatesCount = 0;
	}
};
