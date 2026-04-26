#include "Sub3DGameplayDebugger.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#include "Submarine/CompartmentVolumeComponent.h"
#include "Submarine/SubCrewCharacter.h"
#include "Submarine/SubCrewMovementComponent.h"
#include "Submarine/SubGameMode.h"
#include "Submarine/SubMovementComponent.h"
#include "Submarine/SubPlayerController.h"
#include "Submarine/SubmarineBase.h"
#include "WorldGen/TraversalRouteActor.h"

namespace
{
	const TCHAR* RoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_Authority:        return TEXT("Authority");
		case ROLE_AutonomousProxy:  return TEXT("AutoProxy");
		case ROLE_SimulatedProxy:   return TEXT("SimProxy");
		default:                    return TEXT("None");
		}
	}

	const TCHAR* MovementModeToString(uint8 Mode)
	{
		switch (Mode)
		{
		case MOVE_None:       return TEXT("None");
		case MOVE_Walking:    return TEXT("Walking");
		case MOVE_NavWalking: return TEXT("NavWalking");
		case MOVE_Falling:    return TEXT("Falling");
		case MOVE_Swimming:   return TEXT("Swimming");
		case MOVE_Flying:     return TEXT("Flying");
		case MOVE_Custom:     return TEXT("Custom");
		default:              return TEXT("?");
		}
	}
}

FSub3DGameplayDebuggerCategory::FSub3DGameplayDebuggerCategory()
{
	bShowOnlyWithDebugActor = false;
	CollectDataInterval = 0.1f;
}

TSharedRef<FGameplayDebuggerCategory> FSub3DGameplayDebuggerCategory::MakeInstance()
{
	return MakeShareable(new FSub3DGameplayDebuggerCategory());
}

void FSub3DGameplayDebuggerCategory::CollectData(APlayerController* OwnerPC, AActor* /*DebugActor*/)
{
	UWorld* World = OwnerPC ? OwnerPC->GetWorld() : nullptr;
	if (!World)
	{
		AddTextLine(TEXT("{red}<no world>"));
		return;
	}

	// Owner / network context first — useful to spot which PIE viewport is showing what.
	const FString WorldName = World->GetName();
	const ENetMode NetMode = World->GetNetMode();
	const TCHAR* NetModeStr =
		(NetMode == NM_Standalone)    ? TEXT("Standalone") :
		(NetMode == NM_DedicatedServer) ? TEXT("DedicatedServer") :
		(NetMode == NM_ListenServer)  ? TEXT("ListenServer") :
		(NetMode == NM_Client)        ? TEXT("Client") : TEXT("?");
	AddTextLine(FString::Printf(TEXT("{yellow}== CONTEXT == {white}World=%s NetMode=%s OwnerPC=%s"),
		*WorldName, NetModeStr, *GetNameSafe(OwnerPC)));

	// -- GameMode (server only — clients have no auth GM) --------------------
	AddTextLine(TEXT(""));
	AddTextLine(TEXT("{yellow}== GAMEMODE =="));
	if (ASubGameMode* GM = Cast<ASubGameMode>(World->GetAuthGameMode()))
	{
		AddTextLine(FString::Printf(TEXT("  BootstrapPhase=%s  RunPhase=%s"),
			*StaticEnum<ESubBootstrapPhase>()->GetNameStringByValue(static_cast<int64>(GM->BootstrapPhase)),
			*StaticEnum<ESubRunPhase>()->GetNameStringByValue(static_cast<int64>(GM->RunPhase))));
		AddTextLine(FString::Printf(TEXT("  ActiveSubmarine=%s  ActiveRoute=%s"),
			*GetNameSafe(GM->ActiveSubmarine),
			*GetNameSafe(GM->ActiveRoute)));
	}
	else
	{
		AddTextLine(TEXT("  {grey}<no auth GameMode on this world (client view)>"));
	}

	// -- Submarines ----------------------------------------------------------
	AddTextLine(TEXT(""));
	AddTextLine(TEXT("{yellow}== SUBMARINES =="));
	int32 SubCount = 0;
	for (TActorIterator<ASubmarineBase> It(World); It; ++It)
	{
		ASubmarineBase* Sub = *It;
		if (!Sub) continue;
		++SubCount;

		const FVector Loc = Sub->GetActorLocation();
		const FRotator Rot = Sub->GetActorRotation();
		const float SpeedCmS = Sub->SubMovement ? Sub->SubMovement->Velocity.Size() : 0.f;
		const float Depth = Sub->SubMovement ? Sub->SubMovement->CurrentDepth : 0.f;
		const TCHAR* RoleStr = RoleToString(Sub->GetLocalRole());
		const TCHAR* FreezeStr = Sub->bFreezeMovementForTesting ? TEXT("{red}FROZEN") : TEXT("{green}free");

		AddTextLine(FString::Printf(TEXT("{cyan}%s {white}(%s) %s"),
			*Sub->GetName(), RoleStr, FreezeStr));
		AddTextLine(FString::Printf(TEXT("  Loc=(%.1f, %.1f, %.1f)  Yaw=%.1f°  Speed=%.0f cm/s  Depth=%.1f m"),
			Loc.X, Loc.Y, Loc.Z, Rot.Yaw, SpeedCmS, Depth));
		AddTextLine(FString::Printf(TEXT("  Pilot=%s"),
			*GetNameSafe(Sub->CurrentPilot)));
	}
	if (SubCount == 0)
	{
		AddTextLine(TEXT("  {red}<no submarines in this world>"));
	}

	// -- Crews ---------------------------------------------------------------
	AddTextLine(TEXT(""));
	AddTextLine(TEXT("{yellow}== CREWS =="));
	int32 CrewCount = 0;
	for (TActorIterator<ASubCrewCharacter> It(World); It; ++It)
	{
		ASubCrewCharacter* Crew = *It;
		if (!Crew) continue;
		++CrewCount;

		USubCrewMovementComponent* Mov = Crew->GetCrewMovement();
		ASubPlayerController* PC = Cast<ASubPlayerController>(Crew->GetController());

		const TCHAR* RoleStr = RoleToString(Crew->GetLocalRole());
		const TCHAR* IsLocalStr = Crew->IsLocallyControlled() ? TEXT("{green}LOCAL") : TEXT("{grey}remote");
		const FString PCStr = PC
			? FString::Printf(TEXT("%s slot=%d"), *PC->GetName(), PC->AssignedSpawnSlot)
			: FString(TEXT("<no PC>"));

		AddTextLine(FString::Printf(TEXT("{cyan}%s {white}(%s, %s){white} %s"),
			*Crew->GetName(), RoleStr, IsLocalStr, *PCStr));

		if (Mov)
		{
			const FString EmbarkStr = StaticEnum<ECrewEmbarkState>()->GetNameStringByValue(
				static_cast<int64>(Mov->EmbarkState));
			AddTextLine(FString::Printf(TEXT("  EmbarkState=%s  GridAuth=%d  Mode=%s  Grav=%.1f"),
				*EmbarkStr,
				Mov->IsGridAuthoritative() ? 1 : 0,
				MovementModeToString(Mov->MovementMode),
				Mov->GravityScale));

			const FVector LocalPos = Mov->GridSpaceTransform.GetLocation();
			const float LocalYaw = Mov->GridSpaceTransform.Rotator().Yaw;
			AddTextLine(FString::Printf(TEXT("  GridSpace=(%.1f, %.1f, %.1f)  Yaw=%.1f°"),
				LocalPos.X, LocalPos.Y, LocalPos.Z, LocalYaw));
		}

		AddTextLine(FString::Printf(TEXT("  CurrentSubmarine=%s"),
			*GetNameSafe(Crew->CurrentSubmarine)));

		const UCompartmentVolumeComponent* Comp = Crew->CurrentCompartment.Get();
		AddTextLine(FString::Printf(TEXT("  Compartment=%s  CompartmentId=%s"),
			Comp ? *Comp->GetName() : TEXT("<ocean>"),
			!Crew->CurrentCompartmentId.IsNone() ? *Crew->CurrentCompartmentId.ToString() : TEXT("<none>")));

		if (PC)
		{
			AddTextLine(FString::Printf(TEXT("  ControlMode=%s  StationType=%s  AtHelm=%d"),
				*StaticEnum<ECrewControlMode>()->GetNameStringByValue(static_cast<int64>(PC->CurrentControlMode)),
				*StaticEnum<ESubStationType>()->GetNameStringByValue(static_cast<int64>(PC->CurrentStationType)),
				Crew->bIsAtHelm ? 1 : 0));
		}
	}
	if (CrewCount == 0)
	{
		AddTextLine(TEXT("  {red}<no crews in this world>"));
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
