#include "SubCrewCharacter.h"
#include "SubmarineBase.h"
#include "SubCrewMovementComponent.h"
#include "SubMovementComponent.h"
#include "SubmarineSystemsComponent.h"
#include "SubInteractionComponent.h"
#include "InteractableComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubCrew, Log, All);

namespace
{
static FString DescribeMovementBase(const ACharacter* Character)
{
	const UPrimitiveComponent* Base = Character ? Character->GetMovementBase() : nullptr;
	return FString::Printf(TEXT("%s on %s"), *GetNameSafe(Base), *GetNameSafe(Base ? Base->GetOwner() : nullptr));
}
}

ASubCrewCharacter::ASubCrewCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USubCrewMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	FPSCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPSCamera"));
	FPSCamera->SetupAttachment(GetRootComponent());
	FPSCamera->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	FPSCamera->bUsePawnControlRotation = true;

	InteractionComponent = CreateDefaultSubobject<USubInteractionComponent>(TEXT("InteractionComponent"));

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
	}

	GetCharacterMovement()->MaxWalkSpeed = 300.f;
	GetCharacterMovement()->JumpZVelocity = 0.f;
	GetCharacterMovement()->GravityScale = 1.f;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bEnablePhysicsInteraction = false;
	GetCharacterMovement()->bIgnoreBaseRotation = false;
	GetCharacterMovement()->bAlwaysCheckFloor = true;

	bUseControllerRotationYaw = true;
}

void ASubCrewCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASubCrewCharacter, CurrentSubmarine);
	DOREPLIFETIME(ASubCrewCharacter, bIsAtHelm);
}

void ASubCrewCharacter::OnRep_CurrentSubmarine()
{
	if (CurrentSubmarine)
	{
		if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
		{
			CrewMov->InitializeForSubmarine();
		}
	}
}

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

	UE_LOG(
		LogSubCrew,
		Log,
		TEXT("EnterOnFootInSubmarine | Sub=%s | SpawnLoc=%s | MovementMode=%d | Base=%s"),
		*GetNameSafe(Sub),
		*SpawnXform.GetLocation().ToCompactString(),
		static_cast<int32>(GetCharacterMovement()->MovementMode),
		*DescribeMovementBase(this));

	bool bIsEmbarked = false;
	if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
	{
		CrewMov->InitializeForSubmarine();
		bIsEmbarked = CrewMov->IsEmbarked();
	}

	UE_LOG(
		LogSubCrew,
		Log,
		TEXT("Post-init | MovementMode=%d | Base=%s | IsEmbarked=%d"),
		static_cast<int32>(GetCharacterMovement()->MovementMode),
		*DescribeMovementBase(this),
		bIsEmbarked ? 1 : 0);

	if (CurrentSubmarine && CurrentSubmarine->CurrentPilot == this)
	{
		CurrentSubmarine->ClearPilot();
	}
}

void ASubCrewCharacter::BoardSubmarine(ASubmarineBase* Submarine)
{
	if (!Submarine)
	{
		return;
	}

	CurrentSubmarine = Submarine;
	bIsAtHelm = false;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	GetCharacterMovement()->GravityScale = 1.f;

	if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
	{
		CrewMov->InitializeForSubmarine();
	}

	UE_LOG(
		LogSubCrew,
		Log,
		TEXT("BoardSubmarine | Sub=%s | MovementMode=%d | Base=%s"),
		*GetNameSafe(Submarine),
		static_cast<int32>(GetCharacterMovement()->MovementMode),
		*DescribeMovementBase(this));
}

void ASubCrewCharacter::DisembarkSubmarine()
{
	const ASubmarineBase* PreviousSubmarine = CurrentSubmarine;

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	CurrentSubmarine = nullptr;
	bIsAtHelm = false;
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	GetCharacterMovement()->GravityScale = 1.f;

	UE_LOG(
		LogSubCrew,
		Log,
		TEXT("DisembarkSubmarine | WasSub=%s | MovementMode=%d"),
		*GetNameSafe(PreviousSubmarine),
		static_cast<int32>(GetCharacterMovement()->MovementMode));
}

void ASubCrewCharacter::ForceHelm()
{
	bIsAtHelm = true;
	if (CurrentSubmarine)
	{
		CurrentSubmarine->SetPilot(this);
	}
}

void ASubCrewCharacter::TakeHelm()
{
	if (!CurrentSubmarine)
	{
		return;
	}

	Server_TakeHelm();
}

void ASubCrewCharacter::ReleaseHelm()
{
	Server_ReleaseHelm();
}

void ASubCrewCharacter::Server_TakeHelm_Implementation()
{
	bIsAtHelm = true;
	if (CurrentSubmarine)
	{
		CurrentSubmarine->SetPilot(this);
	}
}

void ASubCrewCharacter::Server_ReleaseHelm_Implementation()
{
	bIsAtHelm = false;
	if (CurrentSubmarine && CurrentSubmarine->CurrentPilot == this)
	{
		CurrentSubmarine->ClearPilot();
	}

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

void ASubCrewCharacter::Interact()
{
	if (InteractionComponent)
	{
		InteractionComponent->TryPrimaryInteract();
	}
}

void ASubCrewCharacter::Server_SetThrust_Implementation(float Value)
{
	if (CurrentSubmarine && CurrentSubmarine->Systems)
	{
		CurrentSubmarine->Systems->SetHelmThrottleCommand(Value);
	}
	else if (CurrentSubmarine && CurrentSubmarine->SubMovement)
	{
		CurrentSubmarine->SubMovement->SetThrustInput(Value);
	}
}

void ASubCrewCharacter::Server_SetRudder_Implementation(float Value)
{
	if (CurrentSubmarine && CurrentSubmarine->Systems)
	{
		CurrentSubmarine->Systems->SetHelmYawCommand(Value);
	}
	else if (CurrentSubmarine && CurrentSubmarine->SubMovement)
	{
		CurrentSubmarine->SubMovement->SetRudderInput(Value);
	}
}

void ASubCrewCharacter::Server_SetDivePlane_Implementation(float Value)
{
	if (CurrentSubmarine && CurrentSubmarine->Systems)
	{
		CurrentSubmarine->Systems->SetHelmTrimCommand(Value);
	}
	else if (CurrentSubmarine && CurrentSubmarine->SubMovement)
	{
		CurrentSubmarine->SubMovement->SetDivePlaneInput(Value);
	}
}

void ASubCrewCharacter::Server_SetBallastTarget_Implementation(int32 Index, float Target)
{
	if (CurrentSubmarine && CurrentSubmarine->Systems)
	{
		CurrentSubmarine->Systems->SetBallastTargetByIndex(Index, Target);
	}
	else if (CurrentSubmarine && CurrentSubmarine->SubMovement)
	{
		CurrentSubmarine->SubMovement->SetBallastTarget(Index, Target);
	}
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
