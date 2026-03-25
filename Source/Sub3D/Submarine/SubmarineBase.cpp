#include "SubmarineBase.h"

#include "Net/UnrealNetwork.h"
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

	HelmSocket = CreateDefaultSubobject<USceneComponent>(TEXT("HelmSocket"));
	HelmSocket->SetupAttachment(HullMesh);
	HelmSocket->SetRelativeLocation(FVector(-200.f, 0.f, 50.f));

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

	if (HullMesh)
	{
		HullMesh->OnComponentHit.AddDynamic(this, &ASubmarineBase::OnHullHit);
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

void ASubmarineBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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

float ASubmarineBase::GetTotalFloodWaterMassKg() const
{
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

void ASubmarineBase::OnRep_RepState()
{
	if (SubMovement)
	{
		SubMovement->HandleReplicatedNetState(RepState);
	}
}
