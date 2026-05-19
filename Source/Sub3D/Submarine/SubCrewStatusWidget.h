#pragma once

#include "CoreMinimal.h"
#include "SubPlayerHUDWidget.h"
#include "SubCrewStatusWidget.generated.h"

/**
 * Minimal runtime character HUD drawn directly with Slate.
 * Editor setup: create a Widget Blueprint using this class and assign it to PC_SubPlayerController.HUDWidgetClass.
 */
UCLASS(BlueprintType, Blueprintable)
class SUB3D_API USubCrewStatusWidget : public USubPlayerHUDWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Layout", meta = (ClampMin = "160.0"))
	FVector2D PanelSizePx = FVector2D(330.f, 122.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Layout", meta = (ClampMin = "0.0"))
	float PanelLeftMarginPx = 34.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Layout", meta = (ClampMin = "0.0"))
	float PanelBottomMarginPx = 34.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Layout", meta = (ClampMin = "4.0"))
	float BarHeightPx = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Layout", meta = (ClampMin = "0.0"))
	float InteractionPromptBottomMarginPx = 92.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Style")
	FLinearColor PanelColor = FLinearColor(0.015f, 0.018f, 0.022f, 0.58f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Style")
	FLinearColor PanelEdgeColor = FLinearColor(0.55f, 0.72f, 0.82f, 0.22f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Style")
	FLinearColor TextColor = FLinearColor(0.82f, 0.91f, 0.94f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Style")
	FLinearColor MutedTextColor = FLinearColor(0.45f, 0.55f, 0.60f, 0.82f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Style")
	FLinearColor HealthColor = FLinearColor(0.90f, 0.22f, 0.16f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Style")
	FLinearColor WaterColor = FLinearColor(0.13f, 0.48f, 0.92f, 0.90f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Style")
	FLinearColor SprintReadyColor = FLinearColor(0.08f, 0.86f, 0.92f, 0.96f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Style")
	FLinearColor SprintActiveColor = FLinearColor(0.80f, 0.98f, 1.00f, 1.00f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|HUD|Style")
	FLinearColor PostureColor = FLinearColor(0.86f, 0.78f, 0.48f, 0.90f);
};
