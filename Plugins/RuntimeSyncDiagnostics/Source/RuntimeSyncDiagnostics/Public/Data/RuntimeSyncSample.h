#pragma once

#include "CoreMinimal.h"
#include "RuntimeSyncSample.generated.h"

/**
 * Pure data representing a single diagnostic sample in time.
 */
USTRUCT(BlueprintType)
struct RUNTIMESYNCDIAGNOSTICS_API FRuntimeSyncSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	float Timestamp = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	int32 FrameNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FTransform ExpectedTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FTransform ActualTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	float ErrorDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	bool bHasExpectedTransform = false;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FString RoleContext;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FString ActorName;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FString TickGroup;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FString MovementBaseName;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FString FloorStateContext;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FTransform MovementBaseTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	float MovementBaseDeltaCm = 0.f;
};
