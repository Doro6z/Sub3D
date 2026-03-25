#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SubCrewMovementComponent.generated.h"

class UPrimitiveComponent;
class USubInteriorFrameComponent;

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

	void InitializeForSubmarine();

	bool IsEmbarked() const;

protected:
	virtual void UpdateBasedMovement(float DeltaSeconds) override;
	virtual void UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation) override;

private:
	USubInteriorFrameComponent* GetInteriorFrame() const;
	void UpdateRelativeState();
	void ApplyYawCompensation();
	void CheckAndLogBaseChange();
	void DebugDrawState();
	void LogPeriodicState(float DeltaTime);

	TWeakObjectPtr<UPrimitiveComponent> LastKnownBase;
	float DebugLogTimer = 0.f;
};
