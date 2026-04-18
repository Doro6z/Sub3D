#pragma once

#include "CoreMinimal.h"
#include "DraggableInstrumentWindowWidget.h"
#include "HelmNavigationDisplayComponent.h"
#include "ReconstructionViewWidget.generated.h"

class UHelmNavigationDisplayComponent;

UENUM(BlueprintType)
enum class EReconstructionViewMode : uint8
{
	CrossSection UMETA(DisplayName = "Cross Section"),
	ForwardProfile UMETA(DisplayName = "Forward Profile")
};

UCLASS(Blueprintable)
class SUB3D_API UReconstructionViewWidget : public UDraggableInstrumentWindowWidget
{
	GENERATED_BODY()

public:
	UReconstructionViewWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Reconstruction")
	void InitForReconstructionView(UHelmNavigationDisplayComponent* InDisplayComponent);

	UFUNCTION(BlueprintPure, Category = "Reconstruction")
	bool IsReconstructionViewBound() const;

	UFUNCTION(BlueprintPure, Category = "Reconstruction")
	UHelmNavigationDisplayComponent* GetBoundNavigationDisplay() const;

	UFUNCTION(BlueprintCallable, Category = "Reconstruction")
	void SetViewMode(EReconstructionViewMode InViewMode);

	UFUNCTION(BlueprintPure, Category = "Reconstruction")
	EReconstructionViewMode GetViewMode() const
	{
		return ViewMode;
	}

	UFUNCTION(BlueprintCallable, Category = "Reconstruction")
	void SetBlendAlpha01(float InBlendAlpha01);

	UFUNCTION(BlueprintPure, Category = "Reconstruction")
	float GetBlendAlpha01() const
	{
		return BlendAlpha01;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reconstruction")
	EReconstructionViewMode ViewMode = EReconstructionViewMode::CrossSection;

	// Legacy property kept for asset compatibility. The split view no longer blends.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reconstruction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlendAlpha01 = 0.f;

	// Legacy property kept for asset compatibility. Content drag interaction is disabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reconstruction")
	bool bEnableBlendInteraction = false;

	// Legacy property kept for asset compatibility. No runtime effect in split mode.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reconstruction", meta = (ClampMin = "32.0"))
	float BlendSensitivityPx = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reconstruction|Style")
	FLinearColor VelocityColor = FLinearColor(1.f, 0.84f, 0.20f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reconstruction|Style")
	FLinearColor SafeColor = FLinearColor(0.24f, 0.82f, 1.f, 1.f);

protected:
	virtual int32 PaintInstrumentContent(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled,
		const FSlateRect& ContentRect) const override;

	virtual bool WantsContentInteraction() const override
	{
		return false;
	}

	virtual FReply HandleContentMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition) override;
	virtual FReply HandleContentMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition) override;
	virtual FReply HandleContentMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition) override;
	virtual void HandleContentMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	void RefreshWindowTitle();

	TWeakObjectPtr<UHelmNavigationDisplayComponent> CachedDisplayComponent;
	bool bAdjustingBlend = false;
	float BlendDragStartScreenX = 0.f;
	float BlendDragStartValue = 0.f;
};
