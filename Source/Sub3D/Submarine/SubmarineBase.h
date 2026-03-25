#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SubmarineRuntimeTypes.h"
#include "SubmarineBase.generated.h"

class USubMovementComponent;
class USubHullComponent;
class USubmarineSystemsComponent;
class USubmarineCompartmentComponent;
class USubmarineStationManagerComponent;
class USubmarineRadarComponent;
class USubInteriorFrameComponent;
class ATurretActor;

/**
 * The submarine entity. APawn, not ACharacter.
 * Physics via USubMovementComponent (math-based, no Chaos).
 * Server authoritative. Replicates movement to all clients.
 * Not possessed directly — players possess ASubCrewCharacter.
 */
UCLASS()
class SUB3D_API ASubmarineBase : public APawn
{
	GENERATED_BODY()

public:
	ASubmarineBase();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ── Components ────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* HullMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubMovementComponent* SubMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubHullComponent* SubHull;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubmarineSystemsComponent* Systems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubmarineCompartmentComponent* Compartments;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubmarineStationManagerComponent* StationManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubmarineRadarComponent* Radar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubInteriorFrameComponent* InteriorFrame;

	// Attach point inside the sub for the helm station
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* HelmSocket;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* TurretHardpoint;

	// ── Pilot tracking ────────────────────────────────────────────────────

	// The crew member currently at the helm (null = unmanned)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Submarine")
	AActor* CurrentPilot;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RepState, Category = "Submarine")
	FSubmarineNetState RepState;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Replicated, Category = "Submarine")
	ATurretActor* ExteriorTurret;

	UFUNCTION(BlueprintCallable, Category = "Submarine")
	void SetPilot(AActor* NewPilot);

	UFUNCTION(BlueprintCallable, Category = "Submarine")
	void ClearPilot();

	// ── Hull damage (Proto 02) ──────────────────────────────────────────

	// Damage scale: damage = NormalImpulse.Size() * Scale
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Damage")
	float HullImpactDamageScale = 0.0001f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Damage")
	float HullImpactRadiusCm = 18.f;

	// Callback for hull collisions
	UFUNCTION()
	void OnHullHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Collision")
	void ApplyHullCollisionDefaults();

	UFUNCTION(BlueprintCallable, Category = "Submarine")
	void RefreshRepState();

	UFUNCTION(BlueprintPure, Category = "Submarine")
	float GetCurrentDepthMeters() const;

	UFUNCTION(BlueprintPure, Category = "Submarine")
	float GetTotalFloodWaterMassKg() const;

	UFUNCTION(BlueprintCallable, Category = "Submarine")
	void ResolveExteriorTurret();

	UFUNCTION()
	void OnRep_RepState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
