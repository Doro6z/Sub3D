#include "SubGameMode.h"
#include "SubCrewCharacter.h"
#include "SubPlayerController.h"
#include "SubmarineBase.h"
#include "EngineUtils.h"

ASubGameMode::ASubGameMode()
{
	DefaultPawnClass       = ASubCrewCharacter::StaticClass();
	PlayerControllerClass  = ASubPlayerController::StaticClass();
	ActiveSubmarine        = nullptr;
}

void ASubGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ASubPlayerController* SubPC = Cast<ASubPlayerController>(NewPlayer);
	if (!SubPC) return;

	ASubmarineBase* Submarine = ResolveActiveSubmarine();
	ASubCrewCharacter* Crew = Cast<ASubCrewCharacter>(SubPC->GetPawn());
	if (!Submarine || !Crew) return;

	FTransform SpawnXform = Submarine->GetActorTransform();
	if (Submarine->HelmSocket)
	{
		SpawnXform = Submarine->HelmSocket->GetComponentTransform();
		SpawnXform.AddToTranslation(CrewSpawnOffset);
	}

	Crew->EnterOnFootInSubmarine(Submarine, SpawnXform);

	SubPC->CurrentControlMode = ECrewControlMode::OnFoot;
	SubPC->CurrentStation = nullptr;
	SubPC->CurrentStationType = ESubStationType::None;
	SubPC->ClientSetControlMode(ECrewControlMode::OnFoot);
}

ASubmarineBase* ASubGameMode::ResolveActiveSubmarine()
{
	if (ActiveSubmarine)
	{
		return ActiveSubmarine;
	}

	if (!GetWorld()) return nullptr;

	for (TActorIterator<ASubmarineBase> It(GetWorld()); It; ++It)
	{
		ActiveSubmarine = *It;
		break;
	}

	return ActiveSubmarine;
}
