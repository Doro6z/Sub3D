#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "SubHullBoundaryComponent.generated.h"

class ASubCrewCharacter;

UENUM(BlueprintType)
enum class EHullBoundaryKind : uint8
{
	Airlock,    // Intended transition (sub intact): walk in/out via the sas.
	Breach      // Chaotic transition (hull damaged): crew can be sucked/flushed.
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCapsuleCrossedHull, ASubCrewCharacter*, Crew, bool, bOutgoing);

/**
 * Hull plane boundary component. Placed at each opening between the sub interior and the ocean
 * (airlock exterior door, breach hole). Tracks which side of its local plane each overlapping
 * crew capsule is on, and fires OnCapsuleCrossedHull when the side flips.
 *
 * Uses collision channel ECC_CompartmentProbe (declared in CompartmentVolumeComponent.h),
 * shared with UCompartmentVolumeComponent. Crew capsule responds Overlap on that channel.
 */
UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubHullBoundaryComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	USubHullBoundaryComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Hull plane geometry is the component's own transform :
	 *  - Plane origin  = GetComponentLocation()  (world)
	 *  - Plane normal  = GetForwardVector()      (world, component's local +X)
	 * Place and rotate this component so +X points outward from the hull.
	 */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HullBoundary")
	EHullBoundaryKind Kind = EHullBoundaryKind::Airlock;

	/** Fires each time a crew capsule crosses the hull plane. Kept available for BP hooks (vfx, sfx, audio). */
	UPROPERTY(BlueprintAssignable, Category = "HullBoundary")
	FOnCapsuleCrossedHull OnCapsuleCrossedHull;

	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

private:
	/**
	 * Per-crew tracking. Single-fire per overlap: once a crossing fires for a given crew,
	 * the side tracker is cleared and rearms only when the crew leaves the box and re-enters.
	 * This prevents ping-pong from CMC mode switches (Walking collision resolution pushing
	 * the capsule back across the plane immediately after an inward crossing).
	 */
	struct FCrewSideState
	{
		int8 LastSide = 0;       // +1 outside, -1 inside, 0 unknown.
		bool bArmed = true;      // False after a crossing has fired; rearmed on OnEndOverlap.
	};
	TMap<TWeakObjectPtr<ASubCrewCharacter>, FCrewSideState> CrewSideStates;

	/** Returns +1 if the crew capsule center is on the outward side of the plane, -1 otherwise. */
	int8 ComputeSide(const ASubCrewCharacter* Crew) const;
};
