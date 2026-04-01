#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SonarAcousticVolumeActor.generated.h"

class UBoxComponent;
class USonarAcousticVolumeComponent;

UCLASS(Blueprintable)
class SUB3D_API ASonarAcousticVolumeActor : public AActor
{
	GENERATED_BODY()

public:
	ASonarAcousticVolumeActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sonar")
	TObjectPtr<UBoxComponent> VolumeBox = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sonar")
	TObjectPtr<USonarAcousticVolumeComponent> SonarAcousticVolume = nullptr;
};
