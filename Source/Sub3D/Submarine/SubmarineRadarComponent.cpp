#include "SubmarineRadarComponent.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

USubmarineRadarComponent::USubmarineRadarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void USubmarineRadarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	TimeSinceLastPing += DeltaTime;
	if (TimeSinceLastPing >= PingIntervalSeconds)
	{
		PerformPing();
		TimeSinceLastPing = 0.f;
	}
}

void USubmarineRadarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USubmarineRadarComponent, Contacts);
	DOREPLIFETIME(USubmarineRadarComponent, TimeSinceLastPing);
}

void USubmarineRadarComponent::ForcePing()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PerformPing();
		TimeSinceLastPing = 0.f;
	}
}

void USubmarineRadarComponent::PerformPing()
{
	Contacts.Reset();

	if (!GetWorld() || !GetOwner())
	{
		return;
	}

	const FVector Origin = GetOwner()->GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjParams;
	if (bDetectWorldStatic)
	{
		ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	}
	if (bDetectDynamicActors)
	{
		ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SubRadarPing), false, GetOwner());
	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		ObjParams,
		FCollisionShape::MakeSphere(PingRadiusCm),
		Params
	);

	for (const FOverlapResult& Result : Overlaps)
	{
		const AActor* Actor = Result.GetActor();
		if (!ShouldIncludeActor(Actor))
		{
			continue;
		}

		FRadarContact Contact;
		Contact.LocalPosition = GetOwner()->GetActorTransform().InverseTransformPosition(Actor->GetActorLocation());
		const float DistNorm = FMath::Clamp(Contact.LocalPosition.Size() / FMath::Max(1.f, PingRadiusCm), 0.f, 1.f);
		Contact.Strength01 = 1.f - DistNorm;
		Contact.Category = Actor->IsA<APawn>() ? 2 : 1;
		Contact.bHostile = Actor->ActorHasTag(FName(TEXT("Hostile")));
		Contacts.Add(Contact);
	}
}

bool USubmarineRadarComponent::ShouldIncludeActor(const AActor* Actor) const
{
	return Actor && Actor != GetOwner();
}
