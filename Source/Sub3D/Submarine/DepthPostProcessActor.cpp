#include "DepthPostProcessActor.h"
#include "SubmarineBase.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"

ADepthPostProcessActor::ADepthPostProcessActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ADepthPostProcessActor::BeginPlay()
{
	Super::BeginPlay();
}

void ADepthPostProcessActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!TrackedSubmarine || !DepthFog || !SunLight)
	{
		return;
	}

	// Depth is -Z (since Z=0 is surface and sub goes down)
	float Depth = -TrackedSubmarine->GetActorLocation().Z;
	float T = FMath::Clamp(Depth / MaxDepthForPP, 0.f, 1.f);

	// Update Fog
	UExponentialHeightFogComponent* FogComp = DepthFog->GetComponent();
	if (FogComp)
	{
		FogComp->SetFogDensity(FMath::Lerp(0.005f, 0.05f, T));
		FogComp->SetFogInscatteringColor(FMath::Lerp(ShallowColor, DeepColor, T));
	}

	// Update Sun Intensity
	UDirectionalLightComponent* LightComp = SunLight->GetComponent();
	if (LightComp)
	{
		LightComp->SetIntensity(FMath::Lerp(2.0f, 0.0f, T));
	}
}
