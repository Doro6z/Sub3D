#include "SubGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SubCrewCharacter.h"
#include "SubPlayerController.h"
#include "SubmarineBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubBootstrap, Log, All);

namespace
{
bool IsAcceptedCrewBaseForSubmarine(const ASubmarineBase* Submarine, const UPrimitiveComponent* CandidateBase)
{
	return Submarine && Submarine->IsInteriorWalkableComponent(CandidateBase);
}
}

ASubGameMode::ASubGameMode()
{
	DefaultPawnClass = ASubCrewCharacter::StaticClass();
	PlayerControllerClass = ASubPlayerController::StaticClass();
}

void ASubGameMode::StartPlay()
{
	Super::StartPlay();

	PhaseEnteredAtSeconds = FPlatformTime::Seconds();
	LastStallLogSeconds = 0.;

	RefreshBootstrapReferences();
	TryAdvanceBootstrap();

	if (BootstrapPhase != ESubBootstrapPhase::Ready && BootstrapPhase != ESubBootstrapPhase::Failed)
	{
		StartBootstrapRetryTimer();
	}
}

void ASubGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	// Crew initialization is deferred to the bootstrap pipeline.
}

void ASubGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	RefreshBootstrapReferences();

	ASubPlayerController* SubPC = Cast<ASubPlayerController>(NewPlayer);
	if (!SubPC)
	{
		return;
	}

	if (SubPC->AssignedSpawnSlot < 0)
	{
		SubPC->AssignedSpawnSlot = AssignNextSpawnSlot();
		UE_LOG(
			LogSubBootstrap,
			Log,
			TEXT("PostLogin | Controller=%s | AssignedSpawnSlot=%d"),
			*GetNameSafe(NewPlayer),
			SubPC->AssignedSpawnSlot);
	}

	if (!PendingBootstrapControllers.Contains(NewPlayer))
	{
		PendingBootstrapControllers.Add(NewPlayer);
	}

	UE_LOG(
		LogSubBootstrap,
		Log,
		TEXT("PostLogin | Controller=%s | Queued for bootstrap | PendingCount=%d"),
		*GetNameSafe(NewPlayer),
		PendingBootstrapControllers.Num());

	if (BootstrapPhase == ESubBootstrapPhase::Ready)
	{
		const int32 PendingBefore = PendingBootstrapControllers.Num();
		SpawnAndEmbarkPendingControllers();
		UE_LOG(
			LogSubBootstrap,
			Log,
			TEXT("PostLogin | Late-arrival spawn | Controller=%s | Pending %d -> %d"),
			*GetNameSafe(NewPlayer),
			PendingBefore,
			PendingBootstrapControllers.Num());
		return;
	}

	TryAdvanceBootstrap();
}

int32 ASubGameMode::AssignNextSpawnSlot()
{
	int32 Max = -1;
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (const ASubPlayerController* OtherPC = Cast<ASubPlayerController>(It->Get()))
			{
				Max = FMath::Max(Max, OtherPC->AssignedSpawnSlot);
			}
		}
	}
	return Max + 1;
}

void ASubGameMode::RestartPlayer(AController* NewPlayer)
{
	RefreshBootstrapReferences();

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

	const ASubPlayerController* SubPC = Cast<ASubPlayerController>(NewPlayer);
	const int32 SlotIndex = SubPC ? SubPC->AssignedSpawnSlot : 0;
	const FTransform SpawnTransform = ResolveCrewSpawnTransform(SlotIndex);
	UE_LOG(
		LogSubBootstrap,
		Log,
		TEXT("RestartPlayer | Controller=%s | Slot=%d | SpawnLoc=%s | SpawnRot=%s | Sub=%s"),
		*GetNameSafe(NewPlayer),
		SlotIndex,
		*SpawnTransform.GetLocation().ToCompactString(),
		*SpawnTransform.GetRotation().Rotator().ToCompactString(),
		*GetNameSafe(ActiveSubmarine));

	RestartPlayerAtTransform(NewPlayer, SpawnTransform);
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

void ASubGameMode::RefreshBootstrapReferences()
{
	ResolveActiveSubmarine();
}

FTransform ASubGameMode::ResolveCrewSpawnTransform(int32 SlotIndex) const
{
	if (IsValid(ActiveSubmarine))
	{
		return ActiveSubmarine->GetCrewSpawnTransformForSlot(SlotIndex);
	}
	return FTransform::Identity;
}

void ASubGameMode::InitializePlayerCrewState(APlayerController* NewPlayer)
{
	RefreshBootstrapReferences();

	ASubPlayerController* SubPC = Cast<ASubPlayerController>(NewPlayer);
	ASubCrewCharacter* Crew = SubPC ? Cast<ASubCrewCharacter>(SubPC->GetPawn()) : nullptr;
	if (!SubPC || !Crew || !ActiveSubmarine)
	{
		UE_LOG(
			LogSubBootstrap,
			Warning,
			TEXT("InitializePlayerCrewState skipped | Controller=%s | Pawn=%s | ActiveSub=%s"),
			*GetNameSafe(NewPlayer),
			*GetNameSafe(NewPlayer ? NewPlayer->GetPawn() : nullptr),
			*GetNameSafe(ActiveSubmarine));
		return;
	}

	const int32 SlotIndex = SubPC->AssignedSpawnSlot >= 0 ? SubPC->AssignedSpawnSlot : 0;
	const FTransform SpawnTransform = ResolveCrewSpawnTransform(SlotIndex);
	Crew->EnterOnFootInSubmarine(ActiveSubmarine, SpawnTransform);

	SubPC->CurrentControlMode = ECrewControlMode::OnFoot;
	SubPC->CurrentStation = nullptr;
	SubPC->CurrentStationType = ESubStationType::None;
	SubPC->ClientSetControlMode(ECrewControlMode::OnFoot, ESubStationType::None);

	UE_LOG(
		LogSubBootstrap,
		Log,
		TEXT("InitializePlayerCrewState | Controller=%s | Crew=%s | SpawnLoc=%s | CurrentSub=%s"),
		*GetNameSafe(SubPC),
		*GetNameSafe(Crew),
		*SpawnTransform.GetLocation().ToCompactString(),
		*GetNameSafe(Crew->CurrentSubmarine));
}

void ASubGameMode::SetBootstrapPhase(ESubBootstrapPhase NewPhase)
{
	const ESubBootstrapPhase OldPhase = BootstrapPhase;
	BootstrapPhase = NewPhase;

	PhaseEnteredAtSeconds = FPlatformTime::Seconds();
	LastStallLogSeconds = 0.;

	UE_LOG(
		LogSubBootstrap,
		Log,
		TEXT("Bootstrap | Phase=%s -> %s"),
		*StaticEnum<ESubBootstrapPhase>()->GetValueAsString(OldPhase),
		*StaticEnum<ESubBootstrapPhase>()->GetValueAsString(BootstrapPhase));

	if (BootstrapPhase == ESubBootstrapPhase::Ready || BootstrapPhase == ESubBootstrapPhase::Failed)
	{
		StopBootstrapRetryTimer();
	}
}

void ASubGameMode::StartBootstrapRetryTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (BootstrapRetryTimerHandle.IsValid())
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		BootstrapRetryTimerHandle,
		this,
		&ASubGameMode::TryAdvanceBootstrap,
		0.5f,
		true);
}

void ASubGameMode::StopBootstrapRetryTimer()
{
	UWorld* World = GetWorld();
	if (!World || !BootstrapRetryTimerHandle.IsValid())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(BootstrapRetryTimerHandle);
	BootstrapRetryTimerHandle.Invalidate();
}

void ASubGameMode::LogStallIfStuck(const TCHAR* Reason)
{
	const double Now = FPlatformTime::Seconds();
	const double SecondsInPhase = Now - PhaseEnteredAtSeconds;

	if (SecondsInPhase < BootstrapPhaseTimeoutSeconds)
	{
		return;
	}

	const double SecondsSinceLog = Now - LastStallLogSeconds;
	if (LastStallLogSeconds > 0. && SecondsSinceLog < BootstrapPhaseTimeoutSeconds)
	{
		return;
	}

	LastStallLogSeconds = Now;

	UE_LOG(
		LogSubBootstrap,
		Error,
		TEXT("Bootstrap STALLED in phase %s for %.1fs | Reason=%s | ActiveSubmarine=%s | PendingControllers=%d"),
		*StaticEnum<ESubBootstrapPhase>()->GetValueAsString(BootstrapPhase),
		SecondsInPhase,
		Reason,
		*GetNameSafe(ActiveSubmarine),
		PendingBootstrapControllers.Num());
}

void ASubGameMode::TryAdvanceBootstrap()
{
	if (BootstrapPhase == ESubBootstrapPhase::Ready || BootstrapPhase == ESubBootstrapPhase::Failed)
	{
		return;
	}

	if (!ResolveWorldBootstrap())
	{
		LogStallIfStuck(TEXT("World not ready"));
		return;
	}
	if (BootstrapPhase < ESubBootstrapPhase::WorldReady)
	{
		SetBootstrapPhase(ESubBootstrapPhase::WorldReady);
	}

	if (!ResolveSubmarineBootstrap())
	{
		LogStallIfStuck(TEXT("Submarine not resolved or movement collision not ready"));
		return;
	}
	if (BootstrapPhase < ESubBootstrapPhase::SubResolved)
	{
		SetBootstrapPhase(ESubBootstrapPhase::SubResolved);
	}

	if (!ValidateSubmarineBootstrap())
	{
		LogStallIfStuck(TEXT("Submarine spawn collision invalid"));
		return;
	}
	if (BootstrapPhase < ESubBootstrapPhase::SubValidated)
	{
		SetBootstrapPhase(ESubBootstrapPhase::SubValidated);
	}

	if (PendingBootstrapControllers.Num() == 0)
	{
		return;
	}

	if (!SpawnAndEmbarkPendingControllers())
	{
		LogStallIfStuck(TEXT("Crew spawn or embark incomplete"));
		return;
	}
	if (BootstrapPhase < ESubBootstrapPhase::CrewEmbarked)
	{
		SetBootstrapPhase(ESubBootstrapPhase::CrewEmbarked);
	}

	SetBootstrapPhase(ESubBootstrapPhase::Ready);
}

bool ASubGameMode::ResolveWorldBootstrap()
{
	return true;
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
		UE_LOG(LogSubBootstrap, Log, TEXT("Bootstrap | Sub movement collision not ready | Sub=%s"), *GetNameSafe(ActiveSubmarine));
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
			const ASubPlayerController* SubPC = Cast<ASubPlayerController>(PC);
			const int32 SlotIndex = SubPC ? SubPC->AssignedSpawnSlot : 0;
			const FTransform SpawnTransform = ResolveCrewSpawnTransform(SlotIndex);
			RestartPlayerAtTransform(PC, SpawnTransform);
			ExistingCrew = Cast<ASubCrewCharacter>(PC->GetPawn());
		}

		if (!ExistingCrew)
		{
			UE_LOG(LogSubBootstrap, Warning, TEXT("Bootstrap | Failed to spawn crew for %s"), *GetNameSafe(PC));
			bAllSucceeded = false;
			continue;
		}

		InitializePlayerCrewState(PC);

		if (!ValidateCrewBootstrap(ExistingCrew))
		{
			const UCharacterMovementComponent* MovementComponent = ExistingCrew->GetCharacterMovement();
			const bool bAcceptedBase = IsAcceptedCrewBaseForSubmarine(ActiveSubmarine, ExistingCrew->GetMovementBase());
			UE_LOG(
				LogSubBootstrap,
				Warning,
				TEXT("CrewValidation | Crew=%s | Base=%s | Walkable=%d | AcceptedBase=%d | Sub=%s"),
				*GetNameSafe(ExistingCrew),
				*GetNameSafe(ExistingCrew->GetMovementBase()),
				MovementComponent && MovementComponent->CurrentFloor.IsWalkableFloor() ? 1 : 0,
				bAcceptedBase ? 1 : 0,
				*GetNameSafe(ExistingCrew->CurrentSubmarine));
			bAllSucceeded = false;
			continue;
		}

		PendingBootstrapControllers.RemoveAt(i);

		UE_LOG(
			LogSubBootstrap,
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

	const UPrimitiveComponent* MovementBase = Crew->GetMovementBase();
	return Crew->CurrentSubmarine == ActiveSubmarine
		&& MovementBase != nullptr
		&& Move->CurrentFloor.IsWalkableFloor()
		&& IsAcceptedCrewBaseForSubmarine(ActiveSubmarine, MovementBase)
		&& Move->MovementMode == MOVE_Walking;
}
