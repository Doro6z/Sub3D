#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SubCrewCharacter.generated.h"

class UCameraComponent;
class ASubmarineBase;
class UInteractableComponent;
class USubInteractionComponent;

/**
 * Crew member character.
 * Input is handled entirely in Blueprint (Event Graph).
 * C++ provides: boarding, helm assignment, Server RPCs to drive sub physics.
 */
UCLASS()
class SUB3D_API ASubCrewCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASubCrewCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── Components ────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* FPSCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USubInteractionComponent* InteractionComponent;

	// ── Submarine attachment ──────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crew")
	ASubmarineBase* CurrentSubmarine;

	// Set the submarine reference without any physical attachment
	UFUNCTION(BlueprintCallable, Category = "Crew")
	void SetCurrentSubmarine(ASubmarineBase* Sub);

	// Teleport into the submarine and ensure walking mode is active
	UFUNCTION(BlueprintCallable, Category = "Crew")
	void EnterOnFootInSubmarine(ASubmarineBase* Sub, const FTransform& SpawnXform);

	// Legacy attachment boarding
	UFUNCTION(BlueprintCallable, Category = "Crew")
	void BoardSubmarine(ASubmarineBase* Submarine);

	UFUNCTION(BlueprintCallable, Category = "Crew")
	void DisembarkSubmarine();

	// ── Helm ──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crew")
	bool bIsAtHelm = false;

	UFUNCTION(BlueprintCallable, Category = "Crew")
	void TakeHelm();

	UFUNCTION(BlueprintCallable, Category = "Crew")
	void ReleaseHelm();

	// Called server-side directly (e.g. from GameMode on first board)
	void ForceHelm();

	// ── Interact ──────────────────────────────────────────────────────────

	// Line-trace from FPS camera, triggers UInteractableComponent if hit
	UFUNCTION(BlueprintCallable, Category = "Crew")
	void Interact();

	// Max interact distance in cm
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew")
	float InteractDistance = 250.f;

	// ── Sub input RPCs — call these from Blueprint Event Graph ────────────

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Crew|Helm")
	void Server_SetThrust(float Value);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Crew|Helm")
	void Server_SetRudder(float Value);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Crew|Helm")
	void Server_SetDivePlane(float Value);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Crew|Helm")
	void Server_SetBallastTarget(int32 Index, float Target);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Crew|Helm")
	void Server_ResyncBallasts(float GlobalTarget);

private:
	UFUNCTION(Server, Reliable)
	void Server_TakeHelm();

	UFUNCTION(Server, Reliable)
	void Server_ReleaseHelm();
};
