#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "SubRunPhase.h"
#include "SubGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSubRunPhaseChangedSignature, ESubRunPhase, NewPhase);

UCLASS()
class SUB3D_API ASubGameState : public AGameState
{
	GENERATED_BODY()

public:
	ASubGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_RunPhase, BlueprintReadOnly, Category = "Run")
	ESubRunPhase CurrentPhase = ESubRunPhase::Boot;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run")
	bool bBreachActive = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run")
	FName BreachedCompartmentId = NAME_None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run")
	bool bDockingAligned = false;

	UPROPERTY(BlueprintAssignable, Category = "Run")
	FSubRunPhaseChangedSignature OnRunPhaseChanged;

	UFUNCTION(BlueprintPure, Category = "Run")
	ESubRunPhase GetCurrentPhase() const
	{
		return CurrentPhase;
	}

	void ApplyRunPhase(ESubRunPhase NewPhase);

protected:
	UFUNCTION()
	void OnRep_RunPhase();
};
