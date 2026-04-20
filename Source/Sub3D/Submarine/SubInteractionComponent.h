#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubInteractionComponent.generated.h"

class AActor;

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Submarine|Interaction")
	void TryPrimaryInteract();

	UFUNCTION(BlueprintCallable, Category = "Submarine|Interaction")
	void BeginToolAction();

	UFUNCTION(BlueprintCallable, Category = "Submarine|Interaction")
	void EndToolAction();

	UFUNCTION(BlueprintCallable, Category = "Submarine|Interaction")
	bool PerformRepairTrace();

	UFUNCTION(BlueprintCallable, Category = "Submarine|Interaction")
	bool TryRepairFocusedTarget(float RepairStrength = 20.f, float RadiusCm = 30.f);

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Interaction")
	AActor* FocusedActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Submarine|Interaction")
	class UInteractableComponent* FocusedInteractable = nullptr;

protected:
	UFUNCTION(Server, Reliable)
	void ServerTryPrimaryInteract(AActor* TargetActor);

	UFUNCTION(Server, Reliable)
	void ServerTryRepairTarget(AActor* TargetActor, FVector_NetQuantize ImpactPoint, float RepairStrength, float RadiusCm);

private:
	AActor* ResolvePrimaryInteractTarget(FVector* OutTraceStart = nullptr, FVector* OutTraceEnd = nullptr) const;
	AActor* ResolveNearbyInteractableFallback(const FVector& TraceStart, const FVector& TraceEnd) const;
	static AActor* ResolveInteractableOwner(AActor* HitActor);
};
