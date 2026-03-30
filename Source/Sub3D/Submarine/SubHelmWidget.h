#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "SubNavWidget.h"
#include "SubSonarV2Types.h"
#include "SubHelmWidget.generated.h"

class USubSonarDisplayWidget;
class USubSonarComponent;
class USubSonarSystemComponent;

/**
 * Specialized widget for the Helm Station.
 * Extends SubNavStationWidget with sonar display binding.
 * The SonarDisplay reference is set in Blueprint after widget construction.
 */
UCLASS()
class SUB3D_API USubHelmWidget : public USubNavStationWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSonarPing();

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSonarPingHeldStart();

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSonarPingHeldStop();

	UFUNCTION(BlueprintPure, Category = "Sonar")
	bool IsSonarDisplayBound() const;

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSetSonarMode(ESonarMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSetSonarFocusBearing(float BearingDeg);

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSetSonarRangePreset(int32 PresetIndex);

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteMarkPriorityTrack(int32 TrackId, bool bPriority);

	// Reference to the sonar CRT display sub-widget.
	// Set this in BP_HelmWidget after creating the sonar display widget.
	UPROPERTY(BlueprintReadWrite, Category = "Sonar")
	TObjectPtr<USubSonarDisplayWidget> SonarDisplay;

	// If SonarDisplay is not provided by BP, create one automatically.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	bool bAutoCreateSonarDisplayIfMissing = false;

	// Optional class for auto-created sonar display (defaults to USubSonarDisplayWidget).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	TSubclassOf<USubSonarDisplayWidget> SonarDisplayClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	int32 AutoCreatedSonarDisplayZOrder = 60;

private:
	void TryBindSonarDisplay();

	UPROPERTY(Transient)
	TWeakObjectPtr<USubSonarComponent> BoundSonar;

	UPROPERTY(Transient)
	TWeakObjectPtr<USubSonarSystemComponent> BoundSonarSystem;

	bool bOwnsAutoCreatedSonarDisplay = false;
	bool bLoggedMissingSonarDisplay = false;
};
