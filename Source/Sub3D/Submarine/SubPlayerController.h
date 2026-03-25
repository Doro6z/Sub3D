#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SubmarineTypes.h"
#include "SubPlayerController.generated.h"

class ASubmarineBase;
class AActor;

UENUM(BlueprintType)
enum class ECrewControlMode : uint8
{
	OnFoot      UMETA(DisplayName = "On Foot"),
	HelmDriving UMETA(DisplayName = "Helm Driving"),
	StationUI   UMETA(DisplayName = "Station UI")
};

/**
 * Player controller for the submarine prototype.
 * Input mapping context setup is handled in Blueprint (BP_SubPlayerController).
 */
UCLASS()
class SUB3D_API ASubPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// The current mode of control for the local player
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Crew Control")
	ECrewControlMode CurrentControlMode = ECrewControlMode::OnFoot;

	// The currently occupied station actor, if any
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crew Control")
	AActor* CurrentStation = nullptr;

	// Replicated station type for UI/input routing
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crew Control")
	ESubStationType CurrentStationType = ESubStationType::None;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Request to enter a station (generic durable API)
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerEnterStation(AActor* Station);

	// Backward-compatible alias used by existing Blueprints
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerEnterHelm(AActor* Station);

	// Request to exit the current station
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerExitStation();

	// Backward-compatible alias used by existing Blueprints
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerExitHelm();

	// Force mode change on client to react visually (camera, HUD)
	UFUNCTION(BlueprintCallable, Client, Reliable, Category = "Crew Control")
	void ClientSetControlMode(ECrewControlMode Mode);

	// BP hook called on client each time control mode is changed.
	UFUNCTION(BlueprintImplementableEvent, Category = "Crew Control")
	void BP_OnControlModeChanged(ECrewControlMode NewMode);

	// Route driving inputs directly from controller to station or sub
	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Crew Control")
	void ServerRouteHelmThrust(float Value);
	
	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Crew Control")
	void ServerRouteHelmSteer(float Value);

	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Crew Control")
	void ServerRouteHelmDive(float Value);

	// Route ballast controls from helm UI (controller-first authority path)
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteBallastGlobal(float Target);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteBallastByIndex(int32 Index, float Target);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRoutePumpActive(bool bActive);

	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Crew Control")
	void ServerRouteTurretAim(FRotator Aim);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteTurretFire(bool bHeld);

private:
	ASubmarineBase* ResolveCurrentSubmarine() const;
};
