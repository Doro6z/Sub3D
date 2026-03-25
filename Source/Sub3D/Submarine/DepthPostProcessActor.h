#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DepthPostProcessActor.generated.h"

class ASubmarineBase;
class AExponentialHeightFog;
class ADirectionalLight;

/**
 * Actor that adjusts the level's fog and lighting based on a submarine's depth.
 * Used in Proto 02 to simulate deep water immersion.
 */
UCLASS()
class SUB3D_API ADepthPostProcessActor : public AActor
{
	GENERATED_BODY()

public:
	ADepthPostProcessActor();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// ── Setup ──────────────────────────────────────────────────────────

	// The submarine to track for depth
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	ASubmarineBase* TrackedSubmarine;

	// The fog actor to modify
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	AExponentialHeightFog* DepthFog;

	// The directional light (sun) to dim
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	ADirectionalLight* SunLight;

	// ── Tuning ─────────────────────────────────────────────────────────

	// Depth (in cm) at which the "Deep" settings are fully applied
	UPROPERTY(EditAnywhere, Category = "Tuning")
	float MaxDepthForPP = 120000.f;

	// Fog color at zero depth
	UPROPERTY(EditAnywhere, Category = "Tuning")
	FLinearColor ShallowColor = FLinearColor(0.0f, 0.5f, 0.8f, 1.f);

	// Fog color at MaxDepthForPP
	UPROPERTY(EditAnywhere, Category = "Tuning")
	FLinearColor DeepColor = FLinearColor(0.0f, 0.0f, 0.05f, 1.f);
};
