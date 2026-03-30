#include "SubRunPhaseVolume.h"

#include "Components/BoxComponent.h"
#include "SubGameMode.h"
#include "SubmarineBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubRunVolume, Log, All);

ASubRunPhaseVolume::ASubRunPhaseVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	SetRootComponent(TriggerVolume);
	TriggerVolume->SetBoxExtent(FVector(1200.f, 1200.f, 1200.f));
	TriggerVolume->SetCollisionProfileName(TEXT("Trigger"));
	TriggerVolume->SetGenerateOverlapEvents(true);
}

void ASubRunPhaseVolume::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerVolume)
	{
		TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ASubRunPhaseVolume::HandleTriggerBeginOverlap);
		TriggerVolume->OnComponentEndOverlap.AddDynamic(this, &ASubRunPhaseVolume::HandleTriggerEndOverlap);
	}
}

void ASubRunPhaseVolume::HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || (bConsumeAfterActivation && bTriggered))
	{
		return;
	}

	ASubmarineBase* OverlappingSubmarine = Cast<ASubmarineBase>(OtherActor);
	if (!OverlappingSubmarine)
	{
		return;
	}

	ASubGameMode* SubGameMode = GetWorld() ? Cast<ASubGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
	if (!SubGameMode)
	{
		UE_LOG(LogSubRunVolume, Warning, TEXT("Run phase volume ignored | No SubGameMode available"));
		return;
	}

	switch (VolumeType)
	{
	case ESubRunVolumeType::DepartureGate:
		SubGameMode->NotifySubmarineClearedDepartureGate();
		if (bConsumeAfterActivation)
		{
			bTriggered = true;
			TriggerVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		break;

	case ESubRunVolumeType::ApproachZone:
		SubGameMode->NotifyApproachZoneStateChanged(true);
		break;

	default:
		break;
	}

	UE_LOG(
		LogSubRunVolume,
		Log,
		TEXT("Run phase volume begin overlap | Volume=%s | Type=%s | Submarine=%s"),
		*GetName(),
		*StaticEnum<ESubRunVolumeType>()->GetValueAsString(VolumeType),
		*GetNameSafe(OverlappingSubmarine));
}

void ASubRunPhaseVolume::HandleTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	if (VolumeType != ESubRunVolumeType::ApproachZone)
	{
		return;
	}

	ASubmarineBase* OverlappingSubmarine = Cast<ASubmarineBase>(OtherActor);
	if (!OverlappingSubmarine)
	{
		return;
	}

	ASubGameMode* SubGameMode = GetWorld() ? Cast<ASubGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
	if (!SubGameMode)
	{
		return;
	}

	SubGameMode->NotifyApproachZoneStateChanged(false);

	UE_LOG(
		LogSubRunVolume,
		Log,
		TEXT("Run phase volume end overlap | Volume=%s | Type=%s | Submarine=%s"),
		*GetName(),
		*StaticEnum<ESubRunVolumeType>()->GetValueAsString(VolumeType),
		*GetNameSafe(OverlappingSubmarine));
}
