#pragma once

#include "CoreMinimal.h"
#include "SubDoorActor.h"
#include "SubDoorActor_Exterior.generated.h"

class UStaticMeshComponent;

/**
 * Airlock exit door (exterior / SAS exit).
 *
 * Two sliding battants (port + starboard) sharing a common center pivot.
 * Each battant slides laterally along its local Y axis by `BattantExtensionCm`
 * when the door opens, and returns to the closed position when the door closes.
 *
 * The state (open / closed) is inherited from ASubDoorActor and replicated as
 * `bClosed`. The visible animation (Timeline / Lerp / FX) is authored in BP;
 * this class exposes the two battant mesh components + the two parameters the
 * animation consumes:
 *   - BattantExtensionCm : lateral distance each leaf slides, default 90 cm
 *   - OpenDurationSec    : full open / close animation duration, default 5 s
 *
 * Use as C++ parent of BP_SubDoor_Exterior.
 */
UCLASS(Blueprintable)
class SUB3D_API ASubDoorActor_Exterior : public ASubDoorActor
{
	GENERATED_BODY()

public:
	ASubDoorActor_Exterior();

	/** Port (-Y) battant of the airlock door. Authored pivot sits at the battant center. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BattantPort;

	/** Starboard (+Y) battant of the airlock door. Authored pivot sits at the battant center. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BattantStbd;

	/** Distance in cm each battant slides laterally along its local Y axis when opening.
	 *  Applied outward: Port moves to -Y, Stbd moves to +Y. Default 90 cm per leaf. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exterior Door", meta = (ClampMin = "0.0"))
	float BattantExtensionCm = 90.f;

	/** Total duration of the open animation in seconds. The close animation uses the
	 *  same value. Default 5 seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exterior Door", meta = (ClampMin = "0.1"))
	float OpenDurationSec = 5.f;

	/** Convenience accessor for BP Timelines that want the world-space open target for Port. */
	UFUNCTION(BlueprintPure, Category = "Exterior Door")
	FVector GetPortOpenOffsetLocal() const { return FVector(0.f, -BattantExtensionCm, 0.f); }

	/** Convenience accessor for BP Timelines that want the world-space open target for Stbd. */
	UFUNCTION(BlueprintPure, Category = "Exterior Door")
	FVector GetStbdOpenOffsetLocal() const { return FVector(0.f,  BattantExtensionCm, 0.f); }
};
