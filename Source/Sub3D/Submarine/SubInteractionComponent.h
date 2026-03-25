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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Interaction|Debug")
	bool bDebugInteractionTrace = false;

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
