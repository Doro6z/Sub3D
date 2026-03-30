#include "SubmarineFloodAudioAnchor.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Sound/SoundBase.h"

ASubmarineFloodAudioAnchor::ASubmarineFloodAudioAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FloodAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("FloodAudio"));
	FloodAudio->SetupAttachment(SceneRoot);
	FloodAudio->SetAutoActivate(false);
	FloodAudio->bAutoActivate = false;
	FloodAudio->bIsUISound = false;
}

void ASubmarineFloodAudioAnchor::SetFloodState(
	bool bActive,
	float WaterLevel01,
	float Turbulence01,
	float GainInput,
	USoundBase* FallbackSound,
	FName WaterLevelParameter,
	FName TurbulenceParameter,
	FName GainParameter)
{
	if (!FloodAudio)
	{
		return;
	}

	USoundBase* DesiredSound = FloodSoundOverride ? FloodSoundOverride.Get() : FallbackSound;
	if (!bActive || !DesiredSound)
	{
		if (FloodAudio->IsPlaying())
		{
			FloodAudio->Stop();
		}
		return;
	}

	if (FloodAudio->Sound != DesiredSound)
	{
		FloodAudio->SetSound(DesiredSound);
	}

	if (!WaterLevelParameter.IsNone())
	{
		FloodAudio->SetFloatParameter(WaterLevelParameter, WaterLevel01);
	}

	if (!TurbulenceParameter.IsNone())
	{
		FloodAudio->SetFloatParameter(TurbulenceParameter, Turbulence01);
	}

	if (!GainParameter.IsNone())
	{
		FloodAudio->SetFloatParameter(GainParameter, GainInput);
	}

	if (!FloodAudio->IsPlaying())
	{
		FloodAudio->Play();
	}
}
