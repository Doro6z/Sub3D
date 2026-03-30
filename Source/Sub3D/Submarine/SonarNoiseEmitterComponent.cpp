#include "SonarNoiseEmitterComponent.h"

#include "GameFramework/Actor.h"

USonarNoiseEmitterComponent::USonarNoiseEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float USonarNoiseEmitterComponent::GetCurrentNoiseStrength() const
{
	if (!bEmitterEnabled)
	{
		return 0.f;
	}

	const AActor* OwnerActor = GetOwner();
	const float SpeedCmS = OwnerActor ? OwnerActor->GetVelocity().Size() : 0.f;
	const float VelocityNoise = FMath::Max(0.f, SpeedCmS * VelocityNoiseScale);
	return FMath::Max(0.f, BaseNoiseStrength + VelocityNoise);
}

FVector USonarNoiseEmitterComponent::GetEmissionLocation() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
}
