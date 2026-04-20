#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HelmCockpitState.h"
#include "HelmDiveBoardWidget.generated.h"

class UButton;
class UTextBlock;

// Pitch ribbon + ballast pipes combined. One instrument for vertical
// command.
// Mode buttons: DIRECT / HOLD VERT / SURFACE / DIVE.
UCLASS(Blueprintable)
class SUB3D_API UHelmDiveBoardWidget : public UUserWidget
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

private:
	UFUNCTION() void HandleDirect();
	UFUNCTION() void HandleHoldVert();
	UFUNCTION() void HandleSurface();
	UFUNCTION() void HandleDiveCommand();

	void BuildWidgetTree();
	void RefreshFromHelmData();

	UPROPERTY(Transient)
	TObjectPtr<UButton> DirectButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HoldVertButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SurfaceButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DiveButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	// Up to 2 ballast tanks cached for painting; larger subs will need a
	// dynamic array of pipes — out of scope here.
	float DisplayedPitchDeg = 0.f;
	float DisplayedTrimCmd = 0.f;
	float DisplayedTank0Fill = 0.5f;
	float DisplayedTank0Target = 0.5f;
	float DisplayedTank1Fill = 0.5f;
	float DisplayedTank1Target = 0.5f;
	int32 CachedTankCount = 0;
	bool bAutoDepthActiveCached = false;
	bool bAutoDepthEnabledCached = false;
};
