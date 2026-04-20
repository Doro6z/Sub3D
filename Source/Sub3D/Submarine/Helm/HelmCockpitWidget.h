#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HelmCockpitWidget.generated.h"

class UBorder;
class UVerticalBox;
class USubHelmWidget;
class UHelmThrottleTelegraphWidget;
class UHelmRudderYokeWidget;
class UHelmDiveBoardWidget;
class UHelmKillSwitchWidget;

// Top-level cockpit container that replaces UHelmControlPanelWidget. Hosts
// 4 instrument widgets (throttle / yoke / dive / kill) and a single helm
// shell pointer that all instruments read through.
UCLASS(Blueprintable)
class SUB3D_API UHelmCockpitWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Helm")
	void InitForHelmShell(USubHelmWidget* InHelmShell);

	UFUNCTION(BlueprintPure, Category = "Helm")
	bool IsBoundToHelmShell() const;

	// Accessor for child instruments to reach the helm shell without
	// caching their own pointer.
	UFUNCTION(BlueprintPure, Category = "Helm")
	USubHelmWidget* GetHelmShell() const { return CachedHelmShell.Get(); }

private:
	void BuildWidgetTree();
	void TryResolveHelmShellFromOuter();

	UPROPERTY(Transient)
	TWeakObjectPtr<USubHelmWidget> CachedHelmShell;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UHelmThrottleTelegraphWidget> ThrottleInstrument;

	UPROPERTY(Transient)
	TObjectPtr<UHelmRudderYokeWidget> RudderInstrument;

	UPROPERTY(Transient)
	TObjectPtr<UHelmDiveBoardWidget> DiveInstrument;

	UPROPERTY(Transient)
	TObjectPtr<UHelmKillSwitchWidget> KillSwitchInstrument;
};
