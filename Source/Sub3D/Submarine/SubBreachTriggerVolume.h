#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubBreachTriggerVolume.generated.h"

class UBoxComponent;

UCLASS(Blueprintable)
class SUB3D_API ASubBreachTriggerVolume : public AActor
{
	GENERATED_BODY()

public:
	ASubBreachTriggerVolume();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerVolume = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run|Breach")
	bool bConsumeAfterTrigger = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Run|Breach")
	bool bTriggered = false;

protected:
	UFUNCTION()
	void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
