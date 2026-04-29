#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ladder.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UInteractableComponent;
class ULadderClimbComponent;
class ASubCrewCharacter;

/**
 * Self-configured ladder actor. Drop in the submarine hierarchy, tune
 * `ClimbComponent->ClimbEndLocal` to set the ladder height, place a mesh, ship.
 *
 * Wires its two interactables to the climb component's TryEnter handlers in
 * BeginPlay so no Blueprint graph is required.
 */
UCLASS()
class SUB3D_API ALadder : public AActor
{
	GENERATED_BODY()

public:
	ALadder();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<UStaticMeshComponent> LadderMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<ULadderClimbComponent> ClimbComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<UInteractableComponent> BottomInteractable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	TObjectPtr<UInteractableComponent> TopInteractable;

private:
	UFUNCTION()
	void HandleBottomInteract(ASubCrewCharacter* Interactor);

	UFUNCTION()
	void HandleTopInteract(ASubCrewCharacter* Interactor);
};
