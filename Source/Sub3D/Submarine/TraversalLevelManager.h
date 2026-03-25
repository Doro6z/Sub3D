#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TraversalLevelManager.generated.h"

class UBoxComponent;
class ASubmarineBase;

/**
 * Manages the traversal session in Proto 02.
 * Handles spawn points, start time tracking, and detects when the sub reaches the end.
 */
UCLASS()
class SUB3D_API ATraversalLevelManager : public AActor
{
	GENERATED_BODY()

public:
	ATraversalLevelManager();

protected:
	virtual void BeginPlay() override;

public:
	// ── Setup ──────────────────────────────────────────────────────────

	// The transform where the submarine should spawn at the start
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FTransform SpawnTransformA;

	// The trigger volume at the end of the traversal (Point B)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level")
	UBoxComponent* EndTrigger;

	// ── Session Data ───────────────────────────────────────────────────

	// World time when the traversal started
	UPROPERTY(BlueprintReadOnly, Category = "Level|Session")
	float TraversalStartTime = 0.f;

	// Recorded duration after completion
	UPROPERTY(BlueprintReadOnly, Category = "Level|Session")
	float TraversalDuration = 0.f;

protected:
	// Callback for when the sub enters the end trigger
	UFUNCTION()
	void OnSubReachedEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
