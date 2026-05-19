#include "SubItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "InteractableComponent.h"
#include "SubCrewCharacter.h"
#include "Net/UnrealNetwork.h"

ASubItemBase::ASubItemBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	RootComponent = ItemMesh;
	ItemMesh->SetSimulatePhysics(true);
	ItemMesh->SetCollisionProfileName(TEXT("PhysicsActor"));

	InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));
	// Action text will be overridden in blueprints usually
	InteractableComponent->InteractionActionText = FText::FromString(TEXT("Ramasser"));
}

void ASubItemBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASubItemBase, CurrentState);
}

void ASubItemBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		InteractableComponent->OnInteract.AddDynamic(this, &ASubItemBase::HandleInteract);
	}
}

void ASubItemBase::OnRep_ItemState()
{
	// Client side visual updates based on state
	if (CurrentState == ESubItemState::Equipped || CurrentState == ESubItemState::Stored)
	{
		if (ItemMesh)
		{
			ItemMesh->SetSimulatePhysics(false);
			ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		if (InteractableComponent)
		{
			InteractableComponent->bIsInteractable = false;
		}
	}
	else if (CurrentState == ESubItemState::Dropped)
	{
		if (ItemMesh)
		{
			ItemMesh->SetSimulatePhysics(true);
			ItemMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		}
		if (InteractableComponent)
		{
			InteractableComponent->bIsInteractable = true;
		}
	}
}

void ASubItemBase::OnEquipped(ASubCrewCharacter* NewOwner)
{
	SetOwner(NewOwner);
	CurrentState = ESubItemState::Equipped;

	if (HasAuthority())
	{
		OnRep_ItemState(); // Force server update directly
	}
}

void ASubItemBase::OnDropped()
{
	SetOwner(nullptr);
	CurrentState = ESubItemState::Dropped;

	if (HasAuthority())
	{
		OnRep_ItemState(); // Force server update directly
	}
}

void ASubItemBase::PrimaryAction(ASubCrewCharacter* User)
{
	// Virtual to be overridden
}

void ASubItemBase::SecondaryAction(ASubCrewCharacter* User)
{
	// Virtual to be overridden
}

void ASubItemBase::HandleInteract(ASubCrewCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
		return;

	// This is the bridge. The Crew character needs an equipment component or similar to actually pick it up.
	// For now, we will expect the EquipmentComponent to call OnEquipped on this item.
	// We will implement USubEquipmentComponent next.
}
