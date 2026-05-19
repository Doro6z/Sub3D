#include "SubEquipmentComponent.h"
#include "SubItemBase.h"
#include "SubCrewCharacter.h"
#include "Net/UnrealNetwork.h"
#include "SubmarineBase.h"

USubEquipmentComponent::USubEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USubEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USubEquipmentComponent, EquippedItem);
	DOREPLIFETIME(USubEquipmentComponent, CarriedHeavyItem);
}

void USubEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USubEquipmentComponent::OnRep_EquippedItem(ASubItemBase* OldItem)
{
	if (EquippedItem)
	{
		AttachItemToCharacter(EquippedItem, HandSocketName);
	}
	else if (OldItem)
	{
		// Si on a perdu l'item en cours de route (drop coté serveur)
		// On ne gère pas la physique ici car c'est géré par OnRep_ItemState dans l'item
	}
}

void USubEquipmentComponent::OnRep_CarriedHeavyItem(ASubItemBase* OldItem)
{
	if (CarriedHeavyItem)
	{
		AttachItemToCharacter(CarriedHeavyItem, CarrySocketName);
	}
}

void USubEquipmentComponent::Server_PickupItem_Implementation(ASubItemBase* ItemToPick)
{
	if (!ItemToPick || ItemToPick->CurrentState != ESubItemState::Dropped)
		return;

	ASubCrewCharacter* Character = Cast<ASubCrewCharacter>(GetOwner());
	if (!Character)
		return;

	// Si c'est un item lourd
	if (ItemToPick->bIsHeavy)
	{
		// On drop tout ce qu'on a déjà (les mains doivent être libres)
		if (EquippedItem) Server_DropCurrentItem();
		if (CarriedHeavyItem) Server_DropCurrentItem();

		CarriedHeavyItem = ItemToPick;
		CarriedHeavyItem->OnEquipped(Character);
		AttachItemToCharacter(CarriedHeavyItem, CarrySocketName);
	}
	else
	{
		// On ne peut pas équiper si on porte un truc lourd
		if (CarriedHeavyItem)
			return;

		if (EquippedItem) Server_DropCurrentItem();

		EquippedItem = ItemToPick;
		EquippedItem->OnEquipped(Character);
		AttachItemToCharacter(EquippedItem, HandSocketName);
	}
}

void USubEquipmentComponent::Server_DropCurrentItem_Implementation()
{
	ASubCrewCharacter* Character = Cast<ASubCrewCharacter>(GetOwner());
	if (!Character)
		return;

	ASubItemBase* ItemToDrop = nullptr;

	if (CarriedHeavyItem)
	{
		ItemToDrop = CarriedHeavyItem;
		CarriedHeavyItem = nullptr;
	}
	else if (EquippedItem)
	{
		ItemToDrop = EquippedItem;
		EquippedItem = nullptr;
	}

	if (ItemToDrop)
	{
		DetachAndDropItem(ItemToDrop);
	}
}

void USubEquipmentComponent::Server_UsePrimaryAction_Implementation()
{
	ASubCrewCharacter* Character = Cast<ASubCrewCharacter>(GetOwner());
	if (EquippedItem && Character)
	{
		EquippedItem->PrimaryAction(Character);
	}
}

void USubEquipmentComponent::Server_UseSecondaryAction_Implementation()
{
	ASubCrewCharacter* Character = Cast<ASubCrewCharacter>(GetOwner());
	if (EquippedItem && Character)
	{
		EquippedItem->SecondaryAction(Character);
	}
}

void USubEquipmentComponent::AttachItemToCharacter(ASubItemBase* Item, FName SocketName)
{
	ASubCrewCharacter* Character = Cast<ASubCrewCharacter>(GetOwner());
	if (Character && Character->GetMesh())
	{
		Item->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	}
}

void USubEquipmentComponent::DetachAndDropItem(ASubItemBase* Item)
{
	Item->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Item->OnDropped();

	// Si on est dans un sous-marin, on transmet la vélocité
	ASubCrewCharacter* Character = Cast<ASubCrewCharacter>(GetOwner());
	if (Character && Character->CurrentSubmarine && Item->ItemMesh)
	{
		FVector SubVelocity = Character->CurrentSubmarine->GetVelocity();
		Item->ItemMesh->SetPhysicsLinearVelocity(SubVelocity);
	}
}
