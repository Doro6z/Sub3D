#include "SubGameState.h"

#include "Net/UnrealNetwork.h"

ASubGameState::ASubGameState()
{
	bReplicates = true;
}

void ASubGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASubGameState, CurrentPhase);
	DOREPLIFETIME(ASubGameState, bBreachActive);
	DOREPLIFETIME(ASubGameState, BreachedCompartmentId);
	DOREPLIFETIME(ASubGameState, bDockingAligned);
}

void ASubGameState::ApplyRunPhase(ESubRunPhase NewPhase)
{
	CurrentPhase = NewPhase;
	OnRunPhaseChanged.Broadcast(CurrentPhase);
}

void ASubGameState::OnRep_RunPhase()
{
	OnRunPhaseChanged.Broadcast(CurrentPhase);
}
