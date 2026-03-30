#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubRunPhaseVolume.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class ESubRunVolumeType : uint8
{
	DepartureGate UMETA(DisplayName = "Departure Gate"),
	ApproachZone UMETA(DisplayName = "Approach Zone")
};

UCLASS(Blueprintable)
class SUB3D_API ASubRunPhaseVolume : public AActor
{
	GENERATED_BODY()

public:
	ASubRunPhaseVolume();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerVolume = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	ESubRunVolumeType VolumeType = ESubRunVolumeType::DepartureGate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	bool bConsumeAfterActivation = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run")
	bool bTriggered = false;

protected:
	UFUNCTION()
	void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
