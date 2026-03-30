#include "SubmarineBase.h"

#include "BreachVfxManagerComponent.h"
#include "FloodWaterVisualsComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/DamageEvents.h"
#include "Net/UnrealNetwork.h"
#include "SubmarineFeedbackDirectorComponent.h"
#include "SubSonarComponent.h"
#include "SubSonarSystemComponent.h"
#include "SubDoorActor.h"
#include "SubHullComponent.h"
#include "SubMovementComponent.h"
#include "SubmarineCompartmentComponent.h"
#include "SubmarineRadarComponent.h"
#include "SubmarineStationManagerComponent.h"
#include "SubmarineSystemsComponent.h"
#include "SubInteriorFrameComponent.h"
#include "TurretActor.h"

ASubmarineBase::ASubmarineBase()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(30.f);
	SetMinNetUpdateFrequency(15.f);

	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	SetRootComponent(HullMesh);
	ApplyHullCollisionDefaults();

	SubMovement = CreateDefaultSubobject<USubMovementComponent>(TEXT("SubMovement"));
	SubHull = CreateDefaultSubobject<USubHullComponent>(TEXT("SubHull"));
	Systems = CreateDefaultSubobject<USubmarineSystemsComponent>(TEXT("Systems"));
	Compartments = CreateDefaultSubobject<USubmarineCompartmentComponent>(TEXT("Compartments"));
	StationManager = CreateDefaultSubobject<USubmarineStationManagerComponent>(TEXT("StationManager"));
	Radar = CreateDefaultSubobject<USubmarineRadarComponent>(TEXT("Radar"));
	InteriorFrame = CreateDefaultSubobject<USubInteriorFrameComponent>(TEXT("InteriorFrame"));
	BreachVfxManager = CreateDefaultSubobject<UBreachVfxManagerComponent>(TEXT("BreachVfxManager"));
	FloodWaterVisuals = CreateDefaultSubobject<UFloodWaterVisualsComponent>(TEXT("FloodWaterVisuals"));
	FeedbackManager = CreateDefaultSubobject<USubmarineFeedbackDirectorComponent>(TEXT("FeedbackManager"));
	Sonar = CreateDefaultSubobject<USubSonarComponent>(TEXT("Sonar"));
	SonarSystem = CreateDefaultSubobject<USubSonarSystemComponent>(TEXT("SonarSystem"));

	HelmSocket = CreateDefaultSubobject<USceneComponent>(TEXT("HelmSocket"));
	HelmSocket->SetupAttachment(HullMesh);
	HelmSocket->SetRelativeLocation(FVector(-200.f, 0.f, 50.f));

	CrewSpawnSocketP1 = CreateDefaultSubobject<USceneComponent>(TEXT("CrewSpawnSocketP1"));
	CrewSpawnSocketP1->SetupAttachment(HullMesh);
	CrewSpawnSocketP1->SetRelativeLocation(FVector(-350.f, 0.f, 92.f));

	TurretHardpoint = CreateDefaultSubobject<USceneComponent>(TEXT("TurretHardpoint"));
	TurretHardpoint->SetupAttachment(HullMesh);
	TurretHardpoint->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
}

void ASubmarineBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyHullCollisionDefaults();
}

void ASubmarineBase::BeginPlay()
{
	Super::BeginPlay();

	ApplyHullCollisionDefaults();
	RefreshMovementCollisionBinding();

	if (StationManager)
	{
		StationManager->DiscoverAttachedStations();
	}

	if (HasAuthority())
	{
		ResolveExteriorTurret();
	}
}

void ASubmarineBase::ApplyHullCollisionDefaults()
{
	if (!HullMesh)
	{
		return;
	}

	HullMesh->SetCollisionProfileName(TEXT("SubmarineHull"));
	HullMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HullMesh->SetNotifyRigidBodyCollision(true);
	HullMesh->SetGenerateOverlapEvents(false);
	HullMesh->SetCanEverAffectNavigation(false);
	HullMesh->SetMobility(EComponentMobility::Movable);
	HullMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

UPrimitiveComponent* ASubmarineBase::GetMovementCollisionComponent() const
{
	return HullMesh;
}

void ASubmarineBase::RefreshMovementCollisionBinding()
{
	if (BoundMovementCollisionComponent.IsValid())
	{
		BoundMovementCollisionComponent->OnComponentHit.RemoveDynamic(this, &ASubmarineBase::OnHullHit);
		BoundMovementCollisionComponent.Reset();
	}

	if (UPrimitiveComponent* CollisionComponent = GetMovementCollisionComponent())
	{
		CollisionComponent->OnComponentHit.RemoveDynamic(this, &ASubmarineBase::OnHullHit);
		CollisionComponent->OnComponentHit.AddUniqueDynamic(this, &ASubmarineBase::OnHullHit);
		BoundMovementCollisionComponent = CollisionComponent;
	}
}

bool ASubmarineBase::CreateDebugBreachOnFirstExteriorSheet(float DamageAmount)
{
	if (!HasAuthority() || DamageAmount <= 0.f || !SubHull)
	{
		return false;
	}

	const FStructuralSheetDef* TargetSheet = SubHull->GetStructuralSheets().FindByPredicate([](const FStructuralSheetDef& Sheet)
	{
		return Sheet.bCanOpenToExterior;
	});

	if (!TargetSheet && SubHull->GetStructuralSheets().Num() > 0)
	{
		TargetSheet = &SubHull->GetStructuralSheets()[0];
	}

	if (!TargetSheet)
	{
		return false;
	}

	const FVector WorldHitPoint = GetActorTransform().TransformPosition(TargetSheet->LocalOrigin);
	FPointDamageEvent DamageEvent;
	DamageEvent.HitInfo.bBlockingHit = true;
	DamageEvent.HitInfo.ImpactPoint = WorldHitPoint;
	DamageEvent.HitInfo.Location = WorldHitPoint;

	return TakeDamage(DamageAmount, DamageEvent, nullptr, this) > 0.f;
}

void ASubmarineBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

float ASubmarineBase::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!HasAuthority() || DamageAmount <= 0.f || !SubHull)
	{
		return 0.f;
	}

	FVector WorldHitPoint = GetActorLocation();

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* PointDamageEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
		if (PointDamageEvent)
		{
			if (PointDamageEvent->HitInfo.bBlockingHit)
			{
				if (!PointDamageEvent->HitInfo.ImpactPoint.IsNearlyZero())
				{
					WorldHitPoint = PointDamageEvent->HitInfo.ImpactPoint;
				}
				else if (!PointDamageEvent->HitInfo.Location.IsNearlyZero())
				{
					WorldHitPoint = PointDamageEvent->HitInfo.Location;
				}
			}
			else if (!PointDamageEvent->HitInfo.ImpactPoint.IsNearlyZero())
			{
				WorldHitPoint = PointDamageEvent->HitInfo.ImpactPoint;
			}
			else if (!PointDamageEvent->HitInfo.Location.IsNearlyZero())
			{
				WorldHitPoint = PointDamageEvent->HitInfo.Location;
			}
		}
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		const FRadialDamageEvent* RadialDamageEvent = static_cast<const FRadialDamageEvent*>(&DamageEvent);
		if (RadialDamageEvent)
		{
			WorldHitPoint = RadialDamageEvent->Origin;
		}
	}

	const FVector LocalHitPosition = GetActorTransform().InverseTransformPosition(WorldHitPoint);
	SubHull->ApplyHullImpact(LocalHitPosition, DamageAmount, HullWeaponDamageRadiusCm);
	if (FeedbackManager)
	{
		FeedbackManager->DispatchHullImpactFeedback(WorldHitPoint, DamageAmount, HullWeaponDamageRadiusCm);
	}
	return DamageAmount;
}

void ASubmarineBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASubmarineBase, CurrentPilot);
	DOREPLIFETIME(ASubmarineBase, RepState);
	DOREPLIFETIME(ASubmarineBase, ExteriorTurret);
}

void ASubmarineBase::SetPilot(AActor* NewPilot)
{
	if (HasAuthority())
	{
		CurrentPilot = NewPilot;
	}
}

void ASubmarineBase::ClearPilot()
{
	if (HasAuthority())
	{
		CurrentPilot = nullptr;
	}
}

void ASubmarineBase::OnHullHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	const float ImpactForce = NormalImpulse.Size();
	const float Damage = ImpactForce * HullImpactDamageScale;

	if (Damage < 1.f)
	{
		return;
	}

	if (SubHull)
	{
		const FVector LocalHitPosition = GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
		SubHull->ApplyHullImpact(LocalHitPosition, Damage, HullImpactRadiusCm);
	}

	if (FeedbackManager)
	{
		FeedbackManager->DispatchHullImpactFeedback(Hit.ImpactPoint, Damage, HullImpactRadiusCm);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			3.f,
			FColor::Red,
			FString::Printf(TEXT("HULL HIT - %.0f damage"), Damage));
	}
}

void ASubmarineBase::RefreshRepState()
{
	RepState.WorldLocation = GetActorLocation();
	RepState.QuantizedRotation = GetActorRotation();

	if (SubMovement)
	{
		RepState.LinearVelocity = SubMovement->Velocity;
		RepState.AngularVelocity = FVector::ZeroVector;
		RepState.ForwardSpeed = FVector::DotProduct(SubMovement->Velocity, GetActorForwardVector());
		RepState.VerticalSpeed = SubMovement->Velocity.Z;
		RepState.DepthMeters = SubMovement->CurrentDepth;
		RepState.FloodedMassKg = SubMovement->FloodedMassKg;
		RepState.BallastGlobal01 = SubMovement->GlobalTargetFill;
		RepState.SimFrame = SubMovement->GetSimFrameCounter();
	}

	if (Systems)
	{
		RepState.MainTrim01 = Systems->GetCommandState().MainTrimBiasCmd;
		RepState.bPumpActive = Systems->GetCommandState().bPumpActive;
	}
}

float ASubmarineBase::GetCurrentDepthMeters() const
{
	return SubMovement ? SubMovement->CurrentDepth : 0.f;
}

FTransform ASubmarineBase::GetPrimaryCrewSpawnTransform() const
{
	if (IsValid(CrewSpawnSocketP1))
	{
		return CrewSpawnSocketP1->GetComponentTransform();
	}

	if (IsValid(HelmSocket))
	{
		return HelmSocket->GetComponentTransform();
	}

	return GetActorTransform();
}

float ASubmarineBase::GetTotalFloodWaterMassKg() const
{
	if (SubHull)
	{
		return SubHull->GetTotalWaterLiters();
	}

	return Compartments ? Compartments->GetTotalWaterMassKg() : 0.f;
}

void ASubmarineBase::ResolveExteriorTurret()
{
	if (ExteriorTurret)
	{
		return;
	}

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for (AActor* Actor : AttachedActors)
	{
		if (ATurretActor* Turret = Cast<ATurretActor>(Actor))
		{
			ExteriorTurret = Turret;
			return;
		}
	}
}

void ASubmarineBase::SetFreezeMovementForTesting(bool bFreeze)
{
	if (!HasAuthority())
	{
		return;
	}

	bFreezeMovementForTesting = bFreeze;

	if (SubMovement)
	{
		SubMovement->Velocity = FVector::ZeroVector;
	}

	RefreshRepState();
}

bool ASubmarineBase::IsMovementCollisionReady() const
{
	const UPrimitiveComponent* CollisionComp = GetMovementCollisionComponent();
	return CollisionComp && CollisionComp->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
}

bool ASubmarineBase::ValidateSpawnCollision() const
{
	const UPrimitiveComponent* CollisionComp = GetMovementCollisionComponent();
	if (!CollisionComp || CollisionComp->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		return false;
	}

	return true;
}

ASubDoorActor* ASubmarineBase::FindAttachedDoorById(FName DoorId) const
{
	if (DoorId.IsNone())
	{
		return nullptr;
	}

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		ASubDoorActor* DoorActor = Cast<ASubDoorActor>(AttachedActor);
		if (DoorActor && DoorActor->DoorId == DoorId)
		{
			return DoorActor;
		}
	}

	return nullptr;
}

void ASubmarineBase::OnRep_RepState()
{
	if (SubMovement)
	{
		SubMovement->HandleReplicatedNetState(RepState);
	}
}
