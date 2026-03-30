#include "SubmarineAlarmBeacon.h"

#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Sound/SoundBase.h"

ASubmarineAlarmBeacon::ASubmarineAlarmBeacon()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BeaconMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconMesh"));
	BeaconMesh->SetupAttachment(SceneRoot);
	BeaconMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconMesh->SetCanEverAffectNavigation(false);

	AlarmLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("AlarmLight"));
	AlarmLight->SetupAttachment(SceneRoot);
	AlarmLight->SetIntensity(IdleLightIntensity);
	AlarmLight->SetLightColor(ActiveLightColor.ToFColor(true));
	AlarmLight->SetCastShadows(false);
	AlarmLight->SetVisibility(false);

	AlarmAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AlarmAudio"));
	AlarmAudio->SetupAttachment(SceneRoot);
	AlarmAudio->SetAutoActivate(false);
	AlarmAudio->bAutoActivate = false;
	AlarmAudio->bIsUISound = false;

	RotatingMovement = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovement"));
	RotatingMovement->RotationRate = FRotator::ZeroRotator;
}

void ASubmarineAlarmBeacon::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisualState(false, 0.f);
}

void ASubmarineAlarmBeacon::SetAlarmState(
	bool bActive,
	float Severity01,
	USoundBase* FallbackSound,
	int32 TriggerPeriodSeconds,
	float GainInput,
	FName TriggerPeriodParameter,
	FName GainParameter)
{
	const float ClampedSeverity01 = FMath::Clamp(Severity01, 0.f, 1.f);
	const bool bStateChanged = bAlarmActive != bActive;
	bAlarmActive = bActive;

	ApplyVisualState(bActive, ClampedSeverity01);

	if (!AlarmAudio)
	{
		return;
	}

	USoundBase* DesiredSound = AlarmSoundOverride ? AlarmSoundOverride.Get() : FallbackSound;
	if (!bActive || !DesiredSound)
	{
		if (AlarmAudio->IsPlaying())
		{
			AlarmAudio->Stop();
		}
		return;
	}

	if (AlarmAudio->Sound != DesiredSound)
	{
		AlarmAudio->SetSound(DesiredSound);
	}

	if (!TriggerPeriodParameter.IsNone())
	{
		AlarmAudio->SetIntParameter(TriggerPeriodParameter, FMath::Max(1, TriggerPeriodSeconds));
	}

	if (!GainParameter.IsNone())
	{
		AlarmAudio->SetFloatParameter(GainParameter, GainInput);
	}

	if (!AlarmAudio->IsPlaying() || bStateChanged)
	{
		AlarmAudio->Play();
	}
}

void ASubmarineAlarmBeacon::ApplyVisualState(bool bActive, float Severity01)
{
	if (AlarmLight)
	{
		const float LightIntensity = bActive
			? FMath::Lerp(ActiveLightIntensity * 0.35f, ActiveLightIntensity, Severity01)
			: IdleLightIntensity;
		AlarmLight->SetVisibility(bUseAlarmLight && bActive);
		AlarmLight->SetIntensity(LightIntensity);
		AlarmLight->SetLightColor(ActiveLightColor.ToFColor(true));
	}

	if (RotatingMovement)
	{
		RotatingMovement->RotationRate = (bActive && bRotateWhenActive)
			? ActiveRotationRate
			: FRotator::ZeroRotator;
	}
}
