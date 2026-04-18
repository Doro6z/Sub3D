#pragma once

#include "CoreMinimal.h"
#include "SubNavWidget.h"
#include "SubTurretWidget.generated.h"

class USubHullComponent;

/**
 * HUD for the exterior turret station.
 * Drawn fully in Slate (NativePaint) so it can overlay the turret camera
 * view without requiring a Blueprint canvas. Reads state from the submarine's
 * ExteriorTurret (ATurretActor) via the owning crew character.
 *
 * Visuals:
 *  - Reticle (crosshair + outer ring).
 *  - Aim-offset indicator (current vs target aim).
 *  - Ammo gauge (horizontal bar, bottom-center).
 *  - Firing flash pulse on each fire event.
 *  - Screen shake on hull damage events.
 *  - Offline overlay when the turret is not online.
 */
UCLASS()
class SUB3D_API USubTurretWidget : public USubNavStationWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Submarine|Turret")
	void SetTurretAim(const FRotator& NewAim);

	UFUNCTION(BlueprintCallable, Category = "Submarine|Turret")
	void SetTurretFireHeld(bool bHeld);

	UFUNCTION(BlueprintPure, Category = "Submarine|Turret")
	FRotator GetCurrentTurretAim() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Turret")
	bool IsTurretOnline() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Turret")
	float GetAmmoPercent() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Turret")
	int32 GetCurrentAmmo() const;

	UFUNCTION(BlueprintPure, Category = "Submarine|Turret")
	int32 GetMaxAmmo() const;

	// --- Visual tuning (editable in Blueprint defaults) ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "Turret|HUD|Reticle")
	FLinearColor ReticleColor = FLinearColor(0.2f, 1.f, 0.3f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Turret|HUD|Reticle")
	float ReticleSizePx = 28.f;

	UPROPERTY(EditDefaultsOnly, Category = "Turret|HUD|Reticle")
	float ReticleRingRadiusPx = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Turret|HUD|Gauge")
	FVector2D AmmoGaugeSizePx = FVector2D(260.f, 14.f);

	UPROPERTY(EditDefaultsOnly, Category = "Turret|HUD|Gauge")
	float AmmoGaugeBottomMarginPx = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "Turret|HUD|FireFlash")
	FLinearColor FireFlashColor = FLinearColor(1.f, 0.85f, 0.4f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Turret|HUD|FireFlash", meta = (ClampMin = "0.01"))
	float FireFlashDecaySeconds = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Turret|HUD|Shake", meta = (ClampMin = "0.0"))
	float ShakeImpulsePx = 14.f;

	UPROPERTY(EditDefaultsOnly, Category = "Turret|HUD|Shake", meta = (ClampMin = "0.01"))
	float ShakeDecayPerSecond = 6.f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	UFUNCTION()
	void HandleHullDamageUpdated();

	USubHullComponent* ResolveHullForBinding();

	// Watches the replicated LastFireServerTime to trigger the fire flash pulse.
	void PollFireEvent();

	// Runtime state
	UPROPERTY(Transient)
	float FireFlashAlpha = 0.f;

	UPROPERTY(Transient)
	FVector2D ShakeOffsetPx = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	float LastObservedFireTime = 0.f;

	UPROPERTY(Transient)
	TWeakObjectPtr<USubHullComponent> BoundHull;
};
