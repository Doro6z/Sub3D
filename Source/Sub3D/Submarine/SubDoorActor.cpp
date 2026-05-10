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
	// Tick is enabled at runtime only when an animation is in flight (SetDoorClosed kickstarts,
	// Tick disables itself when OpenAlpha reaches its target). Idle door = zero tick cost.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// DoorMesh attached directly to Root. Designer is responsible for adding any pivot
	// hierarchy (BattantPivot etc.) in BP and applying the OpenAlpha-driven transform via
	// BP_OnOpenAlphaUpdated — keeps C++ free of component assumptions that conflict with
	// existing BP setups.
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Root);
	DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));
	DoorMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	DoorMesh->SetCanEverAffectNavigation(false);

	// DoorCollision is the authoritative passability volume — toggling it via OpenAlpha
	// threshold is a single binary gate regardless of the leaf's animation path.
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
	// Snap OpenAlpha to current target so we don't animate from 0 → bStartsClosed at spawn.
	OpenAlpha = bClosed ? 0.f : 1.f;
	BP_OnOpenAlphaUpdated(OpenAlpha);

	TryResolveOwningSubmarine();
	RegisterWithCompartments();
	ApplyDoorState();

	if (Interactable)
	{
		Interactable->OnInteract.AddDynamic(this, &ASubDoorActor::HandleInteract);
	}
}

void ASubDoorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Target = bClosed ? 0.f : 1.f;
	if (FMath::IsNearlyEqual(OpenAlpha, Target, 0.001f))
	{
		// Reached target — stop ticking until next state change.
		OpenAlpha = Target;
		PushOpenRatioToFlood(OpenAlpha);
		SetActorTickEnabled(false);
		return;
	}

	const float Direction = (Target > OpenAlpha) ? 1.f : -1.f;
	const float Step = DeltaTime / FMath::Max(0.05f, OpenDurationSeconds);
	const float Prev = OpenAlpha;
	OpenAlpha = FMath::Clamp(OpenAlpha + Direction * Step, 0.f, 1.f);

	// Threshold-cross: flip blocking collision once the door is past CollisionToggleAlpha.
	const bool bWasBlocking = Prev <= CollisionToggleAlpha;
	const bool bNowBlocking = OpenAlpha <= CollisionToggleAlpha;
	if (bWasBlocking != bNowBlocking)
	{
		ApplyCollisionFromAlpha();
	}

	// Push the continuous OpenRatio to the flood graph each tick so partial-open animations
	// produce partial flow (a door at 50% alpha = 50% effective passage area).
	PushOpenRatioToFlood(OpenAlpha);

	BP_OnOpenAlphaUpdated(OpenAlpha);
}

void ASubDoorActor::SetOpenAlphaFromTimeline(float NewAlpha)
{
	const float Prev = OpenAlpha;
	OpenAlpha = FMath::Clamp(NewAlpha, 0.f, 1.f);

	const bool bWasBlocking = Prev <= CollisionToggleAlpha;
	const bool bNowBlocking = OpenAlpha <= CollisionToggleAlpha;
	if (bWasBlocking != bNowBlocking)
	{
		ApplyCollisionFromAlpha();
	}
	// Note: BP_OnOpenAlphaUpdated is NOT fired here — Strategy B owns the update loop, calling
	// this back would create a feedback chain. The BP timeline handles its own transform.
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

	if (bClosed != bNewClosed)
	{
		bClosed = bNewClosed;
		// Wake the tick so the animation runs toward the new target. Tick disables itself
		// once OpenAlpha reaches the target.
		SetActorTickEnabled(true);
	}
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
	// Wake the tick on the client so OpenAlpha animates toward the new replicated target.
	// Mirror of SetDoorClosed (which runs on server).
	SetActorTickEnabled(true);
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
	// Visibility no longer toggled here — the leaf is always rendered, animated via OpenAlpha
	// through BP_OnOpenAlphaUpdated. Collision is now driven by ApplyCollisionFromAlpha() based
	// on the OpenAlpha threshold (so the door remains blocking until the leaf is visibly clear).
	ApplySubmarineCollisionIgnoreToAllPrimitiveComponents();
	ApplyCollisionFromAlpha();

	// Push door state into the flood graph. Use the current OpenAlpha (continuous), so a
	// partially-animating door is reflected as partial flow naturally — the per-tick Tick()
	// call refreshes this each frame.
	UE_LOG(LogTemp, Log,
		TEXT("Door[%s] state changed: bClosed=%d Alpha=%.2f DoorId='%s' A='%s' B='%s'"),
		*GetName(), bClosed ? 1 : 0, OpenAlpha,
		*DoorId.ToString(), *CompartmentA.ToString(), *CompartmentB.ToString());
	PushOpenRatioToFlood(OpenAlpha);

	BP_OnDoorStateChanged(bClosed);
}

void ASubDoorActor::PushOpenRatioToFlood(float Ratio)
{
	if (!OwningSubmarine || !OwningSubmarine->SubFlood)
	{
		return;
	}

	if (!DoorId.IsNone())
	{
		OwningSubmarine->SubFlood->SetDoorOpenRatio(DoorId, Ratio);
	}
	else if (!CompartmentA.IsNone() && !CompartmentB.IsNone())
	{
		OwningSubmarine->SubFlood->SetDoorOpenRatioByCompartments(CompartmentA, CompartmentB, Ratio);
	}
	// Silent skip if neither DoorId nor CompartmentA/B is set — warning fires once from
	// ApplyDoorState on the first state change so we don't spam every tick.
}

void ASubDoorActor::ApplyCollisionFromAlpha()
{
	const bool bBlocking = OpenAlpha <= CollisionToggleAlpha;
	const ECollisionEnabled::Type Mode = bBlocking
		? ECollisionEnabled::QueryAndPhysics
		: ECollisionEnabled::NoCollision;

	if (DoorMesh)
	{
		DoorMesh->SetCollisionEnabled(Mode);
		DoorMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	}
	if (DoorCollision)
	{
		DoorCollision->SetCollisionEnabled(Mode);
		DoorCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	}
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
