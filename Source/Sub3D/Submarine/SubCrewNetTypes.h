#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

// Forward decl only — avoids circular include with SubCrewMovementComponent.h.
class USubCrewMovementComponent;
enum class ECrewEmbarkState : uint8;

/**
 * Handoff event bit flags packed into the move payload. Cleared each tick; set by
 * ASubCrewCharacter::HandleHullCrossing when a boundary crossing fires. The server
 * re-applies the corresponding state flip on receive so the prediction converges
 * even if the server's own boundary overlap missed the crossing this frame.
 */
enum class ECrewHandoffKind : uint8
{
	None = 0,
	Outgoing = 1,  // Embarked -> Outside
	Incoming = 2   // Outside -> Embarked
};

/**
 * Extended saved move. Replays the crew's grid-space pose and locomotion state on
 * client correction, and carries the same data into the network payload via
 * FCharacterNetworkMoveData_SubCrew.
 */
class FSavedMove_SubCrew : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	FTransform SavedGridSpaceTransform = FTransform::Identity;
	// Stored as uint8 to avoid the full enum definition in this header (forward-declared).
	uint8 SavedEmbarkStateByte = 0;
	ECrewHandoffKind SavedHandoff = ECrewHandoffKind::None;

	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* Character) override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
};

/** Prediction data factory that allocates FSavedMove_SubCrew instances. */
class FNetworkPredictionData_Client_SubCrew : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	FNetworkPredictionData_Client_SubCrew(const UCharacterMovementComponent& ClientMovement);
	virtual FSavedMovePtr AllocateNewMove() override;
};

/**
 * Network payload extension. Packs GridSpaceTransform + EmbarkState + handoff event
 * into the ServerMove RPC data. Server-side reads these to converge predictive state.
 */
struct SUB3D_API FCharacterNetworkMoveData_SubCrew : public FCharacterNetworkMoveData
{
	typedef FCharacterNetworkMoveData Super;

	FTransform GridSpaceTransform = FTransform::Identity;
	// EmbarkState as uint8 (opaque here; cast to ECrewEmbarkState server-side).
	uint8 EmbarkStateByte = 0;
	ECrewHandoffKind Handoff = ECrewHandoffKind::None;

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;
};

/**
 * Container of three move data slots (new / pending / old) required by CMC's move
 * combining and re-send logic.
 */
struct SUB3D_API FCharacterNetworkMoveDataContainer_SubCrew : public FCharacterNetworkMoveDataContainer
{
	FCharacterNetworkMoveDataContainer_SubCrew()
	{
		NewMoveData = &MoveData[0];
		PendingMoveData = &MoveData[1];
		OldMoveData = &MoveData[2];
	}

	FCharacterNetworkMoveData_SubCrew MoveData[3];
};
