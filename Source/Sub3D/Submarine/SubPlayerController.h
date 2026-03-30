#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SubSonarV2Types.h"
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
	void ClientSetControlMode(ECrewControlMode Mode, ESubStationType StationType);

	// BP hook called on client each time control mode is changed.
	UFUNCTION(BlueprintImplementableEvent, Category = "Crew Control")
	void BP_OnControlModeChanged(ECrewControlMode NewMode, ESubStationType StationType);

	// Local-only lean state for sonar CRT focus. No server RPC.
	UPROPERTY(BlueprintReadOnly, Category = "Crew Control|Sonar")
	bool bSonarLeanActive = false;

	// Toggle lean state from IA_SonarLean input (Pressed=true / Released=false).
	UFUNCTION(BlueprintCallable, Category = "Crew Control|Sonar")
	void SetSonarLeanActive(bool bActive);

	UFUNCTION(BlueprintImplementableEvent, Category = "Crew Control|Sonar")
	void BP_OnSonarLeanChanged(bool bActive);

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
	void ServerRouteBallastActive(bool bActive);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRoutePumpActive(bool bActive);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRoutePumpPower(float Value);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteEngineBoost(float Value);

	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Crew Control")
	void ServerRouteTurretAim(FRotator Aim);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteTurretFire(bool bHeld);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteDoorToggle(FName DoorId, bool bClosed);

	// Fire a sonar ping — only accepted when CurrentControlMode == HelmDriving
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteSonarPing();

	// Hold sonar ping (Pressed=true / Released=false).
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerSetSonarPingHeld(bool bHeld);

	// Local BP-facing helper: routes to server RPC.
	UFUNCTION(BlueprintCallable, Category = "Crew Control|Sonar")
	void TriggerSonarPing();

	// Local BP-facing helper: routes to server RPC.
	UFUNCTION(BlueprintCallable, Category = "Crew Control|Sonar")
	void SetSonarPingHeld(bool bHeld);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control|Sonar")
	void ServerSetSonarMode(ESonarMode NewMode);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control|Sonar")
	void ServerSetSonarFocusBearing(float NewBearingDeg);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control|Sonar")
	void ServerSetSonarRangePreset(int32 NewPresetIndex);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control|Sonar")
	void ServerMarkSonarPriorityTrack(int32 TrackId, bool bPriority);

	UFUNCTION(BlueprintCallable, Category = "Crew Control|Sonar")
	void SetSonarMode(ESonarMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Crew Control|Sonar")
	void SetSonarFocusBearing(float NewBearingDeg);

	UFUNCTION(BlueprintCallable, Category = "Crew Control|Sonar")
	void SetSonarRangePreset(int32 NewPresetIndex);

	UFUNCTION(BlueprintCallable, Category = "Crew Control|Sonar")
	void MarkSonarPriorityTrack(int32 TrackId, bool bPriority);

	UFUNCTION(BlueprintPure, Category = "Crew Control")
	ASubmarineBase* GetResolvedCurrentSubmarine() const;

	ASubmarineBase* ResolveCurrentSubmarine() const;
};
