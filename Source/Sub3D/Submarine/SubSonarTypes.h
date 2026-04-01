#pragma once

#include "CoreMinimal.h"
#include "SubSonarTypes.generated.h"

/**
 * A single hit point returned by a sonar ping.
 * Distance is stored separately from world location so the display widget
 * can compute the propagation reveal delay without knowing the sub's position at ping time.
 */
USTRUCT(BlueprintType)
struct FSonarHitPoint
{
	GENERATED_BODY()

	// World position of the terrain/geometry hit
	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	FVector_NetQuantize100 WorldLocation = FVector::ZeroVector;

	// Distance from sub origin at time of ping (cm).
	// Used by the display: RevealDelay = DistanceCm / PropagationSpeedCmS
	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float DistanceCm = 0.f;

	// World time (GetTimeSeconds) when the ping was fired.
	// Used to compute age: Age = CurrentTime - (PingTimestamp + DistanceCm / PropagationSpeed)
	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float PingTimestamp = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float PreviousDistanceCm = -1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	float PreviousPingTimestamp = -1.f;

	// Surface normal at the hit point. Reserved for future intensity shading — not used by V1 display.
	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	FVector_NetQuantize Normal = FVector::UpVector;

	// Hit position expressed in submarine local space at ping time.
	// Used by the display to keep the tunnel guide stable instead of reprojecting with current sub transform.
	UPROPERTY(BlueprintReadOnly, Category = "Sonar")
	FVector_NetQuantize100 LocalLocationAtPing = FVector::ZeroVector;
};
