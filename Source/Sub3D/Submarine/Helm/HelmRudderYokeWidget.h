#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HelmCockpitState.h"
#include "HelmRudderYokeWidget.generated.h"

class UButton;
class UTextBlock;

// Rudder yoke: rotary wheel drawn via NativePaint that the player drags
// horizontally to command rudder. Drag releases on mouse-up (classical
// spring-back via RudderReturnRate). HOLD RUDDER latches the current
// command. Heading + yaw rate readouts below.
UCLASS(Blueprintable)
class SUB3D_API UHelmRudderYokeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION() void HandleHoldRudderToggle();
	UFUNCTION() void HandleRecenter();

	void BuildWidgetTree();
	void RefreshFromHelmData();
	void PushRudderFromDrag(float LocalX, float WidgetHalfWidth);

	UPROPERTY(Transient)
	TObjectPtr<UButton> HoldRudderButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RecenterButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	float DisplayedRudderCmd = 0.f;
	float DisplayedYawRate = 0.f;
	float CurrentHeadingDeg = 0.f;
	bool bRudderHoldCached = false;
	bool bDragging = false;
	float LastDragLocalX = 0.f;
};
