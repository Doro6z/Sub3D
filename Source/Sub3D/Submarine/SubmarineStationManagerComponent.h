#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SubmarineTypes.h"
#include "SubmarineStationManagerComponent.generated.h"

class ASubStationBase;
class AController;
class USubmarineDefinition;

UCLASS(ClassGroup = (Submarine), meta = (BlueprintSpawnableComponent))
class SUB3D_API USubmarineStationManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USubmarineStationManagerComponent();

	UFUNCTION(BlueprintCallable, Category = "Submarine|Stations")
	void RegisterStation(ASubStationBase* Station);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Stations")
	void UnregisterStation(ASubStationBase* Station);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Stations")
	void DiscoverAttachedStations();

	/** Spawn station actors from GeneratedDefinition StationSlots and register them. */
	UFUNCTION(BlueprintCallable, Category = "Submarine|Stations")
	void SpawnStationsFromDefinition(const USubmarineDefinition* Definition);

	UFUNCTION(BlueprintPure, Category = "Submarine|Stations")
	ASubStationBase* GetFirstStationOfType(ESubStationType StationType) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Stations")
	bool IsStationTypeOccupied(ESubStationType StationType) const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Stations")
	const TArray<ASubStationBase*>& GetRegisteredStations() const { return RegisteredStations; }

	/** Map station type → concrete class to spawn. Assign in editor or Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine|Stations")
	TMap<ESubStationType, TSubclassOf<ASubStationBase>> StationClassMap;

private:
	UPROPERTY()
	TArray<TObjectPtr<ASubStationBase>> RegisteredStations;
};
