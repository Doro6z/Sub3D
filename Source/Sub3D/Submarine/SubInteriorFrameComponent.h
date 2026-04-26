#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubInteriorFrameComponent.generated.h"

/**
 * Interior reference frame for the submarine.
 * Provides world <-> sub-local conversions and per-frame transform delta.
 */
UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubInteriorFrameComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubInteriorFrameComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Submarine|InteriorFrame")
	FVector WorldToLocal(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|InteriorFrame")
	FVector LocalToWorld(const FVector& LocalPosition) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|InteriorFrame")
	FRotator WorldToLocalRotation(const FRotator& WorldRotation) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|InteriorFrame")
	FRotator LocalToWorldRotation(const FRotator& LocalRotation) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|InteriorFrame")
	FVector GetFrameLocationDelta() const { return FrameLocationDelta; }

	UFUNCTION(BlueprintPure, Category = "Submarine|InteriorFrame")
	FRotator GetFrameRotationDelta() const { return FrameRotationDelta; }

	UFUNCTION(BlueprintPure, Category = "Submarine|InteriorFrame")
	FVector GetLocalLinearVelocity() const { return LocalLinearVelocity; }

	UFUNCTION(BlueprintPure, Category = "Submarine|InteriorFrame")
	FVector GetLocalLinearAcceleration() const { return LocalLinearAcceleration; }

	UFUNCTION(BlueprintPure, Category = "Submarine|InteriorFrame")
	FVector GetLocalAngularVelocityDegrees() const { return LocalAngularVelocityDegrees; }

	UFUNCTION(BlueprintPure, Category = "Submarine|InteriorFrame")
	FVector GetLocalAngularAccelerationDegrees() const { return LocalAngularAccelerationDegrees; }

	UFUNCTION(BlueprintPure, Category = "Submarine|InteriorFrame")
	FTransform GetSubTransform() const;

	bool IsFrameValid() const { return bFrameValid; }

private:
	FVector PreviousLocation;
	FRotator PreviousRotation;
	FVector FrameLocationDelta;
	FRotator FrameRotationDelta;
	FVector PreviousLocalLinearVelocity = FVector::ZeroVector;
	FVector PreviousLocalAngularVelocityDegrees = FVector::ZeroVector;
	FVector LocalLinearVelocity = FVector::ZeroVector;
	FVector LocalLinearAcceleration = FVector::ZeroVector;
	FVector LocalAngularVelocityDegrees = FVector::ZeroVector;
	FVector LocalAngularAccelerationDegrees = FVector::ZeroVector;
	float DebugLogTimer = 0.f;
	bool bFrameValid = false;

	/**
	 * Last sim frame counter observed from USubMovementComponent. Acceleration signals
	 * are recomputed only when this counter advances (i.e. a new sim step fired), using
	 * the sim dt as divisor. Between sim steps, acceleration is held constant.
	 * Prevents the 60Hz spike → 0 → spike pattern when rendering at render rate (e.g.
	 * 144fps) on top of a 60Hz fixed-tick sim, which would otherwise poison Camera Sway
	 * and any consumer that reads LocalLinearAcceleration / LocalAngularAccelerationDegrees.
	 */
	int32 LastSeenSubSimFrame = -1;
};
