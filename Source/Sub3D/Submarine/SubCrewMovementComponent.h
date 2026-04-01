#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SubCrewMovementComponent.generated.h"

class UPrimitiveComponent;
class USubInteriorFrameComponent;
class ASubmarineBase;

/**
 * Crew movement component for interior submarine traversal.
 * Keeps relative state while relying on stock CMC based movement when embarked.
 */
UCLASS()
class SUB3D_API USubCrewMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	USubCrewMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew")
	float SnapThresholdCm = 200.f;

	UPROPERTY(EditAnywhere, Category = "Submarine|Debug")
	bool bDebugLogCrewMovement = false;

	UPROPERTY(EditAnywhere, Category = "Submarine|Debug")
	bool bDebugDrawCrewMovement = false;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Support")
	bool bHasValidEmbarkedFloor = false;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Support")
	bool bHasAcceptedEmbarkedBase = false;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Support")
	bool bNeedsEmbarkedFloorRecovery = false;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Support")
	float SupportQuality01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Support")
	TObjectPtr<UPrimitiveComponent> LastEmbarkedFloorComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector LocalSubLinearVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector LocalSubLinearAcceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector LocalSubAngularVelocityDegrees = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector LocalSubAngularAccelerationDegrees = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	bool bHasNearbyBraceSupport = false;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	float NearbyBraceDistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector NearbyBraceWorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector NearbyBraceWorldNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew|Embodiment")
	FVector BraceQueryOrigin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Support", meta = (ClampMin = "0.01"))
	float FloorRecoveryIntervalSeconds = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Embodiment", meta = (ClampMin = "1.0"))
	float BraceProbeDistanceCm = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew|Embodiment", meta = (ClampMin = "0.0"))
	float BraceProbeHeightOffsetCm = 70.f;

	void InitializeForSubmarine();
	void RefreshEmbarkedFlooring();

	bool IsEmbarked() const;

protected:
	virtual void UpdateBasedMovement(float DeltaSeconds) override;
	virtual void UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation) override;

private:
	ASubmarineBase* GetCurrentSubmarine() const;
	USubInteriorFrameComponent* GetInteriorFrame() const;
	bool IsAcceptedEmbarkedBase(const UPrimitiveComponent* CandidateBase) const;
	void UpdateInertialState();
	void UpdateRelativeState();
	void UpdateSupportState();
	void AttemptEmbarkedFloorRecovery(float DeltaTime);
	void UpdateBraceState();
	bool QueryBraceSupportHit(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	void ApplyYawCompensation();
	void CheckAndLogBaseChange();
	void DebugDrawState();
	void LogPeriodicState(float DeltaTime);

	TWeakObjectPtr<UPrimitiveComponent> LastKnownBase;
	float FloorRecoveryTimer = 0.f;
	float DebugLogTimer = 0.f;
};
