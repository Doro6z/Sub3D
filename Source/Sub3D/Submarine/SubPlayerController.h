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

	/**
	 * Spawn slot assigned by ASubGameMode::PostLogin. Indexes into the game mode's
	 * CrewSpawnSlotOffsetsLocal array. Replicated so the owning client can see its
	 * own assignment (useful for HUD/debug). -1 = not yet assigned.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crew|Spawn")
	int32 AssignedSpawnSlot = -1;

	// ── HUD ───────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	TSubclassOf<class USubPlayerHUDWidget> HUDWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	class USubPlayerHUDWidget* HUDWidget;

	virtual void BeginPlay() override;

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

	// Ramp the throttle up / down while a key is held. Intent is a direction
	// (-1..+1) applied each sim tick at ThrottleRampRate units/sec. Wired
	// from IA_Thrust in BP: pass the axis value on Triggered, 0 on Completed.
	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Crew Control")
	void ServerRouteHelmThrottleRamp(float Intent);

	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Crew Control")
	void ServerRouteHelmSteer(float Value);

	// Hold-to-ramp rudder. Intent in -1..+1 (sign = direction, magnitude
	// scales the rate). 0 stops the ramp and the existing auto-recenter
	// (when RudderHold is off) takes over. Wire IA_Rudder Triggered →
	// pass axis value here, IA_Rudder Completed → pass 0.
	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Crew Control")
	void ServerRouteHelmRudderRamp(float Intent);

	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Crew Control")
	void ServerRouteHelmDive(float Value);

	// Hold-to-ramp dive plane. Same contract as ServerRouteHelmRudderRamp.
	UFUNCTION(BlueprintCallable, Server, Unreliable, Category = "Crew Control")
	void ServerRouteHelmDivePlaneRamp(float Intent);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteRudderHoldEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRoutePlaneHoldEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteStabilizationMaster(bool bEnabled);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteAutoSpeedEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteAutoDepthEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteAutoPitchEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteTargetSpeedCmS(float SpeedCmS);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteTargetDepthMeters(float DepthMeters);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Crew Control")
	void ServerRouteTargetPitchDeg(float PitchDeg);

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

	/**
	 * Dev-cheat resolution: tries ResolveCurrentSubmarine() first, then falls back to iterating
	 * world actors for the first ASubmarineBase with a valid SubFlood. Used by Server_DevCheat_*
	 * handlers where the server-side PC may not yet have a possessed crew or replicated station.
	 * Solo / single-sub assumption — for multi-sub testing, use the normal resolution path.
	 */
	ASubmarineBase* ResolveSubmarineForDevCheat() const;

	// ── Dev cheats (console commands, Exec) ─────────────────────────────
	// Used to validate the First Playable gameplay loop without relying on
	// full runtime simulation. All cheats resolve the current submarine and
	// act on its SubFlood / Definition. Safe no-ops if unavailable.

	/** Create or update a breach on the given compartment at RateLps liters/sec. */
	UFUNCTION(Exec, Category = "Debug|Submarine")
	void DevCheat_CreateBreach(FName CompartmentId, float RateLps);

	/** Server-side counterpart: the Exec routes here so the authoritative SubFlood sim receives the breach. */
	UFUNCTION(Server, Reliable, Category = "Debug|Submarine")
	void Server_DevCheat_CreateBreach(FName CompartmentId, float RateLps);

	/** Create a breach AT the controlled crew's current position. Auto-resolves CompartmentId from
	 *  ASubCrewCharacter::CurrentCompartmentId, converts crew world location to sub-local, and uses
	 *  it as the breach center — so the visual marker (red cube) and the heightfield InjectAt land
	 *  exactly where the crew stands. Reproduces the proto's "InjectAt at SubCrew" workflow. */
	UFUNCTION(Exec, Category = "Debug|Submarine")
	void DevCheat_CreateBreachAtCrew(float RateLps);

	UFUNCTION(Server, Reliable, Category = "Debug|Submarine")
	void Server_DevCheat_CreateBreachAtCrew(FName CompartmentId, FVector LocalCenter, float RateLps);

	/** Force a door/hatch connection state via its ConnectionId. */
	UFUNCTION(Exec, Category = "Debug|Submarine")
	void DevCheat_SetDoorClosed(FName ConnectionId, bool bClosed);

	/** Directly set the flood level of a compartment (0-1), bypassing simulation. */
	UFUNCTION(Exec, Category = "Debug|Submarine")
	void DevCheat_SetFloodLevel(FName CompartmentId, float Level01);

	/** Remove all breaches from all compartments (repair shortcut). */
	UFUNCTION(Exec, Category = "Debug|Submarine")
	void DevCheat_RepairAllBreaches();

	/** Teleport the possessed pawn to the center of the named compartment. */
	UFUNCTION(Exec, Category = "Debug|Submarine")
	void DevCheat_TeleportToCompartment(FName CompartmentId);

	/** Print the list of compartments and their current flood levels to the log. */
	UFUNCTION(Exec, Category = "Debug|Submarine")
	void DevCheat_ListCompartments();

	/** Set a crew anim parameter by name. Usage: Anim LegSwingAxis 2 */
	UFUNCTION(Exec, Category = "Debug|Crew")
	void Anim(const FString& ParamName, float Value);

	/** Print all crew anim parameters */
	UFUNCTION(Exec, Category = "Debug|Crew")
	void AnimList();

	/** Print the current crew animation debug snapshot to the log. */
	UFUNCTION(Exec, Category = "Debug|Crew")
	void CrewAnimDump();
};
