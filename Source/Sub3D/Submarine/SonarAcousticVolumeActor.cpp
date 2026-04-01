#include "SonarAcousticVolumeActor.h"

#include "Components/BoxComponent.h"
#include "SonarAcousticVolumeComponent.h"

ASonarAcousticVolumeActor::ASonarAcousticVolumeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	VolumeBox = CreateDefaultSubobject<UBoxComponent>(TEXT("VolumeBox"));
	SetRootComponent(VolumeBox);
	VolumeBox->InitBoxExtent(FVector(2500.f, 2500.f, 1500.f));
	VolumeBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VolumeBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	VolumeBox->SetGenerateOverlapEvents(true);
	VolumeBox->SetCanEverAffectNavigation(false);
	VolumeBox->SetHiddenInGame(false);

	SonarAcousticVolume = CreateDefaultSubobject<USonarAcousticVolumeComponent>(TEXT("SonarAcousticVolume"));
}
