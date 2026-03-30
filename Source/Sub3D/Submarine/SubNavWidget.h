#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SubNavWidget.generated.h"

class ASubCrewCharacter;
class ASubPlayerController;
class USubMovementComponent;

/**
 * HUD widget for navigation stations.
 * Base class for Helm, Ballast, etc.
 */
UCLASS()
class SUB3D_API USubNavStationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Call this after creating the widget to bind the owning crew character
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void InitForCrew(ASubCrewCharacter* Crew);

	// ── Ballast control ───────────────────────────────────────────────────

	// Set global ballast target (0=surface, 1=dive). Calls Server RPC.
	UFUNCTION(BlueprintCallable, Category = "Navigation|Ballast")
	void SetGlobalBallast(float Target);

	// Set individual ballast (0=front, 1=rear). Calls Server RPC.
	UFUNCTION(BlueprintCallable, Category = "Navigation|Ballast")
	void SetBallastByIndex(int32 Index, float Target);

	// ── Read state (call from Blueprint tick or binding) ──────────────────

	// Returns fill level of ballast at index (0-1). Returns -1 if invalid.
	UFUNCTION(BlueprintPure, Category = "Navigation|Ballast")
	float GetBallastFillLevel(int32 Index) const;

	// Returns current depth in meters
	UFUNCTION(BlueprintPure, Category = "Navigation")
	float GetDepth() const;

	// Returns current speed in km/h
	UFUNCTION(BlueprintPure, Category = "Navigation")
	float GetSpeedKmh() const;

	// Returns current pitch in degrees
	UFUNCTION(BlueprintPure, Category = "Navigation")
	float GetPitch() const;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	TWeakObjectPtr<ASubCrewCharacter> OwnerCrew = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	TWeakObjectPtr<ASubPlayerController> OwnerController = nullptr;

	// Cached reference to SubMovement for reads
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	TWeakObjectPtr<USubMovementComponent> SubMovement = nullptr;

	void ResolveRuntimeRefs();
};
