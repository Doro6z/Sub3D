#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "SubNavWidget.h"
#include "SubSonarV2Types.h"
#include "SubHelmWidget.generated.h"

class USubSonarDisplayWidget;
class USubSonarComponent;
class USubSonarSystemComponent;
class UHelmNavigationDisplayWidget;
class UHelmNavigationDisplayComponent;
class UReconstructionViewWidget;
class UTacticalGraphViewWidget;

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

	UFUNCTION(BlueprintPure, Category = "HelmNav")
	bool IsHelmNavigationDisplayBound() const;

	UFUNCTION(BlueprintPure, Category = "HelmNav")
	bool IsReconstructionViewBound() const;

	UFUNCTION(BlueprintPure, Category = "HelmNav")
	bool IsTacticalGraphViewBound() const;

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSetSonarMode(ESonarMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSetSonarFocusBearing(float BearingDeg);

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSetSonarRangePreset(int32 PresetIndex);

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteIncreaseSonarRangePreset();

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteDecreaseSonarRangePreset();

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteAdjustSonarRangePreset(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteSetSonarRangeNormalized(float Normalized01);

	UFUNCTION(BlueprintPure, Category = "Sonar")
	int32 GetCurrentSonarRangePresetIndex() const;

	UFUNCTION(BlueprintPure, Category = "Sonar")
	int32 GetCurrentSonarRangePresetCount() const;

	UFUNCTION(BlueprintPure, Category = "Sonar")
	float GetCurrentSonarDisplayRangeCm() const;

	UFUNCTION(BlueprintCallable, Category = "Sonar")
	void RouteMarkPriorityTrack(int32 TrackId, bool bPriority);

	// Reference to the sonar CRT display sub-widget.
	// Can be assigned from BP or discovered from the widget tree at runtime.
	UPROPERTY(BlueprintReadWrite, Category = "Sonar", meta = (BindWidgetOptional))
	TObjectPtr<USubSonarDisplayWidget> SonarDisplay;

	// Legacy transitional widget. Prefer ReconstructionView + TacticalGraphView.
	UPROPERTY(BlueprintReadWrite, Category = "HelmNav", meta = (BindWidgetOptional))
	TObjectPtr<UHelmNavigationDisplayWidget> HelmNavigationDisplay;

	UPROPERTY(BlueprintReadWrite, Category = "HelmNav", meta = (BindWidgetOptional))
	TObjectPtr<UReconstructionViewWidget> ReconstructionView;

	UPROPERTY(BlueprintReadWrite, Category = "HelmNav", meta = (BindWidgetOptional))
	TObjectPtr<UTacticalGraphViewWidget> TacticalGraphView;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WidgetBinding")
	bool bAllowWidgetTreeFallbackDiscovery = false;

	// If SonarDisplay is not provided by BP, create one automatically.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	bool bAutoCreateSonarDisplayIfMissing = false;

	// Optional class for auto-created sonar display (defaults to USubSonarDisplayWidget).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	TSubclassOf<USubSonarDisplayWidget> SonarDisplayClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	int32 AutoCreatedSonarDisplayZOrder = 60;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmNav")
	bool bAutoCreateHelmNavigationDisplayIfMissing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmNav")
	TSubclassOf<UHelmNavigationDisplayWidget> HelmNavigationDisplayClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HelmNav")
	int32 AutoCreatedHelmNavigationDisplayZOrder = 61;

private:
	void DiscoverWidgetReferencesFromTree();
	void TryBindSonarDisplay();
	void TryBindHelmNavigationDisplay();
	void TryBindReconstructionView();
	void TryBindTacticalGraphView();

	UPROPERTY(Transient)
	TWeakObjectPtr<USubSonarComponent> BoundSonar;

	UPROPERTY(Transient)
	TWeakObjectPtr<USubSonarSystemComponent> BoundSonarSystem;

	UPROPERTY(Transient)
	TWeakObjectPtr<UHelmNavigationDisplayComponent> BoundHelmNavigationDisplayComponent;

	bool bOwnsAutoCreatedSonarDisplay = false;
	bool bLoggedMissingSonarDisplay = false;
	bool bOwnsAutoCreatedHelmNavigationDisplay = false;
	bool bLoggedMissingHelmNavigationDisplay = false;
	bool bLoggedMissingReconstructionView = false;
	bool bLoggedMissingTacticalGraphView = false;
};
