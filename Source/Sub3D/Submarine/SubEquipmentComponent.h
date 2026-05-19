#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubEquipmentComponent.generated.h"

class ASubItemBase;
class ASubCrewCharacter;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SUB3D_API USubEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubEquipmentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Single hand item (tools, flares, etc) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedItem, Category = "Equipment")
	ASubItemBase* EquippedItem;

	/** Two handed heavy item (crates) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CarriedHeavyItem, Category = "Equipment")
	ASubItemBase* CarriedHeavyItem;

	UFUNCTION()
	void OnRep_EquippedItem(ASubItemBase* OldItem);

	UFUNCTION()
	void OnRep_CarriedHeavyItem(ASubItemBase* OldItem);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Equipment")
	void Server_PickupItem(ASubItemBase* ItemToPick);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Equipment")
	void Server_DropCurrentItem();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Equipment")
	void Server_UsePrimaryAction();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Equipment")
	void Server_UseSecondaryAction();

	/** Socket names to attach items to. Set in Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName HandSocketName = TEXT("Hand_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName CarrySocketName = TEXT("Carry_Socket");

protected:
	virtual void BeginPlay() override;

private:
	void AttachItemToCharacter(ASubItemBase* Item, FName SocketName);
	void DetachAndDropItem(ASubItemBase* Item);
};
