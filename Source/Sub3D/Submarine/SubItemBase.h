#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubItemBase.generated.h"

class UStaticMeshComponent;
class UInteractableComponent;
class ASubCrewCharacter;

UENUM(BlueprintType)
enum class ESubItemState : uint8
{
	Dropped    UMETA(DisplayName = "Dropped"),
	Equipped   UMETA(DisplayName = "Equipped"),
	Stored     UMETA(DisplayName = "Stored")
};

/**
 * Base class for all physical items in the submarine.
 * Can be picked up, dropped, and used.
 */
UCLASS(Abstract)
class SUB3D_API ASubItemBase : public AActor
{
	GENERATED_BODY()

public:
	ASubItemBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* ItemMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInteractableComponent* InteractableComponent;

	/** Unique ID for UI/Database if needed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemID;

	/** If true, requires two hands to carry (e.g. crate) and disables other actions */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bIsHeavy = false;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ItemState, Category = "Item|State")
	ESubItemState CurrentState = ESubItemState::Dropped;

	UFUNCTION()
	virtual void OnRep_ItemState();

	// ── Gameplay Actions ──────────────────────────────────────────────────

	/** Called when picked up and attached to the player */
	virtual void OnEquipped(ASubCrewCharacter* NewOwner);

	/** Called when dropped in the world */
	virtual void OnDropped();

	/** Primary Use (Left Click) */
	UFUNCTION(BlueprintCallable, Category = "Item|Action")
	virtual void PrimaryAction(ASubCrewCharacter* User);

	/** Secondary Use (Right Click) */
	UFUNCTION(BlueprintCallable, Category = "Item|Action")
	virtual void SecondaryAction(ASubCrewCharacter* User);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void HandleInteract(ASubCrewCharacter* Interactor);
};
