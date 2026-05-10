#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "SubGameMode.generated.h"

class ASubCrewCharacter;
class ASubmarineBase;

UENUM(BlueprintType)
enum class ESubBootstrapPhase : uint8
{
	None       UMETA(DisplayName = "None"),
	WorldReady UMETA(DisplayName = "World Ready"),
	SubResolved UMETA(DisplayName = "Sub Resolved"),
	SubValidated UMETA(DisplayName = "Sub Validated"),
	CrewSpawned UMETA(DisplayName = "Crew Spawned"),
	CrewEmbarked UMETA(DisplayName = "Crew Embarked"),
	Ready      UMETA(DisplayName = "Ready"),
	Failed     UMETA(DisplayName = "Failed")
};

/**
 * GameMode for prototype bootstrap.
 * Resolves the active submarine and spawns/embarks crew.
 */
UCLASS()
class SUB3D_API ASubGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ASubGameMode();

	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	UPROPERTY(BlueprintReadWrite, Category = "Submarine")
	ASubmarineBase* ActiveSubmarine = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Submarine|Bootstrap")
	ESubBootstrapPhase BootstrapPhase = ESubBootstrapPhase::None;

	/**
	 * Watchdog: if TryAdvanceBootstrap fails to advance the phase for this many seconds,
	 * an Error log is emitted and repeated at the same cadence while the phase is stuck.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Submarine|Bootstrap", meta = (ClampMin = "1.0"))
	float BootstrapPhaseTimeoutSeconds = 5.f;

	UFUNCTION(BlueprintPure, Category = "Submarine|Bootstrap")
	ESubBootstrapPhase GetBootstrapPhase() const { return BootstrapPhase; }

	UFUNCTION(BlueprintCallable, Category = "Submarine|Bootstrap")
	void TryAdvanceBootstrap();

protected:
	ASubmarineBase* ResolveActiveSubmarine();
	void RefreshBootstrapReferences();
	FTransform ResolveCrewSpawnTransform(int32 SlotIndex) const;
	int32 AssignNextSpawnSlot();
	void InitializePlayerCrewState(APlayerController* NewPlayer);

	void SetBootstrapPhase(ESubBootstrapPhase NewPhase);
	bool ResolveWorldBootstrap();
	bool ResolveSubmarineBootstrap();
	bool ValidateSubmarineBootstrap();
	bool SpawnAndEmbarkPendingControllers();
	bool ValidateCrewBootstrap(ASubCrewCharacter* Crew) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APlayerController>> PendingBootstrapControllers;

private:
	void StartBootstrapRetryTimer();
	void StopBootstrapRetryTimer();
	void LogStallIfStuck(const TCHAR* Reason);

	double PhaseEnteredAtSeconds = 0.;
	double LastStallLogSeconds = 0.;
	FTimerHandle BootstrapRetryTimerHandle;
};
