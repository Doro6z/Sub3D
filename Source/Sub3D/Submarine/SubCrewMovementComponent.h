#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SubCrewMovementComponent.generated.h"

class USubInteriorFrameComponent;

/**
 * Crew movement component for interior submarine traversal.
 * Extends UCharacterMovementComponent with:
 *   - Explicit frame compensation (character follows the moving submarine)
 *   - Crew relative state (position/rotation in submarine-local space)
 *   - Snap recovery when the submarine teleports (network correction)
 *   - Controller yaw compensation for submarine rotation
 *
 * Stock CMC moving-base is disabled when embarked; our compensation is
 * the primary mechanism. CMC floor detection is still used for walking.
 */
UCLASS()
class SUB3D_API USubCrewMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	USubCrewMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ── Relative crew state ──────────────────────────────────────────────

	/** Crew position in submarine-local space (updated every frame when embarked). */
	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew")
	FVector RelativeLocation = FVector::ZeroVector;

	/** Crew rotation in submarine-local space. */
	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Crew")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	// ── Tuning ───────────────────────────────────────────────────────────

	/** If the submarine moves more than this in one frame, treat it as a teleport/snap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Crew")
	float SnapThresholdCm = 200.f;

	// ── Init ─────────────────────────────────────────────────────────────

	/** Call after placing the character inside the submarine to seed the relative state. */
	void InitializeForSubmarine();

	bool IsEmbarked() const;

protected:
	virtual void UpdateBasedMovement(float DeltaSeconds) override;
	virtual void UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation) override;

private:
	USubInteriorFrameComponent* GetInteriorFrame() const;
	void ApplySubmarineFrameCompensation();
	void UpdateRelativeState();

	FTransform LastCompensatedSubTransform;
	bool bHasLastCompensatedTransform = false;
};
