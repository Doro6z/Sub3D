#include "SubDoorActor_Exterior.h"

#include "Components/StaticMeshComponent.h"

ASubDoorActor_Exterior::ASubDoorActor_Exterior()
{
	// Two battants attached to the same root as the base ASubDoorActor.
	// Each is positioned at Y = 0 by default; BP authoring sets the mesh on
	// each and the Timeline animates local Y by +/- BattantExtensionCm.
	BattantPort = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BattantPort"));
	BattantPort->SetupAttachment(Root);
	BattantPort->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	BattantStbd = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BattantStbd"));
	BattantStbd->SetupAttachment(Root);
	BattantStbd->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}
