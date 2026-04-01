#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "DraggableInstrumentWindowWidget.generated.h"

struct FPointerEvent;

UCLASS(Abstract, Blueprintable)
class SUB3D_API UDraggableInstrumentWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDraggableInstrumentWindowWidget(const FObjectInitializer& ObjectInitializer);

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category = "Instrument")
	void SetWindowCollapsed(bool bInCollapsed);

	UFUNCTION(BlueprintPure, Category = "Instrument")
	bool IsWindowCollapsed() const
	{
		return bCollapsed;
	}

	UFUNCTION(BlueprintCallable, Category = "Instrument")
	void SetWindowTitle(const FText& InTitle);

	UFUNCTION(BlueprintPure, Category = "Instrument")
	FText GetWindowTitle() const
	{
		return WindowTitle;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument")
	FText WindowTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument")
	bool bDraggable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument")
	bool bCollapsible = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument")
	bool bCollapsed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument", meta = (ClampMin = "18.0"))
	float TitleBarHeightPx = 24.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument", meta = (ClampMin = "0.0"))
	float ContentPaddingPx = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument", meta = (ClampMin = "8.0"))
	float CollapseButtonWidthPx = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument|Style")
	FLinearColor WindowBackgroundColor = FLinearColor(0.03f, 0.05f, 0.07f, 0.88f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument|Style")
	FLinearColor TitleBarColor = FLinearColor(0.05f, 0.08f, 0.10f, 0.96f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument|Style")
	FLinearColor FrameColor = FLinearColor(0.17f, 0.45f, 0.50f, 0.90f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument|Style")
	FLinearColor AccentColor = FLinearColor(0.30f, 0.95f, 0.88f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instrument|Style")
	FLinearColor WarningColor = FLinearColor(1.f, 0.38f, 0.22f, 1.f);

protected:
	virtual int32 PaintInstrumentContent(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled,
		const FSlateRect& ContentRect) const;

	virtual bool WantsContentInteraction() const
	{
		return false;
	}

	virtual FReply HandleContentMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition);
	virtual FReply HandleContentMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition);
	virtual FReply HandleContentMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, const FVector2D& LocalMousePosition);
	virtual void HandleContentMouseLeave(const FPointerEvent& InMouseEvent);

	FSlateRect GetTitleBarLocalRect(const FVector2D& LocalSize) const;
	FSlateRect GetCollapseButtonLocalRect(const FVector2D& LocalSize) const;
	FSlateRect GetContentLocalRect(const FVector2D& LocalSize) const;
	bool IsPointInsideLocalRect(const FVector2D& LocalPoint, const FSlateRect& LocalRect) const;
	FReply MakeHandledReply(bool bCaptureMouse) const;

private:
	bool bDraggingWindow = false;
	bool bPendingCollapseToggle = false;
	FVector2D DragStartScreenPosition = FVector2D::ZeroVector;
	FVector2D DragStartRenderTranslation = FVector2D::ZeroVector;
};
