#include "SubCrewCharacter.h"
#include "SubmarineBase.h"
#include "SubCrewMovementComponent.h"
#include "SubMovementComponent.h"
#include "SubmarineSystemsComponent.h"
#include "SubInteractionComponent.h"
#include "InteractableComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"

ASubCrewCharacter::ASubCrewCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USubCrewMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	FPSCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPSCamera"));
	FPSCamera->SetupAttachment(GetRootComponent());
	FPSCamera->SetRelativeLocation(FVector(0.f, 0.f, 70.f)); // Eye height
	FPSCamera->bUsePawnControlRotation = true;

	InteractionComponent = CreateDefaultSubobject<USubInteractionComponent>(TEXT("InteractionComponent"));

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore); // Submarine
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);  // SubInterior
	}

	GetCharacterMovement()->MaxWalkSpeed              = 300.f;
	GetCharacterMovement()->JumpZVelocity             = 0.f;
	GetCharacterMovement()->GravityScale              = 1.f;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bEnablePhysicsInteraction = false;
	GetCharacterMovement()->bIgnoreBaseRotation       = false;
	GetCharacterMovement()->bAlwaysCheckFloor         = true;

	bUseControllerRotationYaw = true;
}

void ASubCrewCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASubCrewCharacter, CurrentSubmarine);
	DOREPLIFETIME(ASubCrewCharacter, bIsAtHelm);
}

// ── Boarding ──────────────────────────────────────────────────────────────────

void ASubCrewCharacter::SetCurrentSubmarine(ASubmarineBase* Sub)
{
	CurrentSubmarine = Sub;
}

void ASubCrewCharacter::EnterOnFootInSubmarine(ASubmarineBase* Sub, const FTransform& SpawnXform)
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	CurrentSubmarine = Sub;
	bIsAtHelm = false;

	SetActorTransform(SpawnXform, false, nullptr, ETeleportType::TeleportPhysics);

	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	GetCharacterMovement()->GravityScale = 1.0f;

	if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
	{
		CrewMov->InitializeForSubmarine();
	}

	if (CurrentSubmarine && CurrentSubmarine->CurrentPilot == this)
	{
		CurrentSubmarine->ClearPilot();
	}
}

void ASubCrewCharacter::BoardSubmarine(ASubmarineBase* Submarine)
{
	// Legacy API retained for Blueprint compatibility.
	// Durable Proto03 behavior for traversal is on-foot inside the submarine.
	if (!Submarine) return;
	CurrentSubmarine = Submarine;
	bIsAtHelm = false;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	GetCharacterMovement()->GravityScale = 1.f;

	if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
	{
		CrewMov->InitializeForSubmarine();
	}
}

void ASubCrewCharacter::DisembarkSubmarine()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	CurrentSubmarine = nullptr;
	bIsAtHelm        = false;
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	GetCharacterMovement()->GravityScale = 1.f;
}

// ── Helm ──────────────────────────────────────────────────────────────────────

void ASubCrewCharacter::ForceHelm()
{
	bIsAtHelm = true;
	if (CurrentSubmarine) CurrentSubmarine->SetPilot(this);
}

void ASubCrewCharacter::TakeHelm()
{
	if (!CurrentSubmarine) return;
	Server_TakeHelm();
}

void ASubCrewCharacter::ReleaseHelm()
{
	Server_ReleaseHelm();
}

void ASubCrewCharacter::Server_TakeHelm_Implementation()
{
	bIsAtHelm = true;
	if (CurrentSubmarine) CurrentSubmarine->SetPilot(this);
}

void ASubCrewCharacter::Server_ReleaseHelm_Implementation()
{
	bIsAtHelm = false;
	if (CurrentSubmarine && CurrentSubmarine->CurrentPilot == this)
		CurrentSubmarine->ClearPilot();

	if (CurrentSubmarine && CurrentSubmarine->SubMovement)
	{
		CurrentSubmarine->SubMovement->SetThrustInput(0.f);
		CurrentSubmarine->SubMovement->SetRudderInput(0.f);
		CurrentSubmarine->SubMovement->SetDivePlaneInput(0.f);
	}
	if (CurrentSubmarine && CurrentSubmarine->Systems)
	{
		CurrentSubmarine->Systems->SetHelmThrottleCommand(0.f);
		CurrentSubmarine->Systems->SetHelmYawCommand(0.f);
		CurrentSubmarine->Systems->SetHelmTrimCommand(0.f);
	}
}

// ── Interact ──────────────────────────────────────────────────────────────────

void ASubCrewCharacter::Interact()
{
	if (InteractionComponent)
	{
		InteractionComponent->TryPrimaryInteract();
	}
}

// ── Sub input RPCs ────────────────────────────────────────────────────────────

void ASubCrewCharacter::Server_SetThrust_Implementation(float Value)
{
	if (CurrentSubmarine && CurrentSubmarine->Systems)
		CurrentSubmarine->Systems->SetHelmThrottleCommand(Value);
	else if (CurrentSubmarine && CurrentSubmarine->SubMovement)
		CurrentSubmarine->SubMovement->SetThrustInput(Value);
}

void ASubCrewCharacter::Server_SetRudder_Implementation(float Value)
{
	if (CurrentSubmarine && CurrentSubmarine->Systems)
		CurrentSubmarine->Systems->SetHelmYawCommand(Value);
	else if (CurrentSubmarine && CurrentSubmarine->SubMovement)
		CurrentSubmarine->SubMovement->SetRudderInput(Value);
}

void ASubCrewCharacter::Server_SetDivePlane_Implementation(float Value)
{
	if (CurrentSubmarine && CurrentSubmarine->Systems)
		CurrentSubmarine->Systems->SetHelmTrimCommand(Value);
	else if (CurrentSubmarine && CurrentSubmarine->SubMovement)
		CurrentSubmarine->SubMovement->SetDivePlaneInput(Value);
}

void ASubCrewCharacter::Server_SetBallastTarget_Implementation(int32 Index, float Target)
{
	if (CurrentSubmarine && CurrentSubmarine->Systems)
		CurrentSubmarine->Systems->SetBallastTargetByIndex(Index, Target);
	else if (CurrentSubmarine && CurrentSubmarine->SubMovement)
		CurrentSubmarine->SubMovement->SetBallastTarget(Index, Target);
}

void ASubCrewCharacter::Server_ResyncBallasts_Implementation(float GlobalTarget)
{
	if (CurrentSubmarine && CurrentSubmarine->Systems)
	{
		CurrentSubmarine->Systems->SetGlobalBallastTarget(GlobalTarget);
		CurrentSubmarine->Systems->ResyncAllBallasts();
	}
	else if (CurrentSubmarine && CurrentSubmarine->SubMovement)
	{
		CurrentSubmarine->SubMovement->GlobalTargetFill = GlobalTarget;
		CurrentSubmarine->SubMovement->ResyncAllBallasts();
	}
}
