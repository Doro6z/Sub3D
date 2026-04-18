#include "SubDoorActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Generator/SubmarineDefinitionTypes.h"
#include "InteractableComponent.h"
#include "Net/UnrealNetwork.h"
#include "SubCrewCharacter.h"
#include "SubFloodComponent.h"
#include "SubmarineBase.h"
#include "SubmarineCompartmentComponent.h"
#include "SubmarineLayoutAsset.h"

ASubDoorActor::ASubDoorActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Root);
	DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));
	DoorMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	DoorMesh->SetCanEverAffectNavigation(false);

	DoorCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorCollision"));
	DoorCollision->SetupAttachment(Root);
	DoorCollision->SetCollisionProfileName(TEXT("BlockAll"));
	DoorCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	DoorCollision->SetBoxExtent(FVector(5.f, 45.f, 90.f));
	DoorCollision->SetCanEverAffectNavigation(false);

	Interactable = CreateDefaultSubobject<UInteractableComponent>(TEXT("Interactable"));
}

void ASubDoorActor::BeginPlay()
{
	Super::BeginPlay();

	ApplySubmarineCollisionIgnoreToAllPrimitiveComponents();

	bClosed = bStartsClosed;
	TryResolveOwningSubmarine();
	RegisterWithCompartments();
	ApplyDoorState();

	if (Interactable)
	{
		Interactable->OnInteract.AddDynamic(this, &ASubDoorActor::HandleInteract);
	}
}

void ASubDoorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASubDoorActor, bClosed);
	DOREPLIFETIME(ASubDoorActor, OwningSubmarine);
}

void ASubDoorActor::SetDoorClosed(bool bNewClosed)
{
	if (!HasAuthority() || bLocked)
	{
		return;
	}

	bClosed = bNewClosed;
	ApplyDoorState();
	RegisterWithCompartments();
}

void ASubDoorActor::ToggleDoor()
{
	SetDoorClosed(!bClosed);
}

void ASubDoorActor::InitializeFromDoorDef(const FDoorDef& DoorDef, FName InCompartmentA, FName InCompartmentB, ASubmarineBase* InOwningSubmarine)
{
	DoorId = DoorDef.DoorId;
	CompartmentA = InCompartmentA;
	CompartmentB = InCompartmentB;
	OwningSubmarine = InOwningSubmarine;
}

void ASubDoorActor::InitializeFromConnectionDef(const FGeneratedConnectionDef& Connection, ASubmarineBase* InOwningSubmarine)
{
	DoorId = Connection.ConnectionId;
	CompartmentA = Connection.CompartmentA;
	CompartmentB = Connection.CompartmentB;
	bStartsClosed = Connection.bStartsClosed;
	OwningSubmarine = InOwningSubmarine;

	if (DoorCollision)
	{
		const float HalfWidth = Connection.DoorWidthCm * 0.5f;
		const float HalfHeight = Connection.DoorHeightCm * 0.5f;
		DoorCollision->SetBoxExtent(FVector(5.f, HalfWidth, HalfHeight));
		DoorCollision->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight));
	}
}

void ASubDoorActor::HandleInteract(ASubCrewCharacter* Interactor)
{
	if (!HasAuthority() || !Interactor || bLocked)
	{
		return;
	}

	ToggleDoor();
}

void ASubDoorActor::OnRep_DoorClosed()
{
	ApplyDoorState();
}

void ASubDoorActor::TryResolveOwningSubmarine()
{
	if (OwningSubmarine)
	{
		return;
	}

	for (AActor* Cursor = GetOwner(); Cursor != nullptr; Cursor = Cursor->GetOwner())
	{
		if (ASubmarineBase* Sub = Cast<ASubmarineBase>(Cursor))
		{
			OwningSubmarine = Sub;
			return;
		}
	}

	for (AActor* Cursor = GetAttachParentActor(); Cursor != nullptr; Cursor = Cursor->GetAttachParentActor())
	{
		if (ASubmarineBase* Sub = Cast<ASubmarineBase>(Cursor))
		{
			OwningSubmarine = Sub;
			return;
		}
	}
}

void ASubDoorActor::ApplyDoorState()
{
	ApplySubmarineCollisionIgnoreToAllPrimitiveComponents();

	if (DoorMesh)
	{
		DoorMesh->SetCollisionEnabled(bClosed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		DoorMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
		DoorMesh->SetVisibility(bClosed);
	}

	if (DoorCollision)
	{
		DoorCollision->SetCollisionEnabled(bClosed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		DoorCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	}

	BP_OnDoorStateChanged(bClosed);
}

void ASubDoorActor::ApplySubmarineCollisionIgnoreToAllPrimitiveComponents()
{
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		PrimitiveComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	}
}

void ASubDoorActor::RegisterWithCompartments()
{
	if (!OwningSubmarine || !OwningSubmarine->Compartments || DoorId.IsNone())
	{
		return;
	}

	FDoorState DoorState;
	DoorState.DoorId = DoorId;
	DoorState.CompartmentA = CompartmentA;
	DoorState.CompartmentB = CompartmentB;
	DoorState.bClosed = bClosed;
	DoorState.bLocked = bLocked;

	OwningSubmarine->Compartments->RegisterDoor(DoorState);
	OwningSubmarine->Compartments->SetDoorClosed(DoorId, bClosed);

	// Bridge to SubFlood: DoorId maps to ConnectionId in the flood graph.
	if (OwningSubmarine->SubFlood && OwningSubmarine->SubFlood->IsInitialized())
	{
		OwningSubmarine->SubFlood->SetDoorState(DoorId, bClosed);
	}
}
