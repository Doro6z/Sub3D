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

	/**
	 * Lance un raycast caméra→forward et renvoie le FHitResult complet (impact point,
	 * component, normal). Utilisable au-delà des Interactables — ex : proto eau click→inject,
	 * splash particles, surface annotations, repair trace générique.
	 *
	 * Découplé de ASubCrewCharacter : marche sur tout APawn possédant un UCameraComponent.
	 *  - Si owner est ASubCrewCharacter : utilise la caméra crew + InteractDistance (compat existante).
	 *  - Sinon (APawn générique) : prend le premier UCameraComponent trouvé sur l'owner, distance
	 *    = DefaultTraceDistance.
	 *
	 * Retourne true si raycast a hit quelque chose, OutHit valide dans ce cas.
	 */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Interaction")
	bool TraceFromView(FHitResult& OutHit) const;

	/** Distance de raycast quand l'owner n'est PAS un ASubCrewCharacter (fallback APawn générique). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Interaction",
		meta = (ClampMin = "10.0", ClampMax = "10000.0"))
	float DefaultTraceDistance = 500.f;

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
