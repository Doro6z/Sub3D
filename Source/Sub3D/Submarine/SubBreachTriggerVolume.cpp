#include "SubBreachTriggerVolume.h"

#include "Components/BoxComponent.h"
#include "SubGameMode.h"
#include "SubmarineBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubBreachTrigger, Log, All);

ASubBreachTriggerVolume::ASubBreachTriggerVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	SetRootComponent(TriggerVolume);
	TriggerVolume->SetBoxExtent(FVector(500.f, 500.f, 500.f));
	TriggerVolume->SetCollisionProfileName(TEXT("Trigger"));
	TriggerVolume->SetGenerateOverlapEvents(true);
}

void ASubBreachTriggerVolume::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerVolume)
	{
		TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ASubBreachTriggerVolume::HandleTriggerBeginOverlap);
	}
}

void ASubBreachTriggerVolume::HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || (bConsumeAfterTrigger && bTriggered))
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
		UE_LOG(LogSubBreachTrigger, Warning, TEXT("Breach trigger overlap ignored | No SubGameMode available"));
		return;
	}

	if (SubGameMode->TriggerBreachEvent())
	{
		bTriggered = true;

		if (bConsumeAfterTrigger && TriggerVolume)
		{
			TriggerVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		UE_LOG(
			LogSubBreachTrigger,
			Log,
			TEXT("Breach trigger consumed | Trigger=%s | Submarine=%s"),
			*GetName(),
			*GetNameSafe(OverlappingSubmarine));
	}
}
