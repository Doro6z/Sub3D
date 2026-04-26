#include "SubCrewCharacter.h"
#include "Sub3DDebugSettings.h"
#include "CompartmentVolumeComponent.h"
#include "CrewUnderwaterPPComponent.h"
#include "SubCrewNetTypes.h"
#include "SubHullBoundaryComponent.h"
#include "Engine/DamageEvents.h"
#include "SubmarineBase.h"
#include "SubCrewMovementComponent.h"
#include "SubFloodComponent.h"
#include "SubHullComponent.h"
#include "SubInteriorFrameComponent.h"
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

	UnderwaterPP = CreateDefaultSubobject<UCrewUnderwaterPPComponent>(TEXT("UnderwaterPP"));

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
		// Env axis: capsule reports overlap on compartment / hull-boundary probes.
		Capsule->SetCollisionResponseToChannel(ECC_CompartmentProbe, ECR_Overlap);
	}

	GetCharacterMovement()->MaxWalkSpeed = 300.f;
	GetCharacterMovement()->JumpZVelocity = 0.f;
	GetCharacterMovement()->GravityScale = 1.f;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bEnablePhysicsInteraction = false;
	// bIgnoreBaseRotation is toggled per-tick by USubCrewMovementComponent based
	// on grid authority / submarine binding. This keeps stock based-rotation for
	// any unrelated moving base in the world.
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

	// Hand the designer-configured PP material to the underwater component (if one was set
	// on the BP class default). The component itself handles creation of its PostProcessComponent.
	if (UnderwaterPP && DefaultUnderwaterPPMaterial)
	{
		UnderwaterPP->UnderwaterPostProcessMaterial = DefaultUnderwaterPPMaterial;
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->OnComponentBeginOverlap.AddDynamic(this, &ASubCrewCharacter::OnCompartmentOverlapBegin);
		Capsule->OnComponentEndOverlap.AddDynamic(this, &ASubCrewCharacter::OnCompartmentOverlapEnd);

		// Overlap delegates fire only on transitions. At BeginPlay the capsule may already
		// be overlapping a compartment volume (spawn inside MainDeck) — OnBeginOverlap
		// already fired before we bound, so seed ActiveCompartmentOverlaps manually.
		TArray<UPrimitiveComponent*> OverlappingComponents;
		Capsule->GetOverlappingComponents(OverlappingComponents);
		for (UPrimitiveComponent* Comp : OverlappingComponents)
		{
			if (UCompartmentVolumeComponent* Vol = Cast<UCompartmentVolumeComponent>(Comp))
			{
				ActiveCompartmentOverlaps.Add(Vol);
			}
		}
		if (ActiveCompartmentOverlaps.Num() > 0)
		{
			RecomputeCurrentCompartment();
		}
	}
}

void ASubCrewCharacter::OnCompartmentOverlapBegin(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* /*OtherActor*/,
	UPrimitiveComponent* OtherComp,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (UCompartmentVolumeComponent* Vol = Cast<UCompartmentVolumeComponent>(OtherComp))
	{
		ActiveCompartmentOverlaps.Add(Vol);
		RecomputeCurrentCompartment();
	}
}

void ASubCrewCharacter::OnCompartmentOverlapEnd(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* /*OtherActor*/,
	UPrimitiveComponent* OtherComp,
	int32 /*OtherBodyIndex*/)
{
	if (UCompartmentVolumeComponent* Vol = Cast<UCompartmentVolumeComponent>(OtherComp))
	{
		ActiveCompartmentOverlaps.Remove(Vol);
		RecomputeCurrentCompartment();
	}
}

void ASubCrewCharacter::RecomputeCurrentCompartment()
{
	const FVector CapCenter = GetActorLocation();
	UCompartmentVolumeComponent* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<UCompartmentVolumeComponent>& W : ActiveCompartmentOverlaps)
	{
		UCompartmentVolumeComponent* V = W.Get();
		if (!V)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(V->GetComponentLocation(), CapCenter);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = V;
		}
	}

	UCompartmentVolumeComponent* Prev = CurrentCompartment.Get();
	if (Prev != Best)
	{
		CurrentCompartment = Best;
		// Keep the replicated id in sync so peer clients resolve the same compartment.
		CurrentCompartmentId = Best ? Best->CompartmentId : NAME_None;
		UE_LOG(
			LogSubCrew,
			Log,
			TEXT("CurrentCompartment: %s -> %s"),
			Prev ? *Prev->CompartmentId.ToString() : TEXT("<null>"),
			Best ? *Best->CompartmentId.ToString() : TEXT("<null>"));
	}
}

void ASubCrewCharacter::OnRep_CurrentCompartmentId()
{
	// Non-owning client path: resolve the compartment pointer from the replicated id
	// by iterating the current sub's compartment volume children.
	UCompartmentVolumeComponent* Resolved = nullptr;
	if (CurrentCompartmentId != NAME_None && CurrentSubmarine)
	{
		TArray<UCompartmentVolumeComponent*> Volumes;
		CurrentSubmarine->GetComponents<UCompartmentVolumeComponent>(Volumes);
		for (UCompartmentVolumeComponent* Vol : Volumes)
		{
			if (Vol && Vol->CompartmentId == CurrentCompartmentId)
			{
				Resolved = Vol;
				break;
			}
		}
	}
	CurrentCompartment = Resolved;
}

bool ASubCrewCharacter::IsInWater() const
{
	if (!CurrentCompartment.IsValid())
	{
		// Ocean = infiniment inondé.
		return true;
	}
	// In-compartment : reuse the already-computed immersion (UpdateEnvironmentalEffects).
	// Proper local-Z comparison vs compartment WaterHeightCm is a post-FP refinement.
	return CurrentWaterImmersion01 > 0.01f;
}

bool ASubCrewCharacter::HasOxygen() const
{
	const UCompartmentVolumeComponent* Vol = CurrentCompartment.Get();
	// FP stub : ocean = no oxygen, in-compartment = O2Level01 (stubbed to 1.0 until life-support sim).
	return Vol != nullptr && Vol->O2Level01 > 0.f;
}

void ASubCrewCharacter::HandleHullCrossing(USubHullBoundaryComponent* Boundary, bool bOutgoing)
{
	USubCrewMovementComponent* CrewMov = GetCrewMovement();
	ASubmarineBase* Sub = CurrentSubmarine;
	if (!CrewMov || !Sub || !Sub->InteriorFrame)
	{
		return;
	}

	const FTransform SubXf = Sub->InteriorFrame->GetSubTransform();
	const FVector V_sub_world = Sub->SubMovement ? Sub->SubMovement->Velocity : FVector::ZeroVector;
	const FVector V_crew_world = CrewMov->Velocity;

	if (bOutgoing)
	{
		// Embarked -> Outside: the capsule is already at the correct world pose (rebase put it there).
		// Inject the sub's transport velocity so the crew keeps world momentum continuously.
		CrewMov->Velocity = V_crew_world + V_sub_world;
		CrewMov->SetEmbarkState(ECrewEmbarkState::Outside);
		CurrentCompartment = nullptr;

		// FP EVA: no ocean water-volume in the level yet, so force Flying + zero gravity. Once
		// a proper PhysicsVolume (water=true) is added, swap this for MOVE_Swimming with buoyancy.
		CrewMov->SetMovementMode(MOVE_Flying);
		CrewMov->GravityScale = 0.f;

		// Mark the handoff event so FSavedMove_SubCrew captures it into the next move packet;
		// the server mirrors the state flip on receive even if its own boundary missed the crossing.
		CrewMov->SetPendingHandoff(ECrewHandoffKind::Outgoing);
	}
	else
	{
		// Outside -> Embarked: seed GridSpaceTransform from the current world pose and subtract
		// sub velocity so the local-frame velocity reads as "crew motion relative to sub".
		const FVector LocalPos = SubXf.InverseTransformPosition(GetActorLocation());
		const float LocalYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw - SubXf.Rotator().Yaw);
		CrewMov->GridSpaceTransform = FTransform(FRotator(0.f, LocalYaw, 0.f).Quaternion(), LocalPos);
		CrewMov->LastSubWorldTransform = SubXf;
		CrewMov->Velocity = V_crew_world - V_sub_world;
		CrewMov->SetEmbarkState(ECrewEmbarkState::Embarked);
		// CurrentCompartment is updated by the compartment overlap system when the capsule
		// reaches a UCompartmentVolumeComponent. The boundary's InsideCompartmentId is a hint
		// but not the authority.

		// Restore walking + gravity so the crew lands on the sub floor.
		CrewMov->SetMovementMode(MOVE_Walking);
		CrewMov->GravityScale = 1.f;

		// Mark the handoff event so the server converges on the Outside->Embarked state.
		CrewMov->SetPendingHandoff(ECrewHandoffKind::Incoming);
	}

	UE_LOG(
		LogSubCrew,
		Log,
		TEXT("HullCrossing | Boundary=%s | bOutgoing=%d | V_sub=%s | V_new=%s"),
		Boundary ? *Boundary->GetName() : TEXT("<null>"),
		bOutgoing ? 1 : 0,
		*V_sub_world.ToCompactString(),
		*CrewMov->Velocity.ToCompactString());
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
	// Owner's overlap handlers populate CurrentCompartmentId locally; non-owning clients get
	// the replicated server value and resolve CurrentCompartment pointer in OnRep.
	DOREPLIFETIME_CONDITION(ASubCrewCharacter, CurrentCompartmentId, COND_SkipOwner);
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
	else
	{
		CurrentCompartment = nullptr;
		CurrentCompartmentId = NAME_None;
		ActiveCompartmentOverlaps.Reset();
		ResetEnvironmentalState();

		if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
		{
			CrewMov->SetEmbarkState(ECrewEmbarkState::Outside);
		}
	}
}

void ASubCrewCharacter::SetCurrentSubmarine(ASubmarineBase* Sub)
{
	if (CurrentSubmarine == Sub)
	{
		return;
	}

	// Forbid silent unbind. The only legitimate path that clears CurrentSubmarine is
	// DisembarkSubmarine, which raises bAllowSubmarineUnbind via TGuardValue. Any other
	// caller hitting nullptr here is an upstream bug (replication race, accidental BP wire,
	// sub destruction without disembark) and the ensure surfaces it immediately.
	ensureMsgf(Sub != nullptr || bAllowSubmarineUnbind,
		TEXT("SetCurrentSubmarine(nullptr) called outside DisembarkSubmarine. ")
		TEXT("Crew=%s | PrevSub=%s. Use DisembarkSubmarine for explicit unbind."),
		*GetName(), *GetNameSafe(CurrentSubmarine));

	CurrentSubmarine = Sub;

	if (!CurrentSubmarine)
	{
		CurrentCompartment = nullptr;
		CurrentCompartmentId = NAME_None;
		ActiveCompartmentOverlaps.Reset();
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

	bool bHasSubmarineBinding = false;
	if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
	{
		CrewMov->InitializeForSubmarine();
		CrewMov->RefreshEmbarkedFlooring();
		bHasSubmarineBinding = CrewMov->HasSubmarineBinding();

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
				bHasSubmarineBinding = CrewMov->HasSubmarineBinding();

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

		// Seed GridSpaceTransform from the final world pose (post floor-snap) so
		// the first rebase is a no-op and the extract loop starts coherent.
		if (Sub)
		{
			const FTransform SubTransform = Sub->GetActorTransform();
			const FVector LocalPos = SubTransform.InverseTransformPosition(GetActorLocation());
			const FRotator WorldRot = GetActorRotation();
			const float LocalYaw = FRotator::NormalizeAxis(WorldRot.Yaw - SubTransform.Rotator().Yaw);
			CrewMov->GridSpaceTransform = FTransform(
				FRotator(0.f, LocalYaw, 0.f).Quaternion(),
				LocalPos);
			CrewMov->SetEmbarkState(ECrewEmbarkState::Embarked);
			CrewMov->LastSubWorldTransform = SubTransform;
			bHasSubmarineBinding = CrewMov->HasSubmarineBinding();
		}
	}

	UE_LOG(
		LogSubCrew,
		Log,
		TEXT("Post-init | MovementMode=%d | Base=%s | SubBound=%d | EmbarkState=%d"),
		static_cast<int32>(GetCharacterMovement()->MovementMode),
		*DescribeMovementBase(this),
		bHasSubmarineBinding ? 1 : 0,
		GetCrewMovement() ? static_cast<int32>(GetCrewMovement()->EmbarkState) : static_cast<int32>(ECrewEmbarkState::Outside));

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

	UE_LOG(
		LogSubCrew,
		Warning,
		TEXT("BoardSubmarine is legacy compatibility code. Forwarding to EnterOnFootInSubmarine."));
	EnterOnFootInSubmarine(Submarine, FTransform(GetActorRotation(), GetActorLocation()));
}

void ASubCrewCharacter::DisembarkSubmarine()
{
	UE_LOG(
		LogSubCrew,
		Warning,
		TEXT("DisembarkSubmarine is a legacy hard-detach path. EVA should use hull boundary crossing."));

	ASubmarineBase* PreviousSubmarine = CurrentSubmarine;

	// Before clearing the sub pointer, flush GridSpaceTransform into world pose
	// and inherit sub velocity so the disembark is physically continuous.
	if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(GetCharacterMovement()))
	{
		if (CrewMov->IsGridAuthoritative() && PreviousSubmarine)
		{
			const FTransform SubTransform = PreviousSubmarine->GetActorTransform();
			const FVector WorldPos = SubTransform.TransformPosition(CrewMov->GridSpaceTransform.GetLocation());
			const FRotator LocalRot = CrewMov->GridSpaceTransform.Rotator();
			const FRotator WorldRot(0.f, FRotator::NormalizeAxis(SubTransform.Rotator().Yaw + LocalRot.Yaw), 0.f);
			SetActorLocationAndRotation(WorldPos, WorldRot, false, nullptr, ETeleportType::TeleportPhysics);

			if (USubMovementComponent* SubMov = PreviousSubmarine->SubMovement)
			{
				CrewMov->Velocity = SubMov->Velocity;
			}
		}
		CrewMov->SetEmbarkState(ECrewEmbarkState::Outside);
	}

	// Env axis: clear the compartment pointer so IsInWater()/HasOxygen() report ocean state.
	// Overlap events will repopulate if the disembark leaves the crew inside a compartment volume.
	CurrentCompartment = nullptr;
	ActiveCompartmentOverlaps.Reset();

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	{
		// Authorise the nullptr unbind for this single call: DisembarkSubmarine is the
		// only legitimate path that clears CurrentSubmarine. The ensure in
		// SetCurrentSubmarine fires for any caller that reaches it without this guard.
		TGuardValue<bool> AllowUnbindGuard(bAllowSubmarineUnbind, true);
		SetCurrentSubmarine(nullptr);
	}
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

	if (GetDefault<USub3DDebugSettings>()->bLogCrewEnvironmentState && CurrentSubmarine)
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
