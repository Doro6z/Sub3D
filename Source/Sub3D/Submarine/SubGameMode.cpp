#include "SubGameMode.h"

#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SubCrewCharacter.h"
#include "SubGameState.h"
#include "SubHullComponent.h"
#include "SubMovementComponent.h"
#include "SubPlayerController.h"
#include "SubmarineBase.h"
#include "TraversalRouteActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubRun, Log, All);

namespace
{
void LogPrimitiveCollisionSnapshot(const TCHAR* Context, const AActor* ParentActor, const UPrimitiveComponent* PrimitiveComponent)
{
	if (!PrimitiveComponent)
	{
		return;
	}

	const AActor* ComponentOwner = PrimitiveComponent->GetOwner();
	const bool bInternalAttachment =
		(ComponentOwner == ParentActor)
		|| (ComponentOwner && ComponentOwner->GetOwner() == ParentActor)
		|| (ComponentOwner && ComponentOwner->GetAttachParentActor() == ParentActor)
		|| (ComponentOwner && ComponentOwner->IsAttachedTo(ParentActor));

	UE_LOG(
		LogSubRun,
		Log,
		TEXT("DepartureCollision | Context=%s | Owner=%s | Comp=%s | CompOwner=%s | Profile=%s | Enabled=%s | ObjType=%d | Internal=%d | Overlap=%d | Loc=%s"),
		Context,
		*GetNameSafe(ParentActor),
		*GetNameSafe(PrimitiveComponent),
		*GetNameSafe(ComponentOwner),
		*PrimitiveComponent->GetCollisionProfileName().ToString(),
		*UEnum::GetValueAsString(PrimitiveComponent->GetCollisionEnabled()),
		static_cast<int32>(PrimitiveComponent->GetCollisionObjectType()),
		bInternalAttachment ? 1 : 0,
		PrimitiveComponent->GetGenerateOverlapEvents() ? 1 : 0,
		*PrimitiveComponent->GetComponentLocation().ToCompactString());
}

void LogDepartureCollisionSnapshot(const ASubmarineBase* Submarine)
{
	if (!Submarine)
	{
		return;
	}

	const UPrimitiveComponent* MovementCollision = Submarine->GetMovementCollisionComponent();
	UE_LOG(
		LogSubRun,
		Log,
		TEXT("DepartureCollision | Submarine=%s | MovementCollision=%s | Freeze=%d | Location=%s | Rotation=%s"),
		*GetNameSafe(Submarine),
		*GetNameSafe(MovementCollision),
		Submarine->bFreezeMovementForTesting ? 1 : 0,
		*Submarine->GetActorLocation().ToCompactString(),
		*Submarine->GetActorRotation().ToCompactString());

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	Submarine->GetComponents(PrimitiveComponents);
	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}

		LogPrimitiveCollisionSnapshot(TEXT("SubComponent"), Submarine, PrimitiveComponent);
	}

	TArray<AActor*> AttachedActors;
	Submarine->GetAttachedActors(AttachedActors, true);
	for (const AActor* AttachedActor : AttachedActors)
	{
		if (!AttachedActor)
		{
			continue;
		}

		TInlineComponentArray<UPrimitiveComponent*> AttachedPrimitiveComponents;
		AttachedActor->GetComponents(AttachedPrimitiveComponents);
		for (const UPrimitiveComponent* PrimitiveComponent : AttachedPrimitiveComponents)
		{
			if (!PrimitiveComponent || PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				continue;
			}

			LogPrimitiveCollisionSnapshot(TEXT("AttachedActor"), Submarine, PrimitiveComponent);
		}
	}
}
}

ASubGameMode::ASubGameMode()
{
	DefaultPawnClass = ASubCrewCharacter::StaticClass();
	PlayerControllerClass = ASubPlayerController::StaticClass();
	GameStateClass = ASubGameState::StaticClass();
}

void ASubGameMode::StartPlay()
{
	Super::StartPlay();

	RefreshRunBootstrapReferences();
	TryAdvanceBootstrap();
}

void ASubGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	// Crew initialization is deferred to the bootstrap pipeline.
	// Do NOT call InitializePlayerCrewState() here.
}

void ASubGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	RefreshRunBootstrapReferences();

	ASubPlayerController* SubPC = Cast<ASubPlayerController>(NewPlayer);
	if (!SubPC)
	{
		return;
	}

	if (!PendingBootstrapControllers.Contains(NewPlayer))
	{
		PendingBootstrapControllers.Add(NewPlayer);
	}

	UE_LOG(
		LogSubRun,
		Log,
		TEXT("PostLogin | Controller=%s | Queued for bootstrap | PendingCount=%d"),
		*GetNameSafe(NewPlayer),
		PendingBootstrapControllers.Num());

	TryAdvanceBootstrap();
}

void ASubGameMode::RestartPlayer(AController* NewPlayer)
{
	RefreshRunBootstrapReferences();

	if (!IsValid(NewPlayer))
	{
		Super::RestartPlayer(NewPlayer);
		return;
	}

	if (!ActiveSubmarine)
	{
		Super::RestartPlayer(NewPlayer);
		return;
	}

	const FTransform SpawnTransform = ResolveCrewSpawnTransform();
	UE_LOG(
		LogSubRun,
		Log,
		TEXT("RestartPlayer | Controller=%s | SpawnLoc=%s | SpawnRot=%s | Sub=%s"),
		*GetNameSafe(NewPlayer),
		*SpawnTransform.GetLocation().ToCompactString(),
		*SpawnTransform.GetRotation().Rotator().ToCompactString(),
		*GetNameSafe(ActiveSubmarine));

	RestartPlayerAtTransform(NewPlayer, SpawnTransform);
}

void ASubGameMode::AdvanceRunPhase(ESubRunPhase NewPhase)
{
	if (!CanAdvanceRunPhase(NewPhase))
	{
		UE_LOG(
			LogSubRun,
			Warning,
			TEXT("Invalid run phase transition rejected | From=%s | To=%s"),
			*StaticEnum<ESubRunPhase>()->GetValueAsString(RunPhase),
			*StaticEnum<ESubRunPhase>()->GetValueAsString(NewPhase));
		return;
	}

	const ESubRunPhase OldPhase = RunPhase;
	RunPhase = NewPhase;

	if (RunPhase == ESubRunPhase::Boot)
	{
		bRunBreachTriggered = false;
		bBreachObjectiveActive = false;
		ActiveBreachSheetId = NAME_None;
		ActiveBreachedCompartmentId = NAME_None;
		bSubmarineInApproachZone = false;
	}

	SyncRunStateToGameState();

	UE_LOG(
		LogSubRun,
		Log,
		TEXT("Run phase advanced | From=%s | To=%s | Submarine=%s | Route=%s"),
		*StaticEnum<ESubRunPhase>()->GetValueAsString(OldPhase),
		*StaticEnum<ESubRunPhase>()->GetValueAsString(RunPhase),
		*GetNameSafe(ActiveSubmarine),
		*GetNameSafe(ActiveRoute));
}

void ASubGameMode::BeginDeparture()
{
	if (RunPhase != ESubRunPhase::Boarding)
	{
		UE_LOG(
			LogSubRun,
			Warning,
			TEXT("BeginDeparture rejected | Phase=%s"),
			*StaticEnum<ESubRunPhase>()->GetValueAsString(RunPhase));
		return;
	}

	if (ActiveSubmarine)
	{
		if (ActiveSubmarine->SubMovement
			&& (ActiveSubmarine->SubMovement->bDebugLogCollisionSweeps || ActiveSubmarine->SubMovement->bDebugLogSubMovement))
		{
			LogDepartureCollisionSnapshot(ActiveSubmarine);
		}

		ActiveSubmarine->SetFreezeMovementForTesting(false);
	}

	AdvanceRunPhase(ESubRunPhase::Departure);
}

bool ASubGameMode::CanAdvanceRunPhase(ESubRunPhase NewPhase) const
{
	if (NewPhase == RunPhase)
	{
		return false;
	}

	return IsValidRunPhaseTransition(RunPhase, NewPhase);
}

bool ASubGameMode::TriggerBreachEvent()
{
	RefreshRunBootstrapReferences();

	if (RunPhase != ESubRunPhase::Traverse)
	{
		UE_LOG(
			LogSubRun,
			Warning,
			TEXT("TriggerBreachEvent rejected | Phase=%s"),
			*StaticEnum<ESubRunPhase>()->GetValueAsString(RunPhase));
		return false;
	}

	if (bRunBreachTriggered || bBreachObjectiveActive)
	{
		UE_LOG(
			LogSubRun,
			Warning,
			TEXT("TriggerBreachEvent rejected | AlreadyTriggered=%d | ActiveObjective=%d"),
			bRunBreachTriggered ? 1 : 0,
			bBreachObjectiveActive ? 1 : 0);
		return false;
	}

	if (!ActiveSubmarine || !ActiveSubmarine->SubHull)
	{
		UE_LOG(LogSubRun, Warning, TEXT("TriggerBreachEvent rejected | No active submarine or hull component"));
		return false;
	}

	FName TargetSheetId = NAME_None;
	FName TargetCompartmentId = NAME_None;
	if (!ResolveScriptedBreachTarget(TargetSheetId, TargetCompartmentId))
	{
		UE_LOG(LogSubRun, Warning, TEXT("TriggerBreachEvent rejected | No deterministic breach target could be resolved"));
		return false;
	}

	const bool bAppliedDamage = ActiveSubmarine->CreateDebugBreachOnFirstExteriorSheet(ScriptedBreachDamageAmount);
	if (!bAppliedDamage)
	{
		UE_LOG(
			LogSubRun,
			Warning,
			TEXT("TriggerBreachEvent rejected | Debug breach application failed | Sheet=%s"),
			*TargetSheetId.ToString());
		return false;
	}

	if (!IsBreachClusterPresent(TargetSheetId, ActiveSubmarine->SubHull->GetBreachClusters()))
	{
		UE_LOG(
			LogSubRun,
			Warning,
			TEXT("TriggerBreachEvent rejected | Target breach cluster missing after impact | Sheet=%s"),
			*TargetSheetId.ToString());
		return false;
	}

	bRunBreachTriggered = true;
	bBreachObjectiveActive = true;
	ActiveBreachSheetId = TargetSheetId;
	ActiveBreachedCompartmentId = TargetCompartmentId;

	AdvanceRunPhase(ESubRunPhase::BreachCrisis);

	UE_LOG(
		LogSubRun,
		Log,
		TEXT("TriggerBreachEvent accepted | Sheet=%s | Compartment=%s"),
		*ActiveBreachSheetId.ToString(),
		*ActiveBreachedCompartmentId.ToString());

	return true;
}

void ASubGameMode::NotifyBreachStabilized(FName BreachId)
{
	if (RunPhase != ESubRunPhase::BreachCrisis)
	{
		UE_LOG(
			LogSubRun,
			Warning,
			TEXT("NotifyBreachStabilized ignored | Phase=%s | BreachId=%s"),
			*StaticEnum<ESubRunPhase>()->GetValueAsString(RunPhase),
			*BreachId.ToString());
		return;
	}

	if (!bBreachObjectiveActive)
	{
		UE_LOG(LogSubRun, Warning, TEXT("NotifyBreachStabilized ignored | No active breach objective"));
		return;
	}

	if (!BreachId.IsNone() && !ActiveBreachSheetId.IsNone() && BreachId != ActiveBreachSheetId)
	{
		UE_LOG(
			LogSubRun,
			Warning,
			TEXT("NotifyBreachStabilized ignored | Expected=%s | Received=%s"),
			*ActiveBreachSheetId.ToString(),
			*BreachId.ToString());
		return;
	}

	const ESubRunPhase NextPhase = bSubmarineInApproachZone ? ESubRunPhase::Approach : ESubRunPhase::Traverse;

	bBreachObjectiveActive = false;
	ActiveBreachSheetId = NAME_None;
	ActiveBreachedCompartmentId = NAME_None;

	AdvanceRunPhase(NextPhase);

	UE_LOG(
		LogSubRun,
		Log,
		TEXT("Breach stabilized | NextPhase=%s"),
		*StaticEnum<ESubRunPhase>()->GetValueAsString(NextPhase));
}

void ASubGameMode::TriggerRunFailure()
{
	if (!CanAdvanceRunPhase(ESubRunPhase::Failure))
	{
		UE_LOG(
			LogSubRun,
			Warning,
			TEXT("TriggerRunFailure rejected | Phase=%s"),
			*StaticEnum<ESubRunPhase>()->GetValueAsString(RunPhase));
		return;
	}

	AdvanceRunPhase(ESubRunPhase::Failure);
}

void ASubGameMode::SetSubmarineInApproachZone(bool bInApproachZone)
{
	bSubmarineInApproachZone = bInApproachZone;
	SyncRunStateToGameState();
}

void ASubGameMode::NotifySubmarineClearedDepartureGate()
{
	if (RunPhase != ESubRunPhase::Departure)
	{
		UE_LOG(
			LogSubRun,
			Warning,
			TEXT("NotifySubmarineClearedDepartureGate ignored | Phase=%s"),
			*StaticEnum<ESubRunPhase>()->GetValueAsString(RunPhase));
		return;
	}

	AdvanceRunPhase(ESubRunPhase::Traverse);
}

void ASubGameMode::NotifyApproachZoneStateChanged(bool bInApproachZone)
{
	SetSubmarineInApproachZone(bInApproachZone);

	if (!bInApproachZone)
	{
		return;
	}

	if (RunPhase == ESubRunPhase::Traverse && !bBreachObjectiveActive)
	{
		AdvanceRunPhase(ESubRunPhase::Approach);
	}
}

ASubmarineBase* ASubGameMode::ResolveActiveSubmarine()
{
	if (ActiveSubmarine)
	{
		return ActiveSubmarine;
	}

	if (!GetWorld())
	{
		return nullptr;
	}

	for (TActorIterator<ASubmarineBase> It(GetWorld()); It; ++It)
	{
		ActiveSubmarine = *It;
		break;
	}

	return ActiveSubmarine;
}

ATraversalRouteActor* ASubGameMode::ResolveActiveRoute()
{
	if (ActiveRoute)
	{
		return ActiveRoute;
	}

	if (!GetWorld())
	{
		return nullptr;
	}

	for (TActorIterator<ATraversalRouteActor> It(GetWorld()); It; ++It)
	{
		ActiveRoute = *It;
		break;
	}

	return ActiveRoute;
}

ASubGameState* ASubGameMode::ResolveSubGameState() const
{
	return Cast<ASubGameState>(GameState);
}

void ASubGameMode::RefreshRunBootstrapReferences()
{
	ResolveActiveSubmarine();
	ResolveActiveRoute();
	RefreshCampaignSeamData();
	RefreshBreachObservationBinding();
	SyncRunStateToGameState();
}

void ASubGameMode::RefreshBreachObservationBinding()
{
	USubHullComponent* NextHull = (ActiveSubmarine && ActiveSubmarine->SubHull) ? ActiveSubmarine->SubHull : nullptr;
	if (ObservedSubHull == NextHull)
	{
		return;
	}

	if (ObservedSubHull)
	{
		ObservedSubHull->OnBreachesUpdated.RemoveDynamic(this, &ASubGameMode::HandleBreachesUpdated);
	}

	ObservedSubHull = NextHull;

	if (ObservedSubHull)
	{
		ObservedSubHull->OnBreachesUpdated.AddDynamic(this, &ASubGameMode::HandleBreachesUpdated);
	}
}

void ASubGameMode::RefreshCampaignSeamData()
{
	if (!ActiveRoute)
	{
		CampaignSegmentID = NAME_None;
		RouteStartTransform = FTransform::Identity;
		RouteEndTransform = FTransform::Identity;
		return;
	}

	CampaignSegmentID = ActiveRoute->CampaignSegmentID;
	RouteStartTransform = ActiveRoute->GetRouteStartTransformWorld();
	RouteEndTransform = ActiveRoute->GetRouteEndTransformWorld();
}

FTransform ASubGameMode::ResolveCrewSpawnTransform() const
{
	FTransform SpawnXform = FTransform::Identity;

	if (IsValid(ActiveSubmarine))
	{
		SpawnXform = ActiveSubmarine->GetPrimaryCrewSpawnTransform();
	}

	SpawnXform.AddToTranslation(CrewSpawnOffset);
	return SpawnXform;
}

void ASubGameMode::InitializePlayerCrewState(APlayerController* NewPlayer)
{
	RefreshRunBootstrapReferences();

	ASubPlayerController* SubPC = Cast<ASubPlayerController>(NewPlayer);
	ASubCrewCharacter* Crew = SubPC ? Cast<ASubCrewCharacter>(SubPC->GetPawn()) : nullptr;
	if (!SubPC || !Crew || !ActiveSubmarine)
	{
		UE_LOG(
			LogSubRun,
			Warning,
			TEXT("InitializePlayerCrewState skipped | Controller=%s | Pawn=%s | ActiveSub=%s"),
			*GetNameSafe(NewPlayer),
			*GetNameSafe(NewPlayer ? NewPlayer->GetPawn() : nullptr),
			*GetNameSafe(ActiveSubmarine));
		return;
	}

	const FTransform SpawnTransform = ResolveCrewSpawnTransform();
	Crew->EnterOnFootInSubmarine(ActiveSubmarine, SpawnTransform);

	SubPC->CurrentControlMode = ECrewControlMode::OnFoot;
	SubPC->CurrentStation = nullptr;
	SubPC->CurrentStationType = ESubStationType::None;
	SubPC->ClientSetControlMode(ECrewControlMode::OnFoot, ESubStationType::None);

	UE_LOG(
		LogSubRun,
		Log,
		TEXT("InitializePlayerCrewState | Controller=%s | Crew=%s | SpawnLoc=%s | CurrentSub=%s"),
		*GetNameSafe(SubPC),
		*GetNameSafe(Crew),
		*SpawnTransform.GetLocation().ToCompactString(),
		*GetNameSafe(Crew->CurrentSubmarine));
}

void ASubGameMode::HandleBreachesUpdated(const TArray<FBreachClusterState>& Breaches)
{
	if (RunPhase != ESubRunPhase::BreachCrisis || !bBreachObjectiveActive || ActiveBreachSheetId.IsNone())
	{
		return;
	}

	if (!IsBreachClusterPresent(ActiveBreachSheetId, Breaches))
	{
		NotifyBreachStabilized(ActiveBreachSheetId);
	}
}

bool ASubGameMode::IsValidRunPhaseTransition(ESubRunPhase FromPhase, ESubRunPhase ToPhase) const
{
	switch (FromPhase)
	{
	case ESubRunPhase::Boot:
		return ToPhase == ESubRunPhase::Boarding;

	case ESubRunPhase::Boarding:
		return ToPhase == ESubRunPhase::Departure;

	case ESubRunPhase::Departure:
		return ToPhase == ESubRunPhase::Traverse;

	case ESubRunPhase::Traverse:
		return ToPhase == ESubRunPhase::BreachCrisis
			|| ToPhase == ESubRunPhase::Approach
			|| ToPhase == ESubRunPhase::Failure;

	case ESubRunPhase::BreachCrisis:
		return ToPhase == ESubRunPhase::Traverse
			|| ToPhase == ESubRunPhase::Approach
			|| ToPhase == ESubRunPhase::Failure;

	case ESubRunPhase::Approach:
		return ToPhase == ESubRunPhase::Docking
			|| ToPhase == ESubRunPhase::Failure;

	case ESubRunPhase::Docking:
		return ToPhase == ESubRunPhase::Success
			|| ToPhase == ESubRunPhase::Failure;

	case ESubRunPhase::Success:
	case ESubRunPhase::Failure:
		return ToPhase == ESubRunPhase::Boot;

	default:
		return false;
	}
}

bool ASubGameMode::ResolveScriptedBreachTarget(FName& OutSheetId, FName& OutCompartmentId) const
{
	OutSheetId = NAME_None;
	OutCompartmentId = NAME_None;

	if (!ActiveSubmarine || !ActiveSubmarine->SubHull)
	{
		return false;
	}

	const TArray<FStructuralSheetDef>& Sheets = ActiveSubmarine->SubHull->GetStructuralSheets();
	const FStructuralSheetDef* TargetSheet = Sheets.FindByPredicate([](const FStructuralSheetDef& Sheet)
	{
		return Sheet.bCanOpenToExterior;
	});

	if (!TargetSheet && Sheets.Num() > 0)
	{
		TargetSheet = &Sheets[0];
	}

	if (!TargetSheet)
	{
		return false;
	}

	OutSheetId = TargetSheet->SheetId;
	OutCompartmentId = TargetSheet->ParentCompartmentId;
	return true;
}

bool ASubGameMode::IsBreachClusterPresent(FName BreachSheetId, const TArray<FBreachClusterState>& Breaches) const
{
	if (BreachSheetId.IsNone())
	{
		return false;
	}

	return Breaches.ContainsByPredicate([BreachSheetId](const FBreachClusterState& Breach)
	{
		return Breach.SheetId == BreachSheetId;
	});
}

void ASubGameMode::SyncRunStateToGameState() const
{
	if (ASubGameState* SubGameState = ResolveSubGameState())
	{
		SubGameState->ApplyRunPhase(RunPhase);
		SubGameState->bBreachActive = bBreachObjectiveActive;
		SubGameState->BreachedCompartmentId = ActiveBreachedCompartmentId;
	}
}

// ── Bootstrap Pipeline ───────────────────────────────────────────────────

void ASubGameMode::SetBootstrapPhase(ESubBootstrapPhase NewPhase)
{
	const ESubBootstrapPhase OldPhase = BootstrapPhase;
	BootstrapPhase = NewPhase;

	UE_LOG(
		LogSubRun,
		Log,
		TEXT("Bootstrap | Phase=%s -> %s"),
		*StaticEnum<ESubBootstrapPhase>()->GetValueAsString(OldPhase),
		*StaticEnum<ESubBootstrapPhase>()->GetValueAsString(BootstrapPhase));
}

void ASubGameMode::TryAdvanceBootstrap()
{
	if (BootstrapPhase == ESubBootstrapPhase::Ready || BootstrapPhase == ESubBootstrapPhase::Failed)
	{
		return;
	}

	if (!ResolveWorldBootstrap())
	{
		UE_LOG(LogSubRun, Log, TEXT("Bootstrap waiting | World not ready"));
		return;
	}
	if (BootstrapPhase < ESubBootstrapPhase::WorldReady)
	{
		SetBootstrapPhase(ESubBootstrapPhase::WorldReady);
	}

	if (!ResolveSubmarineBootstrap())
	{
		UE_LOG(LogSubRun, Log, TEXT("Bootstrap waiting | Submarine not resolved"));
		return;
	}
	if (BootstrapPhase < ESubBootstrapPhase::SubResolved)
	{
		SetBootstrapPhase(ESubBootstrapPhase::SubResolved);
	}

	if (!ValidateSubmarineBootstrap())
	{
		UE_LOG(LogSubRun, Warning, TEXT("Bootstrap waiting | Submarine spawn collision invalid"));
		return;
	}
	if (BootstrapPhase < ESubBootstrapPhase::SubValidated)
	{
		SetBootstrapPhase(ESubBootstrapPhase::SubValidated);

		if (ActiveSubmarine)
		{
			ActiveSubmarine->SetFreezeMovementForTesting(true);
		}
	}

	if (PendingBootstrapControllers.Num() == 0)
	{
		UE_LOG(LogSubRun, Log, TEXT("Bootstrap waiting | No pending controllers"));
		return;
	}

	if (!SpawnAndEmbarkPendingControllers())
	{
		UE_LOG(LogSubRun, Warning, TEXT("Bootstrap waiting | Crew spawn/embark incomplete"));
		return;
	}
	if (BootstrapPhase < ESubBootstrapPhase::CrewEmbarked)
	{
		SetBootstrapPhase(ESubBootstrapPhase::CrewEmbarked);
	}

	SetBootstrapPhase(ESubBootstrapPhase::Ready);

	if (RunPhase == ESubRunPhase::Boot)
	{
		AdvanceRunPhase(ESubRunPhase::Boarding);
	}
}

bool ASubGameMode::ResolveWorldBootstrap()
{
	ResolveActiveRoute();
	return ActiveRoute != nullptr;
}

bool ASubGameMode::ResolveSubmarineBootstrap()
{
	ResolveActiveSubmarine();
	if (!ActiveSubmarine)
	{
		return false;
	}

	if (!ActiveSubmarine->IsMovementCollisionReady())
	{
		UE_LOG(LogSubRun, Log, TEXT("Bootstrap | Sub movement collision not ready | Sub=%s"), *GetNameSafe(ActiveSubmarine));
		return false;
	}

	return true;
}

bool ASubGameMode::ValidateSubmarineBootstrap()
{
	if (!ActiveSubmarine)
	{
		return false;
	}

	return ActiveSubmarine->ValidateSpawnCollision();
}

bool ASubGameMode::SpawnAndEmbarkPendingControllers()
{
	if (!ActiveSubmarine)
	{
		return false;
	}

	bool bAllSucceeded = true;

	for (int32 i = PendingBootstrapControllers.Num() - 1; i >= 0; --i)
	{
		APlayerController* PC = PendingBootstrapControllers[i];
		if (!IsValid(PC))
		{
			PendingBootstrapControllers.RemoveAt(i);
			continue;
		}

		ASubCrewCharacter* ExistingCrew = Cast<ASubCrewCharacter>(PC->GetPawn());
		if (!ExistingCrew)
		{
			const FTransform SpawnTransform = ResolveCrewSpawnTransform();
			RestartPlayerAtTransform(PC, SpawnTransform);
			ExistingCrew = Cast<ASubCrewCharacter>(PC->GetPawn());
		}

		if (!ExistingCrew)
		{
			UE_LOG(LogSubRun, Warning, TEXT("Bootstrap | Failed to spawn crew for %s"), *GetNameSafe(PC));
			bAllSucceeded = false;
			continue;
		}

		InitializePlayerCrewState(PC);

		if (!ValidateCrewBootstrap(ExistingCrew))
		{
			UE_LOG(
				LogSubRun,
				Warning,
				TEXT("CrewValidation | Crew=%s | Base=%s | Walkable=%d | Sub=%s"),
				*GetNameSafe(ExistingCrew),
				*GetNameSafe(ExistingCrew->GetMovementBase()),
				ExistingCrew->GetCharacterMovement() && ExistingCrew->GetCharacterMovement()->CurrentFloor.IsWalkableFloor() ? 1 : 0,
				*GetNameSafe(ExistingCrew->CurrentSubmarine));
			bAllSucceeded = false;
			continue;
		}

		PendingBootstrapControllers.RemoveAt(i);

		UE_LOG(
			LogSubRun,
			Log,
			TEXT("Bootstrap | Crew embarked | Controller=%s | Crew=%s"),
			*GetNameSafe(PC),
			*GetNameSafe(ExistingCrew));
	}

	return bAllSucceeded && PendingBootstrapControllers.Num() == 0;
}

bool ASubGameMode::ValidateCrewBootstrap(ASubCrewCharacter* Crew) const
{
	if (!Crew)
	{
		return false;
	}

	const UCharacterMovementComponent* Move = Crew->GetCharacterMovement();
	if (!Move)
	{
		return false;
	}

	return Crew->CurrentSubmarine == ActiveSubmarine
		&& Crew->GetMovementBase() != nullptr
		&& Move->MovementMode == MOVE_Walking;
}
