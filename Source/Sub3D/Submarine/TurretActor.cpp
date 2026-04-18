#include "TurretActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ATurretActor::ATurretActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	YawPivot = CreateDefaultSubobject<USceneComponent>(TEXT("YawPivot"));
	YawPivot->SetupAttachment(Root);

	PitchPivot = CreateDefaultSubobject<USceneComponent>(TEXT("PitchPivot"));
	PitchPivot->SetupAttachment(YawPivot);

	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(PitchPivot);
	TurretMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void ATurretActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		CurrentAmmo = MaxAmmo;
	}
	ApplyAimVisuals();
}

void ATurretActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	CurrentAim = FMath::RInterpTo(CurrentAim, TargetAim, DeltaSeconds, AimInterpSpeed);
	ApplyAimVisuals();

	if (!HasAuthority() || !bOnline)
	{
		return;
	}

	CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaSeconds);
	if (bFireHeld)
	{
		TryFire();
	}
}

void ATurretActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATurretActor, CurrentAim);
	DOREPLIFETIME(ATurretActor, TargetAim);
	DOREPLIFETIME(ATurretActor, bFireHeld);
	DOREPLIFETIME(ATurretActor, bOnline);
	DOREPLIFETIME(ATurretActor, CurrentAmmo);
	DOREPLIFETIME(ATurretActor, LastFireServerTime);
}

void ATurretActor::OnRep_LastFireTime()
{
	BP_OnFiredReplicated();
}

void ATurretActor::SetAimCommand(const FRotator& InAim)
{
	TargetAim.Pitch = FMath::ClampAngle(InAim.Pitch, -60.f, 60.f);
	TargetAim.Yaw = InAim.Yaw;
	TargetAim.Roll = 0.f;
}

void ATurretActor::SetFireHeld(bool bHeld)
{
	bFireHeld = bHeld;
}

void ATurretActor::SetOnline(bool bInOnline)
{
	bOnline = bInOnline;
}

void ATurretActor::ApplyAimVisuals()
{
	if (YawPivot)
	{
		YawPivot->SetRelativeRotation(FRotator(0.f, CurrentAim.Yaw, 0.f));
	}
	if (PitchPivot)
	{
		PitchPivot->SetRelativeRotation(FRotator(CurrentAim.Pitch, 0.f, 0.f));
	}
}

void ATurretActor::TryFire()
{
	if (CooldownRemaining > 0.f || !GetWorld() || CurrentAmmo <= 0)
	{
		return;
	}

	CooldownRemaining = FireCooldown;
	CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
	LastFireServerTime = GetWorld()->GetTimeSeconds();

	const FVector Start = PitchPivot ? PitchPivot->GetComponentLocation() : GetActorLocation();
	const FVector Direction = PitchPivot ? PitchPivot->GetForwardVector() : GetActorForwardVector();
	const FVector End = Start + Direction * FireRange;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SubTurretFire), false, this);

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			UGameplayStatics::ApplyPointDamage(HitActor, DamagePerShot, Direction, Hit, nullptr, this, nullptr);
		}
	}

	DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.15f, 0, 1.5f);
	BP_OnFired();
}
