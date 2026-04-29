#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LadderClimbComponent.generated.h"

class ASubCrewCharacter;
class ASubmarineBase;

/**
 * Drop on a ladder Actor (child of the submarine) to define a climbable segment.
 *
 * Authoring contract:
 *  - Ladder Actor is attached to the submarine root (so the ladder rides the sub).
 *  - ClimbStartLocal / ClimbEndLocal are sub-LOCAL coordinates of the bottom and top
 *    of the climbable line.
 *  - Two UInteractableComponent on the same Actor (or a peer Actor) wire their
 *    OnInteract delegates to TryEnterClimbFromBottom / TryEnterClimbFromTop in the
 *    ladder BP graph.
 *
 * Runtime contract:
 *  - TryEnterClimb*: called from server (via Crew interaction RPC chain). Validates
 *    occupancy + crew embarked state, then calls USubCrewMovementComponent::BeginLadderClimb.
 *  - One occupant at a time. Re-entry while occupied is rejected with a Log warning.
 *  - When the occupant exits (top/bottom/cancel), the ladder clears its slot.
 */
UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API ULadderClimbComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULadderClimbComponent();

	/** Sub-local position of the ladder's bottom (climb start). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	FVector ClimbStartLocal = FVector(0.f, 0.f, 0.f);

	/** Sub-local position of the ladder's top (climb end). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	FVector ClimbEndLocal = FVector(0.f, 0.f, 200.f);

	/** Sub-local yaw the crew faces during climb (toward the ladder). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float ClimbFacingYawLocalDeg = 180.f;

	/** Climb traversal speed expressed as fraction of full ladder per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "0.05"))
	float ClimbSpeedPerSec = 0.5f;

	/** Step-off offset along ClimbDirection (toward the deck) when exiting at either end. cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (ClampMin = "0.0"))
	float StepOffOffsetCm = 50.f;

	/** Server entry from the bottom. Wire the bottom UInteractableComponent::OnInteract here. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void TryEnterClimbFromBottom(ASubCrewCharacter* Interactor);

	/** Server entry from the top. Wire the top UInteractableComponent::OnInteract here. */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void TryEnterClimbFromTop(ASubCrewCharacter* Interactor);

	/** Called by the crew movement when the climb finishes (top, bottom, or cancel). */
	void NotifyClimbFinished(ASubCrewCharacter* Crew);

	UFUNCTION(BlueprintPure, Category = "Ladder")
	FVector GetClimbDirectionLocal() const;

	UFUNCTION(BlueprintPure, Category = "Ladder")
	float GetClimbLengthCm() const;

	UFUNCTION(BlueprintPure, Category = "Ladder")
	bool IsOccupied() const { return CurrentClimber.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Ladder")
	ASubmarineBase* GetOwningSubmarine() const;

	/** World-space pose for the crew at a given climb progress [0..1]. Reads sub Owner.Transform. */
	FTransform ComputeClimbWorldPose(float Progress01) const;

	/** Sub-local pose at progress, used by the rebase to compute the GridSpaceTransform target. */
	FTransform ComputeClimbLocalPose(float Progress01) const;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ASubCrewCharacter> CurrentClimber;

	bool ValidateEntry(ASubCrewCharacter* Interactor, bool bFromBottom) const;
	void StartClimbInternal(ASubCrewCharacter* Crew, bool bFromBottom);
};
