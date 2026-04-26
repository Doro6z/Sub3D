#include "SubCrewNetTypes.h"

#include "SubCrewCharacter.h"
#include "SubCrewMovementComponent.h"

// ─── FSavedMove_SubCrew ─────────────────────────────────────────────────────

void FSavedMove_SubCrew::Clear()
{
	Super::Clear();
	SavedGridSpaceTransform = FTransform::Identity;
	SavedEmbarkStateByte = static_cast<uint8>(ECrewEmbarkState::Outside);
	SavedHandoff = ECrewHandoffKind::None;
}

void FSavedMove_SubCrew::SetMoveFor(
	ACharacter* Character,
	float InDeltaTime,
	FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

	if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(Character->GetCharacterMovement()))
	{
		SavedGridSpaceTransform = CrewMov->GridSpaceTransform;
		SavedEmbarkStateByte = static_cast<uint8>(CrewMov->EmbarkState);
		SavedHandoff = CrewMov->ConsumePendingHandoff();
	}
}

void FSavedMove_SubCrew::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);

	if (USubCrewMovementComponent* CrewMov = Cast<USubCrewMovementComponent>(Character->GetCharacterMovement()))
	{
		// Replay the saved grid-space state back onto the component. This runs when
		// CMC re-simulates after a server correction.
		CrewMov->GridSpaceTransform = SavedGridSpaceTransform;
		CrewMov->EmbarkState = static_cast<ECrewEmbarkState>(SavedEmbarkStateByte);
	}
}

bool FSavedMove_SubCrew::CanCombineWith(
	const FSavedMovePtr& NewMove,
	ACharacter* InCharacter,
	float MaxDelta) const
{
	const FSavedMove_SubCrew* NewSubMove = static_cast<const FSavedMove_SubCrew*>(NewMove.Get());
	// Never combine moves across an EmbarkState change or a handoff event: the server
	// needs to see each state transition explicitly.
	if (NewSubMove->SavedEmbarkStateByte != SavedEmbarkStateByte)
	{
		return false;
	}
	if (NewSubMove->SavedHandoff != ECrewHandoffKind::None || SavedHandoff != ECrewHandoffKind::None)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

// ─── FNetworkPredictionData_Client_SubCrew ──────────────────────────────────

FNetworkPredictionData_Client_SubCrew::FNetworkPredictionData_Client_SubCrew(
	const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_SubCrew::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_SubCrew());
}

// ─── FCharacterNetworkMoveData_SubCrew ──────────────────────────────────────

void FCharacterNetworkMoveData_SubCrew::ClientFillNetworkMoveData(
	const FSavedMove_Character& ClientMove,
	ENetworkMoveType MoveType)
{
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	const FSavedMove_SubCrew& SubMove = static_cast<const FSavedMove_SubCrew&>(ClientMove);
	GridSpaceTransform = SubMove.SavedGridSpaceTransform;
	EmbarkStateByte = SubMove.SavedEmbarkStateByte;
	Handoff = SubMove.SavedHandoff;
}

bool FCharacterNetworkMoveData_SubCrew::Serialize(
	UCharacterMovementComponent& CharacterMovement,
	FArchive& Ar,
	UPackageMap* PackageMap,
	ENetworkMoveType MoveType)
{
	bool bResult = Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	// FTransform has a native archive operator.
	Ar << GridSpaceTransform;
	Ar << EmbarkStateByte;

	uint8 HandoffByte = static_cast<uint8>(Handoff);
	Ar << HandoffByte;
	Handoff = static_cast<ECrewHandoffKind>(HandoffByte);

	return bResult && !Ar.IsError();
}
