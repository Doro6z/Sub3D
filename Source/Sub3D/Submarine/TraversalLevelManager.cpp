#include "TraversalLevelManager.h"
#include "Components/BoxComponent.h"
#include "SubmarineBase.h"
#include "Kismet/GameplayStatics.h"

ATraversalLevelManager::ATraversalLevelManager()
{
	PrimaryActorTick.bCanEverTick = false;

	EndTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EndTrigger"));
	SetRootComponent(EndTrigger);
	EndTrigger->SetBoxExtent(FVector(1000.f, 1000.f, 1000.f));
	EndTrigger->SetCollisionProfileName(TEXT("Trigger"));
}

void ATraversalLevelManager::BeginPlay()
{
	Super::BeginPlay();

	// Bind the overlap event
	EndTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATraversalLevelManager::OnSubReachedEnd);

	// Start tracking time
	TraversalStartTime = GetWorld()->GetTimeSeconds();

	// Robustly find the sub (can be spawned by GameMode or placed)
	// We use a small timer or check in Tick if BeginPlay is too early.
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, [this]()
	{
		AActor* SubActor = UGameplayStatics::GetActorOfClass(this, ASubmarineBase::StaticClass());
		if (SubActor)
		{
			SubActor->SetActorTransform(SpawnTransformA);
			UE_LOG(LogTemp, Log, TEXT("TraversalLevelManager: Found sub and teleported to spawn."));
		}
	}, 0.2f, false);
}

void ATraversalLevelManager::OnSubReachedEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(ASubmarineBase::StaticClass()))
	{
		TraversalDuration = GetWorld()->GetTimeSeconds() - TraversalStartTime;

		int32 Minutes = FMath::FloorToInt(TraversalDuration / 60.f);
		int32 Seconds = FMath::FloorToInt(FMath::Fmod(TraversalDuration, 60.f));

		FString Msg = FString::Printf(TEXT("TRAVERSÉE COMPLÈTE — Durée : %d min %d sec"), Minutes, Seconds);
		
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, Msg);
		}

		UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);
	}
}
