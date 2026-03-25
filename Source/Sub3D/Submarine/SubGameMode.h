#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SubGameMode.generated.h"

class ASubmarineBase;

/**
 * GameMode for the submarine prototype.
 * Spawn and boarding logic handled in Blueprint (BP_SubGameMode).
 */
UCLASS()
class SUB3D_API ASubGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASubGameMode();
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// The shared submarine instance — set by Blueprint after spawn
	UPROPERTY(BlueprintReadWrite, Category = "Submarine")
	ASubmarineBase* ActiveSubmarine;

	// Optional local-space spawn offset from HelmSocket when spawning crew on foot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submarine")
	FVector CrewSpawnOffset = FVector(0.f, 0.f, 0.f);

private:
	ASubmarineBase* ResolveActiveSubmarine();
};
