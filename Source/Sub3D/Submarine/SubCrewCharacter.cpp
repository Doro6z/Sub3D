#include "SubCrewCharacter.h"
#include "Engine/DamageEvents.h"
#include "SubmarineBase.h"
#include "SubCrewMovementComponent.h"
#include "SubFloodComponent.h"
#include "SubHullComponent.h"
#include "SubLegacyLog.h"
#include "SubmarineLayoutAsset.h"
#include "SubMovementComponent.h"
#include "SubmarineSystemsComponent.h"
#include "SubInteractionComponent.h"
#include "InteractableComponent.h"
#include "Generator/SubmarineDefinition.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubCrew, Log, All);

namespace
{
static FString DescribeMovementBase(const ACharacter* Character)
{
	const UPrimitiveComponent* Base = Character ? Character->GetMovementBase() : nullptr;
	return FString::Printf(TEXT("%s on %s"), *GetNameSafe(Base), *GetNameSafe(Base ? Base->GetOwner() : nullptr));
}

bool FindInteriorWalkableHit(
	const ASubmarineBase* Submarine,
	UWorld* World,
	const AActor* IgnoredActor,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	FHitResult& OutHit)
{
	OutHit = FHitResult();

	if (!Submarine || !World)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel2);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CrewFloorSnap), false, IgnoredActor);
	TArray<FHitResult> Hits;
	if (!World->LineTraceMultiByObjectType(Hits, TraceStart, TraceEnd, ObjectQueryParams, QueryParams))
	{
		return false;
	}

	float BestHitTime = TNumericLimits<float>::Max();
	bool bFoundHit = false;
	for (const FHitResult& Hit : Hits)
	{
		if (!Hit.bBlockingHit)
		{
			continue;
		}

		UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (!Submarine->IsInteriorWalkableComponent(HitComponent))
		{
			continue;
		}

		if (!bFoundHit || Hit.Time < BestHitTime)
		{
			OutHit = Hit;
			BestHitTime = Hit.Time;
			bFoundHit = true;
		}
	}

	return bFoundHit;
}
}

ASubCrewCharacter::ASubCrewCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USubCrewMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	FPSCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPSCamera"));
	FPSCamera->SetupAttachment(GetRootComponent());
	FPSCamera->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	FPSCamera->bUsePawnControlRotation = true;

	TPSCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("TPSCameraBoom"));
	TPSCameraBoom->SetupAttachment(GetRootComponent());
	TPSCameraBoom->bUsePawnControlRotation = true;
	TPSCameraBoom->bDoCollisionTest = true;
	TPSCameraBoom->ProbeChannel = ECC_GameTraceChannel2;
	TPSCameraBoom->TargetOffset = FVector(0.f, 0.f, 70.f);
	TPSCameraBoom->TargetArmLength = 0.f;
	TPSCameraBoom->SocketOffset = FVector::ZeroVector;

	TPSCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TPSCamera"));
	TPSCamera->SetupAttachment(TPSCameraBoom, USpringArmComponent::SocketName);
	TPSCamera->bUsePawnControlRotation = false;

	InteractionComponent = CreateDefaultSubobject<USubInteractionComponent>(TEXT("InteractionComponent"));

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
	}

	GetCharacterMovement()->MaxWalkSpeed = 300.f;
	GetCharacterMovement()->JumpZVelocity = 0.f;
	GetCharacterMovement()->GravityScale = 1.f;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bEnablePhysicsInteraction = false;
	// bIgnoreBaseRotation is toggled per-tick by USubCrewMovementComponent based
	// on IsEmbarked(): true while on the sub (custom yaw compensation drives the
	// rotation), false otherwise so stock based-rotation still works on any other
	// moving base in the world.
	GetCharacterMovement()->bIgnoreBaseRotation = false;
	GetCharacterMovement()->bAlwaysCheckFloor = true;

	bUseControllerRotationYaw = true;
}

void ASubCrewCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (const UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		DefaultWalkSpeed = FMath::Max(1.f, MovementComponent->MaxWalkSpeed);
		DefaultSwimSpeed = FMath::Max(1.f, MovementComponent->MaxSwimSpeed > 0.f ? MovementComponent->MaxSwimSpeed : DefaultWalkSpeed * 0.8f);
	}

	Health = MaxHealth;
	UpdateCameraRig();

	if (IsLocallyControlled())
	{
		bWantsFirstPerson = true;
		CameraBlendAlpha = 0.f;
		if (FPSCamera)
		{
			FPSCamera->SetActive(true);
		}
		if (TPSCamera)
		{
			TPSCamera->SetActive(false);
		}
	}

	UpdateLocalHeadVisibility();
	EnsureEmbarkedSubmarineBinding(TEXT("BeginPlay"));
}

USubCrewMovementComponent* ASubCrewCharacter::GetCrewMovement() const
{
	return Cast<USubCrewMovementComponent>(GetCharacterMovement());
}

void ASubCrewCharacter::ToggleCameraMode()
{
	SetFirstPersonMode(!bWantsFirstPerson);
}

void ASubCrewCharacter::SetFirstPersonMode(bool bNewFirstPerson)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	bWantsFirstPerson = bNewFirstPerson;
	UpdateCameraRig();
	UpdateLocalHeadVisibility();

	if (!bWantsFirstPerson && TPSCamera)
	{
		TPSCamera->SetActive(true);
	}

	if (bWantsFirstPerson && FPSCamera && CameraBlendAlpha <= 0.01f)
	{
		FPSCamera->SetActive(true);
	}
}

UCameraComponent* ASubCrewCharacter::GetActiveViewCamera() const
{
	if (IsLocallyControlled() && TPSCamera && TPSCamera->IsActive())
	{
		return TPSCamera;
	}

	return FPSCamera;
}

float ASubCrewCharacter::GetCurrentPostureCameraZ() const
{
	if (const USubCrewMovementComponent* CrewMovement = GetCrewMovement())
	{
		return FMath::Lerp(CrewMovement->ProneCameraZ, CrewMovement->StandingCameraZ, CrewMovement->PostureAlpha);
	}

	return FPSCamera ? FPSCamera->GetRelativeLocation().Z : 70.f;
}

void ASubCrewCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EnsureEmbarkedSubmarineBinding(TEXT("Tick"));
	UpdateCameraMode(DeltaSeconds);
	UpdateEnvironmentalEffects(DeltaSeconds);

	// FPS camera: follow posture Z + subtle sub motion sway
	if (IsLocallyControlled() && FPSCamera)
	{
		USubCrewMovementComponent* CMC = GetCrewMovement();
		if (CMC)
		{
			const float PostureZ = FMath::Lerp(CMC->ProneCameraZ, CMC->StandingCameraZ, CMC->PostureAlpha);

			// Sway from sub motion
			FVector Sway = FVector::ZeroVector;
			Sway.X = CMC->LocalSubLinearAcceleration.X * CameraSwayAccelScale;
			Sway.Y = CMC->LocalSubLinearAcceleration.Y * CameraSwayAccelScale;
			Sway.Z = CMC->LocalSubAngularVelocityDegrees.Y * CameraSwayAngularScale;
			Sway = Sway.GetClampedToMaxSize(CameraSwayMaxCm);

			FPSCamera->SetRelativeLocation(FVector(Sway.X, Sway.Y, PostureZ + Sway.Z));
		}
	}
}

void ASubCrewCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASubCrewCharacter, CurrentSubmarine);
	DOREPLIFETIME(ASubCrewCharacter, bIsAtHelm);
	DOREPLIFETIME(ASubCrewCharacter, Health);
}

void ASubCrewCharacter::OnRep_CurrentSubmarine()
{
	if (CurrentSubmarine)
	{
		if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
		{
			CrewMov->InitializeForSubmarine();
			CrewMov->RefreshEmbarkedFlooring();
		}
	}
}

void ASubCrewCharacter::SetCurrentSubmarine(ASubmarineBase* Sub)
{
	if (CurrentSubmarine == Sub)
	{
		return;
	}

	CurrentSubmarine = Sub;

	if (!CurrentSubmarine)
	{
		ResetEnvironmentalState();
	}

	UE_LOG(LogSubCrew, Log, TEXT("SetCurrentSubmarine | Crew=%s | Sub=%s"), *GetName(), *GetNameSafe(CurrentSubmarine));
}

ASubmarineBase* ASubCrewCharacter::ResolveSubmarineFromMovementBase() const
{
	const UPrimitiveComponent* MovementBase = GetMovementBase();
	const AActor* CurrentOwner = MovementBase ? MovementBase->GetOwner() : nullptr;
	while (CurrentOwner)
	{
		if (ASubmarineBase* Submarine = Cast<ASubmarineBase>(const_cast<AActor*>(CurrentOwner)))
		{
			return Submarine;
		}

		const AActor* NextOwner = CurrentOwner->GetOwner();
		if (!NextOwner)
		{
			NextOwner = CurrentOwner->GetAttachParentActor();
		}

		if (NextOwner == CurrentOwner)
		{
			break;
		}

		CurrentOwner = NextOwner;
	}

	return nullptr;
}

void ASubCrewCharacter::EnsureEmbarkedSubmarineBinding(const TCHAR* Context)
{
	if (CurrentSubmarine)
	{
		return;
	}

	ASubmarineBase* ResolvedSubmarine = ResolveSubmarineFromMovementBase();
	if (!ResolvedSubmarine || !ResolvedSubmarine->IsInteriorWalkableComponent(GetMovementBase()))
	{
		return;
	}

	SetCurrentSubmarine(ResolvedSubmarine);

	if (USubCrewMovementComponent* CrewMovement = GetCrewMovement())
	{
		CrewMovement->InitializeForSubmarine();
		CrewMovement->RefreshEmbarkedFlooring();
	}

	UE_LOG(
		LogSubCrew,
		Log,
		TEXT("Auto-bound submarine from movement base | Context=%s | Crew=%s | Base=%s | Sub=%s"),
		Context,
		*GetName(),
		*DescribeMovementBase(this),
		*GetNameSafe(CurrentSubmarine));
}

void ASubCrewCharacter::EnterOnFootInSubmarine(ASubmarineBase* Sub, const FTransform& SpawnXform)
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetCurrentSubmarine(Sub);
	bIsAtHelm = false;

	SetActorTransform(SpawnXform, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();

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
		CrewMov->RefreshEmbarkedFlooring();
		bIsEmbarked = CrewMov->IsEmbarked();

		// Safety: if floor not found after teleport, sweep downward to snap onto interior floor.
		if (!CrewMov->CurrentFloor.IsWalkableFloor() && Sub)
		{
			const float CapsuleHalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
			const FVector TraceStart = GetActorLocation() + FVector(0.f, 0.f, CapsuleHalfHeight);
			const FVector TraceEnd = TraceStart - FVector(0.f, 0.f, CapsuleHalfHeight * 4.f);

			FHitResult Hit;
			const FVector PreviousLocation = GetActorLocation();
			if (FindInteriorWalkableHit(Sub, GetWorld(), this, TraceStart, TraceEnd, Hit))
			{
				const FVector CorrectedLocation = Hit.ImpactPoint + FVector(0.f, 0.f, CapsuleHalfHeight);
				SetActorLocation(CorrectedLocation, false, nullptr, ETeleportType::TeleportPhysics);
				CrewMov->RefreshEmbarkedFlooring();
				bIsEmbarked = CrewMov->IsEmbarked();

				UE_LOG(
					LogSubCrew,
					Log,
					TEXT("Floor snap applied | From=%s | To=%s | HitComp=%s"),
					*PreviousLocation.ToCompactString(),
					*CorrectedLocation.ToCompactString(),
					*GetNameSafe(Hit.GetComponent()));
			}
			else
			{
				UE_LOG(
					LogSubCrew,
					Warning,
					TEXT("Floor snap failed | No interior floor found below spawn | Start=%s | End=%s"),
					*TraceStart.ToCompactString(),
					*TraceEnd.ToCompactString());
			}
		}
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

	SetCurrentSubmarine(Submarine);
	bIsAtHelm = false;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	GetCharacterMovement()->GravityScale = 1.f;

	if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
	{
		CrewMov->InitializeForSubmarine();
		CrewMov->RefreshEmbarkedFlooring();
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
	SetCurrentSubmarine(nullptr);
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
	// Command state intentionally persists on the submarine after the pilot
	// leaves the helm. A thrust/rudder/dive-plane value set by pilot A is
	// still active when pilot A leaves, and pilot B sees the same values when
	// they sit down. Only the pilot slot is released here.
	bIsAtHelm = false;
	if (CurrentSubmarine && CurrentSubmarine->CurrentPilot == this)
	{
		CurrentSubmarine->ClearPilot();
	}
}

float ASubCrewCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageAmount <= 0.f || Health <= 0.f)
	{
		return 0.f;
	}

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Health = FMath::Max(0.f, Health - ActualDamage);

	if (Health <= 0.f)
	{
		UE_LOG(LogSubCrew, Warning, TEXT("ASubCrewCharacter::TakeDamage | %s died!"), *GetName());
		// TODO: Handle death (ragdoll, respawn, etc.) if needed for Proto03
	}

	return ActualDamage;
}

void ASubCrewCharacter::Interact()
{
	if (InteractionComponent)
	{
		InteractionComponent->TryPrimaryInteract();
	}
}

void ASubCrewCharacter::SetPressureProtectionKPa(float NewPressureProtectionKPa)
{
	PressureProtectionKPa = FMath::Max(0.f, NewPressureProtectionKPa);
}

void ASubCrewCharacter::SetWaterMovementProtectionMultiplier(float NewWaterMovementProtectionMultiplier)
{
	WaterMovementProtectionMultiplier = FMath::Max(0.f, NewWaterMovementProtectionMultiplier);
}

void ASubCrewCharacter::ServerSetPostureTarget_Implementation(float Alpha)
{
	if (USubCrewMovementComponent* CrewMovement = GetCrewMovement())
	{
		CrewMovement->SetPostureTarget(Alpha);
	}
}

void ASubCrewCharacter::ServerSetRunning_Implementation(bool bNewRunning)
{
	if (USubCrewMovementComponent* CrewMovement = GetCrewMovement())
	{
		if (bNewRunning)
		{
			CrewMovement->RequestRunStart();
		}
		else
		{
			CrewMovement->RequestRunStop();
		}
	}
}

bool ASubCrewCharacter::ResolveCurrentCompartment(FCompartmentState& OutState, FBox& OutLocalBounds) const
{
	if (!CurrentSubmarine)
	{
		return false;
	}

	// Resolve compartment spatially from GeneratedDefinition or LayoutAsset.
	const FVector LocalPos = CurrentSubmarine->GetActorTransform().InverseTransformPosition(GetActorLocation());
	bool bFoundCompartment = false;

	if (CurrentSubmarine->GeneratedDefinition)
	{
		const FGeneratedCompartmentDef* Comp = CurrentSubmarine->GeneratedDefinition->FindCompartmentAtLocalLocation(LocalPos);
		if (Comp)
		{
			OutState.CompartmentId = Comp->CompartmentId;
			OutLocalBounds = FBox(Comp->HydroBoundsMin, Comp->HydroBoundsMax);
			bFoundCompartment = true;
		}
	}

	if (!bFoundCompartment && CurrentSubmarine->SubHull && CurrentSubmarine->SubHull->LayoutAsset)
	{
		// LEGACY (Phase 7A, 2026-04-10) — Proto fallback. We resolve crew
		// compartment from LayoutAsset because no GeneratedDefinition is
		// assigned. Log once per process to avoid per-tick spam.
		// Will be removed in Phase 7B.
		static bool bWarnedLegacyLayoutFallback = false;
		if (!bWarnedLegacyLayoutFallback)
		{
			bWarnedLegacyLayoutFallback = true;
			UE_LOG(LogSubLegacy, Warning,
				TEXT("[LEGACY] ASubCrewCharacter::ResolveCurrentCompartment: using LayoutAsset fallback. ")
				TEXT("Assign a GeneratedDefinition on the submarine to use the generator path."));
		}

		const USubmarineLayoutAsset* Layout = CurrentSubmarine->SubHull->LayoutAsset;
		float BestVolume = TNumericLimits<float>::Max();
		for (const FSubCompartmentDef& CompDef : Layout->Compartments)
		{
			const FBox Bounds(CompDef.HydroBoundsMin, CompDef.HydroBoundsMax);
			const FBox Expanded = Bounds.ExpandBy(25.f);
			if (Expanded.IsInsideOrOn(LocalPos))
			{
				const float Volume = FMath::Max(1.f, Expanded.GetVolume());
				if (Volume < BestVolume)
				{
					BestVolume = Volume;
					OutState.CompartmentId = CompDef.CompartmentId;
					OutLocalBounds = Bounds;
					bFoundCompartment = true;
				}
			}
		}
	}

	if (!bFoundCompartment)
	{
		return false;
	}

	// Read flood data from SubFlood.
	OutState.FloodLevel01 = 0.f;
	OutState.WaterHeightCm = 0.f;
	OutState.WaterMassLiters = 0.f;

	USubFloodComponent* SubFlood = CurrentSubmarine->SubFlood;
	if (SubFlood && SubFlood->IsInitialized())
	{
		OutState.FloodLevel01 = SubFlood->GetCompartmentFloodLevel01(OutState.CompartmentId);
		OutState.WaterHeightCm = SubFlood->GetCompartmentWaterHeightCm(OutState.CompartmentId);
		OutState.WaterMassLiters = SubFlood->GetCompartmentWaterLiters(OutState.CompartmentId);
	}

	return true;
}

void ASubCrewCharacter::UpdateEnvironmentalEffects(float DeltaSeconds)
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		return;
	}

	FCompartmentState CompartmentState;
	FBox CompartmentBounds(EForceInit::ForceInit);

	CurrentCompartmentId = NAME_None;
	CurrentWaterHeightCm = 0.f;
	CurrentWaterImmersion01 = 0.f;
	bIsSwimmingByFlood = false;
	bPressureDangerous = false;

	float AmbientPressureKPa = 101.325f;
	if (ResolveCurrentCompartment(CompartmentState, CompartmentBounds))
	{
		CurrentCompartmentId = CompartmentState.CompartmentId;
		CurrentWaterHeightCm = CompartmentState.WaterHeightCm;
		AmbientPressureKPa = CompartmentState.InternalPressureKPa;

		if (CurrentSubmarine)
		{
			const FVector LocalLocation = CurrentSubmarine->GetActorTransform().InverseTransformPosition(GetActorLocation());
			const float CapsuleHalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
			const float CapsuleFullHeight = FMath::Max(1.f, CapsuleHalfHeight * 2.f);
			const float FeetZ = LocalLocation.Z - CapsuleHalfHeight;
			const float WaterSurfaceZ = CompartmentBounds.Min.Z + CompartmentState.WaterHeightCm;
			const float ImmersionDepthCm = FMath::Clamp(WaterSurfaceZ - FeetZ, 0.f, CapsuleFullHeight);
			CurrentWaterImmersion01 = FMath::Clamp(ImmersionDepthCm / CapsuleFullHeight, 0.f, 1.f);
		}
	}
	else if (CurrentSubmarine && CurrentSubmarine->SubMovement)
	{
		AmbientPressureKPa = CurrentSubmarine->SubMovement->GetPressureAtDepth(CurrentSubmarine->SubMovement->CurrentDepth) * 101.325f;
	}

	CurrentAmbientPressureKPa = AmbientPressureKPa;
	ApplyWaterMovementState(CurrentWaterImmersion01);
	ApplyPressureEffects(DeltaSeconds, CurrentAmbientPressureKPa);

	if (bDebugLogEnvironmentState && CurrentSubmarine)
	{
		EnvironmentDebugLogTimer += DeltaSeconds;
		if (EnvironmentDebugLogTimer >= FMath::Max(0.1f, EnvironmentDebugLogIntervalSeconds))
		{
			EnvironmentDebugLogTimer = 0.f;
			UE_LOG(
				LogSubCrew,
				Log,
				TEXT("EnvState | Sub=%s | Comp=%s | Pressure=%.2f kPa | WaterHeight=%.2f cm | Immersion=%.2f | Exposure=%.2f | Swim=%d"),
				*GetNameSafe(CurrentSubmarine),
				*CurrentCompartmentId.ToString(),
				CurrentAmbientPressureKPa,
				CurrentWaterHeightCm,
				CurrentWaterImmersion01,
				PressureExposureSeconds,
				bIsSwimmingByFlood ? 1 : 0);
		}
	}
	else
	{
		EnvironmentDebugLogTimer = 0.f;
	}
}

void ASubCrewCharacter::ApplyPressureEffects(float DeltaSeconds, float AmbientPressureKPa)
{
	const float SafeAmbientPressureKPa = BaseSafeAmbientPressureKPa + PressureProtectionKPa;
	if (AmbientPressureKPa <= SafeAmbientPressureKPa)
	{
		PressureExposureSeconds = FMath::Max(0.f, PressureExposureSeconds - DeltaSeconds * FMath::Max(0.f, PressureRecoveryRate));
		return;
	}

	const float OverPressureKPa = AmbientPressureKPa - SafeAmbientPressureKPa;
	bPressureDangerous = true;
	PressureExposureSeconds += DeltaSeconds;

	if (!HasAuthority() || PressureExposureSeconds <= PressureGraceSeconds)
	{
		return;
	}

	const float Severity01 = 1.f + (OverPressureKPa / FMath::Max(1.f, SafeAmbientPressureKPa)) * FMath::Max(0.f, PressureDamageSeverityScale);
	const float DamageToApply = FMath::Max(0.f, PressureDamagePerSecond) * Severity01 * DeltaSeconds;
	if (DamageToApply <= 0.f)
	{
		return;
	}

	FDamageEvent DamageEvent;
	AActor* DamageSource = CurrentSubmarine ? static_cast<AActor*>(CurrentSubmarine) : static_cast<AActor*>(this);
	TakeDamage(DamageToApply, DamageEvent, nullptr, DamageSource);
}

void ASubCrewCharacter::ApplyWaterMovementState(float WaterImmersion01)
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		return;
	}

	const float MovementProtection = FMath::Max(0.f, WaterMovementProtectionMultiplier);
	float WalkSpeedMultiplier = 1.f;
	float CrewWalkSpeedMultiplier = 1.f;

	if (const USubCrewMovementComponent* CrewMovement = GetCrewMovement())
	{
		CrewWalkSpeedMultiplier = CrewMovement->GetDesiredWalkSpeedMultiplier();
	}

	if (WaterImmersion01 >= SwimThreshold01)
	{
		bIsSwimmingByFlood = true;
	}
	else if (WaterImmersion01 >= DeepWadeThreshold01)
	{
		const float RangeAlpha = FMath::GetRangePct(DeepWadeThreshold01, FMath::Max(DeepWadeThreshold01 + KINDA_SMALL_NUMBER, SwimThreshold01), WaterImmersion01);
		WalkSpeedMultiplier = FMath::Lerp(DeepWadeSpeedMultiplier, NearSwimSpeedMultiplier, RangeAlpha);
	}
	else if (WaterImmersion01 >= ShallowWadeThreshold01)
	{
		const float RangeAlpha = FMath::GetRangePct(ShallowWadeThreshold01, FMath::Max(ShallowWadeThreshold01 + KINDA_SMALL_NUMBER, DeepWadeThreshold01), WaterImmersion01);
		WalkSpeedMultiplier = FMath::Lerp(ShallowWadeSpeedMultiplier, DeepWadeSpeedMultiplier, RangeAlpha);
	}

	MovementComponent->MaxWalkSpeed = DefaultWalkSpeed * CrewWalkSpeedMultiplier * WalkSpeedMultiplier * MovementProtection;
	MovementComponent->MaxSwimSpeed = DefaultSwimSpeed * FMath::Max(0.f, SwimSpeedMultiplier) * MovementProtection;

	if (bIsSwimmingByFlood)
	{
		if (MovementComponent->MovementMode != MOVE_Swimming)
		{
			MovementComponent->SetMovementMode(MOVE_Swimming);
		}
	}
	else if (MovementComponent->MovementMode == MOVE_Swimming)
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
	}
}

void ASubCrewCharacter::ResetEnvironmentalState()
{
	CurrentCompartmentId = NAME_None;
	CurrentAmbientPressureKPa = 101.325f;
	CurrentWaterHeightCm = 0.f;
	CurrentWaterImmersion01 = 0.f;
	PressureExposureSeconds = 0.f;
	bPressureDangerous = false;
	bIsSwimmingByFlood = false;
	EnvironmentDebugLogTimer = 0.f;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		const float CrewWalkSpeedMultiplier = GetCrewMovement() ? GetCrewMovement()->GetDesiredWalkSpeedMultiplier() : 1.f;
		MovementComponent->MaxWalkSpeed = DefaultWalkSpeed * CrewWalkSpeedMultiplier;
		MovementComponent->MaxSwimSpeed = DefaultSwimSpeed;
		if (MovementComponent->MovementMode == MOVE_Swimming)
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
		}
	}
}

void ASubCrewCharacter::UpdateCameraMode(float DeltaSeconds)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	const float TargetAlpha = bWantsFirstPerson ? 0.f : 1.f;
	CameraBlendAlpha = FMath::FInterpTo(CameraBlendAlpha, TargetAlpha, DeltaSeconds, CameraBlendSpeed);
	UpdateCameraRig();

	if (TPSCamera)
	{
		const bool bShouldUseTPSCamera = !bWantsFirstPerson || CameraBlendAlpha > 0.01f;
		TPSCamera->SetActive(bShouldUseTPSCamera);
	}

	if (FPSCamera)
	{
		const bool bShouldUseFPSCamera = bWantsFirstPerson && CameraBlendAlpha <= 0.01f;
		FPSCamera->SetActive(bShouldUseFPSCamera);
	}

	UpdateLocalHeadVisibility();
}

void ASubCrewCharacter::UpdateCameraRig()
{
	if (!TPSCameraBoom)
	{
		return;
	}

	const float PostureAlpha = GetCrewMovement() ? GetCrewMovement()->PostureAlpha : 1.f;
	const FVector ShoulderOffset = FMath::Lerp(ThirdPersonProneSocketOffset, ThirdPersonStandingSocketOffset, PostureAlpha);

	TPSCameraBoom->TargetOffset = FVector(0.f, 0.f, GetCurrentPostureCameraZ());
	TPSCameraBoom->TargetArmLength = FMath::Lerp(0.f, ThirdPersonArmLength, CameraBlendAlpha);
	TPSCameraBoom->SocketOffset = FMath::Lerp(FVector::ZeroVector, ShoulderOffset, CameraBlendAlpha);
}

void ASubCrewCharacter::UpdateLocalHeadVisibility()
{
	if (!IsLocallyControlled() || !bHideHeadInFPS)
	{
		return;
	}

	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh)
	{
		return;
	}

	const bool bShouldHideHead = bWantsFirstPerson && CameraBlendAlpha <= 0.15f;
	if (bShouldHideHead == bHeadHiddenForLocalView)
	{
		return;
	}

	if (bShouldHideHead)
	{
		SkelMesh->HideBoneByName(FName("head"), EPhysBodyOp::PBO_None);
	}
	else
	{
		SkelMesh->UnHideBoneByName(FName("head"));
	}

	bHeadHiddenForLocalView = bShouldHideHead;
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
