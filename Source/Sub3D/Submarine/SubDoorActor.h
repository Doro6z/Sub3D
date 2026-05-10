#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubmarineRuntimeTypes.h"
#include "SubDoorActor.generated.h"

class ASubCrewCharacter;
class ASubmarineBase;
class UBoxComponent;
class UInteractableComponent;
class UStaticMeshComponent;
struct FDoorDef;
struct FGeneratedConnectionDef;

UCLASS(Blueprintable)
class SUB3D_API ASubDoorActor : public AActor
{
	GENERATED_BODY()

public:
	ASubDoorActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* DoorCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInteractableComponent* Interactable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName DoorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName CompartmentA = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	FName CompartmentB = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bStartsClosed = false;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DoorClosed, Category = "Door")
	bool bClosed = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Door")
	TObjectPtr<ASubmarineBase> OwningSubmarine = nullptr;

	// ── Animation contract (Strategy A — C++ tweens OpenAlpha, BP applies transform) ──────
	// bClosed is the authoritative target (replicated). OpenAlpha is the local-only visual
	// state, ticked from current toward bClosed?0:1 over OpenDurationSeconds. Each tick during
	// animation, BP_OnOpenAlphaUpdated fires; the designer applies translation OR rotation OR
	// any combination on BattantPivot (or other components) using OpenAlpha as the input.

	/** Time (seconds) for the leaf to fully open from fully closed (and vice versa). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Animation", meta = (ClampMin = "0.05"))
	float OpenDurationSeconds = 1.0f;

	/** Designer hint: max angle (deg) at full-open. BP uses this as the rotation amplitude.
	 *  Ignored for translation-only doors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Animation")
	float MaxOpenAngleDeg = 90.0f;

	/** Designer hint: max linear distance (cm) at full-open. BP uses this as the translation
	 *  amplitude. Ignored for rotation-only doors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Animation")
	float MaxOpenDistanceCm = 90.0f;

	/** Threshold of OpenAlpha at which the door collision flips from blocking (closed) to
	 *  passable (open). 0.5 = midway through animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CollisionToggleAlpha = 0.5f;

	/** Current open alpha. 0 = fully closed, 1 = fully open. Local-only (each client interpolates
	 *  from its own bClosed transitions — no per-frame replication traffic). Read in BP. */
	UPROPERTY(BlueprintReadOnly, Category = "Door|Animation")
	float OpenAlpha = 0.0f;

	UFUNCTION(BlueprintPure, Category = "Door|Animation")
	float GetOpenAlpha() const { return OpenAlpha; }

	/** Strategy B escape hatch: if a BP drives the animation via its own Timeline component,
	 *  it can push the alpha back here so the C++ collision threshold and BP_OnOpenAlphaUpdated
	 *  stay in sync. Skipping this is fine if you only use Strategy A. */
	UFUNCTION(BlueprintCallable, Category = "Door|Animation")
	void SetOpenAlphaFromTimeline(float NewAlpha);

	/** Fired each tick while OpenAlpha is animating. Designer applies the transform on
	 *  BattantPivot (or any component) using NewAlpha. Examples:
	 *    Rotation door:        BattantPivot.SetRelativeRotation((0, MaxOpenAngleDeg * Alpha, 0))
	 *    Translation door:     BattantPivot.SetRelativeLocation((0, MaxOpenDistanceCm * Alpha, 0))
	 *    2-leaf airlock split: pivotL.SetRelLoc((0, -MaxOpenDistanceCm*Alpha, 0)),
	 *                          pivotR.SetRelLoc((0, +MaxOpenDistanceCm*Alpha, 0)) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Door|Animation")
	void BP_OnOpenAlphaUpdated(float NewAlpha);

	UFUNCTION(BlueprintCallable, Category = "Door")
	void SetDoorClosed(bool bNewClosed);

	UFUNCTION(BlueprintCallable, Category = "Door")
	void ToggleDoor();

	UFUNCTION(BlueprintCallable, Category = "Door")
	void InitializeFromDoorDef(const FDoorDef& DoorDef, FName InCompartmentA, FName InCompartmentB, ASubmarineBase* InOwningSubmarine);

	/**
	 * Initialize from a generator-produced connection definition. Unlike
	 * InitializeFromDoorDef which uses the legacy FDoorDef type, this method
	 * reads all fields directly from the generator's FGeneratedConnectionDef,
	 * including ConnectionId, compartment endpoints, and bStartsClosed.
	 * Intended to be called by ASubmarineBase::SpawnDoorsFromDefinition after
	 * SpawnActor at the connection's local transform.
	 */
	UFUNCTION(BlueprintCallable, Category = "Door")
	void InitializeFromConnectionDef(const FGeneratedConnectionDef& Connection, ASubmarineBase* InOwningSubmarine);

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsDoorClosed() const { return bClosed; }

protected:
	UFUNCTION()
	void HandleInteract(ASubCrewCharacter* Interactor);

	UFUNCTION()
	void OnRep_DoorClosed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Door")
	void BP_OnDoorStateChanged(bool bNowClosed);

private:
	void TryResolveOwningSubmarine();
	void ApplyDoorState();
	void ApplyCollisionFromAlpha();
	void ApplySubmarineCollisionIgnoreToAllPrimitiveComponents();
	void RegisterWithCompartments();

	/** Forward the current OpenAlpha (continuous, 0..1) to the owning sub's flood graph,
	 *  preferring DoorId match and falling back to CompartmentA/B match. Called every tick
	 *  by the alpha tween + once per state change from ApplyDoorState. */
	void PushOpenRatioToFlood(float Ratio);
};
