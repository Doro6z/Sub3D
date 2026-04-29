#include "Ladder.h"
#include "InteractableComponent.h"
#include "LadderClimbComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ALadder::ALadder()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootScene);

	LadderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LadderMesh"));
	LadderMesh->SetupAttachment(RootScene);
	LadderMesh->SetMobility(EComponentMobility::Movable);
	// Default: no collision against the crew capsule — the climb is driven by the
	// rebase, not by walking up step meshes. Authoring can override per-instance.
	LadderMesh->SetCollisionProfileName(TEXT("NoCollision"));

	ClimbComponent = CreateDefaultSubobject<ULadderClimbComponent>(TEXT("ClimbComponent"));
	// Sane defaults — author overrides via Details panel.
	// ClimbStartLocal / ClimbEndLocal remain at the component's class defaults
	// (0,0,0) → (0,0,200) until the user tunes them in the level instance.

	BottomInteractable = CreateDefaultSubobject<UInteractableComponent>(TEXT("BottomInteractable"));
	BottomInteractable->InteractionActionText = NSLOCTEXT("Sub3DLadder", "Climb", "Monter");
	BottomInteractable->InteractRange = 200.f;

	TopInteractable = CreateDefaultSubobject<UInteractableComponent>(TEXT("TopInteractable"));
	TopInteractable->InteractionActionText = NSLOCTEXT("Sub3DLadder", "Descend", "Descendre");
	TopInteractable->InteractRange = 200.f;
}

void ALadder::BeginPlay()
{
	Super::BeginPlay();

	// Bind interactables → climb component entry handlers. Server-side only.
	if (HasAuthority())
	{
		if (BottomInteractable)
		{
			BottomInteractable->OnInteract.AddDynamic(this, &ALadder::HandleBottomInteract);
		}
		if (TopInteractable)
		{
			TopInteractable->OnInteract.AddDynamic(this, &ALadder::HandleTopInteract);
		}
	}
}

void ALadder::HandleBottomInteract(ASubCrewCharacter* Interactor)
{
	if (ClimbComponent)
	{
		ClimbComponent->TryEnterClimbFromBottom(Interactor);
	}
}

void ALadder::HandleTopInteract(ASubCrewCharacter* Interactor)
{
	if (ClimbComponent)
	{
		ClimbComponent->TryEnterClimbFromTop(Interactor);
	}
}
