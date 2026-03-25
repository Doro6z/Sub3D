#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubStationInterface.h"
#include "SubStationBase.generated.h"

class AController;
class ASubmarineBase;

/**
 * Base class for all interactive stations in the submarine.
 * Handles station occupancy and ownership plumbing.
 */
UCLASS(Abstract, Blueprintable)
class SUB3D_API ASubStationBase : public AActor, public ISubStationInterface
{
	GENERATED_BODY()

public:
	ASubStationBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Station configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station", meta = (ExposeOnSpawn = "true"))
	ESubStationType StationType = ESubStationType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	bool bStationEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	bool bExclusiveOccupancy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	bool bAutoResolveOwningSubmarine = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station")
	bool bRequireOwningSubmarine = true;

	// Runtime state
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Station")
	TObjectPtr<ASubmarineBase> OwningSubmarine = nullptr;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Station")
	TObjectPtr<AController> CurrentOccupant = nullptr;

	UFUNCTION(BlueprintPure, Category = "Station")
	bool IsOccupied() const;

	UFUNCTION(BlueprintCallable, Category = "Station")
	void SetOwningSubmarine(ASubmarineBase* InSubmarine);

	// ISubStationInterface implementation
	virtual bool CanEnterStation_Implementation(AController* Controller) const override;
	virtual void RequestEnterStation_Implementation(AController* Controller) override;
	virtual void RequestExitStation_Implementation(AController* Controller) override;
	virtual ESubStationType GetStationType_Implementation() const override;
	virtual AController* GetCurrentOccupant_Implementation() const override;
	virtual ASubmarineBase* GetOwningSubmarine_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Blueprint hooks for UI and feedback.
	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnStationEntered(AController* Controller);

	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnStationExited(AController* Controller);

private:
	void TryAutoResolveOwningSubmarine();
};
