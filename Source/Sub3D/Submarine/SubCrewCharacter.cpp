#include "SubCrewCharacter.h"
#include "Engine/DamageEvents.h"
#include "SubmarineBase.h"
#include "SubCrewMovementComponent.h"
#include "SubHullComponent.h"
#include "SubMovementComponent.h"
#include "SubmarineSystemsComponent.h"
#include "SubInteractionComponent.h"
#include "InteractableComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
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

	const TArray<UPrimitiveComponent*> WalkableComponents = Submarine->GetInteriorWalkableComponents();
	if (WalkableComponents.Num() == 0)
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
		if (!HitComponent || !WalkableComponents.Contains(HitComponent))
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

void ASubCrewCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (const UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		DefaultWalkSpeed = FMath::Max(1.f, MovementComponent->MaxWalkSpeed);
		DefaultSwimSpeed = FMath::Max(1.f, MovementComponent->MaxSwimSpeed > 0.f ? MovementComponent->MaxSwimSpeed : DefaultWalkSpeed * 0.8f);
	}
}

void ASubCrewCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateEnvironmentalEffects(DeltaSeconds);
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

void ASubCrewCharacter::SetPressureProtectionKPa(float NewPressureProtectionKPa)
{
	PressureProtectionKPa = FMath::Max(0.f, NewPressureProtectionKPa);
}

void ASubCrewCharacter::SetWaterMovementProtectionMultiplier(float NewWaterMovementProtectionMultiplier)
{
	WaterMovementProtectionMultiplier = FMath::Max(0.f, NewWaterMovementProtectionMultiplier);
}

bool ASubCrewCharacter::ResolveCurrentCompartment(FCompartmentState& OutState, FBox& OutLocalBounds) const
{
	if (!CurrentSubmarine || !CurrentSubmarine->SubHull)
	{
		return false;
	}

	return CurrentSubmarine->SubHull->SampleCompartmentStateAtWorldLocation(GetActorLocation(), OutState, &OutLocalBounds);
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

	MovementComponent->MaxWalkSpeed = DefaultWalkSpeed * WalkSpeedMultiplier * MovementProtection;
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
		MovementComponent->MaxWalkSpeed = DefaultWalkSpeed;
		MovementComponent->MaxSwimSpeed = DefaultSwimSpeed;
		if (MovementComponent->MovementMode == MOVE_Swimming)
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
		}
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
