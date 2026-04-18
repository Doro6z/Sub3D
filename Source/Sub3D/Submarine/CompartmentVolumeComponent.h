#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "CompartmentVolumeComponent.generated.h"

/**
 * Editor-placed box volume that defines a compartment region inside the submarine.
 * Multiple volumes with the same CompartmentId are merged during bake.
 * Place these as children of the submarine root in the BP, then call BakeLayoutFromVolumes.
 */
UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API UCompartmentVolumeComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UCompartmentVolumeComponent();

	/** Compartment name. Volumes sharing the same name are merged into one compartment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FName CompartmentId = NAME_None;

	/** Display name shown in UI (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	FText DisplayName;

	/** Water capacity in liters. 0 = auto-calculate from volume box dimensions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment", meta = (ClampMin = "0.0"))
	float CapacityLitersOverride = 0.f;

	/** Floor Z in local space of the submarine. 0 = use bottom of the box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment")
	float WalkableFloorZCmOverride = 0.f;

	/** Editor wireframe color for this compartment volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compartment|Visual")
	FColor VolumeColor = FColor(50, 180, 220, 255);

	virtual void OnRegister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
