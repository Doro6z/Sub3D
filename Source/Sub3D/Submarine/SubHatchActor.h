#pragma once

#include "CoreMinimal.h"
#include "SubDoorActor.h"
#include "SubHatchActor.generated.h"

/**
 * Horizontal hatch passage between two stacked compartments. Inherits the entire
 * interaction + flood graph + replication contract from ASubDoorActor — only the
 * intent is different (hatch = horizontal opening, door = vertical opening).
 *
 * Use cases for Sub3D:
 *  - Vertical access between decks (e.g. C_main_Fwd → C_lower_Hub)
 *  - Floor / ceiling trapdoor inside a multi-deck compartment
 *
 * Spawn selection: ASubmarineBase::SpawnDoorsFromDefinition spawns this class for
 * connections typed `EConnectionType::Hatch`. The corresponding UPROPERTY on the sub is
 * `GeneratorHatchActorClass`.
 *
 * Subclass via Blueprint (BP_SubHatch) to assign a horizontal mesh + animation. All
 * gameplay (open/close, replication, flood bridge) works out of the box.
 */
UCLASS(Blueprintable)
class SUB3D_API ASubHatchActor : public ASubDoorActor
{
	GENERATED_BODY()

public:
	ASubHatchActor();
};
