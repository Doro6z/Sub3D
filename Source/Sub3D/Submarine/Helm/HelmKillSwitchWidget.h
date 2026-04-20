#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HelmKillSwitchWidget.generated.h"

class UButton;
class UTextBlock;

// Single big red latch that toggles bStabilizationMasterEnabled. When
// "killed", the master is off and every autopilot is inert regardless
// of its per-axis flag. Pressing again re-enables.
UCLASS(Blueprintable)
class SUB3D_API UHelmKillSwitchWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION() void HandleToggle();

	void BuildWidgetTree();
	void RefreshFromHelmData();

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;

	bool bMasterEnabledCached = true;
};
