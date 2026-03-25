#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SubHelmWidget.generated.h"

class ASubCrewCharacter;
class ASubPlayerController;
class USubMovementComponent;

/**
 * HUD widget for helm control.
 * Bind to BP_SubHelmWidget in Blueprint.
 * Shows ballast fill, depth, speed. Exposes controls to Blueprint.
 */
UCLASS()
class SUB3D_API USubHelmWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Call this after creating the widget to bind the owning crew character
	UFUNCTION(BlueprintCallable, Category = "Helm")
	void InitForCrew(ASubCrewCharacter* Crew);

	// ── Ballast control ───────────────────────────────────────────────────

	// Set global ballast target (0=surface, 1=dive). Calls Server RPC.
	UFUNCTION(BlueprintCallable, Category = "Helm|Ballast")
	void SetGlobalBallast(float Target);

	// Set individual ballast (0=front, 1=rear). Calls Server RPC.
	UFUNCTION(BlueprintCallable, Category = "Helm|Ballast")
	void SetBallastByIndex(int32 Index, float Target);

	// ── Read state (call from Blueprint tick or binding) ──────────────────

	// Returns fill level of ballast at index (0-1). Returns -1 if invalid.
	UFUNCTION(BlueprintPure, Category = "Helm|Ballast")
	float GetBallastFillLevel(int32 Index) const;

	// Returns current depth in meters
	UFUNCTION(BlueprintPure, Category = "Helm")
	float GetDepth() const;

	// Returns current speed in km/h
	UFUNCTION(BlueprintPure, Category = "Helm")
	float GetSpeedKmh() const;

	// Returns current pitch in degrees
	UFUNCTION(BlueprintPure, Category = "Helm")
	float GetPitch() const;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	ASubCrewCharacter* OwnerCrew = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	ASubPlayerController* OwnerController = nullptr;

	// Cached reference to SubMovement for reads
	UPROPERTY(BlueprintReadOnly, Category = "Helm")
	USubMovementComponent* SubMovement = nullptr;

	void ResolveRuntimeRefs();
};
